
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/file.h>
#include <winpr/pipe.h>
#include <winpr/tchar.h>
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/crypto.h>
#include <winpr/schannel.h>

HANDLE g_ClientEvent = NULL;
HANDLE g_ServerEvent = NULL;
BOOL g_ClientWait = FALSE;
BOOL g_ServerWait = FALSE;

HANDLE g_ClientReadPipe = NULL;
HANDLE g_ClientWritePipe = NULL;
HANDLE g_ServerReadPipe = NULL;
HANDLE g_ServerWritePipe = NULL;

static void* schannel_test_server_thread(void* arg)
{
	BOOL extraData;
	BYTE* lpTokenIn;
	BYTE* lpTokenOut;
	TimeStamp expiry;
	UINT32 cbMaxToken;
	UINT32 fContextReq;
	ULONG fContextAttr;
	SCHANNEL_CRED cred;
	CtxtHandle context;
	CredHandle credentials;
	DWORD cchNameString;
	LPTSTR pszNameString;
	HCERTSTORE hCertStore;
	PCCERT_CONTEXT pCertContext;
	PSecBuffer pSecBuffer;
	SecBuffer SecBuffer_in[2];
	SecBuffer SecBuffer_out[2];
	SecBufferDesc SecBufferDesc_in;
	SecBufferDesc SecBufferDesc_out;
	DWORD NumberOfBytesRead;
	SECURITY_STATUS status;
	PSecPkgInfo pPackageInfo;
	PSecurityFunctionTable table;
	DWORD NumberOfBytesWritten;

	printf("Starting Server\n");

	SecInvalidateHandle(&context);
	SecInvalidateHandle(&credentials);

	table = InitSecurityInterface();

	status = QuerySecurityPackageInfo(SCHANNEL_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo failure: 0x%08X\n", status);
		return NULL;
	}

	cbMaxToken = pPackageInfo->cbMaxToken;

	hCertStore = CertOpenSystemStore(0, _T("MY"));

	if (!hCertStore)
	{
		printf("Error opening system store\n");
		//return NULL;
	}

	pCertContext = CertFindCertificateInStore(hCertStore, X509_ASN_ENCODING, 0, CERT_FIND_HAS_PRIVATE_KEY, NULL, NULL);

	if (!pCertContext)
	{
		printf("Error finding certificate in store\n");
		//return NULL;
	}

	cchNameString = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, 0);

	pszNameString = (LPTSTR) malloc(status * sizeof(TCHAR));
	cchNameString = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, pszNameString, cchNameString);

	_tprintf(_T("Certificate Name: %s\n"), pszNameString);

	ZeroMemory(&cred, sizeof(SCHANNEL_CRED));
	cred.dwVersion = SCHANNEL_CRED_VERSION;

	cred.cCreds = 1;
	cred.paCred = &pCertContext;

	cred.cSupportedAlgs = 0;
	cred.palgSupportedAlgs = NULL;

	cred.grbitEnabledProtocols = SP_PROT_TLS1_SERVER;

	cred.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER;

	status = table->AcquireCredentialsHandle(NULL, SCHANNEL_NAME,
			SECPKG_CRED_INBOUND, NULL, &cred, NULL, NULL, &credentials, NULL);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle failure: 0x%08X\n", status);
		return NULL;
	}

	extraData = FALSE;
	lpTokenIn = (BYTE*) malloc(cbMaxToken);
	lpTokenOut = (BYTE*) malloc(cbMaxToken);

	fContextReq = ASC_REQ_STREAM |
		ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT |
		ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR;

	do
	{
		if (!extraData)
		{
			WaitForSingleObject(g_ServerEvent, INFINITE);
			ResetEvent(g_ServerEvent);

			if (g_ServerWait)
			{
				if (!ReadFile(g_ServerReadPipe, lpTokenIn, cbMaxToken, &NumberOfBytesRead, NULL))
				{
					printf("Failed to read from server pipe\n");
					return NULL;
				}
			}
			else
			{
				NumberOfBytesRead = 0;
			}
		}
		extraData = FALSE;

		printf("Server Received %d bytes:\n", NumberOfBytesRead);
		winpr_HexDump(lpTokenIn, NumberOfBytesRead);

		SecBuffer_in[0].BufferType = SECBUFFER_TOKEN;
		SecBuffer_in[0].pvBuffer = lpTokenIn;
		SecBuffer_in[0].cbBuffer = NumberOfBytesRead;

		SecBuffer_in[1].BufferType = SECBUFFER_EMPTY;
		SecBuffer_in[1].pvBuffer = NULL;
		SecBuffer_in[1].cbBuffer = 0;

		SecBufferDesc_in.ulVersion = SECBUFFER_VERSION;
		SecBufferDesc_in.cBuffers = 2;
		SecBufferDesc_in.pBuffers = SecBuffer_in;

		SecBuffer_out[0].BufferType = SECBUFFER_TOKEN;
		SecBuffer_out[0].pvBuffer = lpTokenOut;
		SecBuffer_out[0].cbBuffer = cbMaxToken;

		SecBufferDesc_out.ulVersion = SECBUFFER_VERSION;
		SecBufferDesc_out.cBuffers = 1;
		SecBufferDesc_out.pBuffers = SecBuffer_out;

		status = table->AcceptSecurityContext(&credentials, SecIsValidHandle(&context) ? &context : NULL,
			&SecBufferDesc_in, fContextReq, 0, &context, &SecBufferDesc_out, &fContextAttr, &expiry);

		if (status == SEC_E_OK)
		{
			printf("AcceptSecurityContext SEC_E_OK, TLS connection complete\n");
			break;
		}

		if ((status != SEC_I_CONTINUE_NEEDED) && (status != SEC_E_INCOMPLETE_MESSAGE))
		{
			printf("AcceptSecurityContext unexpected status: 0x%08X\n", status);
			return NULL;
		}

		NumberOfBytesWritten = 0;

		if (status == SEC_I_CONTINUE_NEEDED)
			printf("AcceptSecurityContext status: SEC_I_CONTINUE_NEEDED\n");
		else if (status == SEC_E_INCOMPLETE_MESSAGE)
			printf("AcceptSecurityContext status: SEC_E_INCOMPLETE_MESSAGE\n");

		printf("Server cBuffers: %d pBuffers[0]: %d type: %d\n",
			SecBufferDesc_out.cBuffers, SecBufferDesc_out.pBuffers[0].cbBuffer, SecBufferDesc_out.pBuffers[0].BufferType);
		printf("Server Input cBuffers: %d pBuffers[1]: %d type: %d\n",
			SecBufferDesc_in.cBuffers, SecBufferDesc_in.pBuffers[1].cbBuffer, SecBufferDesc_in.pBuffers[1].BufferType);

		if (SecBufferDesc_in.pBuffers[1].BufferType == SECBUFFER_EXTRA)
		{
			printf("AcceptSecurityContext SECBUFFER_EXTRA\n");
			pSecBuffer = &SecBufferDesc_in.pBuffers[1];
			CopyMemory(lpTokenIn, &lpTokenIn[NumberOfBytesRead - pSecBuffer->cbBuffer], pSecBuffer->cbBuffer);
			NumberOfBytesRead = pSecBuffer->cbBuffer;
			continue;
		}

		if (status != SEC_E_INCOMPLETE_MESSAGE)
		{
			pSecBuffer = &SecBufferDesc_out.pBuffers[0];
			winpr_HexDump((BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);

			g_ClientWait = TRUE;
			SetEvent(g_ClientEvent);

			if (!WriteFile(g_ClientWritePipe, pSecBuffer->pvBuffer, pSecBuffer->cbBuffer, &NumberOfBytesWritten, NULL))
			{
				printf("failed to write to client pipe\n");
				return NULL;
			}
		}
		else
		{
			g_ClientWait = FALSE;
			SetEvent(g_ClientEvent);
		}

		printf("Server wrote %d bytes\n", NumberOfBytesWritten);
	}
	while (1);

	return NULL;
}

