// CmdWebServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "..\BasicWebServer\HttpBasicServer.h"


int _tmain(int argc, _TCHAR* argv[])
{
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

