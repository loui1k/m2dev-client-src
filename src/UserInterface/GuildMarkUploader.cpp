#include "StdAfx.h"
#include "GuildMarkUploader.h"
#include "Packet.h"

#include "stb_image.h"
#include "stb_image_write.h"

#ifdef __VTUNE__
#else

CGuildMarkUploader::CGuildMarkUploader()
 : m_pbySymbolBuf(NULL)
{
	SetRecvBufferSize(1024);
	SetSendBufferSize(1024);
	__Inialize();
}

CGuildMarkUploader::~CGuildMarkUploader()
{
	__OfflineState_Set();
}

void CGuildMarkUploader::Disconnect()
{
	__OfflineState_Set();
}

bool CGuildMarkUploader::IsCompleteUploading()
{
	return STATE_OFFLINE == m_eState;
}

bool CGuildMarkUploader::__Save(const char* c_szFileName)
{
	/*
	int width = CGuildMarkImage::WIDTH;
	int height = CGuildMarkImage::HEIGHT;
	std::vector<unsigned char> rgba(width * height * 4);

	for (int i = 0; i < width * height; ++i) {
		rgba[i * 4 + 0] = m_kMark.m_apxBuf[i * 4 + 2]; // R
		rgba[i * 4 + 1] = m_kMark.m_apxBuf[i * 4 + 1]; // G
		rgba[i * 4 + 2] = m_kMark.m_apxBuf[i * 4 + 0]; // B
		rgba[i * 4 + 3] = m_kMark.m_apxBuf[i * 4 + 3]; // A
	}

	// Save as PNG
	if (!stbi_write_png(c_szFileName, width, height, 4, rgba.data(), width * 4)) {
		return false;
	}

	return true;
	*/
	return true;
}

bool CGuildMarkUploader::__Load(const char* c_szFileName, UINT* peError)
{
	int width, height, channels;
	unsigned char* data = stbi_load(c_szFileName, &width, &height, &channels, 4); // force RGBA

	if (!data) {
		*peError = ERROR_LOAD;
		return false;
	}

	if (width != SGuildMark::WIDTH) {
		stbi_image_free(data);
		*peError = ERROR_WIDTH;
		return false;
	}

	if (height != SGuildMark::HEIGHT) {
		stbi_image_free(data);
		*peError = ERROR_HEIGHT;
		return false;
	}

	// Copy into our mark buffer (BGRA expected)
	for (uint32_t i = 0; i < width * height; ++i) {
		const uint8_t R = data[i * 4 + 0];
		const uint8_t G = data[i * 4 + 1];
		const uint8_t B = data[i * 4 + 2];
		const uint8_t A = data[i * 4 + 3];

		m_kMark.m_apxBuf[i] = (uint32_t(A) << 24) | (uint32_t(R) << 16) | (uint32_t(G) << 8) | uint32_t(B);
	}

	stbi_image_free(data);
	return true;
}

bool CGuildMarkUploader::__LoadSymbol(const char* c_szFileName, UINT* peError)
{
	int width, height, channels;
	unsigned char* data = stbi_load(c_szFileName, &width, &height, &channels, 4);

	if (!data) {
		*peError = ERROR_LOAD;
		return false;
	}

	if (width != 64) {
		stbi_image_free(data);
		*peError = ERROR_WIDTH;
		return false;
	}

	if (height != 128) {
		stbi_image_free(data);
		*peError = ERROR_HEIGHT;
		return false;
	}

	stbi_image_free(data);

	// Now read raw file into m_pbySymbolBuf (same as original code did)
	FILE* file = fopen(c_szFileName, "rb");
	if (!file) {
		*peError = ERROR_LOAD;
		return false;
	}

	fseek(file, 0, SEEK_END);
	m_dwSymbolBufSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	m_pbySymbolBuf = new uint8_t[m_dwSymbolBufSize];
	fread(m_pbySymbolBuf, m_dwSymbolBufSize, 1, file);
	fclose(file);

	m_dwSymbolCRC32 = GetFileCRC32(c_szFileName);
	return true;
}

bool CGuildMarkUploader::Connect(const CNetworkAddress& c_rkNetAddr, DWORD dwHandle, DWORD dwRandomKey, DWORD dwGuildID, const char* c_szFileName, UINT* peError)
{
	__OfflineState_Set();
	SetRecvBufferSize(1024);
	SetSendBufferSize(1024);

	if (!CNetworkStream::Connect(c_rkNetAddr))
	{
		*peError=ERROR_CONNECT;
		return false;
	}

	m_dwSendType=SEND_TYPE_MARK;
	m_dwHandle=dwHandle;
	m_dwRandomKey=dwRandomKey;
	m_dwGuildID=dwGuildID;

	if (!__Load(c_szFileName, peError))
		return false;
		
	//if (!__Save(CGraphicMarkInstance::GetImageFileName().c_str()))
	//	return false;
	//CGraphicMarkInstance::ReloadImageFile();
	return true;
}

