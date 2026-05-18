#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/winpr.h>

static const char* TEST_USER = "Username";
static const char* TEST_DOMAIN = "Domain";

static SECURITY_STATUS run_client_exchange(SecurityFunctionTable* table, ULONG cbMaxToken,
                                           const char* password, UINT32 extra_identity_flags)
{
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	CredHandle clientCreds = WINPR_C_ARRAY_INIT;
	CredHandle serverCreds = WINPR_C_ARRAY_INIT;
	CtxtHandle clientCtx = WINPR_C_ARRAY_INIT;
	CtxtHandle serverCtx = WINPR_C_ARRAY_INIT;
	TimeStamp expiration = WINPR_C_ARRAY_INIT;
	SEC_WINNT_AUTH_IDENTITY identity = WINPR_C_ARRAY_INIT;
	SecBuffer clientOut = WINPR_C_ARRAY_INIT;
	SecBuffer serverOut = WINPR_C_ARRAY_INIT;
	SecBufferDesc clientOutDesc = { SECBUFFER_VERSION, 1, &clientOut };
	SecBufferDesc serverOutDesc = { SECBUFFER_VERSION, 1, &serverOut };
	/* Each side reads what the other just wrote, so input descs alias the peer's output buffer. */
	SecBufferDesc serverInDesc = { SECBUFFER_VERSION, 1, &clientOut };
	SecBufferDesc clientInDesc = { SECBUFFER_VERSION, 1, &serverOut };
	ULONG attrs = 0;
	const ULONG fContextReq =
	    ISC_REQ_MUTUAL_AUTH | ISC_REQ_CONFIDENTIALITY | ISC_REQ_USE_SESSION_KEY;
	const ULONG fAcceptReq = ASC_REQ_MUTUAL_AUTH | ASC_REQ_CONFIDENTIALITY | ASC_REQ_CONNECTION;

	SecInvalidateHandle(&clientCtx);
	SecInvalidateHandle(&serverCtx);

	if (sspi_SetAuthIdentity(&identity, TEST_USER, TEST_DOMAIN, password) < 0)
		goto fail;

	identity.Flags |= extra_identity_flags;

	status =
	    table->AcquireCredentialsHandle(nullptr, NTLM_SSP_NAME, SECPKG_CRED_OUTBOUND, nullptr,
	                                    &identity, nullptr, nullptr, &clientCreds, &expiration);
	if (status != SEC_E_OK)
		goto fail;

	status = table->AcquireCredentialsHandle(nullptr, NTLM_SSP_NAME, SECPKG_CRED_INBOUND, nullptr,
	                                         nullptr, nullptr, nullptr, &serverCreds, &expiration);
	if (status != SEC_E_OK)
		goto fail;

	/* Client -> Negotiate */
	clientOut.BufferType = SECBUFFER_TOKEN;
	clientOut.cbBuffer = cbMaxToken;
	clientOut.pvBuffer = malloc(cbMaxToken);
	if (!clientOut.pvBuffer)
	{
		status = SEC_E_INSUFFICIENT_MEMORY;
		goto fail;
	}

	status = table->InitializeSecurityContext(&clientCreds, nullptr, nullptr, fContextReq, 0,
	                                          SECURITY_NATIVE_DREP, nullptr, 0, &clientCtx,
	                                          &clientOutDesc, &attrs, &expiration);
	if (status != SEC_I_CONTINUE_NEEDED)
		goto fail;

	/* Server <- Negotiate, Server -> Challenge */
	serverOut.BufferType = SECBUFFER_TOKEN;
	serverOut.cbBuffer = cbMaxToken;
	serverOut.pvBuffer = malloc(cbMaxToken);
	if (!serverOut.pvBuffer)
	{
		status = SEC_E_INSUFFICIENT_MEMORY;
		goto fail;
	}

	status = table->AcceptSecurityContext(&serverCreds, nullptr, &serverInDesc, fAcceptReq,
	                                      SECURITY_NATIVE_DREP, &serverCtx, &serverOutDesc, &attrs,
	                                      &expiration);
	if (status != SEC_I_CONTINUE_NEEDED && status != SEC_E_OK)
		goto fail;

	/* Client <- Challenge, Client -> Authenticate */
	free(clientOut.pvBuffer);
	clientOut.cbBuffer = cbMaxToken;
	clientOut.pvBuffer = malloc(cbMaxToken);
	if (!clientOut.pvBuffer)
	{
		status = SEC_E_INSUFFICIENT_MEMORY;
		goto fail;
	}

	status = table->InitializeSecurityContext(&clientCreds, &clientCtx, nullptr, fContextReq, 0,
	                                          SECURITY_NATIVE_DREP, &clientInDesc, 0, &clientCtx,
	                                          &clientOutDesc, &attrs, &expiration);

fail:
	free(clientOut.pvBuffer);
	free(serverOut.pvBuffer);
	if (SecIsValidHandle(&clientCtx))
		(void)table->DeleteSecurityContext(&clientCtx);
	if (SecIsValidHandle(&serverCtx))
		(void)table->DeleteSecurityContext(&serverCtx);
	if (SecIsValidHandle(&clientCreds))
		(void)table->FreeCredentialsHandle(&clientCreds);
	if (SecIsValidHandle(&serverCreds))
		(void)table->FreeCredentialsHandle(&serverCreds);
	sspi_FreeAuthIdentity(&identity);
	return status;
}

