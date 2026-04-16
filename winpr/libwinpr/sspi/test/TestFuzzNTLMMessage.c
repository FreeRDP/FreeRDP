/**
 * WinPR: Windows Portable Runtime
 * libFuzzer harness for the NTLM SSPI lifecycle
 */

#include <stddef.h>
#include <stdint.h>

#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/wlog.h>

#include "../sspi.h"

#define TEST_SSPI_INTERFACE SSPI_INTERFACE_WINPR
#define NTLM_PACKAGE_NAME NTLM_SSP_NAME

static const char* TEST_NTLM_USER = "Username";
static const char* TEST_NTLM_DOMAIN = "Domain";
static const char* TEST_NTLM_PASSWORD = "P4ss123!";
static const BYTE TEST_NTLM_V2_HASH[16] = { 0x4c, 0x7f, 0x70, 0x6f, 0x7d, 0xde, 0x05, 0xa9,
	                                        0xd1, 0xa0, 0xf4, 0xe7, 0xff, 0xe3, 0xbf, 0xb8 };

typedef struct
{
	CtxtHandle context;
	ULONG cbMaxToken;
	ULONG fContextReq;
	ULONG pfContextAttr;
	TimeStamp expiration;
	SecBuffer inputBuffer[1];
	SecBuffer outputBuffer[1];
	BOOL haveContext;
	BOOL haveInputBuffer;
	SecBufferDesc inputBufferDesc;
	SecBufferDesc outputBufferDesc;
	CredHandle credentials;
	SecPkgInfo* pPackageInfo;
	SecurityFunctionTable* table;
	SEC_WINNT_AUTH_IDENTITY identity;
} FUZZ_NTLM_CLIENT;

typedef struct
{
	CtxtHandle context;
	ULONG cbMaxToken;
	ULONG fContextReq;
	ULONG pfContextAttr;
	TimeStamp expiration;
	SecBuffer inputBuffer[1];
	SecBuffer outputBuffer[1];
	BOOL haveContext;
	BOOL haveInputBuffer;
	SecBufferDesc inputBufferDesc;
	SecBufferDesc outputBufferDesc;
	CredHandle credentials;
	SecPkgInfo* pPackageInfo;
	SecurityFunctionTable* table;
} FUZZ_NTLM_SERVER;

static void fuzz_ntlm_client_uninit(FUZZ_NTLM_CLIENT* client)
{
	if (!client)
		return;

	free(client->outputBuffer[0].pvBuffer);
	client->outputBuffer[0].pvBuffer = NULL;

	free(client->identity.User);
	free(client->identity.Domain);
	free(client->identity.Password);

	if (client->table)
	{
		if (SecIsValidHandle(&client->credentials))
			(void)client->table->FreeCredentialsHandle(&client->credentials);
		if (client->pPackageInfo)
			(void)client->table->FreeContextBuffer(client->pPackageInfo);
		if (client->haveContext && SecIsValidHandle(&client->context))
			(void)client->table->DeleteSecurityContext(&client->context);
	}
}

static BOOL fuzz_ntlm_client_init(FUZZ_NTLM_CLIENT* client)
{
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;

	WINPR_ASSERT(client);

	ZeroMemory(client, sizeof(*client));
	SecInvalidateHandle(&client->context);
	SecInvalidateHandle(&client->credentials);

	client->table = InitSecurityInterfaceEx(TEST_SSPI_INTERFACE);
	if (!client->table)
		return FALSE;

	if (sspi_SetAuthIdentity(&client->identity, TEST_NTLM_USER, TEST_NTLM_DOMAIN, TEST_NTLM_PASSWORD) < 0)
		return FALSE;

	status = client->table->QuerySecurityPackageInfo(NTLM_PACKAGE_NAME, &client->pPackageInfo);
	if (status != SEC_E_OK)
		return FALSE;

	client->cbMaxToken = client->pPackageInfo->cbMaxToken;
	status = client->table->AcquireCredentialsHandle(nullptr, NTLM_PACKAGE_NAME, SECPKG_CRED_OUTBOUND,
	                                                 nullptr, &client->identity, nullptr, nullptr,
	                                                 &client->credentials, &client->expiration);
	if (status != SEC_E_OK)
		return FALSE;

	client->fContextReq = ISC_REQ_MUTUAL_AUTH | ISC_REQ_CONFIDENTIALITY | ISC_REQ_USE_SESSION_KEY;
	return TRUE;
}

