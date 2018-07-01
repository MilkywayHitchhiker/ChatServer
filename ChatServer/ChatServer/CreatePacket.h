#pragma once
#include"PacketPool.h"


Packet *PACKET_SC_CHAT_RES_LOGIN (BYTE Status, INT64 AccountNo);


Packet *PACKET_SC_CHAT_REQ_SECTOR_MOVE (INT64 AccountNo, WORD PosX, WORD PosY);


Packet *PACKET_SC_CHAT_REQ_MESSAGE (INT64 AccountNo, char *ID,int ID_Size, char *Nick,int Nick_Size, char *Msg, WORD Msg_Len);