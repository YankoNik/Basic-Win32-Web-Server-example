#include "pch.h"
#include "framework.h"
<<<<<<<< HEAD:HttpWebServer/MainWebSrv.cpp
#include "MainWebSrv.h"
========
#include "HttpWebServer.h"
>>>>>>>> 4aff00d34bf3e906b2cce53c9f161e5d93f9eed8:HttpWebServer/HttpWebServer.cpp

#include <fcntl.h>     
#include <io.h>        
#include <windows.h>    

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include "HttpBasicServer.h"


CWinApp theApp;

using namespace std;

/// <summary>
/// </summary>
int main()
{
    int nRetCode = 0;

    HMODULE hModule = ::GetModuleHandle(nullptr);

	if (hModule != nullptr)
    {
        if (!AfxWinInit(hModule, nullptr, ::GetCommandLine(), 0))
        {
           
            wprintf(L"Fatal Error: MFC initialization failed\n");
            nRetCode = 1;
        }
        else
        {
            CSoftHttp::CHttpBasicServer oBasicHttpServer(_T("https://localhost"), 49180, "66cda0318f6241890859fe229f651f326dd0e246");
            if (!oBasicHttpServer.Init())
            {
                wprintf(L"Fatal Error: initialization CHttpBasicServer\n");
            }

            if (!oBasicHttpServer.Run())
            {
                wprintf(L"Fatal Error: Runing CHttpBasicServer\n");
            }
        }
    }
    else
    {
        wprintf(L"Fatal Error: GetModuleHandle failed\n");
        nRetCode = 1;
    }

    return nRetCode;
}
