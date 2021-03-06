// CLanServer.cpp : 콘솔 응용 프로그램에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include "NetServer.h"
#include "Protocol.h"
#include "ServerConfig.h"
#include "CreatePacket.h"
#include <map>
#include <list>

using namespace std;
CCrashDump Dump;

#define df_Sector_X 50
#define df_Sector_Y 50
#define LEAVE 9999


class ChatServer :public CNetServer
{
public :
	struct QueuePack
	{
		UINT64 SessionID;
		WORD Type;
		Packet *packet;
		WCHAR IP[32];
	};

	CQueue_LF<QueuePack *> UpdateQueue;
	CMemoryPool<QueuePack> *QueuePool;

private:
	struct Player
	{
		UINT64 SessionID;
		
		INT64	AccountNo;
		WCHAR	ID[20];		// null 포함
		WCHAR	Nickname[20];		// null 포함
		char	SessionKey[64];

		WCHAR IP[32];

		WORD	MessageLen;
		WCHAR	*Message;		// MessageLen/2 null 미포함

		int PosX;
		int PosY;

		DWORD Last_Message_Time;
	};
	
	CMemoryPool<Player> *Player_Pool;

	//플레이어 리스트
	map<UINT64, Player *> Playerlist;

	//케릭터 섹터
	list<Player *> g_Sector[df_Sector_Y][df_Sector_X];

	HANDLE Thread;
	HANDLE WakeUp;
	bool Stopflag;
	UINT _UpdateTPS;
	
public:
	ChatServer (void)
	{
	}
	~ChatServer (void)
	{
	}
	virtual void OnStart (void)
	{
		QueuePool = new CMemoryPool<QueuePack> (0);
		Player_Pool = new CMemoryPool<Player> (0);
		WakeUp = CreateEvent (NULL, FALSE, FALSE, NULL);
		Stopflag = false;

		Thread = ( HANDLE )_beginthreadex (NULL, 0, UpdateThread, (void *)this, NULL, NULL);

		return;
	}
	virtual void OnStop (void)
	{
		Stopflag = true;
		SetEvent (WakeUp);

	//	delete QueuePool;
		delete Player_Pool;
		return;
	}
	virtual void OnRecv (UINT64 SessionID, Packet *p)
	{
		QueuePack *pack = QueuePool->Alloc();

		p->Add ();
		pack->packet = p;
		*p >> pack->Type;
		pack->SessionID = SessionID;
		if ( UpdateQueue.Enqueue (pack) == false )
		{
			LOG_LOG (L"ChatServer", LOG_ERROR, L"UPDATE QUEUE OverFlow");
			Stop ();
		}
		SetEvent (WakeUp);
		
		return;
	}
	virtual void OnSend (UINT64 SessionID, INT SendByte)
	{
		return;
	}

	virtual bool OnClientJoin (UINT64 SessionID, WCHAR *IP, int PORT)
	{
		QueuePack *pack = QueuePool->Alloc ();
		pack->Type = en_PACKET_CS_CHAT_SERVER;
		pack->SessionID = SessionID;
		lstrcpyW (pack->IP, IP);

		if ( UpdateQueue.Enqueue (pack) == false )
		{
			LOG_LOG (L"Update", LOG_ERROR, L"UPDATE QUEUE OverFlow");
			Stop ();
		}

		SetEvent (WakeUp);
		return true;
	}
	virtual void OnClientLeave (UINT64 SessionID)
	{
		QueuePack *pack = QueuePool->Alloc ();
		pack->Type = LEAVE;
		pack->SessionID = SessionID;

		if ( UpdateQueue.Enqueue (pack) == false )
		{
			LOG_LOG (L"Update", LOG_ERROR, L"UPDATE QUEUE OverFlow");
			Stop ();
		}

		SetEvent (WakeUp);
		return;
	}


	/*======================================================================
	//Update
	//설명 : ChatServer Main.
	//인자 : LPVOID pParam; = CLanServer this pointer 를 일로 넘겨받음.
	//리턴 : 0
	======================================================================*/
	static unsigned int WINAPI UpdateThread (LPVOID pParam)
	{
		ChatServer *p = ( ChatServer * )pParam;
		wprintf (L"Update_Thread_Start\n");
		LOG_LOG (L"Update", LOG_SYSTEM, L"Update Thread_Start");

		p->Update ();

		wprintf (L"\nUpdate_Thread_End\n");
		LOG_LOG (L"Update", LOG_SYSTEM, L"Update Thread_End");
		return 0;
	}

