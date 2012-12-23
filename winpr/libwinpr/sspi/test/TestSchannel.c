
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/thread.h>
#include <winpr/schannel.h>

static void* schannel_test_server_thread(void* arg)
{
	UINT32 cbMaxToken;
	SCHANNEL_CRED cred;
	CredHandle credentials;
	SECURITY_STATUS status;
	PSecPkgInfo pPackageInfo;
	PSecurityFunctionTable table;

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

	return NULL;
}

int TestSchannel(int argc, char* argv[])
{
	int index;
	HANDLE ServerThread;
	UINT32 cbMaxToken;
	SCHANNEL_CRED cred;
	UINT32 fContextReq;
	ULONG fContextAttr;
	CtxtHandle context;
	CredHandle credentials;
	SECURITY_STATUS status;
	PSecPkgInfo pPackageInfo;
	SecBuffer SecBuffer_in[2];
	SecBuffer SecBuffer_out[1];
	SecBufferDesc SecBufferDesc_in;
	SecBufferDesc SecBufferDesc_out;
	PSecurityFunctionTable table;
	SecPkgCred_SupportedAlgs SupportedAlgs;
	SecPkgCred_CipherStrengths CipherStrengths;
	SecPkgCred_SupportedProtocols SupportedProtocols;

	sspi_GlobalInit();

	CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) schannel_test_server_thread, NULL, 0, NULL);

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
		printf(" 0x%04X", SupportedAlgs.palgSupportedAlgs[index]);
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

	fContextReq = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_RET_EXTENDED_ERROR |
			ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM | ISC_REQ_MANUAL_CRED_VALIDATION | ISC_REQ_INTEGRITY;

	ZeroMemory(&SecBuffer_in, sizeof(SecBuffer_in));
	ZeroMemory(&SecBuffer_out, sizeof(SecBuffer_out));
	ZeroMemory(&SecBufferDesc_in, sizeof(SecBufferDesc));
	ZeroMemory(&SecBufferDesc_out, sizeof(SecBufferDesc));

	SecBuffer_in[0].BufferType = SECBUFFER_TOKEN;
	SecBuffer_in[0].pvBuffer = malloc(cbMaxToken);
	SecBuffer_in[0].cbBuffer = cbMaxToken;

	SecBuffer_in[1].BufferType = SECBUFFER_EMPTY;

	SecBufferDesc_in.ulVersion = SECBUFFER_VERSION;
	SecBufferDesc_in.cBuffers = 2;
	SecBufferDesc_in.pBuffers = SecBuffer_in;

	SecBuffer_out[0].BufferType = SECBUFFER_TOKEN;

	SecBufferDesc_out.ulVersion = SECBUFFER_VERSION;
	SecBufferDesc_out.cBuffers = 1;
	SecBufferDesc_out.pBuffers = SecBuffer_out;

	status = table->InitializeSecurityContext(&credentials, NULL, _T("localhost"),
			fContextReq, 0, 0, NULL, 0, &context, &SecBufferDesc_out, &fContextAttr, NULL);

	if (status != SEC_I_CONTINUE_NEEDED)
	{
		printf("InitializeSecurityContext unexpected status: 0x%08X\n", status);
		return -1;
	}

	sspi_GlobalFinish();

	return 0;
}
