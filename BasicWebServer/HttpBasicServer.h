#pragma once
#include "Http.h"

#define _DF_CERT_HASH_LEN_ (20)

namespace HttpCore
{
	///////////////////////////////////////////////////////////////////////////////////////////
	// class CHttpBasicServer
	class CHttpBasicServer
	{
	public:
		CHttpBasicServer(CString strIPAddress, int nPort, LPCSTR lpszCertThumbPrint = NULL);
		~CHttpBasicServer();

	public:
		bool Init();

		bool Run();

	protected:
		DWORD DoReceiveRequests();

		/// <summary>
		/// The function to receive a request.
		/// </summary>
		/// <param name="pRequest"></param>
		/// <param name="StatusCode"></param>
		/// <param name="pReason"></param>
		/// <param name="pEntityString"></param>
		/// <returns></returns>
		DWORD SendHttpResponse(
			IN PHTTP_REQUEST pRequest,
			IN USHORT        StatusCode,
			IN PSTR          pReason,
			IN PSTR          pEntityString
		);

		/// <summary>
		/// Sends a HTTP response after reading the entity body.
		/// </summary>
		/// <param name="pRequest"></param>
		/// <returns></returns>
		DWORD SendHttpPostResponse(IN PHTTP_REQUEST pRequest);

	private:
		CStringW GetFullyQualifiedURI() const;

		bool InitializeSsl();

		bool ReleaseSsl();

		bool CleanUp();

	private:
		CString		m_strIPAddress;
		int			m_nPort;

		CStringA	m_strCertThumbPrint;


		HANDLE		m_hReqQueue;
		bool 		m_bUrlAdded;

		bool		m_bServerSslCertInit;
		BYTE		m_arrCertHash[_DF_CERT_HASH_LEN_];

		SOCKADDR_IN m_oSockAddrIn;
		HTTP_SERVICE_CONFIG_SSL_SET* m_pHttpServiceConfigSsl;
	};
};

