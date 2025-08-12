// Project Management System.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include "framework.h"
#include "Project Management System.h"

#include <fcntl.h>     
#include <io.h>        
#include <windows.h>    

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#include "HttpBasicServer.h"


CWinApp theApp;

using namespace std;

// <summary>
/// Главна функция на програмата.
/// Извиква примерни демонстрационни функции,
/// инициализира MFC и връща статус код.
/// </summary>
int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    //_setmode(_fileno(stdout), _O_U8TEXT);

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
            CSoftHttp::CHttpBasicServer oBasicHttpServer(_T("127.0.0.1"), 49180);
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
