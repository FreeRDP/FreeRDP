
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

BOOL g_ClientWait = FALSE;
BOOL g_ServerWait = FALSE;

HANDLE g_ClientReadPipe = NULL;
HANDLE g_ClientWritePipe = NULL;
HANDLE g_ServerReadPipe = NULL;
HANDLE g_ServerWritePipe = NULL;

BYTE test_DummyMessage[64] =
{
	0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA,
	0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB, 0xBB,
	0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC,
	0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD
};

BYTE test_LastDummyMessage[64] =
{
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

int schannel_send(PSecurityFunctionTable table, HANDLE hPipe, PCtxtHandle phContext, BYTE* buffer, UINT32 length)
{
	BYTE* ioBuffer;
	UINT32 ioBufferLength;
	BYTE* pMessageBuffer;
	SecBuffer Buffers[4];
	SecBufferDesc Message;
	SECURITY_STATUS status;
	DWORD NumberOfBytesWritten;
	SecPkgContext_StreamSizes StreamSizes;

	ZeroMemory(&StreamSizes, sizeof(SecPkgContext_StreamSizes));
	status = table->QueryContextAttributes(phContext, SECPKG_ATTR_STREAM_SIZES, &StreamSizes);

	ioBufferLength = StreamSizes.cbHeader + StreamSizes.cbMaximumMessage + StreamSizes.cbTrailer;
	ioBuffer = (BYTE*) malloc(ioBufferLength);
	ZeroMemory(ioBuffer, ioBufferLength);

	pMessageBuffer = ioBuffer + StreamSizes.cbHeader;
	CopyMemory(pMessageBuffer, buffer, length);

	Buffers[0].pvBuffer = ioBuffer;
	Buffers[0].cbBuffer = StreamSizes.cbHeader;
	Buffers[0].BufferType = SECBUFFER_STREAM_HEADER;

	Buffers[1].pvBuffer = pMessageBuffer;
	Buffers[1].cbBuffer = length;
	Buffers[1].BufferType = SECBUFFER_DATA;

	Buffers[2].pvBuffer = pMessageBuffer + length;
	Buffers[2].cbBuffer = StreamSizes.cbTrailer;
	Buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;

	Buffers[3].pvBuffer = NULL;
	Buffers[3].cbBuffer = 0;
	Buffers[3].BufferType = SECBUFFER_EMPTY;

	Message.ulVersion = SECBUFFER_VERSION;
	Message.cBuffers = 4;
	Message.pBuffers = Buffers;

	ioBufferLength = Message.pBuffers[0].cbBuffer + Message.pBuffers[1].cbBuffer + Message.pBuffers[2].cbBuffer;

	status = table->EncryptMessage(phContext, 0, &Message, 0);

	printf("EncryptMessage status: 0x%08X\n", status);

	printf("EncryptMessage output: cBuffers: %ld [0]: %lu / %lu [1]: %lu / %lu [2]: %lu / %lu [3]: %lu / %lu\n", Message.cBuffers,
		Message.pBuffers[0].cbBuffer, Message.pBuffers[0].BufferType,
		Message.pBuffers[1].cbBuffer, Message.pBuffers[1].BufferType,
		Message.pBuffers[2].cbBuffer, Message.pBuffers[2].BufferType,
		Message.pBuffers[3].cbBuffer, Message.pBuffers[3].BufferType);

	if (status != SEC_E_OK)
		return -1;

	printf("Client > Server (%d)\n", ioBufferLength);
	winpr_HexDump(ioBuffer, ioBufferLength);

	if (!WriteFile(hPipe, ioBuffer, ioBufferLength, &NumberOfBytesWritten, NULL))
	{
		printf("schannel_send: failed to write to pipe\n");
		return -1;
	}

	return 0;
}

int schannel_recv(PSecurityFunctionTable table, HANDLE hPipe, PCtxtHandle phContext)
{
	BYTE* ioBuffer;
	UINT32 ioBufferLength;
	//BYTE* pMessageBuffer;
	SecBuffer Buffers[4];
	SecBufferDesc Message;
	SECURITY_STATUS status;
	DWORD NumberOfBytesRead;
	SecPkgContext_StreamSizes StreamSizes;

	ZeroMemory(&StreamSizes, sizeof(SecPkgContext_StreamSizes));
	status = table->QueryContextAttributes(phContext, SECPKG_ATTR_STREAM_SIZES, &StreamSizes);

	ioBufferLength = StreamSizes.cbHeader + StreamSizes.cbMaximumMessage + StreamSizes.cbTrailer;
	ioBuffer = (BYTE*) malloc(ioBufferLength);
	ZeroMemory(ioBuffer, ioBufferLength);

	if (!ReadFile(hPipe, ioBuffer, ioBufferLength, &NumberOfBytesRead, NULL))
	{
		printf("schannel_recv: failed to read from pipe\n");
		return -1;
	}

	Buffers[0].pvBuffer = ioBuffer;
	Buffers[0].cbBuffer = NumberOfBytesRead;
	Buffers[0].BufferType = SECBUFFER_DATA;

	Buffers[1].pvBuffer = NULL;
	Buffers[1].cbBuffer = 0;
	Buffers[1].BufferType = SECBUFFER_EMPTY;

	Buffers[2].pvBuffer = NULL;
	Buffers[2].cbBuffer = 0;
	Buffers[2].BufferType = SECBUFFER_EMPTY;

	Buffers[3].pvBuffer = NULL;
	Buffers[3].cbBuffer = 0;
	Buffers[3].BufferType = SECBUFFER_EMPTY;

	Message.ulVersion = SECBUFFER_VERSION;
	Message.cBuffers = 4;
	Message.pBuffers = Buffers;

	status = table->DecryptMessage(phContext, &Message, 0, NULL);

	printf("DecryptMessage status: 0x%08X\n", status);

	printf("DecryptMessage output: cBuffers: %ld [0]: %lu / %lu [1]: %lu / %lu [2]: %lu / %lu [3]: %lu / %lu\n", Message.cBuffers,
		Message.pBuffers[0].cbBuffer, Message.pBuffers[0].BufferType,
		Message.pBuffers[1].cbBuffer, Message.pBuffers[1].BufferType,
		Message.pBuffers[2].cbBuffer, Message.pBuffers[2].BufferType,
		Message.pBuffers[3].cbBuffer, Message.pBuffers[3].BufferType);

	if (status != SEC_E_OK)
		return -1;

	printf("Decrypted Message (%ld)\n", Message.pBuffers[1].cbBuffer);
	winpr_HexDump((BYTE*) Message.pBuffers[1].pvBuffer, Message.pBuffers[1].cbBuffer);

	if (memcmp(Message.pBuffers[1].pvBuffer, test_LastDummyMessage, sizeof(test_LastDummyMessage)) == 0)
		return -1;

	return 0;
}

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

#ifdef CERT_FIND_HAS_PRIVATE_KEY
	pCertContext = CertFindCertificateInStore(hCertStore, X509_ASN_ENCODING, 0, CERT_FIND_HAS_PRIVATE_KEY, NULL, NULL);
#else
	pCertContext = CertFindCertificateInStore(hCertStore, X509_ASN_ENCODING, 0, CERT_FIND_ANY, NULL, NULL);
#endif

	if (!pCertContext)
	{
		printf("Error finding certificate in store\n");
		//return NULL;
	}

	cchNameString = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, 0);

	pszNameString = (LPTSTR) malloc(cchNameString * sizeof(TCHAR));
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
	g_ServerWait = TRUE;

	lpTokenIn = (BYTE*) malloc(cbMaxToken);
	lpTokenOut = (BYTE*) malloc(cbMaxToken);

	fContextReq = ASC_REQ_STREAM |
		ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT |
		ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR;

	do
	{
		if (!extraData)
		{
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
		g_ServerWait = TRUE;

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

		if ((status != SEC_E_OK) && (status != SEC_I_CONTINUE_NEEDED) && (status != SEC_E_INCOMPLETE_MESSAGE))
		{
			printf("AcceptSecurityContext unexpected status: 0x%08X\n", status);
			return NULL;
		}

		NumberOfBytesWritten = 0;

		if (status == SEC_E_OK)
			printf("AcceptSecurityContext status: SEC_E_OK\n");
		else if (status == SEC_I_CONTINUE_NEEDED)
			printf("AcceptSecurityContext status: SEC_I_CONTINUE_NEEDED\n");
		else if (status == SEC_E_INCOMPLETE_MESSAGE)
			printf("AcceptSecurityContext status: SEC_E_INCOMPLETE_MESSAGE\n");

		printf("Server cBuffers: %lu pBuffers[0]: %lu type: %lu\n",
			SecBufferDesc_out.cBuffers, SecBufferDesc_out.pBuffers[0].cbBuffer, SecBufferDesc_out.pBuffers[0].BufferType);
		printf("Server Input cBuffers: %ld pBuffers[0]: %lu type: %lu pBuffers[1]: %lu type: %lu\n", SecBufferDesc_in.cBuffers,
			SecBufferDesc_in.pBuffers[0].cbBuffer, SecBufferDesc_in.pBuffers[0].BufferType,
			SecBufferDesc_in.pBuffers[1].cbBuffer, SecBufferDesc_in.pBuffers[1].BufferType);

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

			if (pSecBuffer->cbBuffer > 0)
			{
				printf("Server > Client (%ld)\n", pSecBuffer->cbBuffer);
				winpr_HexDump((BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);

				if (!WriteFile(g_ClientWritePipe, pSecBuffer->pvBuffer, pSecBuffer->cbBuffer, &NumberOfBytesWritten, NULL))
				{
					printf("failed to write to client pipe\n");
					return NULL;
				}
			}
		}

		if (status == SEC_E_OK)
		{
			printf("Server Handshake Complete\n");
			break;
		}
	}
	while (1);

	do
	{
		if (schannel_recv(table, g_ServerReadPipe, &context) < 0)
			break;
	}
	while(1);

	return NULL;
}