static BOOL status_is_success(SECURITY_STATUS status)
{
	switch (status)
	{
		case SEC_E_OK:
		case SEC_I_CONTINUE_NEEDED:
		case SEC_I_COMPLETE_NEEDED:
		case SEC_I_COMPLETE_AND_CONTINUE:
			return TRUE;
		default:
			return FALSE;
	}
}

/* Long plaintext password (>512 chars) must not be misinterpreted as a pass-the-hash hex blob. */
static BOOL test_long_plaintext_password(SecurityFunctionTable* table, ULONG cbMaxToken)
{
	char long_password[1025];
	memset(long_password, 'A', sizeof(long_password) - 1);
	long_password[sizeof(long_password) - 1] = '\0';

	const SECURITY_STATUS status = run_client_exchange(table, cbMaxToken, long_password, 0);
	if (status == SEC_E_NO_CREDENTIALS)
	{
		printf("FAIL: long plaintext password produced SEC_E_NO_CREDENTIALS\n");
		return FALSE;
	}
	if (!status_is_success(status))
	{
		printf("FAIL: long plaintext password rejected with 0x%08" PRIX32 " (%s)\n", status,
		       GetSecurityStatusString(status));
		return FALSE;
	}
	return TRUE;
}

/* 32-hex-char password tagged with SEC_WINPR_AUTH_IDENTITY_PASSWORD_HASH must route through the
 * pass-the-hash path. */
static BOOL test_password_hash_flag(SecurityFunctionTable* table, ULONG cbMaxToken)
{
	const char* hash_hex = "d5922a65c4d5c082ca444af1be0001db";

	const SECURITY_STATUS status =
	    run_client_exchange(table, cbMaxToken, hash_hex, SEC_WINPR_AUTH_IDENTITY_PASSWORD_HASH);
	if (!status_is_success(status))
	{
		printf("FAIL: pass-the-hash with explicit flag rejected with 0x%08" PRIX32 " (%s)\n",
		       status, GetSecurityStatusString(status));
		return FALSE;
	}
	return TRUE;
}

int TestSspiPasswordHash(int argc, char* argv[])
{
	int rc = -1;
	SecPkgInfo* packageInfo = nullptr;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	sspi_GlobalInit();
	SecurityFunctionTable* table = InitSecurityInterfaceEx(SSPI_INTERFACE_WINPR);
	if (!table)
		goto cleanup;

	if (table->QuerySecurityPackageInfo(NTLM_SSP_NAME, &packageInfo) != SEC_E_OK)
		goto cleanup;

	const ULONG cbMaxToken = packageInfo->cbMaxToken;

	if (!test_long_plaintext_password(table, cbMaxToken))
		goto cleanup;

	if (!test_password_hash_flag(table, cbMaxToken))
		goto cleanup;

	rc = 0;

cleanup:
	if (packageInfo)
		(void)table->FreeContextBuffer(packageInfo);
	sspi_GlobalFinish();
	return rc;
}
