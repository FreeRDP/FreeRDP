
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/file.h>
#include <winpr/pipe.h>
#include <winpr/print.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/schannel.h>

HANDLE g_ClientReadPipe = NULL;
HANDLE g_ClientWritePipe = NULL;
HANDLE g_ServerReadPipe = NULL;
HANDLE g_ServerWritePipe = NULL;

static void* schannel_test_server_thread(void* arg)
{
	BYTE* lpTokenIn;
	BYTE* lpTokenOut;
	UINT32 cbMaxToken;
	UINT32 fContextReq;
	ULONG fContextAttr;
	SCHANNEL_CRED cred;
	CtxtHandle context;
	CredHandle credentials;
	//PSecBuffer pSecBuffer;
	SecBuffer SecBuffer_in[2];
	SecBuffer SecBuffer_out[2];
	SecBufferDesc SecBufferDesc_in;
	SecBufferDesc SecBufferDesc_out;
	DWORD NumberOfBytesRead;
	SECURITY_STATUS status;
	PSecPkgInfo pPackageInfo;
	PSecurityFunctionTable table;

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

	ZeroMemory(&cred, sizeof(SCHANNEL_CRED));
	cred.dwVersion = SCHANNEL_CRED_VERSION;
	cred.cSupportedAlgs = 0;
	cred.palgSupportedAlgs = NULL;

	status = table->AcquireCredentialsHandle(NULL, SCHANNEL_NAME,
			SECPKG_CRED_INBOUND, NULL, &cred, NULL, NULL, &credentials, NULL);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle failure: 0x%08X\n", status);
		return NULL;
	}

	lpTokenIn = (BYTE*) malloc(cbMaxToken);
	lpTokenOut = (BYTE*) malloc(cbMaxToken);

	if (!ReadFile(g_ServerReadPipe, lpTokenIn, cbMaxToken, &NumberOfBytesRead, NULL))
	{
		printf("Failed to read from server pipe\n");
		return NULL;
	}

	printf("Server Received:\n");
	winpr_HexDump(lpTokenIn, NumberOfBytesRead);

	fContextReq = ASC_REQ_STREAM;

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
	SecBuffer_out[0].cbBuffer = NumberOfBytesRead;

	SecBuffer_out[1].BufferType = SECBUFFER_EMPTY;
	SecBuffer_out[1].pvBuffer = NULL;
	SecBuffer_out[1].cbBuffer = 0;

	SecBufferDesc_out.ulVersion = SECBUFFER_VERSION;
	SecBufferDesc_out.cBuffers = 2;
	SecBufferDesc_out.pBuffers = SecBuffer_out;

	status = table->AcceptSecurityContext(&credentials, SecIsValidHandle(&context) ? &context : NULL,
		&SecBufferDesc_in, fContextReq, 0, &context, &SecBufferDesc_out, &fContextAttr, NULL);

	if (status != SEC_I_CONTINUE_NEEDED)
	{
		printf("AcceptSecurityContext unexpected status: 0x%08X\n", status);
		return NULL;
	}

	return NULL;
}

int TestSchannel(int argc, char* argv[])
{
	int index;
	ALG_ID algId;
	HANDLE thread;
	BYTE* lpTokenIn;
	BYTE* lpTokenOut;
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
	cred.cSupportedAlgs = 0;
	cred.palgSupportedAlgs = NULL;

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

	fContextReq = ISC_REQ_STREAM;

	lpTokenIn = (BYTE*) malloc(cbMaxToken);
	lpTokenOut = (BYTE*) malloc(cbMaxToken);

	ZeroMemory(&SecBuffer_in, sizeof(SecBuffer_in));
	ZeroMemory(&SecBuffer_out, sizeof(SecBuffer_out));
	ZeroMemory(&SecBufferDesc_in, sizeof(SecBufferDesc));
	ZeroMemory(&SecBufferDesc_out, sizeof(SecBufferDesc));

	SecBuffer_in[0].BufferType = SECBUFFER_TOKEN;
	SecBuffer_in[0].pvBuffer = lpTokenIn;
	SecBuffer_in[0].cbBuffer = cbMaxToken;

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
			fContextReq, 0, 0, NULL, 0, &context, &SecBufferDesc_out, &fContextAttr, NULL);

	if (status != SEC_I_CONTINUE_NEEDED)
	{
		printf("InitializeSecurityContext unexpected status: 0x%08X\n", status);
		return -1;
	}

	printf("SecBufferDesc_out.cBuffers: %d\n", SecBufferDesc_out.cBuffers);

	pSecBuffer = &SecBufferDesc_out.pBuffers[0];
	winpr_HexDump((BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);

	/**
	 * 0000 16 03 03 00 96 01 00 00 92 03 03 50 d7 7e 56 dd ...........P.~V.
	 * 0010 c9 01 59 01 49 d3 2f 99 ef da 77 01 a0 97 06 e7 ..Y.I./...w.....
	 * 0020 a4 fe 2d 71 46 55 fc 42 dd 2e 58 00 00 2a 00 3c ..-qFU.B..X..*.<
	 * 0030 00 2f 00 3d 00 35 00 05 00 0a c0 27 c0 13 c0 14 ./.=.5.....'....
	 * 0040 c0 2b c0 23 c0 2c c0 24 c0 09 c0 0a 00 40 00 32 .+.#.,.$.....@.2
	 * 0050 00 6a 00 38 00 13 00 04 01 00 00 3f ff 01 00 01 .j.8.......?....
	 * 0060 00 00 00 00 0e 00 0c 00 00 09 6c 6f 63 61 6c 68 ..........localh
	 * 0070 6f 73 74 00 0a 00 06 00 04 00 17 00 18 00 0b 00 ost.............
	 * 0080 02 01 00 00 0d 00 10 00 0e 04 01 05 01 02 01 04 ................
	 * 0090 03 05 03 02 03 02 02 00 23 00 00                ........#..
	 */

	if (!WriteFile(g_ServerWritePipe, pSecBuffer->pvBuffer, pSecBuffer->cbBuffer, &NumberOfBytesWritten, NULL))
	{
		printf("failed to write to server pipe\n");
		return -1;
	}

	printf("Client wrote %d bytes\n", NumberOfBytesWritten);

	WaitForSingleObject(thread, INFINITE);

	sspi_GlobalFinish();

	return 0;
}
