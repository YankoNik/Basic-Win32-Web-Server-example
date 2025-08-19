#include "StdAfx.h"
#include "HttpBasicServer.h"

#pragma comment(lib, "Httpapi.lib")
#pragma comment(lib, "Ws2_32.lib")


///////////////////////////////////////////////////////////////////////////////////////////////////////////
// {6DD0FE89-728D-4AFF-BC4E-5CB8C1FE5482}
static const GUID glb_MyAppId = { 0x6dd0fe89, 0x728d, 0x4aff, { 0xbc, 0x4e, 0x5c, 0xb8, 0xc1, 0xfe, 0x54, 0x82 } };

///////////////////////////////////////////////////////////////////////////////////////////////////////////
#define INITIALIZE_HTTP_RESPONSE( resp, status, reason )    \
    do                                                      \
    {                                                       \
        RtlZeroMemory( (resp), sizeof(*(resp)) );           \
        (resp)->StatusCode = (status);                      \
        (resp)->pReason = (reason);                         \
        (resp)->ReasonLength = (USHORT) strlen(reason);     \
    } while (FALSE)

#define ADD_KNOWN_HEADER(Response, HeaderId, RawValue)											\
    do																							\
    {																							\
        (Response).Headers.KnownHeaders[(HeaderId)].pRawValue = (RawValue);						\
        (Response).Headers.KnownHeaders[(HeaderId)].RawValueLength = (USHORT) strlen(RawValue);	\
    } while(FALSE)

#define ALLOC_MEM(cb) HeapAlloc(GetProcessHeap(), 0, (cb))

#define FREE_MEM(ptr) HeapFree(GetProcessHeap(), 0, (ptr))

///////////////////////////////////////////////////////////
extern bool g_bTraceRequest = true;


///////////////////////////////////////////////////////////
static CStringW GetIpAddress(const PSOCKADDR pSocketIPAddress)
{
	if (!pSocketIPAddress)
		return CStringW("");

	CStringW strIP;

	int nValue = (pSocketIPAddress->sa_data[2] & 0xff);
	strIP.Format(L"%ld", nValue);

	for (int i = 3; i < 6; i++)
	{
		nValue = (pSocketIPAddress->sa_data[i] & 0xff);
		strIP.AppendFormat(L".%ld", nValue);
	}

	return strIP;
}

static CStringW GetRequestHeader(const PHTTP_REQUEST pHttpRequest)
{
	if (!pHttpRequest)
		return CStringW("");

	CStringW strResult;
	for (int i = (int)HttpHeaderCacheControl; i < HttpHeaderRequestMaximum; i++)
	{
		HTTP_KNOWN_HEADER& httpHeare = pHttpRequest->Headers.KnownHeaders[i];
		if (httpHeare.RawValueLength <= 0 || !httpHeare.pRawValue)
			continue;
		CStringA strText = (LPCSTR)httpHeare.pRawValue;
		strResult.AppendFormat(L"\n\t header id %ld: %ws", i, (LPCWSTR)CA2W(strText));
	}

	return strResult;
}

