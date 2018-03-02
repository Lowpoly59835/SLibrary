#include "stdafx.h"
#include "IOCP.h"
#include "SSocket.h"
#include "Log.h"
#include <process.h>
#include "SUserContainer.h"

unsigned WINAPI Accept(LPVOID pAcceptOL);
unsigned WINAPI WorkThread(void* pOL);

IOCP* IOCP::m_instance = NULL;
DWORD IOCP::g_userID = USER_ID_INDEX;

IOCP* IOCP::GetInstance()
{
	if (m_instance == NULL)
	{
		m_instance = new IOCP();
	}
	return m_instance;
}


IOCP::IOCP()
	:m_cs()
{
}


IOCP::~IOCP()
{
	CleanUp();
}


BOOL IOCP::CreateIOCP()
{
	WSADATA             wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		//Log::Instance()->WriteLog("Project-socket", "WASStartup error");
		ERROR_LOG("WASStartup error");
		return FALSE;
	}

//	InitializeCriticalSection(&m_criticalsection);

	m_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

	if (m_handle == NULL)
	{
		ERROR_LOG("WASStartup error");
		//Log::Instance()->WriteLog("Project-socket", "create IOCP handle error");
		CleanUp();
		return FALSE;
	}

	CreateIOCPThread();

	return true;
}


void IOCP::CleanUp()
{
	if (m_handle)
	{
		for (BYTE i = 0; i < m_threadcount; i++)
		{
			//1을 넣으면 스레드 종료
			BOOL ret = PostCompletionStatus(1);
		}
	}


	if (WAIT_FAILED == WaitForMultipleObjects(m_threadcount, m_threads, FALSE, 10000))
	{
		printf("WaitForMultipleObjects failed");
	}

	for (BYTE i = 0; i < m_threadcount; i++)
	{
		if (m_threads[i] != INVALID_HANDLE_VALUE)
			CloseHandle(m_threads[i]);

		m_threads[i] = INVALID_HANDLE_VALUE;
	}

	if (m_handle)
	{
		CloseHandle(m_handle);
		m_handle = NULL;
	}

	WSACleanup();
}

void IOCP::CreateIOCPThread()
{
	GetSystemInfo(&m_system);

	m_threadcount = (BYTE)m_system.dwNumberOfProcessors * 2 - 1;
	//*2인 이유는 프로세서의 2배까지 스레드를 생성해야함.
	//m_threadcount = threadcount;

	unsigned int threadid = 0;


	for (DWORD i = 0; i < m_threadcount; i++)
	{
		//		CreateThread()

		//m_threads[i] = CreateThread(NULL, 0, WorkThread, this, 0, &threadid);
		m_threads[i] = (HANDLE)_beginthreadex(NULL, 0, WorkThread, this, 0, &threadid);
		//WorkThread만들자.
		if (m_threads[i] == NULL)
		{
			Log::Instance()->WriteLog("Project-socket", "Thread Create Fail");
			CleanUp();
			return;
		}
	}

	m_acceptthread = (HANDLE)_beginthreadex(NULL, 0, Accept, this, 0, &threadid);
	SetThreadPriority(m_acceptthread, THREAD_PRIORITY_HIGHEST);

}

BOOL IOCP::RegisterCompletionPort(SOCKET_CONTEXT* lpPerSocketContext)
{
	if (lpPerSocketContext == NULL)
		return false;


	HANDLE handle = (HANDLE)lpPerSocketContext->m_socket;

	CSLOCK(m_cs)
	{
		// 할당된 구조체와 소켓을 IOCP와 연결한다. 
		if (!CreateIoCompletionPort(handle, m_handle, (DWORD)lpPerSocketContext, 0))
		{
#ifdef _DEBUG
			char buff[100];
			int ret = WSAGetLastError();
			wsprintfA(buff, "[ERROR : %d] 에러 발생[%s] \n", ret, __FUNCTION__);
			OutputDebugStringA(buff);
#else
			SOCKET_ERROR_LOG_CODE
#endif

				//UnLock();
				return false;
		}
	}
	//UnLock();

	return true;
}


BOOL IOCP::GetCompletionStatus(LPDWORD pdwOutBytesTransferred, ULONG_PTR * pOutCompletionKey, WSAOVERLAPPED ** pOutOverlapped, int * pErrCode, DWORD dwWaitingTime)
{
	BOOL result = GetQueuedCompletionStatus(m_handle, pdwOutBytesTransferred, pOutCompletionKey, pOutOverlapped, dwWaitingTime);

	return result;
}

BOOL IOCP::PostCompletionStatus(DWORD CompleitonKey, DWORD dwBytesTransferred, WSAOVERLAPPED * pOverlapped)
{	
	BOOL result = PostQueuedCompletionStatus(m_handle, CompleitonKey, dwBytesTransferred, pOverlapped);

	if (!result)
	{
	#ifdef _DEBUG
			char buff[100];
			int ret = WSAGetLastError();
			wsprintfA(buff, "[ERROR : %d] 에러 발생[%s] \n", ret, __FUNCTION__);
			OutputDebugStringA(buff);
	#else
			SOCKET_ERROR_LOG_CODE
	#endif
	}

	return TRUE;
}