int TestSchannel(int argc, char* argv[])
{
	int index;
	ALG_ID algId;
	HANDLE thread;
	BYTE* lpTokenIn;
	BYTE* lpTokenOut;
	TimeStamp expiry;
	UINT32 cbMaxToken;
	SCHANNEL_CRED cred;
	UINT32 fContextReq;
	ULONG fContextAttr;
	CtxtHandle context;
	CredHandle credentials;
	SECURITY_STATUS status;
	PSecPkgInfo pPackageInfo;
	PSecBuffer pSecBuffer;
	SecBuffer SecBuffer_in[2];
	SecBuffer SecBuffer_out[1];
	SecBufferDesc SecBufferDesc_in;
	SecBufferDesc SecBufferDesc_out;
	PSecurityFunctionTable table;
	DWORD NumberOfBytesRead;
	DWORD NumberOfBytesWritten;
	SecPkgCred_SupportedAlgs SupportedAlgs;
	SecPkgCred_CipherStrengths CipherStrengths;
	SecPkgCred_SupportedProtocols SupportedProtocols;

	sspi_GlobalInit();

	g_ClientEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	g_ServerEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	SecInvalidateHandle(&context);
	SecInvalidateHandle(&credentials);

	if (!CreatePipe(&g_ClientReadPipe, &g_ClientWritePipe, NULL, 0))
	{
		printf("Failed to create client pipe\n");
		return -1;
	}

	if (!CreatePipe(&g_ServerReadPipe, &g_ServerWritePipe, NULL, 0))
	{
		printf("Failed to create server pipe\n");
		return -1;
	}

	thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) schannel_test_server_thread, NULL, 0, NULL);

	table = InitSecurityInterface();

	status = QuerySecurityPackageInfo(SCHANNEL_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo failure: 0x%08X\n", status);
		return -1;
	}

	cbMaxToken = pPackageInfo->cbMaxToken;

	ZeroMemory(&cred, sizeof(SCHANNEL_CRED));
	cred.dwVersion = SCHANNEL_CRED_VERSION;

	cred.cCreds = 0;
	cred.paCred = NULL;

	cred.cSupportedAlgs = 0;
	cred.palgSupportedAlgs = NULL;

	cred.grbitEnabledProtocols = SP_PROT_SSL3TLS1_CLIENTS;

	cred.dwFlags = SCH_CRED_NO_DEFAULT_CREDS;
	cred.dwFlags |= SCH_CRED_MANUAL_CRED_VALIDATION;
	cred.dwFlags |= SCH_CRED_NO_SERVERNAME_CHECK;

	status = table->AcquireCredentialsHandle(NULL, SCHANNEL_NAME,
			SECPKG_CRED_OUTBOUND, NULL, &cred, NULL, NULL, &credentials, NULL);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle failure: 0x%08X\n", status);
		return -1;
	}

	ZeroMemory(&SupportedAlgs, sizeof(SecPkgCred_SupportedAlgs));
	status = table->QueryCredentialsAttributes(&credentials, SECPKG_ATTR_SUPPORTED_ALGS, &SupportedAlgs);

	if (status != SEC_E_OK)
	{
		printf("QueryCredentialsAttributes SECPKG_ATTR_SUPPORTED_ALGS failure: 0x%08X\n", status);
		return -1;
	}

	/**
	 * SupportedAlgs: 15
	 * 0x660E 0x6610 0x6801 0x6603 0x6601 0x8003 0x8004
	 * 0x800C 0x800D 0x800E 0x2400 0xAA02 0xAE06 0x2200 0x2203
	 */

	printf("SupportedAlgs: %d\n", SupportedAlgs.cSupportedAlgs);

	for (index = 0; index < SupportedAlgs.cSupportedAlgs; index++)
	{
		algId = SupportedAlgs.palgSupportedAlgs[index];
		printf("\t0x%04X CLASS: %d TYPE: %d SID: %d\n", algId,
				((GET_ALG_CLASS(algId)) >> 13), ((GET_ALG_TYPE(algId)) >> 9), GET_ALG_SID(algId));
	}
	printf("\n");

	ZeroMemory(&CipherStrengths, sizeof(SecPkgCred_CipherStrengths));
	status = table->QueryCredentialsAttributes(&credentials, SECPKG_ATTR_CIPHER_STRENGTHS, &CipherStrengths);

	if (status != SEC_E_OK)
	{
		printf("QueryCredentialsAttributes SECPKG_ATTR_CIPHER_STRENGTHS failure: 0x%08X\n", status);
		return -1;
	}

	/* CipherStrengths: Minimum: 40 Maximum: 256 */

	printf("CipherStrengths: Minimum: %d Maximum: %d\n",
		CipherStrengths.dwMinimumCipherStrength, CipherStrengths.dwMaximumCipherStrength);

	ZeroMemory(&SupportedProtocols, sizeof(SecPkgCred_SupportedProtocols));
	status = table->QueryCredentialsAttributes(&credentials, SECPKG_ATTR_SUPPORTED_PROTOCOLS, &SupportedProtocols);

	if (status != SEC_E_OK)
	{
		printf("QueryCredentialsAttributes SECPKG_ATTR_SUPPORTED_PROTOCOLS failure: 0x%08X\n", status);
		return -1;
	}

	/* SupportedProtocols: 0x208A0 */

	printf("SupportedProtocols: 0x%04X\n", SupportedProtocols.grbitProtocol);

	fContextReq = ISC_REQ_STREAM |
		ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT |
		ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR |
		ISC_REQ_MANUAL_CRED_VALIDATION | ISC_REQ_INTEGRITY;

	lpTokenIn = (BYTE*) malloc(cbMaxToken);
	lpTokenOut = (BYTE*) malloc(cbMaxToken);

	ZeroMemory(&SecBuffer_in, sizeof(SecBuffer_in));
	ZeroMemory(&SecBuffer_out, sizeof(SecBuffer_out));
	ZeroMemory(&SecBufferDesc_in, sizeof(SecBufferDesc));
	ZeroMemory(&SecBufferDesc_out, sizeof(SecBufferDesc));

	g_ClientWait = FALSE;
	SetEvent(g_ClientEvent);

	do
	{
		WaitForSingleObject(g_ClientEvent, INFINITE);
		ResetEvent(g_ClientEvent);

		if (g_ClientWait)
		{
			if (!ReadFile(g_ClientReadPipe, lpTokenIn, cbMaxToken, &NumberOfBytesRead, NULL))
			{
				printf("failed to read from server pipe\n");
				return -1;
			}
		}
		else
		{
			NumberOfBytesRead = 0;
		}
		
		printf("NumberOfBytesRead: %d\n", NumberOfBytesRead);

		SecBuffer_in[0].BufferType = SECBUFFER_TOKEN;
		SecBuffer_in[0].pvBuffer = lpTokenIn;
		SecBuffer_in[0].cbBuffer = NumberOfBytesRead;

		SecBuffer_in[1].pvBuffer   = NULL;
		SecBuffer_in[1].cbBuffer   = 0;
		SecBuffer_in[1].BufferType = SECBUFFER_EMPTY;

		SecBufferDesc_in.ulVersion = SECBUFFER_VERSION;
		SecBufferDesc_in.cBuffers = 2;
		SecBufferDesc_in.pBuffers = SecBuffer_in;

		SecBuffer_out[0].BufferType = SECBUFFER_TOKEN;
		SecBuffer_out[0].pvBuffer = lpTokenOut;
		SecBuffer_out[0].cbBuffer = cbMaxToken;

		SecBufferDesc_out.ulVersion = SECBUFFER_VERSION;
		SecBufferDesc_out.cBuffers = 1;
		SecBufferDesc_out.pBuffers = SecBuffer_out;

		status = table->InitializeSecurityContext(&credentials, SecIsValidHandle(&context) ? &context : NULL, _T("localhost"),
				fContextReq, 0, 0, &SecBufferDesc_in, 0, &context, &SecBufferDesc_out, &fContextAttr, &expiry);

		if ((status != SEC_I_CONTINUE_NEEDED) && (status != SEC_E_INCOMPLETE_MESSAGE))
		{
			printf("InitializeSecurityContext unexpected status: 0x%08X\n", status);
			return -1;
		}

		NumberOfBytesWritten = 0;

		if (status == SEC_I_CONTINUE_NEEDED)
			printf("InitializeSecurityContext status: SEC_I_CONTINUE_NEEDED\n");
		else if (status == SEC_E_INCOMPLETE_MESSAGE)
			printf("InitializeSecurityContext status: SEC_E_INCOMPLETE_MESSAGE\n");

		printf("Client Output cBuffers: %d pBuffers[0]: %d type: %d\n",
			SecBufferDesc_out.cBuffers, SecBufferDesc_out.pBuffers[0].cbBuffer, SecBufferDesc_out.pBuffers[0].BufferType);
		printf("Client Input cBuffers: %d pBuffers[1]: %d type: %d\n",
			SecBufferDesc_in.cBuffers, SecBufferDesc_in.pBuffers[1].cbBuffer, SecBufferDesc_in.pBuffers[1].BufferType);

		if (status != SEC_E_INCOMPLETE_MESSAGE)
		{
			pSecBuffer = &SecBufferDesc_out.pBuffers[0];
			winpr_HexDump((BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);

			g_ServerWait = TRUE;
			SetEvent(g_ServerEvent);

			if (!WriteFile(g_ServerWritePipe, pSecBuffer->pvBuffer, pSecBuffer->cbBuffer, &NumberOfBytesWritten, NULL))
			{
				printf("failed to write to server pipe\n");
				return -1;
			}
		}
		else
		{
			g_ServerWait = FALSE;
			SetEvent(g_ServerEvent);
		}

		printf("Client wrote %d bytes\n", NumberOfBytesWritten);
	}
	while(1);

	WaitForSingleObject(thread, INFINITE);

	sspi_GlobalFinish();

	return 0;
}
