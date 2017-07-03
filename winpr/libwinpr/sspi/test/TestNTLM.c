
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/wlog.h>

static BYTE TEST_NTLM_TIMESTAMP[8] = { 0x33, 0x57, 0xbd, 0xb1, 0x07, 0x8b, 0xcf, 0x01 };

static BYTE TEST_NTLM_CLIENT_CHALLENGE[8] = { 0x20, 0xc0, 0x2b, 0x3d, 0xc0, 0x61, 0xa7, 0x73 };

static BYTE TEST_NTLM_SERVER_CHALLENGE[8] = { 0xa4, 0xf1, 0xba, 0xa6, 0x7c, 0xdc, 0x1a, 0x12 };

static BYTE TEST_NTLM_NEGOTIATE[] =
    "\x4e\x54\x4c\x4d\x53\x53\x50\x00\x01\x00\x00\x00\x07\x82\x08\xa2"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x06\x03\x80\x25\x00\x00\x00\x0f";

static BYTE TEST_NTLM_CHALLENGE[] =
    "\x4e\x54\x4c\x4d\x53\x53\x50\x00\x02\x00\x00\x00\x00\x00\x00\x00"
    "\x38\x00\x00\x00\x07\x82\x88\xa2\xa4\xf1\xba\xa6\x7c\xdc\x1a\x12"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x66\x00\x66\x00\x38\x00\x00\x00"
    "\x06\x03\x80\x25\x00\x00\x00\x0f\x02\x00\x0e\x00\x4e\x00\x45\x00"
    "\x57\x00\x59\x00\x45\x00\x41\x00\x52\x00\x01\x00\x0e\x00\x4e\x00"
    "\x45\x00\x57\x00\x59\x00\x45\x00\x41\x00\x52\x00\x04\x00\x1c\x00"
    "\x6c\x00\x61\x00\x62\x00\x2e\x00\x77\x00\x61\x00\x79\x00\x6b\x00"
    "\x2e\x00\x6c\x00\x6f\x00\x63\x00\x61\x00\x6c\x00\x03\x00\x0e\x00"
    "\x6e\x00\x65\x00\x77\x00\x79\x00\x65\x00\x61\x00\x72\x00\x07\x00"
    "\x08\x00\x33\x57\xbd\xb1\x07\x8b\xcf\x01\x00\x00\x00\x00";

static BYTE TEST_NTLM_AUTHENTICATE[] =
    "\x4e\x54\x4c\x4d\x53\x53\x50\x00\x03\x00\x00\x00\x18\x00\x18\x00"
    "\x82\x00\x00\x00\x08\x01\x08\x01\x9a\x00\x00\x00\x0c\x00\x0c\x00"
    "\x58\x00\x00\x00\x10\x00\x10\x00\x64\x00\x00\x00\x0e\x00\x0e\x00"
    "\x74\x00\x00\x00\x00\x00\x00\x00\xa2\x01\x00\x00\x05\x82\x88\xa2"
    "\x06\x03\x80\x25\x00\x00\x00\x0f\x12\xe5\x5a\xf5\x80\xee\x3f\x29"
    "\xe1\xde\x90\x4d\x73\x77\x06\x25\x44\x00\x6f\x00\x6d\x00\x61\x00"
    "\x69\x00\x6e\x00\x55\x00\x73\x00\x65\x00\x72\x00\x6e\x00\x61\x00"
    "\x6d\x00\x65\x00\x4e\x00\x45\x00\x57\x00\x59\x00\x45\x00\x41\x00"
    "\x52\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x62\x14\x68\xc8\x98\x12"
    "\xe7\x39\xd8\x76\x1b\xe9\xf7\x54\xb5\xe3\x01\x01\x00\x00\x00\x00"
    "\x00\x00\x33\x57\xbd\xb1\x07\x8b\xcf\x01\x20\xc0\x2b\x3d\xc0\x61"
    "\xa7\x73\x00\x00\x00\x00\x02\x00\x0e\x00\x4e\x00\x45\x00\x57\x00"
    "\x59\x00\x45\x00\x41\x00\x52\x00\x01\x00\x0e\x00\x4e\x00\x45\x00"
    "\x57\x00\x59\x00\x45\x00\x41\x00\x52\x00\x04\x00\x1c\x00\x6c\x00"
    "\x61\x00\x62\x00\x2e\x00\x77\x00\x61\x00\x79\x00\x6b\x00\x2e\x00"
    "\x6c\x00\x6f\x00\x63\x00\x61\x00\x6c\x00\x03\x00\x0e\x00\x6e\x00"
    "\x65\x00\x77\x00\x79\x00\x65\x00\x61\x00\x72\x00\x07\x00\x08\x00"
    "\x33\x57\xbd\xb1\x07\x8b\xcf\x01\x06\x00\x04\x00\x02\x00\x00\x00"
    "\x08\x00\x30\x00\x30\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00"
    "\x00\x20\x00\x00\x1e\x10\xf5\x2c\x54\x2f\x2e\x77\x1c\x13\xbf\xc3"
    "\x3f\xe1\x7b\x28\x7e\x0b\x93\x5a\x39\xd2\xce\x12\xd7\xbd\x8c\x4e"
    "\x2b\xb5\x0b\xf5\x0a\x00\x10\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00\x00\x00\x00\x00\x00\x00\x09\x00\x1a\x00\x48\x00\x54\x00"
    "\x54\x00\x50\x00\x2f\x00\x72\x00\x77\x00\x2e\x00\x6c\x00\x6f\x00"
    "\x63\x00\x61\x00\x6c\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00"
    "\x00\x00";