static void TraceRequest(const PHTTP_REQUEST pHttpRequest)
{
	if (!g_bTraceRequest)
		return;

	if (!pHttpRequest)
		return;

	CStringW strHttpVerb = L"unknown";
	if (HttpVerbGET == pHttpRequest->Verb)
		strHttpVerb = L"GET";
	else  if (HttpVerbPOST == pHttpRequest->Verb)
		strHttpVerb = L"POST";

	wprintf(L"\nGot a %ws request for %ws, from IP address %ws, request headers:%ws"
		, (LPCWSTR)strHttpVerb
		, pHttpRequest->CookedUrl.pFullUrl
		, (LPCWSTR)GetIpAddress(pHttpRequest->Address.pRemoteAddress)
		, (LPCWSTR)GetRequestHeader(pHttpRequest));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////
#define _DF_HTTPS_ _T("https:")
#define _DF_CMD_EXIST_	"/api/exit"
#define _DF_CMD_INFO_	"/api/info"

HTTPAPI_VERSION glb_HttpApiVersion = HTTPAPI_VERSION_1;

///////////////////////////////////////////////////////////////////////////////////////////////////////////
namespace HttpCore
{
	///////////////////////////////////////////////////////////////////////////////////////////
	// class CHttpBasicServer
	CHttpBasicServer::CHttpBasicServer(CString strIPAddress, int nPort, LPCSTR lpszCertThumbPrint /*= NULL*/)
		: m_strIPAddress(strIPAddress)
		, m_nPort(nPort)
		, m_hReqQueue(NULL)
		, m_bUrlAdded(false)
		, m_strCertThumbPrint(lpszCertThumbPrint)
		, m_bServerSslCertInit(false)
		, m_pHttpServiceConfigSsl(NULL)
	{
		memset(&m_oSockAddrIn, 0, sizeof(m_oSockAddrIn));
		memset(m_arrCertHash, 0, _countof(m_arrCertHash));
	}

	CHttpBasicServer::~CHttpBasicServer()
	{
		CleanUp();
	}

	bool CHttpBasicServer::Init()
	{
		//if (!m_strIPAddress.Left(6).CompareNoCase(_DF_HTTPS_) && !InitializeSsl())
		//{
		//	return false;
		//}
		
		// Initialize HTTP Server APIs
		int retCode = HttpInitialize(glb_HttpApiVersion, HTTP_INITIALIZE_SERVER, NULL);

		if (retCode != NO_ERROR)
		{
			wprintf(L"HttpInitialize failed with %lu \n", retCode);
			return false;
		}

		//
		// Create a Request Queue Handle
		retCode = HttpCreateHttpHandle(&m_hReqQueue, 0);

		if (retCode != NO_ERROR)
		{
			wprintf(L"HttpCreateHttpHandle failed with %lu \n", retCode);
			CleanUp();
		}

		CStringW strUriPort = GetFullyQualifiedURI();
		//
		// The command line arguments represent URIs that to 
		// listen on. Call HttpAddUrl for each URI.
		//
		// The URI is a fully qualified URI and must include the
		// terminating (/) character.
		//

		wprintf(L"listening for requests on the following url: %s\n", (LPCWSTR)strUriPort);

		retCode = HttpAddUrl(
			m_hReqQueue,   // Req Queue
			strUriPort,    // Fully qualified URL
			NULL           // Reserved
		);

		if (retCode != NO_ERROR)
		{
			wprintf(L"HttpAddUrl failed with %lu \n", retCode);
			CleanUp();
		}
		else
		{
			//
			// Track the currently added URLs.
			//
			m_bUrlAdded = true;
		}

		return (retCode == NO_ERROR);
	}

	bool CHttpBasicServer::Run()
	{
		if (DoReceiveRequests() != 0)
		{
			return false;
		}

		return true;
	}

	bool CHttpBasicServer::CleanUp()
	{
		CStringW strUri = GetFullyQualifiedURI();

		//
		// Call HttpRemoveUrl for all added URLs.
		if (m_bUrlAdded)
		{
			HttpRemoveUrl(m_hReqQueue, strUri);
		}

		if (m_bServerSslCertInit)
		{
			ReleaseSsl();
		}

		//
		// Close the Request Queue handle.
		if (m_hReqQueue)
		{
			CloseHandle(m_hReqQueue);
		}

		// 
		// Call HttpTerminate.
		HttpTerminate(HTTP_INITIALIZE_SERVER, NULL);

		return true;
	}

	CStringW CHttpBasicServer::GetFullyQualifiedURI() const
	{
		CStringW strUriPort, strIP = m_strIPAddress;

		const CString strHttp = _T("http");
		if (m_strIPAddress.Left(strHttp.GetLength()).CompareNoCase(strHttp) != 0)
			strUriPort = L"http://";

		strUriPort.AppendFormat(L"%s:%d", (LPCWSTR)strIP, m_nPort);

		strUriPort += L"/api/";

		return strUriPort;
	}

	bool CHttpBasicServer::InitializeSsl()
	{
		const int lenghtCertThumbPrin = m_strCertThumbPrint.GetLength();
		if (lenghtCertThumbPrin != _DF_CERT_HASH_LEN_*2)
		{
			wprintf(L"Incorrect cert thumbprint length %d\n", m_strCertThumbPrint.GetLength());
			return false;
		}

		int retCode = HttpInitialize(glb_HttpApiVersion, HTTP_INITIALIZE_CONFIG, NULL);

		if (retCode != NO_ERROR)
		{
			wprintf(L"HttpInitialize failed with %lu \n", retCode);
			return false;
		}

		m_pHttpServiceConfigSsl = new HTTP_SERVICE_CONFIG_SSL_SET();
		memset(m_pHttpServiceConfigSsl, 0, sizeof(HTTP_SERVICE_CONFIG_SSL_SET));

		m_oSockAddrIn = SOCKADDR_IN();
		m_oSockAddrIn.sin_family = AF_INET;
		m_oSockAddrIn.sin_addr.s_addr = htonl(INADDR_ANY); // Listen on all IPs
		m_oSockAddrIn.sin_port = htons(m_nPort); // Convert port to network byte order

		// Convert hex string to binary hash (20 bytes for SHA-1)
		const char* hexHash = (LPCSTR)m_strCertThumbPrint.GetBuffer();
		for (int i = 0; i < _DF_CERT_HASH_LEN_; ++i) {
			sscanf_s(hexHash + 2 * i, "%2hhx", &m_arrCertHash[i]);
		}

		m_pHttpServiceConfigSsl->KeyDesc.pIpPort = (PSOCKADDR)&m_oSockAddrIn;
		m_pHttpServiceConfigSsl->ParamDesc.pSslHash = (PVOID)m_arrCertHash;
		m_pHttpServiceConfigSsl->ParamDesc.SslHashLength = _DF_CERT_HASH_LEN_;
		m_pHttpServiceConfigSsl->ParamDesc.AppId = glb_MyAppId;
		m_pHttpServiceConfigSsl->ParamDesc.pSslCertStoreName = (PWSTR)L"MY";

		m_pHttpServiceConfigSsl->ParamDesc.DefaultFlags = HTTP_SERVICE_CONFIG_SSL_FLAG_NEGOTIATE_CLIENT_CERT;
		m_pHttpServiceConfigSsl->ParamDesc.DefaultCertCheckMode = 0;
		m_pHttpServiceConfigSsl->ParamDesc.pDefaultSslCtlStoreName = NULL;
		m_pHttpServiceConfigSsl->ParamDesc.pDefaultSslCtlIdentifier = NULL;
		m_pHttpServiceConfigSsl->ParamDesc.DefaultRevocationFreshnessTime = 0;
		m_pHttpServiceConfigSsl->ParamDesc.DefaultRevocationUrlRetrievalTimeout = 0;

		retCode = HttpSetServiceConfiguration(NULL
			, HttpServiceConfigSSLCertInfo
			, (PVOID)m_pHttpServiceConfigSsl
			, sizeof(HTTP_SERVICE_CONFIG_SSL_SET), NULL);

		//if (retCode == ERROR_NO_SUCH_LOGON_SESSION || retCode == ERROR_INVALID_PARAMETER || retCode == ERROR_ALREADY_EXISTS)
		//	return false;
		if (retCode == ERROR_ALREADY_EXISTS && retCode == ERROR_INVALID_PARAMETER)
		{
			m_bServerSslCertInit = true;
			ReleaseSsl();
			return false;
		}

		if (retCode != NO_ERROR)
		{
			wprintf(L"HttpSetServiceConfiguration failed with %lu \n", retCode);
			return false;
		}

		m_bServerSslCertInit = true;
		return true;
	}

	bool CHttpBasicServer::ReleaseSsl()
	{
		if (!m_bServerSslCertInit || !m_pHttpServiceConfigSsl)
			return true;

		long retCode = HttpDeleteServiceConfiguration(NULL
			, HttpServiceConfigSSLCertInfo
			, (PVOID)m_pHttpServiceConfigSsl
			, sizeof(HTTP_SERVICE_CONFIG_SSL_SET), NULL);

		delete m_pHttpServiceConfigSsl; m_pHttpServiceConfigSsl = NULL;

		if (retCode != NO_ERROR)
		{
			wprintf(L"HttpDeleteServiceConfiguration failed with %lu \n", retCode);
			return false;
		}

		return true;
	}

	DWORD CHttpBasicServer::DoReceiveRequests()
	{
		ULONG              result;
		HTTP_REQUEST_ID    requestId;
		DWORD              bytesRead = 0;
		PHTTP_REQUEST      pRequest = NULL;
		PCHAR              pRequestBuffer = NULL;
		ULONG              RequestBufferLength = 0;

		//
		// Allocate a 2 KB buffer. This size should work for most 
		// requests. The buffer size can be increased if required. Space
		// is also required for an HTTP_REQUEST structure.
		//
		RequestBufferLength = sizeof(HTTP_REQUEST) + 2048;
		pRequestBuffer = (PCHAR)ALLOC_MEM(RequestBufferLength);

		if (pRequestBuffer == NULL)
		{
			return ERROR_NOT_ENOUGH_MEMORY;
		}

		pRequest = (PHTTP_REQUEST)pRequestBuffer;

		//
		// Wait for a new request. This is indicated by a NULL 
		// request ID.
		//

		HTTP_SET_NULL_ID(&requestId);

		for (;;)
		{
			RtlZeroMemory(pRequest, RequestBufferLength);

			result = HttpReceiveHttpRequest(
				m_hReqQueue,        // Req Queue
				requestId,          // Req ID
				0,                  // Flags
				pRequest,           // HTTP request buffer
				RequestBufferLength,// req buffer length
				&bytesRead,         // bytes received
				NULL                // LPOVERLAPPED
			);

			if (NO_ERROR == result)
			{
				//
				// Worked! 
				// 
				switch (pRequest->Verb)
				{
				case HttpVerbGET:
					TraceRequest(pRequest);
					result = SendHttpResponse(pRequest, 200, PSTR("OK"), PSTR("Hey! You hit the server \r\n"));
					break;

				case HttpVerbPOST:
					TraceRequest(pRequest);
					result = SendHttpPostResponse(pRequest);
					break;

				default:
					TraceRequest(pRequest);
					result = SendHttpResponse(pRequest, 503, PSTR("Not Implemented"), NULL);
					break;
				}


				if (result != NO_ERROR)
				{
					break;
				}

				CStringA strRawUrl = CStringA(pRequest->pRawUrl).MakeLower();
				if (!strRawUrl.CollateNoCase(_DF_CMD_EXIST_))
				{
					break;
				}

				//
				// Reset the Request ID to handle the next request.
				//
				HTTP_SET_NULL_ID(&requestId);
			}
			else if (result == ERROR_MORE_DATA)
			{
				//
				// The input buffer was too small to hold the request
				// headers. Increase the buffer size and call the 
				// API again. 
				//
				// When calling the API again, handle the request
				// that failed by passing a RequestID.
				//
				// This RequestID is read from the old buffer.
				//
				requestId = pRequest->RequestId;

				//
				// Free the old buffer and allocate a new buffer.
				//
				RequestBufferLength = bytesRead;
				FREE_MEM(pRequestBuffer);
				pRequestBuffer = (PCHAR)ALLOC_MEM(RequestBufferLength);

				if (pRequestBuffer == NULL)
				{
					result = ERROR_NOT_ENOUGH_MEMORY;
					break;
				}

				pRequest = (PHTTP_REQUEST)pRequestBuffer;

			}
			else if (ERROR_CONNECTION_INVALID == result && !HTTP_IS_NULL_ID(&requestId))
			{
				// The TCP connection was corrupted by the peer when
				// attempting to handle a request with more buffer. 
				// Continue to the next request.

				HTTP_SET_NULL_ID(&requestId);
			}
			else
			{
				break;
			}
		}

		if (pRequestBuffer)
		{
			FREE_MEM(pRequestBuffer);
		}

		return result;
	}

	DWORD CHttpBasicServer::SendHttpResponse(IN PHTTP_REQUEST pRequest,
		IN USHORT        StatusCode,
		IN PSTR          pReason,
		IN PSTR          pEntityString
	)
	{
		HTTP_RESPONSE   response;
		HTTP_DATA_CHUNK dataChunk;
		DWORD           result;
		DWORD           bytesSent;

		//
		// Initialize the HTTP response structure.
		//
		INITIALIZE_HTTP_RESPONSE(&response, StatusCode, pReason);

		//
		// Add a known header.
		//
		ADD_KNOWN_HEADER(response, HttpHeaderContentType, "text/html");

		if (pEntityString)
		{
			// 
			// Add an entity chunk.
			//
			dataChunk.DataChunkType = HttpDataChunkFromMemory;
			dataChunk.FromMemory.pBuffer = pEntityString;
			dataChunk.FromMemory.BufferLength = (ULONG)strlen(pEntityString);

			response.EntityChunkCount = 1;
			response.pEntityChunks = &dataChunk;
		}

		// 
		// Because the entity body is sent in one call, it is not
		// required to specify the Content-Length.
		//
		result = HttpSendHttpResponse(
			m_hReqQueue,         // ReqQueueHandle
			pRequest->RequestId, // Request ID
			0,                   // Flags
			&response,           // HTTP response
			NULL,                // pReserved1
			&bytesSent,          // bytes sent  (OPTIONAL)
			NULL,                // pReserved2  (must be NULL)
			0,                   // Reserved3   (must be 0)
			NULL,                // LPOVERLAPPED(OPTIONAL)
			NULL                 // pReserved4  (must be NULL)
		);

		if (result != NO_ERROR)
		{
			wprintf(L"HttpSendHttpResponse failed with %lu \n", result);
		}

		return result;
	}

#define MAX_ULONG_STR ((ULONG) sizeof("4294967295"))


	DWORD CHttpBasicServer::SendHttpPostResponse(IN PHTTP_REQUEST pRequest)
	{
		HTTP_RESPONSE   response;
		DWORD           result;
		DWORD           bytesSent;
		PUCHAR          pEntityBuffer;
		ULONG           EntityBufferLength;
		ULONG           BytesRead;
		ULONG           TempFileBytesWritten;
		HANDLE          hTempFile;
		TCHAR           szTempName[MAX_PATH + 1];
		CHAR            szContentLength[MAX_ULONG_STR];
		HTTP_DATA_CHUNK dataChunk;
		ULONG           TotalBytesRead = 0;

		BytesRead = 0;
		hTempFile = INVALID_HANDLE_VALUE;

		DWORD dwReturnCode = (DWORD)-1;


		//
		// Allocate space for an entity buffer. Buffer can be increased 
		// on demand.
		//
		EntityBufferLength = 2048;
		pEntityBuffer = (PUCHAR)ALLOC_MEM(EntityBufferLength);

		if (pEntityBuffer == NULL)
		{
			result = ERROR_NOT_ENOUGH_MEMORY;
			wprintf(L"Insufficient resources \n");
			return dwReturnCode;
		}

		//
		// Initialize the HTTP response structure.
		//
		INITIALIZE_HTTP_RESPONSE(&response, 200, "OK");

		//
		// For POST, echo back the entity from the
		// client
		//
		// NOTE: If the HTTP_RECEIVE_REQUEST_FLAG_COPY_BODY flag had been
		//       passed with HttpReceiveHttpRequest(), the entity would 
		//       have been a part of HTTP_REQUEST (using the pEntityChunks
		//       field). Because that flag was not passed, there are no
		//       o entity bodies in HTTP_REQUEST.
		//

		if (pRequest->Flags & HTTP_REQUEST_FLAG_MORE_ENTITY_BODY_EXISTS)
		{
			// The entity body is sent over multiple calls. Collect 
			// these in a file and send back. Create a temporary 
			// file.
			//

			if (GetTempFileName(L".", L"New", 0, szTempName) == 0)
			{
				result = GetLastError();
				wprintf(L"GetTempFileName failed with %lu \n", result);
				return dwReturnCode;
			}

			hTempFile = CreateFile(szTempName,
				GENERIC_READ | GENERIC_WRITE,
				0,                  // Do not share.
				NULL,               // No security descriptor.
				CREATE_ALWAYS,      // Overwrite existing.
				FILE_ATTRIBUTE_NORMAL,    // Normal file.
				NULL
			);

			if (hTempFile == INVALID_HANDLE_VALUE)
			{
				result = GetLastError();
				wprintf(L"Cannot create temporary file. Error %lu \n", result);
				return dwReturnCode;
			}

			do
			{
				//
				// Read the entity chunk from the request.
				//
				BytesRead = 0;
				result = HttpReceiveRequestEntityBody(
					m_hReqQueue,
					pRequest->RequestId,
					0,
					pEntityBuffer,
					EntityBufferLength,
					&BytesRead,
					NULL
				);

				switch (result)
				{
				case NO_ERROR:

					if (BytesRead != 0)
					{
						TotalBytesRead += BytesRead;
						WriteFile(hTempFile, pEntityBuffer, BytesRead, &TempFileBytesWritten, NULL);
					}
					break;

				case ERROR_HANDLE_EOF:

					//
					// The last request entity body has been read.
					// Send back a response. 
					//
					// To illustrate entity sends via 
					// HttpSendResponseEntityBody, the response will 
					// be sent over multiple calls. To do this,
					// pass the HTTP_SEND_RESPONSE_FLAG_MORE_DATA
					// flag.

					if (BytesRead != 0)
					{
						TotalBytesRead += BytesRead;
						WriteFile(hTempFile, pEntityBuffer, BytesRead, &TempFileBytesWritten, NULL);
					}

					//
					// Because the response is sent over multiple
					// API calls, add a content-length.
					//
					// Alternatively, the response could have been
					// sent using chunked transfer encoding, by  
					// passing "Transfer-Encoding: Chunked".
					//

					// NOTE: Because the TotalBytesread in a ULONG
					//       are accumulated, this will not work
					//       for entity bodies larger than 4 GB. 
					//       For support of large entity bodies,
					//       use a ULONGLONG.
					// 


					sprintf_s(szContentLength, MAX_ULONG_STR, "%lu", TotalBytesRead);

					ADD_KNOWN_HEADER(response, HttpHeaderContentLength, szContentLength);

					result =
						HttpSendHttpResponse(
							m_hReqQueue,           // ReqQueueHandle
							pRequest->RequestId, // Request ID
							HTTP_SEND_RESPONSE_FLAG_MORE_DATA,
							&response,       // HTTP response
							NULL,            // pReserved1
							&bytesSent,      // bytes sent-optional
							NULL,            // pReserved2
							0,               // Reserved3
							NULL,            // LPOVERLAPPED
							NULL             // pReserved4
						);

					if (result != NO_ERROR)
					{
						wprintf(L"HttpSendHttpResponse failed with %lu \n", result);
						return dwReturnCode;
					}

					//
					// Send entity body from a file handle.
					//
					dataChunk.DataChunkType = HttpDataChunkFromFileHandle;

					dataChunk.FromFileHandle.ByteRange.StartingOffset.QuadPart = 0;

					dataChunk.FromFileHandle.ByteRange.Length.QuadPart = HTTP_BYTE_RANGE_TO_EOF;

					dataChunk.FromFileHandle.FileHandle = hTempFile;

					result = HttpSendResponseEntityBody(
						m_hReqQueue,
						pRequest->RequestId,
						0,           // This is the last send.
						1,           // Entity Chunk Count.
						&dataChunk,
						NULL,
						NULL,
						0,
						NULL,
						NULL
					);

					if (result != NO_ERROR)
					{
						wprintf(L"HttpSendResponseEntityBody failed %lu\n", result);
					}
					break;


				default:
					wprintf(L"HttpReceiveRequestEntityBody failed with %lu \n", result);
					break;
				}

			} while (TRUE);
		}
		else
		{
			// This request does not have an entity body.
			//
			result = HttpSendHttpResponse(
				m_hReqQueue,         // ReqQueueHandle
				pRequest->RequestId, // Request ID
				0,
				&response,           // HTTP response
				NULL,                // pReserved1
				&bytesSent,          // bytes sent (optional)
				NULL,                // pReserved2
				0,                   // Reserved3
				NULL,                // LPOVERLAPPED
				NULL                 // pReserved4
			);

			if (result != NO_ERROR)
			{
				wprintf(L"HttpSendHttpResponse failed with %lu \n", result);
			}
			else
				dwReturnCode = 0;
		}

		return dwReturnCode;
	}
}