int TestSchannel(int argc, char* argv[])
{
	int count;
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

	printf("SupportedAlgs: %ld\n", SupportedAlgs.cSupportedAlgs);

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

	printf("CipherStrengths: Minimum: %ld Maximum: %ld\n",
		CipherStrengths.dwMinimumCipherStrength, CipherStrengths.dwMaximumCipherStrength);

	ZeroMemory(&SupportedProtocols, sizeof(SecPkgCred_SupportedProtocols));
	status = table->QueryCredentialsAttributes(&credentials, SECPKG_ATTR_SUPPORTED_PROTOCOLS, &SupportedProtocols);

	if (status != SEC_E_OK)
	{
		printf("QueryCredentialsAttributes SECPKG_ATTR_SUPPORTED_PROTOCOLS failure: 0x%08X\n", status);
		return -1;
	}

	/* SupportedProtocols: 0x208A0 */

	printf("SupportedProtocols: 0x%04lX\n", SupportedProtocols.grbitProtocol);

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

	do
	{
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

		g_ClientWait = TRUE;
		printf("NumberOfBytesRead: %ld\n", NumberOfBytesRead);

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

		if ((status != SEC_E_OK) && (status != SEC_I_CONTINUE_NEEDED) && (status != SEC_E_INCOMPLETE_MESSAGE))
		{
			printf("InitializeSecurityContext unexpected status: 0x%08X\n", status);
			return -1;
		}

		NumberOfBytesWritten = 0;

		if (status == SEC_E_OK)
			printf("InitializeSecurityContext status: SEC_E_OK\n");
		else if (status == SEC_I_CONTINUE_NEEDED)
			printf("InitializeSecurityContext status: SEC_I_CONTINUE_NEEDED\n");
		else if (status == SEC_E_INCOMPLETE_MESSAGE)
			printf("InitializeSecurityContext status: SEC_E_INCOMPLETE_MESSAGE\n");

		printf("Client Output cBuffers: %ld pBuffers[0]: %ld type: %ld\n",
			SecBufferDesc_out.cBuffers, SecBufferDesc_out.pBuffers[0].cbBuffer, SecBufferDesc_out.pBuffers[0].BufferType);
		printf("Client Input cBuffers: %ld pBuffers[0]: %ld type: %ld pBuffers[1]: %ld type: %ld\n", SecBufferDesc_in.cBuffers,
			SecBufferDesc_in.pBuffers[0].cbBuffer, SecBufferDesc_in.pBuffers[0].BufferType,
			SecBufferDesc_in.pBuffers[1].cbBuffer, SecBufferDesc_in.pBuffers[1].BufferType);

		if (status != SEC_E_INCOMPLETE_MESSAGE)
		{
			pSecBuffer = &SecBufferDesc_out.pBuffers[0];

			if (pSecBuffer->cbBuffer > 0)
			{
				printf("Client > Server (%ld)\n", pSecBuffer->cbBuffer);
				winpr_HexDump((BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);

				if (!WriteFile(g_ServerWritePipe, pSecBuffer->pvBuffer, pSecBuffer->cbBuffer, &NumberOfBytesWritten, NULL))
				{
					printf("failed to write to server pipe\n");
					return -1;
				}
			}
		}

		if (status == SEC_E_OK)
		{
			printf("Client Handshake Complete\n");
			break;
		}
	}
	while(1);

	count = 0;

	do
	{
		if (schannel_send(table, g_ServerWritePipe, &context, test_DummyMessage, sizeof(test_DummyMessage)) < 0)
			break;

		for (index = 0; index < sizeof(test_DummyMessage); index++)
		{
			BYTE b, ln, hn;

			b = test_DummyMessage[index];

			ln = (b & 0x0F);
			hn = ((b & 0xF0) >> 4);

			ln = (ln + 1) % 0xF;
			hn = (ln + 1) % 0xF;

			b = (ln | (hn << 4));

			test_DummyMessage[index] = b;
		}

		Sleep(100);
		count++;
	}
	while(count < 3);

	schannel_send(table, g_ServerWritePipe, &context, test_LastDummyMessage, sizeof(test_LastDummyMessage));

	WaitForSingleObject(thread, INFINITE);

	sspi_GlobalFinish();

	return 0;
}
