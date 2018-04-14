#include "stdafx.h"
#include "SIOCP.h"
#include "SSocket.h"
#include "Log.h"
#include <process.h>
#include "GameMessage.h"


unsigned WINAPI Accept(LPVOID pAcceptOL);
unsigned WINAPI WorkThread(LPVOID pOL);

DWORD SIOCP::g_userID = USER_ID_INDEX;


SIOCP::SIOCP()
	:m_cs(), m_ThreadAccept(NULL), m_ThreadWork(NULL), m_ThreadDisconnect(NULL)
{
}


SIOCP::~SIOCP()
{
	CleanUp();
}


BOOL SIOCP::CreateIOCP()
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


	if (CreateIOCPThread() == false)
	{
		ERROR_LOG("CreateIOCPThread Error!");
		//Log::Instance()->WriteLog("Project-socket", "create IOCP handle error");
		CleanUp();
		return FALSE;
	}
	return true;
}


void SIOCP::CleanUp()
{
	if (m_handle)
	{
		for (BYTE i = 0; i < m_threadcount; i++)
		{
			//1�� ������ ������ ����
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

	if (m_acceptthread != INVALID_HANDLE_VALUE)
		CloseHandle(m_acceptthread);
	m_acceptthread = INVALID_HANDLE_VALUE;

	if (m_handle)
	{
		CloseHandle(m_handle);
		m_handle = NULL;
	}

	WSACleanup();
}

BOOL SIOCP::CreateIOCPThread()
{
	//	if (m_ThreadAccept == NULL || m_ThreadWork == NULL) return false;

	GetSystemInfo(&m_system);

	m_threadcount = (BYTE)m_system.dwNumberOfProcessors * 2 - 1;
	//*2�� ������ ���μ����� 2����� �����带 �����ؾ���.
	//m_threadcount = threadcount;

	unsigned int threadid = 0;


	for (unsigned int i = 0; i < m_threadcount; i++)
	{

		//m_threads[i] = CreateThread(NULL, 0, WorkThread, this, 0, &threadid);
		m_threads[i] = (HANDLE)_beginthreadex(NULL, 0, WorkThread, this, 0, &threadid);
		//WorkThread������.
		if (m_threads[i] == NULL)
		{
			Log::Instance()->WriteLog("Project-socket", "Thread Create Fail");
			CleanUp();
			return false;
		}
	}

	m_acceptthread = (HANDLE)_beginthreadex(NULL, 0, Accept, this, 0, &threadid);
	SetThreadPriority(m_acceptthread, THREAD_PRIORITY_HIGHEST);

	return true;
}

BOOL SIOCP::RegisterCompletionPort(SOCKET socket, SPeer* context)
{
	if ((context == NULL) || (socket == INVALID_SOCKET))
		return false;

	HANDLE handle = (HANDLE)socket;

	CSLOCK(m_cs)
	{
		// �Ҵ�� ����ü�� ������ IOCP�� �����Ѵ�. 
		if (!CreateIoCompletionPort(handle, m_handle, reinterpret_cast<ULONG_PTR>(context), 0))
		{
#ifdef _DEBUG
			char buff[100];
			int ret = WSAGetLastError();
			wsprintfA(buff, "[ERROR : %d] ���� �߻�[%s] \n", ret, __FUNCTION__);
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


BOOL SIOCP::GetCompletionStatus(LPDWORD pdwOutBytesTransferred, ULONG_PTR * pOutCompletionKey, WSAOVERLAPPED ** pOutOverlapped, int * pErrCode, DWORD dwWaitingTime)
{
	BOOL result = GetQueuedCompletionStatus(m_handle, pdwOutBytesTransferred, pOutCompletionKey, pOutOverlapped, dwWaitingTime);

	return result;
}

BOOL SIOCP::PostCompletionStatus(DWORD CompleitonKey, DWORD dwBytesTransferred, WSAOVERLAPPED * pOverlapped)
{
	BOOL result = PostQueuedCompletionStatus(m_handle, CompleitonKey, dwBytesTransferred, pOverlapped);

	if (!result)
	{
#ifdef _DEBUG
		char buff[100];
		int ret = WSAGetLastError();
		wsprintfA(buff, "[ERROR : %d] ���� �߻�[%s] \n", ret, __FUNCTION__);
		OutputDebugStringA(buff);
#else
		SOCKET_ERROR_LOG_CODE
#endif
	}

	return TRUE;
}

unsigned WINAPI Accept(LPVOID pAcceptOL)
{
	SIOCP* pIocp = (SIOCP*)pAcceptOL;

	SSocket accept_socket;
	SOCKET client_socket;
	SOCKADDR_IN client_addr;

	if (accept_socket.CreateWSASocket() == FALSE)
	{
		//Log::Instance()->WriteLog("Project-socket", "Accept Socket Create Fail");
		SOCKET_ERROR_LOG_CODE;
		return 0;
	}

	accept_socket.SetAddr(AF_INET, SERVER_PORT, INADDR_ANY);

	if (SocketTool::Bind(accept_socket.m_socket, accept_socket.m_addr) == INVALID_SOCKET)
	{

#ifdef _DEBUG
		char buff[100];
		int ret = WSAGetLastError();
		wsprintfA(buff, "[ERROR : %d] ���� �߻�[%s] \n", ret, __FUNCTION__);
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


	//listen_socket�� Send buffer�� �ɼ��� Ȯ���ϱ����� �Լ�
	if (getsockopt(accept_socket.m_socket, SOL_SOCKET, SO_SNDBUF, (char *)&send_size, &send_len) != SOCKET_ERROR)
	{
		send_size *= 100; //send buffer ũ�⸦ �� 100���?
		if (setsockopt(accept_socket.m_socket, SOL_SOCKET, SO_SNDBUF, (char*)&send_size, sizeof(send_len)) == SOCKET_ERROR)
		{
			//setscokopt�� �������� ��

			ERROR_LOG("connect fail1");
		}
	}
	else // getsockopt ���н�
	{
		ERROR_LOG("connect faile2");
	}

	if (SocketTool::Listen(accept_socket.m_socket, ListenQueue) == INVALID_SOCKET)
	{
		accept_socket.CloseSocket();
		SOCKET_ERROR_LOG_CODE;
		return 0;
	}
	else
		printf("Listen Sucess! IP : %s / Port : %d \n", SERVER_IP, SERVER_PORT);


	//accept �ݺ�
	while (1)
	{
		client_socket = SocketTool::Accept(accept_socket.m_socket, client_addr);

		if (client_socket == INVALID_SOCKET)
		{
			accept_socket.CloseSocket();
			continue;
		}

		//Aceept�� ������ �Ŀ�...
		//		SPeer* puser = UserContainer::GetInstance()->Pop_EmptyUser();
		pIocp->m_ThreadAccept(client_socket, client_addr);
	}
}


unsigned WINAPI WorkThread(LPVOID pOL)
{
	SIOCP* pIocp = (SIOCP*)pOL;
	DWORD DwNumberBytes = 0;
	SPeer* pCompletionKey = NULL;
	IO_OVERLAPPED* pOverlapped = NULL;
	BOOL result = FALSE;
	//

	while (true)
	{
		result = pIocp->GetCompletionStatus(&DwNumberBytes, reinterpret_cast<ULONG_PTR*>(&pCompletionKey), reinterpret_cast<WSAOVERLAPPED **>(&pOverlapped));

		if (result) // ���������� ����.
		{
			if (pOverlapped == NULL) continue;
		}
		else
		{
			if (pOverlapped != NULL)
			{
				pIocp->m_ThreadDisconnect(pCompletionKey);
				pIocp->PostCompletionStatus((DWORD)pCompletionKey, 0, (OVERLAPPED*)pOverlapped);
				GameMessageManager::GetInstance()->SendGameMessage(GM_DISCONNECTUSER, (DWORD)pCompletionKey, (DWORD)pOverlapped, NULL);
			}

			continue;
		}

		//�̹� ������ ����
		if ((pCompletionKey == NULL) || (pCompletionKey->GetId() == -100)) continue;

		//Ŭ�� ������ ����
		if (DwNumberBytes == 0)
		{
			pIocp->PostCompletionStatus((DWORD)pCompletionKey, 0, (OVERLAPPED*)pOverlapped);
			continue;
		}

		pIocp->m_ThreadWork(pCompletionKey, pOverlapped, DwNumberBytes);
	}

	return 0;
}