#define TEST_SSPI_INTERFACE SSPI_INTERFACE_WINPR

static const char* TEST_NTLM_USER = "Username";
static const char* TEST_NTLM_DOMAIN = "Domain";
static const char* TEST_NTLM_PASSWORD = "P4ss123!";

//static const char* TEST_NTLM_HASH_STRING = "d5922a65c4d5c082ca444af1be0001db";

static const BYTE TEST_NTLM_HASH[16] =
{ 0xd5, 0x92, 0x2a, 0x65, 0xc4, 0xd5, 0xc0, 0x82, 0xca, 0x44, 0x4a, 0xf1, 0xbe, 0x00, 0x01, 0xdb };

//static const char* TEST_NTLM_HASH_V2_STRING = "4c7f706f7dde05a9d1a0f4e7ffe3bfb8";

static const BYTE TEST_NTLM_V2_HASH[16] =
{ 0x4c, 0x7f, 0x70, 0x6f, 0x7d, 0xde, 0x05, 0xa9, 0xd1, 0xa0, 0xf4, 0xe7, 0xff, 0xe3, 0xbf, 0xb8 };

#define NTLM_PACKAGE_NAME	NTLM_SSP_NAME

struct _TEST_NTLM_CLIENT
{
	CtxtHandle context;
	ULONG cbMaxToken;
	ULONG fContextReq;
	ULONG pfContextAttr;
	TimeStamp expiration;
	PSecBuffer pBuffer;
	SecBuffer inputBuffer[2];
	SecBuffer outputBuffer[2];
	BOOL haveContext;
	BOOL haveInputBuffer;
	LPTSTR ServicePrincipalName;
	SecBufferDesc inputBufferDesc;
	SecBufferDesc outputBufferDesc;
	CredHandle credentials;
	BOOL confidentiality;
	SecPkgInfo* pPackageInfo;
	SecurityFunctionTable* table;
	SEC_WINNT_AUTH_IDENTITY identity;
};
typedef struct _TEST_NTLM_CLIENT TEST_NTLM_CLIENT;