	int  Update (void)
	{

		Packet *pPacket;
		QueuePack *Pack;
	while ( 1 )
		{
			WaitForSingleObject (WakeUp, INFINITE);

			Pack = NULL;
			while ( 1 )
			{

				if ( UpdateQueue.Dequeue (&Pack) == false )
				{
					break;
					//continue;
				}
				try
				{
					
					pPacket = NULL;
					switch ( Pack->Type )
					{
					case en_PACKET_CS_CHAT_SERVER:
						PACKET_CS_CHAT_SERVER (Pack);
						break;

					case en_PACKET_CS_CHAT_REQ_LOGIN:
						pPacket = PACKET_CS_CHAT_REQ_LOGIN (Pack);
						break;

					case en_PACKET_CS_CHAT_REQ_SECTOR_MOVE:
						pPacket = PACKET_CS_CHAT_REQ_SECTOR_MOVE (Pack);
						break;
					case en_PACKET_CS_CHAT_REQ_MESSAGE:
						PACKET_CS_CHAT_REQ_MESSAGE (Pack);
						break;
					case en_PACKET_CS_CHAT_REQ_HEARTBEAT:
						PACKET_CS_CHAT_REQ_HEARTBEAT ();
						break;
					case LEAVE:
						PACKET_LEAVE (Pack);
						break;
					default:
						LOG_LOG (L"Update", LOG_ERROR, L"SessionID 0x%p, Type Error");
						Disconnect (Pack->SessionID);
						break;
					}
				}
				catch ( ErrorAlloc Err )
				{
					WCHAR GetErr[20];
					switch ( Err.Flag )
					{
					case Get_Error:
						swprintf_s (GetErr, L"GetData Error");

					case Put_Error:
						swprintf_s (GetErr, L"PutData Error");

					case PutHeader_Error:
						swprintf_s (GetErr, L"PutHeader Error");

					}

					LOG_LOG (L"Update", LOG_ERROR, L"SessionID 0x%p, PacketError HeaderSize = %d, DataSize = %d, GetSize = %d, PutSize = %d, ErrorType = %s", Pack->SessionID,Err.UseHeaderSize, Err.UseDataSize, Err.GetSize, Err.PutSize, GetErr);

					if ( Pack->packet != NULL )
					{
						Packet::Free (pPacket);
					}
					Disconnect (Pack->SessionID);
					QueuePool->Free (Pack);
					break;

				}
				if ( pPacket != NULL )
				{
					SendPacket (Pack->SessionID, pPacket);
					Packet::Free (pPacket);
				}
				_UpdateTPS++;
				QueuePool->Free (Pack);
			}

			if ( Stopflag )
			{
				break;
			}
		}
		return 0;
	}

	Player *FindPlayer (UINT64 SessionID)
	{
		map<UINT64, Player *>::iterator iter;

		iter = Playerlist.find (SessionID);

		if ( iter == Playerlist.end () )
		{
			return NULL;
		}

		return iter->second;
	}

	void PACKET_CS_CHAT_SERVER (QueuePack *Pack)
	{
		Player *pNewPlayer = Player_Pool->Alloc();
		lstrcpyW (pNewPlayer->IP, Pack->IP);
		pNewPlayer->PosX = -1;
		pNewPlayer->PosY = -1;
		pNewPlayer->Last_Message_Time = GetTickCount ();
		pNewPlayer->SessionID = Pack->SessionID;

		Playerlist.insert (pair<UINT64, Player *> (pNewPlayer->SessionID, pNewPlayer));
		
		return;
	}

	Packet *PACKET_CS_CHAT_REQ_LOGIN (QueuePack *Pack)
	{
		UINT64 SessionID = Pack->SessionID;

		INT64	AccountNo;
		
		WCHAR	ID[20];
		WCHAR	Nickname[20];
		char	SessionKey[64];
		Packet *pPacket = Pack->packet;

		//------------------------------------------------------------
		// 채팅서버 로그인 요청
		//
		//	{
		//		WORD	Type
		//
		//		INT64	AccountNo
		//		WCHAR	ID[20]				// null 포함
		//		WCHAR	Nickname[20]		// null 포함
		//		char	SessionKey[64];
		//	}
		//
		//------------------------------------------------------------
		*pPacket >> AccountNo;
		pPacket->GetData (( char * )ID, sizeof (ID));
		pPacket->GetData (( char * )Nickname, sizeof (Nickname));
		pPacket->GetData (SessionKey, sizeof (SessionKey));
		Packet::Free (pPacket);

		Player *pPlayer = FindPlayer (SessionID);
		if ( pPlayer == NULL )
		{
			return NULL;
		}

		lstrcpyW (pPlayer->ID, ID);
		lstrcpyW (pPlayer->Nickname, Nickname);
		memcpy(pPlayer->SessionKey, SessionKey,sizeof(SessionKey));
		pPlayer->AccountNo = AccountNo;
		pPlayer->SessionID = SessionID;
		pPlayer->Last_Message_Time = GetTickCount ();




		// 채팅서버 로그인 응답
		return PACKET_SC_CHAT_RES_LOGIN (1, AccountNo);
	}