unsigned WINAPI Accept(LPVOID pAcceptOL)
{
	//WinSocket listen_socket;
	SSocket accept_socket;
//	SocketTool tool;
	SOCKET client_socket;
	SOCKADDR_IN client_addr;

	if (accept_socket.CreateWSASocket() == FALSE)
	{
		//Log::Instance()->WriteLog("Project-socket", "Accept Socket Create Fail");
		SOCKET_ERROR_LOG_CODE;
		return 0;
	}


	accept_socket.SetAddr(AF_INET, 14483, INADDR_ANY);


	if( !SocketTool::Bind(accept_socket.socket, accept_socket.addr) )
	{

#ifdef _DEBUG
		char buff[100];
		int ret = WSAGetLastError();
		wsprintfA(buff, "[ERROR : %d] 에러 발생[%s] \n", ret, __FUNCTION__);
		OutputDebugStringA(buff);
#else
		SOCKET_ERROR_LOG_CODE
#endif

		accept_socket.CloseSocket();
		return 0;
		//	Log::Instance()->WriteLog("Project-socket", "Socket Bind Error");
	}

	int send_size;
	int send_len = sizeof(send_size);


	//listen_socket의 Send buffer의 옵션을 확인하기위한 함수
	if (getsockopt(accept_socket.socket, SOL_SOCKET, SO_SNDBUF, (char *)&send_size, &send_len) != SOCKET_ERROR)
	{
		send_size *= 100; //send buffer 크기를 왜 100배로?
		if (setsockopt(accept_socket.socket, SOL_SOCKET, SO_SNDBUF, (char*)&send_size, sizeof(send_len)) == SOCKET_ERROR)
		{
			//setscokopt에 실패했을 때

			ERROR_LOG("connect fail1");
		}
	}
	else // getsockopt 실패시
	{
		ERROR_LOG("connect faile2");
	}

	if (SocketTool::Listen(accept_socket.socket, ListenQueue) == FALSE)
	{
		accept_socket.CloseSocket();
		SOCKET_ERROR_LOG_CODE;
		return 0;
	}
	else
		printf("Listen Sucess! \n");


	//accept 반복
	while (1)
	{
		//		if (tool.Accept(&listen_socket) == INVALID_SOCKET)

		//SCOPE_EXIT(printf("accept sucess \n"););

		client_socket = SocketTool::Accept(accept_socket.socket, client_addr);

		if (client_socket == INVALID_SOCKET)
		{
			accept_socket.CloseSocket();
			continue;
		}

		//Aceept를 성공한 후에...
		SUser* puser = UserContainer::GetInstance()->Pop_EmptyUser();

		if (puser != NULL)
		{
			UserContainer::GetInstance()->Add_CurUser(IOCP::g_userID, puser);

			puser->InitUser(client_socket, client_addr, IOCP::g_userID++);
		}
	}
}


unsigned WINAPI WorkThread(void* pOL)
{

	//IOCP* iocp = ;
	DWORD DwNumberBytes = 0;
	SOCKET_CONTEXT* pCompletionKey = NULL;
	IO_OVERLAPPED* pOverlapped = NULL;
	BOOL result = FALSE;
	//

	while (true)
	{
		result = IOCP::GetInstance()->GetCompletionStatus(&DwNumberBytes, reinterpret_cast<ULONG_PTR*>(&pCompletionKey), reinterpret_cast<WSAOVERLAPPED **>(&pOverlapped));

		//수정하자...pOverlapped를 어떻게 수정하지ㅠㅠ
		if (result) // 정상적으로 실행.
		{
			//SOCKET_ERROR_LOG_CODE;
			//continue;

			//if((int)pCompletionKey->)

			if (pOverlapped == NULL) continue;
		}
		else
		{
			if (pOverlapped != NULL)
			{
				UserContainer::GetInstance()->DisConnect(pCompletionKey);
			}

			continue;
		}

		//이미 연결이 끊김
		if ((pCompletionKey == NULL) || (pCompletionKey->m_socket == INVALID_SOCKET)) continue;

		//클라가 연결을 끊음
		if (DwNumberBytes == 0)
		{
			UserContainer::GetInstance()->DisConnect(pCompletionKey);
			continue;
		}

		if (pOverlapped->io_type == IO_RECV)
		{
			//리시브 패킷 처리 함수
			pCompletionKey->m_puser->RecvGamePacket(DwNumberBytes);
		}
		else if (pOverlapped->io_type == )
		{
			//샌드 완료 함수
			//지금은 특별히 없는 것같은데?
		}
		else // IO_NONE 혹은 에러
		{

		}

	}


	return 0;
}