#include "stdafx.h"
#include "CreatePacket.h"
#include "Protocol.h"
Packet *PACKET_SC_CHAT_RES_LOGIN (BYTE Status, INT64 AccountNo)
{
	WORD Type = en_PACKET_CS_CHAT_RES_LOGIN;
	Packet *pPacket = Packet::Alloc ();
	*pPacket << Type;
	*pPacket << Status;
	*pPacket << AccountNo;

	return pPacket;
}

Packet * PACKET_SC_CHAT_REQ_SECTOR_MOVE (INT64 AccountNo, WORD PosX, WORD PosY)
{
	WORD Type = en_PACKET_CS_CHAT_RES_SECTOR_MOVE;
	Packet *pPacket = Packet::Alloc ();
	*pPacket << Type;
	*pPacket << AccountNo;
	*pPacket << PosX;
	*pPacket << PosY;

	return pPacket;
}

//------------------------------------------------------------
// 채팅서버 채팅보내기 응답  (다른 클라가 보낸 채팅도 이걸로 받음)
//
//	{
//		WORD	Type
//
//		INT64	AccountNo
//		WCHAR	ID[20]						// null 포함
//		WCHAR	Nickname[20]				// null 포함
//		
//		WORD	MessageLen
//		WCHAR	Message[MessageLen]		// null 미포함
//	}
//
//------------------------------------------------------------
Packet * PACKET_SC_CHAT_REQ_MESSAGE (INT64 AccountNo, char * ID, int ID_Size, char * Nick, int Nick_Size, char * Msg, WORD Msg_Len)
{
	WORD Type = en_PACKET_CS_CHAT_RES_MESSAGE;
	Packet *pPacket = Packet::Alloc ();
	*pPacket << Type;
	*pPacket << AccountNo;
	pPacket->PutData (ID, ID_Size);
	pPacket->PutData (Nick, Nick_Size);
	*pPacket << Msg_Len;
	pPacket->PutData (Msg, Msg_Len);

	return pPacket;
}
