#include "StdAfx.h"
#include "PythonNetworkStream.h"
#include "PythonApplication.h"
#include "Packet.h"

// HandShake ---------------------------------------------------------------------------
void CPythonNetworkStream::HandShakePhase()
{
	TPacketHeader header;

	if (!CheckPacket(&header))
		return;

	switch (header)
	{
		case HEADER_GC_PHASE:
			if (RecvPhasePacket())
				return;
			break;

		case HEADER_GC_BINDUDP:
			{
				TPacketGCBindUDP BindUDP;

				if (!Recv(sizeof(TPacketGCBindUDP), &BindUDP))
					return;

				return;
			}
			break;

		case HEADER_GC_HANDSHAKE:
			{
				if (!Recv(sizeof(TPacketGCHandshake), &m_HandshakeData))
					return;

				Tracenf("HANDSHAKE RECV %u %d", m_HandshakeData.dwTime, m_HandshakeData.lDelta);

				ELTimer_SetServerMSec(m_HandshakeData.dwTime+ m_HandshakeData.lDelta);

				m_HandshakeData.dwTime = m_HandshakeData.dwTime + m_HandshakeData.lDelta + m_HandshakeData.lDelta;
				m_HandshakeData.lDelta = 0;

				Tracenf("HANDSHAKE SEND %u", m_HandshakeData.dwTime);

				if (!Send(sizeof(TPacketGCHandshake), &m_HandshakeData))
				{
					assert(!"Failed Sending Handshake");
					return;
				}

				CTimer::Instance().SetBaseTime();
				return;
			}
			break;

		case HEADER_GC_PING:
			RecvPingPacket();
			return;
			break;

		case HEADER_GC_KEY_CHALLENGE:
			RecvKeyChallenge();
			return;
			break;

		case HEADER_GC_KEY_COMPLETE:
			RecvKeyComplete();
			return;
			break;
	}

	RecvErrorPacket(header);
}

void CPythonNetworkStream::SetHandShakePhase()
{
	if ("HandShake"!=m_strPhase)
		m_phaseLeaveFunc.Run();

	Tracen("");
	Tracen("## Network - Hand Shake Phase ##");
	Tracen("");

	m_strPhase = "HandShake";

	m_dwChangingPhaseTime = ELTimer_GetMSec();
	m_phaseProcessFunc.Set(this, &CPythonNetworkStream::HandShakePhase);
	m_phaseLeaveFunc.Set(this, &CPythonNetworkStream::__LeaveHandshakePhase);

	SetGameOnline();

	if (__DirectEnterMode_IsSet())
	{
	}
	else
	{
		PyCallClassMemberFunc(m_apoPhaseWnd[PHASE_WINDOW_LOGIN], "OnHandShake", Py_BuildValue("()"));
	}
}

bool CPythonNetworkStream::RecvHandshakePacket()
{
	TPacketGCHandshake kHandshakeData;
	if (!Recv(sizeof(TPacketGCHandshake), &kHandshakeData))
		return false;

	Tracenf("HANDSHAKE RECV %u %d", kHandshakeData.dwTime, kHandshakeData.lDelta);

	m_kServerTimeSync.m_dwChangeServerTime = kHandshakeData.dwTime + kHandshakeData.lDelta;
	m_kServerTimeSync.m_dwChangeClientTime = ELTimer_GetMSec();

	kHandshakeData.dwTime = kHandshakeData.dwTime + kHandshakeData.lDelta + kHandshakeData.lDelta;
	kHandshakeData.lDelta = 0;

	Tracenf("HANDSHAKE SEND %u", kHandshakeData.dwTime);

	kHandshakeData.header = HEADER_CG_TIME_SYNC;
	if (!Send(sizeof(TPacketGCHandshake), &kHandshakeData))
	{
		assert(!"Failed Sending Handshake");
		return false;
	}

	SendSequence();

	return true;
}

bool CPythonNetworkStream::RecvHandshakeOKPacket()
{
	TPacketGCBlank kBlankPacket;
	if (!Recv(sizeof(TPacketGCBlank), &kBlankPacket))
		return false;

	DWORD dwDelta=ELTimer_GetMSec()-m_kServerTimeSync.m_dwChangeClientTime;
	ELTimer_SetServerMSec(m_kServerTimeSync.m_dwChangeServerTime+dwDelta);

	Tracenf("HANDSHAKE OK RECV %u %u", m_kServerTimeSync.m_dwChangeServerTime, dwDelta);

	return true;
}

// Secure key exchange handlers (libsodium/XChaCha20-Poly1305)
bool CPythonNetworkStream::RecvKeyChallenge()
{
	TPacketGCKeyChallenge packet;
	if (!Recv(sizeof(packet), &packet))
	{
		return false;
	}

	Tracen("SECURE KEY_CHALLENGE RECV");

	SecureCipher& cipher = GetSecureCipher();
	if (!cipher.Initialize())
	{
		TraceError("Failed to initialize SecureCipher");
		Disconnect();
		return false;
	}

	if (!cipher.ComputeClientKeys(packet.server_pk))
	{
		TraceError("Failed to compute client session keys");
		Disconnect();
		return false;
	}

	TPacketCGKeyResponse response;
	response.bHeader = HEADER_CG_KEY_RESPONSE;
	cipher.GetPublicKey(response.client_pk);
	cipher.ComputeChallengeResponse(packet.challenge, response.challenge_response);

	if (!Send(sizeof(response), &response))
	{
		TraceError("Failed to send KeyResponse");
		return false;
	}

	Tracen("SECURE KEY_RESPONSE SENT");
	return true;
}

bool CPythonNetworkStream::RecvKeyComplete()
{
	TPacketGCKeyComplete packet;
	if (!Recv(sizeof(packet), &packet))
	{
		return false;
	}

	Tracen("SECURE KEY_COMPLETE RECV");

	SecureCipher& cipher = GetSecureCipher();

	uint8_t decrypted_token[SecureCipher::SESSION_TOKEN_SIZE];
	if (!cipher.DecryptToken(packet.encrypted_token, sizeof(packet.encrypted_token),
	                          packet.nonce, decrypted_token))
	{
		TraceError("Failed to decrypt session token");
		Disconnect();
		return false;
	}

	cipher.SetSessionToken(decrypted_token);
	cipher.SetActivated(true);

	Tracen("SECURE CIPHER ACTIVATED");
	return true;
}