	Packet *PACKET_CS_CHAT_REQ_SECTOR_MOVE (QueuePack *Pack)
	{
		Packet *pPacket = Pack->packet;
		UINT64 SessionID = Pack->SessionID;
		Player *pPlayer = FindPlayer (SessionID);
		if ( pPlayer == NULL )
		{
			return NULL;
		}

		pPlayer->Last_Message_Time = GetTickCount ();


		//기존 섹터 검색 및 플레이어 삭제.
		if ( pPlayer->PosX != -1 && pPlayer->PosY != -1 )
		{
			list < Player *> &SectorList = g_Sector[pPlayer->PosY][pPlayer->PosX];

			list<Player *>::iterator iter;
			for ( iter = SectorList.begin (); iter != SectorList.end ();)
			{
				if ( pPlayer == (*iter) )
				{
					SectorList.erase (iter);
					break;
				}
				iter++;
			}
		}

		INT64 AccountNo;
		WORD SectorX;
		WORD SectorY;

		*pPacket >> AccountNo;
		*pPacket >> SectorX;
		*pPacket >> SectorY;
		Packet::Free (pPacket);

		//새로운 섹터에 플레이어 삽입.
		g_Sector[SectorY][SectorX].push_back (pPlayer);
		pPlayer->PosX = SectorX;
		pPlayer->PosY = SectorY;


		return PACKET_SC_CHAT_REQ_SECTOR_MOVE (AccountNo, SectorX, SectorY);
	}

	void PACKET_CS_CHAT_REQ_MESSAGE (QueuePack *Pack)
	{
		Packet *pPacket = Pack->packet;
		UINT64 SessionID = Pack->SessionID;
		Player *pPlayer = FindPlayer (SessionID);
		if ( pPlayer == NULL )
		{
			return;
		}

		pPlayer->Last_Message_Time = GetTickCount ();


		// 채팅서버 채팅보내기 요청
		//
		//	{
		//		WORD	Type
		//
		//		INT64	AccountNo
		//		WORD	MessageLen
		//		WCHAR	Message[MessageLen]		// null 미포함
		//	}
		INT64	AccountNo;
		WORD	MessageLen;
		WCHAR	*Message;//Message[MessageLen] null 미포함
		*pPacket >> AccountNo;
		*pPacket >> MessageLen;

		Message = new WCHAR[MessageLen];
		memset (Message, 0, sizeof (MessageLen));
		pPacket->GetData (( char* )Message, MessageLen);
		Packet::Free (pPacket);


		Packet *pMsgPacket = PACKET_SC_CHAT_REQ_MESSAGE (AccountNo, ( char * )pPlayer->ID, sizeof (pPlayer->ID), ( char * )pPlayer->Nickname, sizeof (pPlayer->Nickname), ( char * )Message, MessageLen);

		delete Message;


		Player *pUser = NULL;
		//현재섹터 검색.
		if ( pPlayer->PosX != -1 && pPlayer->PosY != -1 )
		{
			list < Player *> &SectorList = g_Sector[pPlayer->PosY][pPlayer->PosX];

			//돌면서 플레이어 객체 얻어서 SendPacket 할것.
			list<Player *>::iterator iter;
			for ( iter = SectorList.begin (); iter != SectorList.end ();)
			{
				pUser = *iter;
				SendPacket (pUser->SessionID, pMsgPacket);

				iter++;
			}
		}

		Packet::Free (pMsgPacket);

		return;
	}

	void PACKET_CS_CHAT_REQ_HEARTBEAT ()
	{
		Player *pUser;
		DWORD CurrentTime = GetTickCount ();


		map<UINT64, Player *>::iterator iter;
		for ( iter = Playerlist.begin (); iter != Playerlist.end ();)
		{
			pUser = iter->second;
			if ( CurrentTime - pUser->Last_Message_Time > 40000 )
			{
				Disconnect (pUser->SessionID);
			}
			iter++;
		}


		return;
	}