int test_ntlm_client_init(TEST_NTLM_CLIENT* ntlm, const char* user, const char* domain,
                          const char* password)
{
	SECURITY_STATUS status;
	SecInvalidateHandle(&(ntlm->context));
	ntlm->table = InitSecurityInterfaceEx(TEST_SSPI_INTERFACE);
	sspi_SetAuthIdentity(&(ntlm->identity), user, domain, password);
	status = ntlm->table->QuerySecurityPackageInfo(NTLM_PACKAGE_NAME, &ntlm->pPackageInfo);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "QuerySecurityPackageInfo status: %s (0x%08"PRIX32")\n",
		        GetSecurityStatusString(status), status);
		return -1;
	}

	ntlm->cbMaxToken = ntlm->pPackageInfo->cbMaxToken;
	status = ntlm->table->AcquireCredentialsHandle(NULL, NTLM_PACKAGE_NAME,
	         SECPKG_CRED_OUTBOUND, NULL, &ntlm->identity, NULL, NULL, &ntlm->credentials, &ntlm->expiration);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "AcquireCredentialsHandle status: %s (0x%08"PRIX32")\n",
		        GetSecurityStatusString(status), status);
		return -1;
	}

	ntlm->haveContext = FALSE;
	ntlm->haveInputBuffer = FALSE;
	ZeroMemory(&ntlm->inputBuffer, sizeof(SecBuffer));
	ZeroMemory(&ntlm->outputBuffer, sizeof(SecBuffer));
	ntlm->fContextReq = 0;
#if 0
	/* HTTP authentication flags */
	ntlm->fContextReq |= ISC_REQ_CONFIDENTIALITY;
#endif
	/* NLA authentication flags */
	ntlm->fContextReq |= ISC_REQ_MUTUAL_AUTH;
	ntlm->fContextReq |= ISC_REQ_CONFIDENTIALITY;
	ntlm->fContextReq |= ISC_REQ_USE_SESSION_KEY;
	return 1;
}

void test_ntlm_client_uninit(TEST_NTLM_CLIENT* ntlm)
{
	if (!ntlm)
		return;

	if (ntlm->outputBuffer[0].pvBuffer)
	{
		free(ntlm->outputBuffer[0].pvBuffer);
		ntlm->outputBuffer[0].pvBuffer = NULL;
	}

	free(ntlm->identity.User);
	free(ntlm->identity.Domain);
	free(ntlm->identity.Password);
	free(ntlm->ServicePrincipalName);

	if (ntlm->table)
	{
		ntlm->table->FreeCredentialsHandle(&ntlm->credentials);
		ntlm->table->FreeContextBuffer(ntlm->pPackageInfo);
		ntlm->table->DeleteSecurityContext(&ntlm->context);
	}
}

/**
 *                                        SSPI Client Ceremony
 *
 *                                           --------------
 *                                          ( Client Begin )
 *                                           --------------
 *                                                 |
 *                                                 |
 *                                                \|/
 *                                      -----------+--------------
 *                                     | AcquireCredentialsHandle |
 *                                      --------------------------
 *                                                 |
 *                                                 |
 *                                                \|/
 *                                    -------------+--------------
 *                 +---------------> / InitializeSecurityContext /
 *                 |                 ----------------------------
 *                 |                               |
 *                 |                               |
 *                 |                              \|/
 *     ---------------------------        ---------+-------------            ----------------------
 *    / Receive blob from server /      < Received security blob? > --Yes-> / Send blob to server /
 *    -------------+-------------         -----------------------           ----------------------
 *                /|\                              |                                |
 *                 |                               No                               |
 *                Yes                             \|/                               |
 *                 |                   ------------+-----------                     |
 *                 +---------------- < Received Continue Needed > <-----------------+
 *                                     ------------------------
 *                                                 |
 *                                                 No
 *                                                \|/
 *                                           ------+-------
 *                                          (  Client End  )
 *                                           --------------
 */

