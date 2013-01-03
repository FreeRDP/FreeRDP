
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

#ifndef _WIN32
#define SCHANNEL_PKG_NAME	SCHANNEL_NAME
#else
#define SCHANNEL_PKG_NAME	UNISP_NAME
#endif

static void* schannel_test_server_thread(void* arg)
{
	BYTE* lpTokenIn;
	BYTE* lpTokenOut;
	TimeStamp expiry;
	UINT32 cbMaxToken;
	UINT32 fContextReq;
	ULONG fContextAttr;
	SCHANNEL_CRED cred;
	CtxtHandle context;
	CredHandle credentials;
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

	status = QuerySecurityPackageInfo(SCHANNEL_PKG_NAME, &pPackageInfo);

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

	pCertContext = CertFindCertificateInStore(hCertStore, X509_ASN_ENCODING, 0, CERT_FIND_ANY, NULL, NULL);

	if (!pCertContext)
	{
		printf("Error finding certificate in store\n");
		//return NULL;
	}

	ZeroMemory(&cred, sizeof(SCHANNEL_CRED));
	cred.dwVersion = SCHANNEL_CRED_VERSION;

	cred.cCreds = 1;
	cred.paCred = &pCertContext;

	cred.cSupportedAlgs = 0;
	cred.palgSupportedAlgs = NULL;

	cred.grbitEnabledProtocols = SP_PROT_TLS1_SERVER;

	cred.dwFlags = SCH_CRED_NO_SYSTEM_MAPPER;

	status = table->AcquireCredentialsHandle(NULL, SCHANNEL_PKG_NAME,
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

	fContextReq = ASC_REQ_STREAM |
		ASC_REQ_SEQUENCE_DETECT | ASC_REQ_REPLAY_DETECT |
		ASC_REQ_CONFIDENTIALITY | ASC_REQ_EXTENDED_ERROR;

	SecBuffer_in[0].BufferType = SECBUFFER_TOKEN;
	SecBuffer_in[0].pvBuffer = lpTokenIn;
	SecBuffer_in[0].cbBuffer = NumberOfBytesRead;

	SecBufferDesc_in.ulVersion = SECBUFFER_VERSION;
	SecBufferDesc_in.cBuffers = 1;
	SecBufferDesc_in.pBuffers = SecBuffer_in;

	SecBuffer_out[0].BufferType = SECBUFFER_TOKEN;
	SecBuffer_out[0].pvBuffer = lpTokenOut;
	SecBuffer_out[0].cbBuffer = cbMaxToken;

	SecBufferDesc_out.ulVersion = SECBUFFER_VERSION;
	SecBufferDesc_out.cBuffers = 1;
	SecBufferDesc_out.pBuffers = SecBuffer_out;

	status = table->AcceptSecurityContext(&credentials, SecIsValidHandle(&context) ? &context : NULL,
		&SecBufferDesc_in, fContextReq, SECURITY_NATIVE_DREP, &context, &SecBufferDesc_out, &fContextAttr, &expiry);

	if (status != SEC_I_CONTINUE_NEEDED)
	{
		printf("AcceptSecurityContext unexpected status: 0x%08X\n", status);
		return NULL;
	}

	printf("SecBufferDesc_out.cBuffers: %d\n", SecBufferDesc_out.cBuffers);

	pSecBuffer = &SecBufferDesc_out.pBuffers[0];
	winpr_HexDump((BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);

	if (!WriteFile(g_ClientWritePipe, pSecBuffer->pvBuffer, pSecBuffer->cbBuffer, &NumberOfBytesWritten, NULL))
	{
		printf("failed to write to client pipe\n");
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

	status = QuerySecurityPackageInfo(SCHANNEL_PKG_NAME, &pPackageInfo);

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

	status = table->AcquireCredentialsHandle(NULL, SCHANNEL_PKG_NAME,
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

	SecBuffer_in[0].BufferType = SECBUFFER_TOKEN;
	SecBuffer_in[0].pvBuffer = lpTokenIn;
	SecBuffer_in[0].cbBuffer = cbMaxToken;

	SecBufferDesc_in.ulVersion = SECBUFFER_VERSION;
	SecBufferDesc_in.cBuffers = 1;
	SecBufferDesc_in.pBuffers = SecBuffer_in;

	SecBuffer_out[0].BufferType = SECBUFFER_TOKEN;
	SecBuffer_out[0].pvBuffer = lpTokenOut;
	SecBuffer_out[0].cbBuffer = cbMaxToken;

	SecBufferDesc_out.ulVersion = SECBUFFER_VERSION;
	SecBufferDesc_out.cBuffers = 1;
	SecBufferDesc_out.pBuffers = SecBuffer_out;

	status = table->InitializeSecurityContext(&credentials, SecIsValidHandle(&context) ? &context : NULL, _T("localhost"),
			fContextReq, SECURITY_NATIVE_DREP, 0, NULL, 0, &context, &SecBufferDesc_out, &fContextAttr, &expiry);

	if (status != SEC_I_CONTINUE_NEEDED)
	{
		printf("InitializeSecurityContext unexpected status: 0x%08X\n", status);
		return -1;
	}

	printf("SecBufferDesc_out.cBuffers: %d\n", SecBufferDesc_out.cBuffers);

	pSecBuffer = &SecBufferDesc_out.pBuffers[0];
	winpr_HexDump((BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);

	if (!WriteFile(g_ServerWritePipe, pSecBuffer->pvBuffer, pSecBuffer->cbBuffer, &NumberOfBytesWritten, NULL))
	{
		printf("failed to write to server pipe\n");
		return -1;
	}

	printf("Client wrote %d bytes\n", NumberOfBytesWritten);

	if (!ReadFile(g_ClientReadPipe, lpTokenIn, cbMaxToken, &NumberOfBytesRead, NULL))
	{
		printf("failed to read from server pipe\n");
		return -1;
	}

	WaitForSingleObject(thread, INFINITE);

	sspi_GlobalFinish();

	return 0;
}
