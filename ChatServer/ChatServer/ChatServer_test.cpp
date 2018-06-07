// CLanServer.cpp : �ܼ� ���� ���α׷��� ���� �������� �����մϴ�.
//

#include "stdafx.h"
#include "NetServer.h"
#include "Protocol.h"
#include <map>
using namespace std;
CCrashDump Dump;

class ChatServer :public CNetServer
{
private:
	struct Player
	{
		UINT64 SessionID;
		
		INT64	AccountNo;
		WCHAR	ID[20];		// null ����
		WCHAR	Nickname[20];		// null ����
		char	SessionKey[64];

		WCHAR IP[32];

		WORD	MessageLen;
		WCHAR	*Message;		// MessageLen/2 null ������

		int PosX;
		int PosY;


	};
	
	map<UINT64, Player *> Playerlist;

	CQueue_LF<Packet *> UpdateQueue;
public:
	ChatServer (void)
	{
	}
	~ChatServer (void)
	{
		Stop ();
	}

	virtual void OnRecv (UINT64 SessionID, Packet *p)
	{
		INT64 Num;
		*p >> Num;

		Packet *Pack = Packet::Alloc ();
		*Pack << Num;

		SendPacket (SessionID, Pack);

		Packet::Free (Pack);
		return;
	}
	virtual void OnSend (UINT64 SessionID, INT SendByte)
	{
		return;
	}

	virtual bool OnClientJoin (UINT64 SessionID, WCHAR *IP, int PORT)
	{
		INT64 Data = 0x7FFFFFFFFFFFFFFF;

		Packet *Pack = Packet::Alloc ();
		*Pack << Data;

		SendPacket (SessionID, Pack);


		Packet::Free (Pack);

		return true;
	}
	virtual void OnClientLeave (UINT64 SessionID)
	{
		return;
	}


	/*======================================================================
	//Update
	//���� : ChatServer Main.
	//���� : LPVOID pParam; = CLanServer this pointer �� �Ϸ� �Ѱܹ���.
	//���� : 0
	======================================================================*/
	static unsigned int WINAPI Update (LPVOID pParam)
	{

	}
};

ChatServer Chat;





int main ()
{
	wprintf (L"MainThread Start\n");
	Chat.Start (L"127.0.0.1", 6000, 5000, 3);


	UINT AcceptTotal = 0;
	UINT AcceptTPS = 0;
	UINT RecvTPS = 0;
	UINT SendTPS = 0;
	UINT ConnectSessionCnt = 0;
	int MemoryPoolCnt = 0;
	int MemoryPoolUse = 0;

	DWORD StartTime = GetTickCount ();
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
			wprintf (L"MemoryPoolFull Cnt = %d\n", MemoryPoolCnt);
			wprintf (L"MemoryPoolUse Cnt = %d \n", MemoryPoolUse);

			wprintf (L"==========================\n");

			AcceptTotal = Chat.AcceptTotal ();
			AcceptTPS = Chat.AcceptTPS (true);
			RecvTPS = Chat.RecvTPS (true);
			SendTPS = Chat.SendTPS (true);
			ConnectSessionCnt = Chat.Use_SessionCnt ();
			MemoryPoolCnt = Chat.Full_MemPoolCnt ();
			MemoryPoolUse = Chat.Alloc_MemPoolCnt ();

			StartTime = EndTime;
		}


		if ( GetAsyncKeyState ('E') & 0x8001 )
		{
			Chat.Stop ();
			break;
		}

		PROFILE_KEYPROC ();
		/*
		else if ( GetAsyncKeyState ('S') & 0x8001 )
		{
		Chat.Start (L"127.0.0.1", 6000, 200, 3);
		}
		*/

		Sleep (200);
	}


	return 0;
}