int test_ntlm_client_authenticate(TEST_NTLM_CLIENT* ntlm)
{
	SECURITY_STATUS status;

	if (ntlm->outputBuffer[0].pvBuffer)
	{
		free(ntlm->outputBuffer[0].pvBuffer);
		ntlm->outputBuffer[0].pvBuffer = NULL;
	}

	ntlm->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
	ntlm->outputBufferDesc.cBuffers = 1;
	ntlm->outputBufferDesc.pBuffers = ntlm->outputBuffer;
	ntlm->outputBuffer[0].BufferType = SECBUFFER_TOKEN;
	ntlm->outputBuffer[0].cbBuffer = ntlm->cbMaxToken;
	ntlm->outputBuffer[0].pvBuffer = malloc(ntlm->outputBuffer[0].cbBuffer);

	if (!ntlm->outputBuffer[0].pvBuffer)
		return -1;

	if (ntlm->haveInputBuffer)
	{
		ntlm->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
		ntlm->inputBufferDesc.cBuffers = 1;
		ntlm->inputBufferDesc.pBuffers = ntlm->inputBuffer;
		ntlm->inputBuffer[0].BufferType = SECBUFFER_TOKEN;
	}

	if ((!ntlm) || (!ntlm->table))
	{
		fprintf(stderr, "ntlm_authenticate: invalid ntlm context\n");
		return -1;
	}

	status = ntlm->table->InitializeSecurityContext(&ntlm->credentials,
	         (ntlm->haveContext) ? &ntlm->context : NULL,
	         (ntlm->ServicePrincipalName) ? ntlm->ServicePrincipalName : NULL,
	         ntlm->fContextReq, 0, SECURITY_NATIVE_DREP,
	         (ntlm->haveInputBuffer) ? &ntlm->inputBufferDesc : NULL,
	         0, &ntlm->context, &ntlm->outputBufferDesc,
	         &ntlm->pfContextAttr, &ntlm->expiration);

	if ((status == SEC_I_COMPLETE_AND_CONTINUE) || (status == SEC_I_COMPLETE_NEEDED))
	{
		if (ntlm->table->CompleteAuthToken)
			ntlm->table->CompleteAuthToken(&ntlm->context, &ntlm->outputBufferDesc);

		if (status == SEC_I_COMPLETE_NEEDED)
			status = SEC_E_OK;
		else if (status == SEC_I_COMPLETE_AND_CONTINUE)
			status = SEC_I_CONTINUE_NEEDED;
	}

	if (ntlm->haveInputBuffer)
	{
		free(ntlm->inputBuffer[0].pvBuffer);
	}

	ntlm->haveInputBuffer = TRUE;
	ntlm->haveContext = TRUE;
	return (status == SEC_I_CONTINUE_NEEDED) ? 1 : 0;
}

TEST_NTLM_CLIENT* test_ntlm_client_new()
{
	TEST_NTLM_CLIENT* ntlm;
	ntlm = (TEST_NTLM_CLIENT*) calloc(1, sizeof(TEST_NTLM_CLIENT));

	if (!ntlm)
		return NULL;

	return ntlm;
}

void test_ntlm_client_free(TEST_NTLM_CLIENT* ntlm)
{
	if (!ntlm)
		return;

	test_ntlm_client_uninit(ntlm);
	free(ntlm);
}

struct _TEST_NTLM_SERVER
{
	CtxtHandle context;
	ULONG cbMaxToken;
	ULONG fContextReq;
	ULONG pfContextAttr;
	TimeStamp expiration;
	PSecBuffer pBuffer;
	SecBuffer inputBuffer[2];
	SecBuffer outputBuffer[2];
	BOOL haveContext;
	BOOL haveInputBuffer;
	BOOL UseNtlmV2Hash;
	LPTSTR ServicePrincipalName;
	SecBufferDesc inputBufferDesc;
	SecBufferDesc outputBufferDesc;
	CredHandle credentials;
	BOOL confidentiality;
	SecPkgInfo* pPackageInfo;
	SecurityFunctionTable* table;
	SEC_WINNT_AUTH_IDENTITY identity;
};
typedef struct _TEST_NTLM_SERVER TEST_NTLM_SERVER;

