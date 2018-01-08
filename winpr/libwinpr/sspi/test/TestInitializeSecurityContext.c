
#include <stdio.h>
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>

static const char* test_User = "User";
static const char* test_Domain = "Domain";
static const char* test_Password = "Password";

int TestInitializeSecurityContext(int argc, char* argv[])
{
	int rc = -1;
	UINT32 cbMaxLen;
	UINT32 fContextReq;
	void* output_buffer = NULL;
	CtxtHandle context;
	ULONG pfContextAttr;
	SECURITY_STATUS status;
	CredHandle credentials = { 0 };
	TimeStamp expiration;
	PSecPkgInfo pPackageInfo;
	SEC_WINNT_AUTH_IDENTITY identity = { 0 };
	SecurityFunctionTable* table;
	PSecBuffer p_SecBuffer;
	SecBuffer output_SecBuffer;
	SecBufferDesc output_SecBuffer_desc;
	sspi_GlobalInit();
	table = InitSecurityInterface();
	status = QuerySecurityPackageInfo(NTLM_SSP_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		printf("QuerySecurityPackageInfo status: 0x%08"PRIX32"\n", status);
		goto fail;
	}

	cbMaxLen = pPackageInfo->cbMaxToken;
	identity.User = (UINT16*) _strdup(test_User);
	identity.Domain = (UINT16*) _strdup(test_Domain);
	identity.Password = (UINT16*) _strdup(test_Password);

	if (!identity.User || !identity.Domain || !identity.Password)
		goto fail;

	identity.UserLength = strlen(test_User);
	identity.DomainLength = strlen(test_Domain);
	identity.PasswordLength = strlen(test_Password);
	identity.Flags = SEC_WINNT_AUTH_IDENTITY_ANSI;
	status = table->AcquireCredentialsHandle(NULL, NTLM_SSP_NAME,
	         SECPKG_CRED_OUTBOUND, NULL, &identity, NULL, NULL, &credentials, &expiration);

	if (status != SEC_E_OK)
	{
		printf("AcquireCredentialsHandle status: 0x%08"PRIX32"\n", status);
		goto fail;
	}

	fContextReq = ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_CONFIDENTIALITY |
	              ISC_REQ_DELEGATE;
	output_buffer = malloc(cbMaxLen);

	if (!output_buffer)
	{
		printf("Memory allocation failed\n");
		goto fail;
	}

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
		printf("InitializeSecurityContext status: 0x%08"PRIX32"\n", status);
		goto fail;
	}

	printf("cBuffers: %"PRIu32" ulVersion: %"PRIu32"\n", output_SecBuffer_desc.cBuffers,
	       output_SecBuffer_desc.ulVersion);
	p_SecBuffer = &output_SecBuffer_desc.pBuffers[0];
	printf("BufferType: 0x%08"PRIX32" cbBuffer: %"PRIu32"\n", p_SecBuffer->BufferType,
	       p_SecBuffer->cbBuffer);
	status = table->DeleteSecurityContext(&context);

	if (status != SEC_E_OK)
	{
		printf("DeleteSecurityContext status: 0x%08"PRIX32"\n", status);
		goto fail;
	}

	rc = 0;
fail:
	free(identity.User);
	free(identity.Domain);
	free(identity.Password);
	free(output_buffer);

	if (SecIsValidHandle(&credentials))
		table->FreeCredentialsHandle(&credentials);

	FreeContextBuffer(pPackageInfo);
	sspi_GlobalFinish();
	return rc;
}

