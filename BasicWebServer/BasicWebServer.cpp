// BasicWebServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "BasicWebServer.h"
#include "HttpBasicServer.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// The one and only application object

CWinApp theApp;

using namespace std;

UINT __cdecl MyHttpBasicServerControllingFunction( LPVOID pParam );

int _tmain(int argc, TCHAR* argv[], TCHAR* envp[])
{
	int nRetCode = 0;

	// initialize MFC and print and error on failure
	if (!AfxWinInit(::GetModuleHandle(NULL), NULL, ::GetCommandLine(), 0))
	{
		// TODO: change error code to suit your needs
		_tprintf(_T("Fatal Error: MFC initialization failed\n"));
		nRetCode = 1;
	}
	else
	{
		// TODO: code your application's behavior here.
		CWinThread *pThread = AfxBeginThread(MyHttpBasicServerControllingFunction, NULL);
		for ( DWORD i=0; i < UINT_MAX; i++)
		{
			Sleep(5000);
		}
	}


	return nRetCode;
}


UINT __cdecl MyHttpBasicServerControllingFunction( LPVOID pParam )
{
	//HttpCore::CHttpBasicServer oBasicHttpServer(_T("127.0.0.1"), 49180);
	HttpCore::CHttpBasicServer oBasicHttpServer(_T("192.168.210.23"), 8080);
	if (!oBasicHttpServer.Init())
	{
		wprintf(L"Fatal Error: initialization CHttpBasicServer\n");
	}

	if (!oBasicHttpServer.Run())
	{
		wprintf(L"Fatal Error: running CHttpBasicServer\n");
	}

	return 0;
}