int test_ntlm_server_init(TEST_NTLM_SERVER* ntlm)
{
	SECURITY_STATUS status;
	ntlm->UseNtlmV2Hash = TRUE;
	SecInvalidateHandle(&(ntlm->context));
	ntlm->table = InitSecurityInterfaceEx(TEST_SSPI_INTERFACE);
	status = ntlm->table->QuerySecurityPackageInfo(NTLM_PACKAGE_NAME, &ntlm->pPackageInfo);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "QuerySecurityPackageInfo status: %s (0x%08"PRIX32")\n",
		        GetSecurityStatusString(status), status);
		return -1;
	}

	ntlm->cbMaxToken = ntlm->pPackageInfo->cbMaxToken;
	status = ntlm->table->AcquireCredentialsHandle(NULL, NTLM_PACKAGE_NAME,
	         SECPKG_CRED_INBOUND, NULL, NULL, NULL, NULL,
	         &ntlm->credentials, &ntlm->expiration);

	if (status != SEC_E_OK)
	{
		fprintf(stderr, "AcquireCredentialsHandle status: %s (0x%08"PRIX32")\n",
		        GetSecurityStatusString(status), status);
		return -1;
	}

	ntlm->haveContext = FALSE;
	ntlm->haveInputBuffer = FALSE;
	ZeroMemory(&ntlm->inputBuffer, sizeof(SecBuffer));
	ZeroMemory(&ntlm->outputBuffer, sizeof(SecBuffer));
	ntlm->fContextReq = 0;
	/* NLA authentication flags */
	ntlm->fContextReq |= ASC_REQ_MUTUAL_AUTH;
	ntlm->fContextReq |= ASC_REQ_CONFIDENTIALITY;
	ntlm->fContextReq |= ASC_REQ_CONNECTION;
	ntlm->fContextReq |= ASC_REQ_USE_SESSION_KEY;
	ntlm->fContextReq |= ASC_REQ_REPLAY_DETECT;
	ntlm->fContextReq |= ASC_REQ_SEQUENCE_DETECT;
	ntlm->fContextReq |= ASC_REQ_EXTENDED_ERROR;
	return 1;
}

void test_ntlm_server_uninit(TEST_NTLM_SERVER* ntlm)
{
	if (!ntlm)
		return;

	if (ntlm->outputBuffer[0].pvBuffer)
	{
		free(ntlm->outputBuffer[0].pvBuffer);
		ntlm->outputBuffer[0].pvBuffer = NULL;
	}

	free(ntlm->identity.User);
	free(ntlm->identity.Domain);
	free(ntlm->identity.Password);
	free(ntlm->ServicePrincipalName);

	if (ntlm->table)
	{
		ntlm->table->FreeCredentialsHandle(&ntlm->credentials);
		ntlm->table->FreeContextBuffer(ntlm->pPackageInfo);
		ntlm->table->DeleteSecurityContext(&ntlm->context);
	}
}