bool CGuildMarkUploader::ConnectToSendSymbol(const CNetworkAddress& c_rkNetAddr, DWORD dwHandle, DWORD dwRandomKey, DWORD dwGuildID, const char* c_szFileName, UINT* peError)
{
	__OfflineState_Set();
	SetRecvBufferSize(1024);
	SetSendBufferSize(64*1024);

	if (!CNetworkStream::Connect(c_rkNetAddr))
	{
		*peError=ERROR_CONNECT;
		return false;
	}

	m_dwSendType=SEND_TYPE_SYMBOL;
	m_dwHandle=dwHandle;
	m_dwRandomKey=dwRandomKey;
	m_dwGuildID=dwGuildID;

	if (!__LoadSymbol(c_szFileName, peError))
		return false;

	return true;
}

void CGuildMarkUploader::Process()
{
	CNetworkStream::Process();

	if (!__StateProcess())
	{
		__OfflineState_Set();
		Disconnect();
	}
}

void CGuildMarkUploader::OnConnectFailure()
{
	__OfflineState_Set();
}

void CGuildMarkUploader::OnConnectSuccess()
{
	__LoginState_Set();
}

void CGuildMarkUploader::OnRemoteDisconnect()
{
	__OfflineState_Set();
}

void CGuildMarkUploader::OnDisconnect()
{
	__OfflineState_Set();
}

void CGuildMarkUploader::__Inialize()
{
	m_eState = STATE_OFFLINE;

	m_dwGuildID = 0;
	m_dwHandle = 0;
	m_dwRandomKey = 0;

	if (m_pbySymbolBuf)
	{
		delete[] m_pbySymbolBuf;
	}

	m_dwSymbolBufSize = 0;
	m_pbySymbolBuf = NULL;
}

bool CGuildMarkUploader::__StateProcess()
{
	switch (m_eState)
	{
		case STATE_LOGIN:
			return __LoginState_Process();
			break;
	}

	return true;
}

void CGuildMarkUploader::__OfflineState_Set()
{
	__Inialize();
}

void CGuildMarkUploader::__CompleteState_Set()
{
	m_eState=STATE_COMPLETE;

	__OfflineState_Set();
}


void CGuildMarkUploader::__LoginState_Set()
{
	m_eState=STATE_LOGIN;
}

bool CGuildMarkUploader::__LoginState_Process()
{
	if (!__AnalyzePacket(HEADER_GC_PHASE, sizeof(TPacketGCPhase), &CGuildMarkUploader::__LoginState_RecvPhase))
		return false;

	if (!__AnalyzePacket(HEADER_GC_HANDSHAKE, sizeof(TPacketGCHandshake), &CGuildMarkUploader::__LoginState_RecvHandshake))
		return false;

	if (!__AnalyzePacket(HEADER_GC_PING, sizeof(TPacketGCPing), &CGuildMarkUploader::__LoginState_RecvPing))
		return false;

#ifdef _IMPROVED_PACKET_ENCRYPTION_
	if (!__AnalyzePacket(HEADER_GC_KEY_AGREEMENT, sizeof(TPacketKeyAgreement), &CGuildMarkUploader::__LoginState_RecvKeyAgreement))
		return false;

	if (!__AnalyzePacket(HEADER_GC_KEY_AGREEMENT_COMPLETED, sizeof(TPacketKeyAgreementCompleted), &CGuildMarkUploader::__LoginState_RecvKeyAgreementCompleted))
		return false;
#endif

	return true;
}

bool CGuildMarkUploader::__SendMarkPacket()
{
	TPacketCGMarkUpload kPacketMarkUpload;
	kPacketMarkUpload.header=HEADER_CG_MARK_UPLOAD;
	kPacketMarkUpload.gid=m_dwGuildID;

	assert(sizeof(kPacketMarkUpload.image) == sizeof(m_kMark.m_apxBuf));
	memcpy(kPacketMarkUpload.image, m_kMark.m_apxBuf, sizeof(kPacketMarkUpload.image));

	if (!Send(sizeof(kPacketMarkUpload), &kPacketMarkUpload))
		return false;

	return true;
}
bool CGuildMarkUploader::__SendSymbolPacket()
{
	if (!m_pbySymbolBuf)
		return false;

	TPacketCGSymbolUpload kPacketSymbolUpload;
	kPacketSymbolUpload.header=HEADER_CG_GUILD_SYMBOL_UPLOAD;
	kPacketSymbolUpload.handle=m_dwGuildID;
	kPacketSymbolUpload.size=sizeof(TPacketCGSymbolUpload) + m_dwSymbolBufSize;

	if (!Send(sizeof(TPacketCGSymbolUpload), &kPacketSymbolUpload))
		return false;
	if (!Send(m_dwSymbolBufSize, m_pbySymbolBuf))
		return false;

#ifdef _DEBUG
	printf("__SendSymbolPacket : [GuildID:%d/PacketSize:%d/BufSize:%d/CRC:%d]\n", m_dwGuildID, kPacketSymbolUpload.size, m_dwSymbolBufSize, m_dwSymbolCRC32);
#endif

	CNetworkStream::__SendInternalBuffer();
	__CompleteState_Set();

	return true;
}

