#pragma once

#include "Http.h"

namespace HttpCore
{
	///////////////////////////////////////////////////////////////////////////////////////////
	// class CHttpBasicServer
	class CHttpBasicServer
	{
	public:
		CHttpBasicServer(CString strIPAddress, int nPort);
		virtual ~CHttpBasicServer(void);

	public:
		bool Init();
		bool CleanUp();

	public:
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
		DWORD SendHttpResponse( IN PHTTP_REQUEST pRequest,
			IN USHORT        StatusCode,
			IN PSTR          pReason,
			IN PSTR          pEntityString
			);

		/// <summary>
		/// Sends a HTTP response after reading the entity body.
		/// </summary>
		/// <param name="pRequest"></param>
		/// <returns></returns>
		DWORD SendHttpPostResponse( IN PHTTP_REQUEST pRequest );
	private:
		CStringW GetFullyQualifiedURL();

	private:
		CString m_strIPAddress;
		int		m_nPort;

		HANDLE  m_hReqQueue;
		bool 	m_bUrlAdded;
	};
}