int test_ntlm_server_authenticate(TEST_NTLM_SERVER* ntlm)
{
	SECURITY_STATUS status;
	ntlm->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
	ntlm->inputBufferDesc.cBuffers = 1;
	ntlm->inputBufferDesc.pBuffers = ntlm->inputBuffer;
	ntlm->inputBuffer[0].BufferType = SECBUFFER_TOKEN;
	ntlm->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
	ntlm->outputBufferDesc.cBuffers = 1;
	ntlm->outputBufferDesc.pBuffers = &ntlm->outputBuffer[0];
	ntlm->outputBuffer[0].BufferType = SECBUFFER_TOKEN;
	ntlm->outputBuffer[0].cbBuffer = ntlm->cbMaxToken;
	ntlm->outputBuffer[0].pvBuffer = malloc(ntlm->outputBuffer[0].cbBuffer);

	if (!ntlm->outputBuffer[0].pvBuffer)
		return -1;

	status = ntlm->table->AcceptSecurityContext(&ntlm->credentials,
	         ntlm->haveContext ? &ntlm->context : NULL,
	         &ntlm->inputBufferDesc, ntlm->fContextReq, SECURITY_NATIVE_DREP, &ntlm->context,
	         &ntlm->outputBufferDesc, &ntlm->pfContextAttr, &ntlm->expiration);

	if ((status == SEC_I_COMPLETE_AND_CONTINUE) || (status == SEC_I_COMPLETE_NEEDED))
	{
		SecPkgContext_AuthIdentity AuthIdentity;
		SecPkgContext_AuthNtlmHash AuthNtlmHash;
		ZeroMemory(&AuthIdentity, sizeof(SecPkgContext_AuthIdentity));
		ZeroMemory(&AuthNtlmHash, sizeof(SecPkgContext_AuthNtlmHash));
		status = ntlm->table->QueryContextAttributes(&ntlm->context, SECPKG_ATTR_AUTH_IDENTITY,
		         &AuthIdentity);

		if (status == SEC_E_OK)
		{
			if (strcmp(AuthIdentity.User, TEST_NTLM_USER) == 0)
			{
				if (ntlm->UseNtlmV2Hash)
				{
					AuthNtlmHash.Version = 2;
					CopyMemory(AuthNtlmHash.NtlmHash, TEST_NTLM_V2_HASH, 16);
				}
				else
				{
					AuthNtlmHash.Version = 1;
					CopyMemory(AuthNtlmHash.NtlmHash, TEST_NTLM_HASH, 16);
				}

				status = ntlm->table->SetContextAttributes(&ntlm->context,
				         SECPKG_ATTR_AUTH_NTLM_HASH, &AuthNtlmHash, sizeof(SecPkgContext_AuthNtlmHash));
			}
		}

		if (ntlm->table->CompleteAuthToken)
			status = ntlm->table->CompleteAuthToken(&ntlm->context, &ntlm->outputBufferDesc);

		if (status == SEC_I_COMPLETE_NEEDED)
			status = SEC_E_OK;
		else if (status == SEC_I_COMPLETE_AND_CONTINUE)
			status = SEC_I_CONTINUE_NEEDED;
	}

	if ((status != SEC_E_OK) && (status != SEC_I_CONTINUE_NEEDED))
	{
		fprintf(stderr, "AcceptSecurityContext status: %s (0x%08"PRIX32")\n",
		        GetSecurityStatusString(status), status);
		return -1; /* Access Denied */
	}

	ntlm->haveContext = TRUE;
	return (status == SEC_I_CONTINUE_NEEDED) ? 1 : 0;
}

TEST_NTLM_SERVER* test_ntlm_server_new()
{
	TEST_NTLM_SERVER* ntlm;
	ntlm = (TEST_NTLM_SERVER*) calloc(1, sizeof(TEST_NTLM_SERVER));

	if (!ntlm)
		return NULL;

	return ntlm;
}

void test_ntlm_server_free(TEST_NTLM_SERVER* ntlm)
{
	if (!ntlm)
		return;

	test_ntlm_server_uninit(ntlm);
	free(ntlm);
}

