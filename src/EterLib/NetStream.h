#pragma once

#include "EterBase/SecureCipher.h"
#include "NetAddress.h"

#include <pcg_random.hpp>

#define SEQUENCE_SEED 0

class CNetworkStream
{
	public:
		CNetworkStream();
		virtual ~CNetworkStream();

		void SetRecvBufferSize(int recvBufSize);
		void SetSendBufferSize(int sendBufSize);

		bool IsSecurityMode();

		int	GetRecvBufferSize();

		void Clear();
		void ClearRecvBuffer();

		void Process();

		bool Connect(const CNetworkAddress& c_rkNetAddr, int limitSec = 3);
		bool Connect(const char* c_szAddr, int port, int limitSec = 3);
		bool Connect(DWORD dwAddr, int port, int limitSec = 3);
		void Disconnect();

		bool Peek(int len);
		bool Peek(int len, char* pDestBuf);
		bool Recv(int len);
		bool Recv(int len, char* pDestBuf);
		bool Send(int len, const char* pSrcBuf);

		bool Peek(int len, void* pDestBuf);
		bool Recv(int len, void* pDestBuf);

		bool Send(int len, const void* pSrcBuf);
		bool SendFlush(int len, const void* pSrcBuf);

		bool IsOnline();

		void SetPacketSequenceMode(bool isOn);
		bool SendSequence();
		uint8_t GetNextSequence();

	protected:
		virtual void OnConnectSuccess();
		virtual void OnConnectFailure();
		virtual void OnRemoteDisconnect();
		virtual void OnDisconnect();
		virtual bool OnProcess();

		bool __SendInternalBuffer();
		bool __RecvInternalBuffer();

		void __PopSendBuffer();

		int __GetSendBufferSize();

		// Secure cipher methods (libsodium)
		SecureCipher& GetSecureCipher() { return m_secureCipher; }
		bool IsSecureCipherActivated() const { return m_secureCipher.IsActivated(); }
		void ActivateSecureCipher() { m_secureCipher.SetActivated(true); }

	private:
		time_t	m_connectLimitTime;

		char*	m_recvBuf;
		int		m_recvBufSize;
		int		m_recvBufInputPos;
		int		m_recvBufOutputPos;

		char*	m_sendBuf;
		int		m_sendBufSize;
		int		m_sendBufInputPos;
		int		m_sendBufOutputPos;

		bool	m_isOnline;

		// Secure cipher (libsodium/XChaCha20-Poly1305)
		SecureCipher m_secureCipher;

		SOCKET	m_sock;

		CNetworkAddress m_addr;

		// Sequence
		pcg32					m_SequenceGenerator;
		bool					m_bUseSequence;
};
