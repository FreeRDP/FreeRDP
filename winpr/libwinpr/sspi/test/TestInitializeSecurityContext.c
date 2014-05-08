
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>

static const char* test_User = "User";
static const char* test_Domain = "Domain";
static const char* test_Password = "Password";

int TestInitializeSecurityContext(int argc, char* argv[])
{
	UINT32 cbMaxLen;
	UINT32 fContextReq;
	void* output_buffer;
	CtxtHandle context;
	ULONG pfContextAttr;
	SECURITY_STATUS status;
	CredHandle credentials;
	TimeStamp expiration;
	PSecPkgInfo pPackageInfo;
	SEC_WINNT_AUTH_IDENTITY identity;
	SecurityFunctionTable* table;
	PSecBuffer p_SecBuffer;
	SecBuffer output_SecBuffer;
	SecBufferDesc output_SecBuffer_desc;

	sspi_GlobalInit();

	table = InitSecurityInterface();

	status = QuerySecurityPackageInfo(NTLMSP_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo status: 0x%08X\n", status);
		return -1;
	}

	cbMaxLen = pPackageInfo->cbMaxToken;

	identity.User = (UINT16*) _strdup(test_User);
	identity.UserLength = sizeof(test_User);
	identity.Domain = (UINT16*) _strdup(test_Domain);
	identity.DomainLength = sizeof(test_Domain);
	identity.Password = (UINT16*) _strdup(test_Password);
	identity.PasswordLength = sizeof(test_Password);
	identity.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;

	status = table->AcquireCredentialsHandle(NULL, NTLMSP_NAME,
			SECPKG_CRED_OUTBOUND, NULL, &identity, NULL, NULL, &credentials, &expiration);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle status: 0x%08X\n", status);
		sspi_GlobalFinish();
		return -1;
	}

	fContextReq = ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_DELEGATE;

	output_buffer = malloc(cbMaxLen);

	output_SecBuffer_desc.ulVersion = 0;
	output_SecBuffer_desc.cBuffers = 1;
	output_SecBuffer_desc.pBuffers = &output_SecBuffer;

	output_SecBuffer.cbBuffer = cbMaxLen;
	output_SecBuffer.BufferType = SECBUFFER_TOKEN;
	output_SecBuffer.pvBuffer = output_buffer;

	status = table->InitializeSecurityContext(&credentials, NULL, NULL, fContextReq, 0, 0, NULL, 0,
			&context, &output_SecBuffer_desc, &pfContextAttr, &expiration);

	if (status != SEC_I_CONTINUE_NEEDED)
	{
		printf("InitializeSecurityContext status: 0x%08X\n", status);
		sspi_GlobalFinish();
		return -1;
	}

	printf("cBuffers: %d ulVersion: %d\n", output_SecBuffer_desc.cBuffers, output_SecBuffer_desc.ulVersion);

	p_SecBuffer = &output_SecBuffer_desc.pBuffers[0];

	printf("BufferType: 0x%04X cbBuffer: %d\n", p_SecBuffer->BufferType, p_SecBuffer->cbBuffer);

	table->FreeCredentialsHandle(&credentials);

	FreeContextBuffer(pPackageInfo);

	sspi_GlobalFinish();

	return 0;
}