int TestNTLM(int argc, char* argv[])
{
	int status;
	PSecBuffer pSecBuffer;
	TEST_NTLM_CLIENT* client;
	TEST_NTLM_SERVER* server;
	BOOL DynamicTest = TRUE;
	/**
	 * Client Initialization
	 */
	client = test_ntlm_client_new();

	if (!client)
	{
		printf("Memory allocation failed");
		return -1;
	}

	status = test_ntlm_client_init(client, TEST_NTLM_USER, TEST_NTLM_DOMAIN, TEST_NTLM_PASSWORD);

	if (status < 0)
	{
		printf("test_ntlm_client_init failure\n");
		return -1;
	}

	/**
	 * Server Initialization
	 */
	server = test_ntlm_server_new();

	if (!server)
	{
		printf("Memory allocation failed\n");
		return -1;
	}

	status = test_ntlm_server_init(server);

	if (status < 0)
	{
		printf("test_ntlm_server_init failure\n");
		return -1;
	}

	/**
	 * Client -> Negotiate Message
	 */
	status = test_ntlm_client_authenticate(client);

	if (status < 0)
	{
		printf("test_ntlm_client_authenticate failure\n");
		return -1;
	}

	if (!DynamicTest)
	{
		SecPkgContext_AuthNtlmTimestamp AuthNtlmTimestamp;
		SecPkgContext_AuthNtlmClientChallenge AuthNtlmClientChallenge;
		SecPkgContext_AuthNtlmServerChallenge AuthNtlmServerChallenge;
		CopyMemory(AuthNtlmTimestamp.Timestamp, TEST_NTLM_TIMESTAMP, 8);
		AuthNtlmTimestamp.ChallengeOrResponse = TRUE;
		client->table->SetContextAttributes(&client->context, SECPKG_ATTR_AUTH_NTLM_TIMESTAMP,
		                                    &AuthNtlmTimestamp, sizeof(SecPkgContext_AuthNtlmTimestamp));
		AuthNtlmTimestamp.ChallengeOrResponse = FALSE;
		client->table->SetContextAttributes(&client->context, SECPKG_ATTR_AUTH_NTLM_TIMESTAMP,
		                                    &AuthNtlmTimestamp, sizeof(SecPkgContext_AuthNtlmTimestamp));
		CopyMemory(AuthNtlmClientChallenge.ClientChallenge, TEST_NTLM_CLIENT_CHALLENGE, 8);
		CopyMemory(AuthNtlmServerChallenge.ServerChallenge, TEST_NTLM_SERVER_CHALLENGE, 8);
		client->table->SetContextAttributes(&client->context, SECPKG_ATTR_AUTH_NTLM_CLIENT_CHALLENGE,
		                                    &AuthNtlmClientChallenge, sizeof(SecPkgContext_AuthNtlmClientChallenge));
		client->table->SetContextAttributes(&client->context, SECPKG_ATTR_AUTH_NTLM_SERVER_CHALLENGE,
		                                    &AuthNtlmServerChallenge, sizeof(SecPkgContext_AuthNtlmServerChallenge));
	}

	pSecBuffer = &(client->outputBuffer[0]);

	if (!DynamicTest)
	{
		pSecBuffer->cbBuffer = sizeof(TEST_NTLM_NEGOTIATE) - 1;
		pSecBuffer->pvBuffer = (void*) malloc(pSecBuffer->cbBuffer);

		if (!pSecBuffer->pvBuffer)
		{
			printf("Memory allocation failed\n");
			return -1;
		}

		CopyMemory(pSecBuffer->pvBuffer, TEST_NTLM_NEGOTIATE, pSecBuffer->cbBuffer);
	}

	fprintf(stderr, "NTLM_NEGOTIATE (length = %"PRIu32"):\n", pSecBuffer->cbBuffer);
	winpr_HexDump("sspi.test", WLOG_DEBUG, (BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);
	/**
	 * Server <- Negotiate Message
	 * Server -> Challenge Message
	 */
	server->haveInputBuffer = TRUE;
	server->inputBuffer[0].BufferType = SECBUFFER_TOKEN;
	server->inputBuffer[0].pvBuffer = pSecBuffer->pvBuffer;
	server->inputBuffer[0].cbBuffer = pSecBuffer->cbBuffer;
	status = test_ntlm_server_authenticate(server);

	if (status < 0)
	{
		printf("test_ntlm_server_authenticate failure\n");
		return -1;
	}

	if (!DynamicTest)
	{
		SecPkgContext_AuthNtlmTimestamp AuthNtlmTimestamp;
		SecPkgContext_AuthNtlmClientChallenge AuthNtlmClientChallenge;
		SecPkgContext_AuthNtlmServerChallenge AuthNtlmServerChallenge;
		CopyMemory(AuthNtlmTimestamp.Timestamp, TEST_NTLM_TIMESTAMP, 8);
		AuthNtlmTimestamp.ChallengeOrResponse = TRUE;
		client->table->SetContextAttributes(&server->context, SECPKG_ATTR_AUTH_NTLM_TIMESTAMP,
		                                    &AuthNtlmTimestamp, sizeof(SecPkgContext_AuthNtlmTimestamp));
		AuthNtlmTimestamp.ChallengeOrResponse = FALSE;
		client->table->SetContextAttributes(&server->context, SECPKG_ATTR_AUTH_NTLM_TIMESTAMP,
		                                    &AuthNtlmTimestamp, sizeof(SecPkgContext_AuthNtlmTimestamp));
		CopyMemory(AuthNtlmClientChallenge.ClientChallenge, TEST_NTLM_CLIENT_CHALLENGE, 8);
		CopyMemory(AuthNtlmServerChallenge.ServerChallenge, TEST_NTLM_SERVER_CHALLENGE, 8);
		server->table->SetContextAttributes(&server->context, SECPKG_ATTR_AUTH_NTLM_CLIENT_CHALLENGE,
		                                    &AuthNtlmClientChallenge, sizeof(SecPkgContext_AuthNtlmClientChallenge));
		server->table->SetContextAttributes(&server->context, SECPKG_ATTR_AUTH_NTLM_SERVER_CHALLENGE,
		                                    &AuthNtlmServerChallenge, sizeof(SecPkgContext_AuthNtlmServerChallenge));
	}

	pSecBuffer = &(server->outputBuffer[0]);

	if (!DynamicTest)
	{
		SecPkgContext_AuthNtlmMessage AuthNtlmMessage;
		pSecBuffer->cbBuffer = sizeof(TEST_NTLM_CHALLENGE) - 1;
		pSecBuffer->pvBuffer = (void*) malloc(pSecBuffer->cbBuffer);

		if (!pSecBuffer->pvBuffer)
		{
			printf("Memory allocation failed\n");
			return -1;
		}

		CopyMemory(pSecBuffer->pvBuffer, TEST_NTLM_CHALLENGE, pSecBuffer->cbBuffer);
		AuthNtlmMessage.type = 2;
		AuthNtlmMessage.length = pSecBuffer->cbBuffer;
		AuthNtlmMessage.buffer = (BYTE*) pSecBuffer->pvBuffer;
		server->table->SetContextAttributes(&server->context, SECPKG_ATTR_AUTH_NTLM_MESSAGE,
		                                    &AuthNtlmMessage, sizeof(SecPkgContext_AuthNtlmMessage));
	}

	fprintf(stderr, "NTLM_CHALLENGE (length = %"PRIu32"):\n", pSecBuffer->cbBuffer);
	winpr_HexDump("sspi.test", WLOG_DEBUG, (BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);
	/**
	 * Client <- Challenge Message
	 * Client -> Authenticate Message
	 */
	client->haveInputBuffer = TRUE;
	client->inputBuffer[0].BufferType = SECBUFFER_TOKEN;
	client->inputBuffer[0].pvBuffer = pSecBuffer->pvBuffer;
	client->inputBuffer[0].cbBuffer = pSecBuffer->cbBuffer;
	status = test_ntlm_client_authenticate(client);

	if (status < 0)
	{
		printf("test_ntlm_client_authenticate failure\n");
		return -1;
	}

	pSecBuffer = &(client->outputBuffer[0]);

	if (!DynamicTest)
	{
		pSecBuffer->cbBuffer = sizeof(TEST_NTLM_AUTHENTICATE) - 1;
		pSecBuffer->pvBuffer = (void*) malloc(pSecBuffer->cbBuffer);

		if (!pSecBuffer->pvBuffer)
		{
			printf("Memory allocation failed\n");
			return -1;
		}

		CopyMemory(pSecBuffer->pvBuffer, TEST_NTLM_AUTHENTICATE, pSecBuffer->cbBuffer);
	}

	fprintf(stderr, "NTLM_AUTHENTICATE (length = %"PRIu32"):\n", pSecBuffer->cbBuffer);
	winpr_HexDump("sspi.test", WLOG_DEBUG, (BYTE*) pSecBuffer->pvBuffer, pSecBuffer->cbBuffer);
	/**
	 * Server <- Authenticate Message
	 */
	server->haveInputBuffer = TRUE;
	server->inputBuffer[0].BufferType = SECBUFFER_TOKEN;
	server->inputBuffer[0].pvBuffer = pSecBuffer->pvBuffer;
	server->inputBuffer[0].cbBuffer = pSecBuffer->cbBuffer;
	status = test_ntlm_server_authenticate(server);

	if (status < 0)
	{
		printf("test_ntlm_server_authenticate failure\n");
		return -1;
	}

	/**
	 * Cleanup & Termination
	 */
	test_ntlm_client_free(client);
	test_ntlm_server_free(server);
	return 0;
}