static SECURITY_STATUS fuzz_ntlm_client_step(FUZZ_NTLM_CLIENT* client)
{
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;

	WINPR_ASSERT(client);

	free(client->outputBuffer[0].pvBuffer);
	client->outputBuffer[0].pvBuffer = NULL;

	client->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
	client->outputBufferDesc.cBuffers = ARRAYSIZE(client->outputBuffer);
	client->outputBufferDesc.pBuffers = client->outputBuffer;
	client->outputBuffer[0].BufferType = SECBUFFER_TOKEN;
	client->outputBuffer[0].cbBuffer = client->cbMaxToken;
	client->outputBuffer[0].pvBuffer = calloc(1, client->outputBuffer[0].cbBuffer);
	if (!client->outputBuffer[0].pvBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (client->haveInputBuffer)
	{
		client->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
		client->inputBufferDesc.cBuffers = ARRAYSIZE(client->inputBuffer);
		client->inputBufferDesc.pBuffers = client->inputBuffer;
		client->inputBuffer[0].BufferType = SECBUFFER_TOKEN;
	}

	status = client->table->InitializeSecurityContext(
	    &client->credentials, client->haveContext ? &client->context : nullptr, nullptr,
	    client->fContextReq, 0, SECURITY_NATIVE_DREP,
	    client->haveInputBuffer ? &client->inputBufferDesc : nullptr, 0, &client->context,
	    &client->outputBufferDesc, &client->pfContextAttr, &client->expiration);

	if ((status == SEC_I_COMPLETE_AND_CONTINUE) || (status == SEC_I_COMPLETE_NEEDED))
	{
		if (client->table->CompleteAuthToken)
			(void)client->table->CompleteAuthToken(&client->context, &client->outputBufferDesc);

		if (status == SEC_I_COMPLETE_NEEDED)
			status = SEC_E_OK;
		else
			status = SEC_I_CONTINUE_NEEDED;
	}

	if (!IsSecurityStatusError(status))
		client->haveContext = TRUE;

	return status;
}

static void fuzz_ntlm_server_uninit(FUZZ_NTLM_SERVER* server)
{
	if (!server)
		return;

	free(server->outputBuffer[0].pvBuffer);
	server->outputBuffer[0].pvBuffer = NULL;

	if (server->table)
	{
		if (SecIsValidHandle(&server->credentials))
			(void)server->table->FreeCredentialsHandle(&server->credentials);
		if (server->pPackageInfo)
			(void)server->table->FreeContextBuffer(server->pPackageInfo);
		if (server->haveContext && SecIsValidHandle(&server->context))
			(void)server->table->DeleteSecurityContext(&server->context);
	}
}

static BOOL fuzz_ntlm_server_init(FUZZ_NTLM_SERVER* server)
{
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;

	WINPR_ASSERT(server);

	ZeroMemory(server, sizeof(*server));
	SecInvalidateHandle(&server->context);
	SecInvalidateHandle(&server->credentials);

	server->table = InitSecurityInterfaceEx(TEST_SSPI_INTERFACE);
	if (!server->table)
		return FALSE;

	status = server->table->QuerySecurityPackageInfo(NTLM_PACKAGE_NAME, &server->pPackageInfo);
	if (status != SEC_E_OK)
		return FALSE;

	server->cbMaxToken = server->pPackageInfo->cbMaxToken;
	status = server->table->AcquireCredentialsHandle(nullptr, NTLM_PACKAGE_NAME, SECPKG_CRED_INBOUND,
	                                                 nullptr, nullptr, nullptr, nullptr,
	                                                 &server->credentials, &server->expiration);
	if (status != SEC_E_OK)
		return FALSE;

	server->fContextReq = ASC_REQ_MUTUAL_AUTH | ASC_REQ_CONFIDENTIALITY | ASC_REQ_CONNECTION |
	                      ASC_REQ_USE_SESSION_KEY | ASC_REQ_REPLAY_DETECT |
	                      ASC_REQ_SEQUENCE_DETECT | ASC_REQ_EXTENDED_ERROR;
	return TRUE;
}

static SECURITY_STATUS fuzz_ntlm_server_step(FUZZ_NTLM_SERVER* server)
{
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;

	WINPR_ASSERT(server);

	free(server->outputBuffer[0].pvBuffer);
	server->outputBuffer[0].pvBuffer = NULL;

	server->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
	server->inputBufferDesc.cBuffers = ARRAYSIZE(server->inputBuffer);
	server->inputBufferDesc.pBuffers = server->inputBuffer;
	server->inputBuffer[0].BufferType = SECBUFFER_TOKEN;

	server->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
	server->outputBufferDesc.cBuffers = ARRAYSIZE(server->outputBuffer);
	server->outputBufferDesc.pBuffers = server->outputBuffer;
	server->outputBuffer[0].BufferType = SECBUFFER_TOKEN;
	server->outputBuffer[0].cbBuffer = server->cbMaxToken;
	server->outputBuffer[0].pvBuffer = calloc(1, server->outputBuffer[0].cbBuffer);
	if (!server->outputBuffer[0].pvBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	status = server->table->AcceptSecurityContext(
	    &server->credentials, server->haveContext ? &server->context : nullptr, &server->inputBufferDesc,
	    server->fContextReq, SECURITY_NATIVE_DREP, &server->context, &server->outputBufferDesc,
	    &server->pfContextAttr, &server->expiration);

	if (!IsSecurityStatusError(status))
		server->haveContext = TRUE;

	if (status == SEC_I_CONTINUE_NEEDED)
	{
		SecPkgContext_AuthNtlmHash hash = WINPR_C_ARRAY_INIT;
		hash.Version = 2;
		CopyMemory(hash.NtlmHash, TEST_NTLM_V2_HASH, sizeof(TEST_NTLM_V2_HASH));
		(void)server->table->SetContextAttributes(&server->context, SECPKG_ATTR_AUTH_NTLM_HASH, &hash,
		                                          sizeof(hash));
	}

	return status;
}

static void fuzz_ntlm_set_input(SecBuffer* buffer, BOOL* haveInputBuffer, const uint8_t* data, size_t size)
{
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(haveInputBuffer);

	buffer[0].BufferType = SECBUFFER_TOKEN;
	buffer[0].pvBuffer = (void*)data;
	buffer[0].cbBuffer = (ULONG)size;
	*haveInputBuffer = TRUE;
}

static void fuzz_ntlm_negotiate(const uint8_t* data, size_t size)
{
	FUZZ_NTLM_SERVER server = { 0 };

	if (!fuzz_ntlm_server_init(&server))
		goto fail;

	fuzz_ntlm_set_input(server.inputBuffer, &server.haveInputBuffer, data, size);
	(void)fuzz_ntlm_server_step(&server);

fail:
	fuzz_ntlm_server_uninit(&server);
}

static void fuzz_ntlm_challenge(const uint8_t* data, size_t size)
{
	FUZZ_NTLM_CLIENT client = { 0 };

	if (!fuzz_ntlm_client_init(&client))
		goto fail;

	if (IsSecurityStatusError(fuzz_ntlm_client_step(&client)))
		goto fail;

	fuzz_ntlm_set_input(client.inputBuffer, &client.haveInputBuffer, data, size);
	(void)fuzz_ntlm_client_step(&client);

fail:
	fuzz_ntlm_client_uninit(&client);
}

static void fuzz_ntlm_authenticate(const uint8_t* data, size_t size)
{
	FUZZ_NTLM_CLIENT client = { 0 };
	FUZZ_NTLM_SERVER server = { 0 };

	if (!fuzz_ntlm_client_init(&client))
		goto fail;
	if (!fuzz_ntlm_server_init(&server))
		goto fail;

	if (fuzz_ntlm_client_step(&client) != SEC_I_CONTINUE_NEEDED)
		goto fail;

	fuzz_ntlm_set_input(server.inputBuffer, &server.haveInputBuffer, client.outputBuffer[0].pvBuffer,
	                    client.outputBuffer[0].cbBuffer);
	if (fuzz_ntlm_server_step(&server) != SEC_I_CONTINUE_NEEDED)
		goto fail;

	fuzz_ntlm_set_input(server.inputBuffer, &server.haveInputBuffer, data, size);
	(void)fuzz_ntlm_server_step(&server);

fail:
	fuzz_ntlm_client_uninit(&client);
	fuzz_ntlm_server_uninit(&server);
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	static BOOL loggingInitialized = FALSE;

	if (!loggingInitialized)
	{
		(void)WLog_SetLogLevel(WLog_GetRoot(), WLOG_OFF);
		loggingInitialized = TRUE;
	}

	if (size < 2)
		return 0;
	if (size > (1u << 20))
		return 0;

	switch (data[0] % 3)
	{
		case 0:
			fuzz_ntlm_negotiate(data + 1, size - 1);
			break;
		case 1:
			fuzz_ntlm_challenge(data + 1, size - 1);
			break;
		case 2:
			fuzz_ntlm_authenticate(data + 1, size - 1);
			break;
	}

	return 0;
}