bool CGuildMarkUploader::__LoginState_RecvPhase()
{
	TPacketGCPhase kPacketPhase;
	if (!Recv(sizeof(kPacketPhase), &kPacketPhase))
		return false;

	if (kPacketPhase.phase==PHASE_LOGIN)
	{
#ifndef _IMPROVED_PACKET_ENCRYPTION_
		const char* key = GetSecurityKey();
		SetSecurityMode(true, key);
#endif

		if (SEND_TYPE_MARK == m_dwSendType)
		{
			if (!__SendMarkPacket())
				return false;
		}
		else if (SEND_TYPE_SYMBOL == m_dwSendType)
		{
			if (!__SendSymbolPacket())
				return false;
		}
	}

	return true;
}

bool CGuildMarkUploader::__LoginState_RecvHandshake()
{
	TPacketGCHandshake kPacketHandshake;
	if (!Recv(sizeof(kPacketHandshake), &kPacketHandshake))
		return false;

	{
		TPacketCGMarkLogin kPacketMarkLogin;
		kPacketMarkLogin.header=HEADER_CG_MARK_LOGIN;
		kPacketMarkLogin.handle=m_dwHandle;
		kPacketMarkLogin.random_key=m_dwRandomKey;
		if (!Send(sizeof(kPacketMarkLogin), &kPacketMarkLogin))
			return false;
	}

	return true;
}

bool CGuildMarkUploader::__LoginState_RecvPing()
{
	TPacketGCPing kPacketPing;
	if (!Recv(sizeof(kPacketPing), &kPacketPing))
		return false;

	TPacketCGPong kPacketPong;
	kPacketPong.bHeader = HEADER_CG_PONG;

	if (!Send(sizeof(TPacketCGPong), &kPacketPong))
		return false;

	if (IsSecurityMode())
		return SendSequence();
	else
		return true;
}

#ifdef _IMPROVED_PACKET_ENCRYPTION_
bool CGuildMarkUploader::__LoginState_RecvKeyAgreement()
{
	TPacketKeyAgreement packet;
	if (!Recv(sizeof(packet), &packet))
	{
		return false;
	}

	Tracenf("KEY_AGREEMENT RECV %u", packet.wDataLength);

	TPacketKeyAgreement packetToSend;
	size_t dataLength = TPacketKeyAgreement::MAX_DATA_LEN;
	size_t agreedLength = Prepare(packetToSend.data, &dataLength);
	if (agreedLength == 0)
	{
		// 초기화 실패
		Disconnect();
		return false;
	}
	assert(dataLength <= TPacketKeyAgreement::MAX_DATA_LEN);

	if (Activate(packet.wAgreedLength, packet.data, packet.wDataLength))
	{
		// Key agreement 성공, 응답 전송
		packetToSend.bHeader = HEADER_CG_KEY_AGREEMENT;
		packetToSend.wAgreedLength = (WORD)agreedLength;
		packetToSend.wDataLength = (WORD)dataLength;

		if (!Send(sizeof(packetToSend), &packetToSend))
		{
			Tracen(" CAccountConnector::__AuthState_RecvKeyAgreement - SendKeyAgreement Error");
			return false;
		}
		Tracenf("KEY_AGREEMENT SEND %u", packetToSend.wDataLength);
	}
	else
	{
		// 키 협상 실패
		Disconnect();
		return false;
	}
	return true;
}

bool CGuildMarkUploader::__LoginState_RecvKeyAgreementCompleted()
{
	TPacketKeyAgreementCompleted packet;
	if (!Recv(sizeof(packet), &packet))
	{
		return false;
	}

	Tracenf("KEY_AGREEMENT_COMPLETED RECV");

	ActivateCipher();

	return true;
}
#endif // _IMPROVED_PACKET_ENCRYPTION_

bool CGuildMarkUploader::__AnalyzePacket(UINT uHeader, UINT uPacketSize, bool (CGuildMarkUploader::*pfnDispatchPacket)())
{
	BYTE bHeader;
	if (!Peek(sizeof(bHeader), &bHeader))
		return true;

	if (bHeader!=uHeader)
		return true;

	if (!Peek(uPacketSize))
		return true;

	return (this->*pfnDispatchPacket)();
}
#endif