	void PACKET_LEAVE (QueuePack *Pack)
	{
		UINT64 SessionID = Pack->SessionID;
		Player *pPlayer = FindPlayer (SessionID);
		if ( pPlayer == NULL )
		{
			return;
		}
		if ( pPlayer->PosX != -1 && pPlayer->PosY != -1 )
		{
			list < Player *> &SectorList = g_Sector[pPlayer->PosY][pPlayer->PosX];
			list<Player *>::iterator iter;
			for ( iter = SectorList.begin (); iter != SectorList.end ();)
			{
				if ( *iter == pPlayer )
				{
					SectorList.erase (iter);
					break;
				}
				iter++;
			}
		}

		Playerlist.erase (SessionID);
		Player_Pool->Free(pPlayer);
		return;
	}





	UINT UpdateTPS (bool Reset)
	{
		UINT UpdateTPS = _UpdateTPS;
		if ( Reset )
		{
			InterlockedExchange (( volatile LONG * )&_UpdateTPS, 0);
		}
		return UpdateTPS;
	}

	INT64 UpdateQueueUse (void)
	{
		return UpdateQueue.GetUseSize ();
	}
	UINT Up_Q_PoolAlloc (void)
	{
		return QueuePool->GetAllocCount ();
	}
	UINT Up_Q_PoolFull (void)
	{
		return QueuePool->GetFullCount ();
	}
};

ChatServer Chat;











int main ()
{
	LOG_DIRECTORY (L"LOG_FILE");
	LOG_LEVEL (LOG_WARNING, false);

	CServerConfig::Initialize ();
	wprintf (L"MainThread Start\n");
	if ( Chat.Start (_BIND_IP, _BIND_PORT, _CLIENT_MAX, _WORKER_THREAD_NUM) == false )
	{
		return 0;
	}


	UINT AcceptTotal = 0;
	UINT AcceptTPS = 0;
	UINT UpdateTPS = 0;
	UINT RecvTPS = 0;
	UINT SendTPS = 0;
	UINT ConnectSessionCnt = 0;

	UINT Player_Cnt = 0;
	int MemoryPoolCnt = 0;
	int MemoryPoolUse = 0;

	DWORD StartTime = GetTickCount ();
	DWORD HeartBeatTime = StartTime;
	DWORD EndTime;
	while ( 1 )
	{
		EndTime = GetTickCount ();
		if ( EndTime - StartTime >= 1000 )
		{
			wprintf (L"==========================\n");
			wprintf (L"Accept User Total = %d \n", AcceptTotal);
			wprintf (L"Connect Session Cnt = %d \n", ConnectSessionCnt);
			wprintf (L"AcceptTPS = %d \n", AcceptTPS);
			wprintf (L"Sec RecvTPS = %d \n", RecvTPS);
			wprintf (L"Sec SendTPS = %d \n", SendTPS);
			wprintf (L"UpdateTPS = %d \n\n", UpdateTPS);
			wprintf (L"MemoryPoolFull Cnt = %d\n", MemoryPoolCnt);
			wprintf (L"MemoryPoolUse Cnt = %d \n", MemoryPoolUse);

			wprintf (L"==========================\n");

			AcceptTotal = Chat.AcceptTotal ();
			AcceptTPS = Chat.AcceptTPS (true);
			UpdateTPS = Chat.UpdateTPS (true);
			RecvTPS = Chat.RecvTPS (true);
			SendTPS = Chat.SendTPS (true);
			ConnectSessionCnt = Chat.Use_SessionCnt ();
			MemoryPoolCnt = Chat.Full_MemPoolCnt ();
			MemoryPoolUse = Chat.Alloc_MemPoolCnt ();

			StartTime = EndTime;
		}


		if ( EndTime - StartTime >= 150000 )
		{
			ChatServer::QueuePack *pack = Chat.QueuePool->Alloc ();
			pack->Type = en_PACKET_CS_CHAT_REQ_HEARTBEAT;
			if ( Chat.UpdateQueue.Enqueue (pack) == false )
			{
				LOG_LOG (L"ChatServer", LOG_ERROR, L"UPDATE QUEUE OverFlow");
				Chat.Stop ();
			}
		}
		PROFILE_KEYPROC ();

		/*
		if ( GetAsyncKeyState ('E') & 0x8001 )
		{
			Chat.Stop ();
			break;
		}


		
		else if ( GetAsyncKeyState ('S') & 0x8001 )
		{
		Chat.Start (L"127.0.0.1", 6000, 200, 3);
		}
		*/

		Sleep (200);
	}


	return 0;
}

