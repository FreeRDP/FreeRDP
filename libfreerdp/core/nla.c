/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Level Authentication (NLA)
 *
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 Martin Fleisz <martin.fleisz@thincast.com>
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <time.h>

#ifndef _WIN32
#include <unistd.h>
#endif

#include <freerdp/log.h>
#include <freerdp/crypto/tls.h>
#include <freerdp/build-config.h>
#include <freerdp/peer.h>

#include <winpr/crt.h>
#include <winpr/sam.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/tchar.h>
#include <winpr/dsparse.h>
#include <winpr/library.h>
#include <winpr/registry.h>

#include "nla.h"

#define TAG FREERDP_TAG("core.nla")

#define SERVER_KEY "Software\\" FREERDP_VENDOR_STRING "\\" FREERDP_PRODUCT_STRING "\\Server"

/**
 * TSRequest ::= SEQUENCE {
 * 	version    [0] INTEGER,
 * 	negoTokens [1] NegoData OPTIONAL,
 * 	authInfo   [2] OCTET STRING OPTIONAL,
 * 	pubKeyAuth [3] OCTET STRING OPTIONAL,
 * 	errorCode  [4] INTEGER OPTIONAL
 * }
 *
 * NegoData ::= SEQUENCE OF NegoDataItem
 *
 * NegoDataItem ::= SEQUENCE {
 * 	negoToken [0] OCTET STRING
 * }
 *
 * TSCredentials ::= SEQUENCE {
 * 	credType    [0] INTEGER,
 * 	credentials [1] OCTET STRING
 * }
 *
 * TSPasswordCreds ::= SEQUENCE {
 * 	domainName  [0] OCTET STRING,
 * 	userName    [1] OCTET STRING,
 * 	password    [2] OCTET STRING
 * }
 *
 * TSSmartCardCreds ::= SEQUENCE {
 * 	pin        [0] OCTET STRING,
 * 	cspData    [1] TSCspDataDetail,
 * 	userHint   [2] OCTET STRING OPTIONAL,
 * 	domainHint [3] OCTET STRING OPTIONAL
 * }
 *
 * TSCspDataDetail ::= SEQUENCE {
 * 	keySpec       [0] INTEGER,
 * 	cardName      [1] OCTET STRING OPTIONAL,
 * 	readerName    [2] OCTET STRING OPTIONAL,
 * 	containerName [3] OCTET STRING OPTIONAL,
 * 	cspName       [4] OCTET STRING OPTIONAL
 * }
 *
 */

#define NLA_PKG_NAME NEGO_SSP_NAME

#define TERMSRV_SPN_PREFIX "TERMSRV/"

struct rdp_nla
{
	BOOL server;
	NLA_STATE state;
	int sendSeqNum;
	int recvSeqNum;
	freerdp* instance;
	CtxtHandle context;
	LPTSTR SspiModule;
	char* SamFile;
	rdpSettings* settings;
	rdpTransport* transport;
	UINT32 cbMaxToken;
#if defined(UNICODE)
	SEC_WCHAR* packageName;
#else
	SEC_CHAR* packageName;
#endif
	UINT32 version;
	UINT32 peerVersion;
	UINT32 errorCode;
	ULONG fContextReq;
	ULONG pfContextAttr;
	BOOL haveContext;
	BOOL haveInputBuffer;
	BOOL havePubKeyAuth;
	SECURITY_STATUS status;
	CredHandle credentials;
	TimeStamp expiration;
	PSecPkgInfo pPackageInfo;
	SecBuffer inputBuffer;
	SecBuffer outputBuffer;
	SecBufferDesc inputBufferDesc;
	SecBufferDesc outputBufferDesc;
	SecBuffer negoToken;
	SecBuffer pubKeyAuth;
	SecBuffer authInfo;
	SecBuffer ClientNonce;
	SecBuffer PublicKey;
	SecBuffer tsCredentials;
	LPTSTR ServicePrincipalName;
	SEC_WINNT_AUTH_IDENTITY* identity;
	PSecurityFunctionTable table;
	SecPkgContext_Sizes ContextSizes;
};

static BOOL nla_send(rdpNla* nla, const char* msg);
static int nla_recv(rdpNla* nla, const char* msg);
static void nla_buffer_print(rdpNla* nla);
static void nla_buffer_free(rdpNla* nla);
static SECURITY_STATUS nla_encrypt_public_key_echo(rdpNla* nla);
static SECURITY_STATUS nla_encrypt_public_key_hash(rdpNla* nla);
static SECURITY_STATUS nla_decrypt_public_key_echo(rdpNla* nla);
static SECURITY_STATUS nla_decrypt_public_key_hash(rdpNla* nla);
static SECURITY_STATUS nla_encrypt_ts_credentials(rdpNla* nla);
static SECURITY_STATUS nla_decrypt_ts_credentials(rdpNla* nla);
static BOOL nla_read_ts_password_creds(rdpNla* nla, wStream* s);
static void nla_identity_free(SEC_WINNT_AUTH_IDENTITY* identity);

#define ber_sizeof_sequence_octet_string(length) \
	ber_sizeof_contextual_tag(ber_sizeof_octet_string(length)) + ber_sizeof_octet_string(length)

static INLINE size_t ber_write_sequence_octet_string(wStream* stream, BYTE context,
                                                     const BYTE* value, size_t length)
{
	size_t rv = ber_write_contextual_tag(stream, context, ber_sizeof_octet_string(length), TRUE);
	rv += ber_write_octet_string(stream, value, length);
	return rv;
}

/* CredSSP Client-To-Server Binding Hash\0 */
static const BYTE ClientServerHashMagic[] = { 0x43, 0x72, 0x65, 0x64, 0x53, 0x53, 0x50, 0x20,
	                                          0x43, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x2D, 0x54,
	                                          0x6F, 0x2D, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,
	                                          0x20, 0x42, 0x69, 0x6E, 0x64, 0x69, 0x6E, 0x67,
	                                          0x20, 0x48, 0x61, 0x73, 0x68, 0x00 };

/* CredSSP Server-To-Client Binding Hash\0 */
static const BYTE ServerClientHashMagic[] = { 0x43, 0x72, 0x65, 0x64, 0x53, 0x53, 0x50, 0x20,
	                                          0x53, 0x65, 0x72, 0x76, 0x65, 0x72, 0x2D, 0x54,
	                                          0x6F, 0x2D, 0x43, 0x6C, 0x69, 0x65, 0x6E, 0x74,
	                                          0x20, 0x42, 0x69, 0x6E, 0x64, 0x69, 0x6E, 0x67,
	                                          0x20, 0x48, 0x61, 0x73, 0x68, 0x00 };

static const UINT32 NonceLength = 32;

void nla_identity_free(SEC_WINNT_AUTH_IDENTITY* identity)
{
	if (identity)
	{
		/* Password authentication */
		if (identity->User)
		{
			memset(identity->User, 0, identity->UserLength * 2);
			free(identity->User);
		}

		if (identity->Password)
		{
			size_t len = identity->PasswordLength;

			if (len > LB_PASSWORD_MAX_LENGTH) /* [pth] Password hash */
				len -= LB_PASSWORD_MAX_LENGTH;

			memset(identity->Password, 0, len * 2);
			free(identity->Password);
		}

		if (identity->Domain)
		{
			memset(identity->Domain, 0, identity->DomainLength * 2);
			free(identity->Domain);
		}
	}

	free(identity);
}

/**
 * Initialize NTLM/Kerberos SSP authentication module (client).
 * @param credssp
 */

static BOOL is_empty(const char* str)
{
	if (!str)
		return TRUE;
	if (strlen(str) == 0)
		return TRUE;
	return FALSE;
}

static int nla_client_init(rdpNla* nla)
{
	char* spn;
	size_t length;
	rdpTls* tls = NULL;
	BOOL PromptPassword = FALSE;
	freerdp* instance = nla->instance;
	rdpSettings* settings = nla->settings;
	WINPR_SAM* sam;
	WINPR_SAM_ENTRY* entry;
	nla->state = NLA_STATE_INITIAL;

	if (settings->RestrictedAdminModeRequired)
		settings->DisableCredentialsDelegation = TRUE;

	if (is_empty(settings->Username) ||
	    (is_empty(settings->Password) && is_empty((const char*)settings->RedirectionPassword)))
	{
		PromptPassword = TRUE;
	}

	if (PromptPassword && !is_empty(settings->Username))
	{
		sam = SamOpen(NULL, TRUE);

		if (sam)
		{
			entry = SamLookupUserA(sam, settings->Username, strlen(settings->Username), NULL, 0);

			if (entry)
			{
				/**
				 * The user could be found in SAM database.
				 * Use entry in SAM database later instead of prompt
				 */
				PromptPassword = FALSE;
				SamFreeEntry(sam, entry);
			}

			SamClose(sam);
		}
	}

#ifndef _WIN32

	if (PromptPassword)
	{
		if (settings->RestrictedAdminModeRequired)
		{
			if (!is_empty(settings->PasswordHash))
				PromptPassword = FALSE;
		}
	}

#endif

	if (PromptPassword)
	{
		if (freerdp_shall_disconnect(instance))
			return 0;

		if (!instance->Authenticate)
		{
			freerdp_set_last_error_log(instance->context,
			                           FREERDP_ERROR_CONNECT_NO_OR_MISSING_CREDENTIALS);
			return 0;
		}
		else
		{
			BOOL proceed = instance->Authenticate(instance, &settings->Username,
			                                      &settings->Password, &settings->Domain);

			if (!proceed)
			{
				freerdp_set_last_error_log(instance->context,
				                           FREERDP_ERROR_CONNECT_NO_OR_MISSING_CREDENTIALS);
				return 0;
			}
		}
	}

	if (!settings->Username)
	{
		nla_identity_free(nla->identity);
		nla->identity = NULL;
	}
	else
	{
		if (settings->RedirectionPassword && settings->RedirectionPasswordLength > 0)
		{
			if (sspi_SetAuthIdentityWithUnicodePassword(
			        nla->identity, settings->Username, settings->Domain,
			        (UINT16*)settings->RedirectionPassword,
			        settings->RedirectionPasswordLength / sizeof(WCHAR) - 1) < 0)
				return -1;
		}
		else
		{
			BOOL usePassword = TRUE;

			if (settings->RestrictedAdminModeRequired)
			{
				if (settings->PasswordHash)
				{
					if (strlen(settings->PasswordHash) == 32)
					{
						if (sspi_SetAuthIdentity(nla->identity, settings->Username,
						                         settings->Domain, settings->PasswordHash) < 0)
							return -1;

						/**
						 * Increase password hash length by LB_PASSWORD_MAX_LENGTH to obtain a
						 * length exceeding the maximum (LB_PASSWORD_MAX_LENGTH) and use it this for
						 * hash identification in WinPR.
						 */
						nla->identity->PasswordLength += LB_PASSWORD_MAX_LENGTH;
						usePassword = FALSE;
					}
				}
			}

			if (usePassword)
			{
				if (sspi_SetAuthIdentity(nla->identity, settings->Username, settings->Domain,
				                         settings->Password) < 0)
					return -1;
			}
		}
	}

	tls = nla->transport->tls;

	if (!tls)
	{
		WLog_ERR(TAG, "Unknown NLA transport layer");
		return -1;
	}

	if (!sspi_SecBufferAlloc(&nla->PublicKey, tls->PublicKeyLength))
	{
		WLog_ERR(TAG, "Failed to allocate sspi secBuffer");
		return -1;
	}

	CopyMemory(nla->PublicKey.pvBuffer, tls->PublicKey, tls->PublicKeyLength);
	length = sizeof(TERMSRV_SPN_PREFIX) + strlen(settings->ServerHostname);
	spn = (SEC_CHAR*)malloc(length + 1);

	if (!spn)
		return -1;

	sprintf_s(spn, length + 1, "%s%s", TERMSRV_SPN_PREFIX, settings->ServerHostname);
#ifdef UNICODE
	nla->ServicePrincipalName = NULL;
	ConvertToUnicode(CP_UTF8, 0, spn, -1, &nla->ServicePrincipalName, 0);
	free(spn);
#else
	nla->ServicePrincipalName = spn;
#endif
	nla->table = InitSecurityInterfaceEx(0);
#ifdef WITH_GSSAPI /* KERBEROS SSP */
	nla->status = nla->table->QuerySecurityPackageInfo(KERBEROS_SSP_NAME, &nla->pPackageInfo);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QuerySecurityPackageInfo status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

#else /* NTLM SSP */
	nla->status = nla->table->QuerySecurityPackageInfo(NLA_PKG_NAME, &nla->pPackageInfo);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QuerySecurityPackageInfo status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

#endif
	nla->cbMaxToken = nla->pPackageInfo->cbMaxToken;
	nla->packageName = nla->pPackageInfo->Name;
	WLog_DBG(TAG, "%s %" PRIu32 " : packageName=%ls ; cbMaxToken=%d", __FUNCTION__, __LINE__,
	         nla->packageName, nla->cbMaxToken);
	nla->status = nla->table->AcquireCredentialsHandle(NULL, NLA_PKG_NAME, SECPKG_CRED_OUTBOUND,
	                                                   NULL, nla->identity, NULL, NULL,
	                                                   &nla->credentials, &nla->expiration);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "AcquireCredentialsHandle status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

	nla->haveContext = FALSE;
	nla->haveInputBuffer = FALSE;
	nla->havePubKeyAuth = FALSE;
	ZeroMemory(&nla->inputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->outputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->ContextSizes, sizeof(SecPkgContext_Sizes));
	/*
	 * from tspkg.dll: 0x00000132
	 * ISC_REQ_MUTUAL_AUTH
	 * ISC_REQ_CONFIDENTIALITY
	 * ISC_REQ_USE_SESSION_KEY
	 * ISC_REQ_ALLOCATE_MEMORY
	 */
	nla->fContextReq = ISC_REQ_MUTUAL_AUTH | ISC_REQ_CONFIDENTIALITY | ISC_REQ_USE_SESSION_KEY;
	return 1;
}

int nla_client_begin(rdpNla* nla)
{
	if (nla_client_init(nla) < 1)
		return -1;

	if (nla->state != NLA_STATE_INITIAL)
		return -1;

	nla->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
	nla->outputBufferDesc.cBuffers = 1;
	nla->outputBufferDesc.pBuffers = &nla->outputBuffer;
	nla->outputBuffer.BufferType = SECBUFFER_TOKEN;
	nla->outputBuffer.cbBuffer = nla->cbMaxToken;
	nla->outputBuffer.pvBuffer = malloc(nla->outputBuffer.cbBuffer);

	if (!nla->outputBuffer.pvBuffer)
		return -1;

	nla->status = nla->table->InitializeSecurityContext(
	    &nla->credentials, NULL, nla->ServicePrincipalName, nla->fContextReq, 0,
	    SECURITY_NATIVE_DREP, NULL, 0, &nla->context, &nla->outputBufferDesc, &nla->pfContextAttr,
	    &nla->expiration);
	WLog_VRB(TAG, " InitializeSecurityContext status %s [0x%08" PRIX32 "]",
	         GetSecurityStatusString(nla->status), nla->status);

	/* Handle kerberos context initialization failure.
	 * After kerberos failed initialize NTLM context */
	if (nla->status == SEC_E_NO_CREDENTIALS)
	{
		nla->status = nla->table->InitializeSecurityContext(
		    &nla->credentials, NULL, nla->ServicePrincipalName, nla->fContextReq, 0,
		    SECURITY_NATIVE_DREP, NULL, 0, &nla->context, &nla->outputBufferDesc,
		    &nla->pfContextAttr, &nla->expiration);
		WLog_VRB(TAG, " InitializeSecurityContext status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);

		if (nla->status)
		{
			SECURITY_STATUS status =
			    nla->table->QuerySecurityPackageInfo(NTLM_SSP_NAME, &nla->pPackageInfo);

			if (status != SEC_E_OK)
			{
				WLog_ERR(TAG, "QuerySecurityPackageInfo status %s [0x%08" PRIX32 "]",
				         GetSecurityStatusString(nla->status), status);
				return -1;
			}

			nla->cbMaxToken = nla->pPackageInfo->cbMaxToken;
			nla->packageName = nla->pPackageInfo->Name;
		}
	}

	if ((nla->status == SEC_I_COMPLETE_AND_CONTINUE) || (nla->status == SEC_I_COMPLETE_NEEDED))
	{
		if (nla->table->CompleteAuthToken)
		{
			SECURITY_STATUS status;
			status = nla->table->CompleteAuthToken(&nla->context, &nla->outputBufferDesc);

			if (status != SEC_E_OK)
			{
				WLog_WARN(TAG, "CompleteAuthToken status %s [0x%08" PRIX32 "]",
				          GetSecurityStatusString(status), status);
				return -1;
			}
		}

		if (nla->status == SEC_I_COMPLETE_NEEDED)
			nla->status = SEC_E_OK;
		else if (nla->status == SEC_I_COMPLETE_AND_CONTINUE)
			nla->status = SEC_I_CONTINUE_NEEDED;
	}

	if (nla->status != SEC_I_CONTINUE_NEEDED)
		return -1;

	if (nla->outputBuffer.cbBuffer < 1)
		return -1;

	nla->negoToken.pvBuffer = nla->outputBuffer.pvBuffer;
	nla->negoToken.cbBuffer = nla->outputBuffer.cbBuffer;

	if (!nla_send(nla, "Client: Sending Authentication Token"))
	{
		nla_buffer_free(nla);
		return -1;
	}

	nla_buffer_free(nla);
	nla->state = NLA_STATE_NEGO_TOKEN;
	return 1;
}

static int nla_client_recv(rdpNla* nla)
{
	int status = -1;

	if (nla->state == NLA_STATE_NEGO_TOKEN)
	{
		nla->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->inputBufferDesc.cBuffers = 1;
		nla->inputBufferDesc.pBuffers = &nla->inputBuffer;
		nla->inputBuffer.BufferType = SECBUFFER_TOKEN;
		nla->inputBuffer.pvBuffer = nla->negoToken.pvBuffer;
		nla->inputBuffer.cbBuffer = nla->negoToken.cbBuffer;
		nla->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->outputBufferDesc.cBuffers = 1;
		nla->outputBufferDesc.pBuffers = &nla->outputBuffer;
		nla->outputBuffer.BufferType = SECBUFFER_TOKEN;
		nla->outputBuffer.cbBuffer = nla->cbMaxToken;
		nla->outputBuffer.pvBuffer = malloc(nla->outputBuffer.cbBuffer);

		if (!nla->outputBuffer.pvBuffer)
			return -1;

		nla->status = nla->table->InitializeSecurityContext(
		    &nla->credentials, &nla->context, nla->ServicePrincipalName, nla->fContextReq, 0,
		    SECURITY_NATIVE_DREP, &nla->inputBufferDesc, 0, &nla->context, &nla->outputBufferDesc,
		    &nla->pfContextAttr, &nla->expiration);
		WLog_VRB(TAG, "InitializeSecurityContext  %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		free(nla->inputBuffer.pvBuffer);
		nla->inputBuffer.pvBuffer = NULL;

		if ((nla->status == SEC_I_COMPLETE_AND_CONTINUE) || (nla->status == SEC_I_COMPLETE_NEEDED))
		{
			if (nla->table->CompleteAuthToken)
			{
				SECURITY_STATUS status;
				status = nla->table->CompleteAuthToken(&nla->context, &nla->outputBufferDesc);

				if (status != SEC_E_OK)
				{
					WLog_WARN(TAG, "CompleteAuthToken status %s [0x%08" PRIX32 "]",
					          GetSecurityStatusString(status), status);
					return -1;
				}
			}

			if (nla->status == SEC_I_COMPLETE_NEEDED)
				nla->status = SEC_E_OK;
			else if (nla->status == SEC_I_COMPLETE_AND_CONTINUE)
				nla->status = SEC_I_CONTINUE_NEEDED;
		}

		if (nla->status == SEC_E_OK)
		{
			nla->havePubKeyAuth = TRUE;
			nla->status = nla->table->QueryContextAttributes(&nla->context, SECPKG_ATTR_SIZES,
			                                                 &nla->ContextSizes);

			if (nla->status != SEC_E_OK)
			{
				WLog_ERR(TAG,
				         "QueryContextAttributes SECPKG_ATTR_SIZES failure %s [0x%08" PRIX32 "]",
				         GetSecurityStatusString(nla->status), nla->status);
				return -1;
			}

			if (nla->peerVersion < 5)
				nla->status = nla_encrypt_public_key_echo(nla);
			else
				nla->status = nla_encrypt_public_key_hash(nla);

			if (nla->status != SEC_E_OK)
				return -1;
		}

		nla->negoToken.pvBuffer = nla->outputBuffer.pvBuffer;
		nla->negoToken.cbBuffer = nla->outputBuffer.cbBuffer;

		if (!nla_send(nla, "Client: Sending Authentication Token"))
		{
			nla_buffer_free(nla);
			return -1;
		}

		nla_buffer_free(nla);

		if (nla->status == SEC_E_OK)
			nla->state = NLA_STATE_PUB_KEY_AUTH;

		status = 1;
	}
	else if (nla->state == NLA_STATE_PUB_KEY_AUTH)
	{
		/* Verify Server Public Key Echo */
		if (nla->peerVersion < 5)
			nla->status = nla_decrypt_public_key_echo(nla);
		else
			nla->status = nla_decrypt_public_key_hash(nla);

		nla_buffer_free(nla);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "Could not verify public key echo %s [0x%08" PRIX32 "]",
			         GetSecurityStatusString(nla->status), nla->status);
			return -1;
		}

		/* Send encrypted credentials */
		nla->status = nla_encrypt_ts_credentials(nla);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "nla_encrypt_ts_credentials status %s [0x%08" PRIX32 "]",
			         GetSecurityStatusString(nla->status), nla->status);
			return -1;
		}

		if (!nla_send(nla, "Client: Sending PubKeyAuth Token"))
		{
			nla_buffer_free(nla);
			return -1;
		}

		nla_buffer_free(nla);

		if (SecIsValidHandle(&nla->credentials))
		{
			nla->table->FreeCredentialsHandle(&nla->credentials);
			SecInvalidateHandle(&nla->credentials);
		}

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "FreeCredentialsHandle status %s [0x%08" PRIX32 "]",
			         GetSecurityStatusString(nla->status), nla->status);
		}

		nla->status = nla->table->FreeContextBuffer(nla->pPackageInfo);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "FreeContextBuffer status %s [0x%08" PRIX32 "]",
			         GetSecurityStatusString(nla->status), nla->status);
		}

		if (nla->status != SEC_E_OK)
			return -1;

		nla->state = NLA_STATE_AUTH_INFO;
		status = 1;
	}

	return status;
}

static int nla_client_authenticate(rdpNla* nla)
{
	wStream* s;
	int status;
	s = Stream_New(NULL, 4096);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return -1;
	}

	if (nla_client_begin(nla) < 1)
	{
		Stream_Free(s, TRUE);
		return -1;
	}

	while (nla->state < NLA_STATE_AUTH_INFO)
	{
		Stream_SetPosition(s, 0);
		status = transport_read_pdu(nla->transport, s);

		if (status < 0)
		{
			WLog_ERR(TAG, "nla_client_authenticate failure");
			Stream_Free(s, TRUE);
			return -1;
		}

		status = nla_recv_pdu(nla, s);

		if (status < 0)
		{
			Stream_Free(s, TRUE);
			return -1;
		}
	}

	Stream_Free(s, TRUE);
	return 1;
}

/**
 * Initialize NTLMSSP authentication module (server).
 * @param credssp
 */

static int nla_server_init(rdpNla* nla)
{
	rdpTls* tls = nla->transport->tls;

	if (!sspi_SecBufferAlloc(&nla->PublicKey, tls->PublicKeyLength))
	{
		WLog_ERR(TAG, "Failed to allocate SecBuffer for public key");
		return -1;
	}

	CopyMemory(nla->PublicKey.pvBuffer, tls->PublicKey, tls->PublicKeyLength);

	if (nla->SspiModule)
	{
		HMODULE hSSPI;
		INIT_SECURITY_INTERFACE pInitSecurityInterface;
		hSSPI = LoadLibrary(nla->SspiModule);

		if (!hSSPI)
		{
			WLog_ERR(TAG, "Failed to load SSPI module: %s", nla->SspiModule);
			return -1;
		}

#ifdef UNICODE
		pInitSecurityInterface =
		    (INIT_SECURITY_INTERFACE)GetProcAddress(hSSPI, "InitSecurityInterfaceW");
#else
		pInitSecurityInterface =
		    (INIT_SECURITY_INTERFACE)GetProcAddress(hSSPI, "InitSecurityInterfaceA");
#endif
		nla->table = pInitSecurityInterface();
	}
	else
	{
		nla->table = InitSecurityInterfaceEx(0);
	}

	nla->status = nla->table->QuerySecurityPackageInfo(NLA_PKG_NAME, &nla->pPackageInfo);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QuerySecurityPackageInfo status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

	nla->cbMaxToken = nla->pPackageInfo->cbMaxToken;
	nla->packageName = nla->pPackageInfo->Name;
	nla->status =
	    nla->table->AcquireCredentialsHandle(NULL, NLA_PKG_NAME, SECPKG_CRED_INBOUND, NULL, NULL,
	                                         NULL, NULL, &nla->credentials, &nla->expiration);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "AcquireCredentialsHandle status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

	nla->haveContext = FALSE;
	nla->haveInputBuffer = FALSE;
	nla->havePubKeyAuth = FALSE;
	ZeroMemory(&nla->inputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->outputBuffer, sizeof(SecBuffer));
	ZeroMemory(&nla->inputBufferDesc, sizeof(SecBufferDesc));
	ZeroMemory(&nla->outputBufferDesc, sizeof(SecBufferDesc));
	ZeroMemory(&nla->ContextSizes, sizeof(SecPkgContext_Sizes));
	/*
	 * from tspkg.dll: 0x00000112
	 * ASC_REQ_MUTUAL_AUTH
	 * ASC_REQ_CONFIDENTIALITY
	 * ASC_REQ_ALLOCATE_MEMORY
	 */
	nla->fContextReq = 0;
	nla->fContextReq |= ASC_REQ_MUTUAL_AUTH;
	nla->fContextReq |= ASC_REQ_CONFIDENTIALITY;
	nla->fContextReq |= ASC_REQ_CONNECTION;
	nla->fContextReq |= ASC_REQ_USE_SESSION_KEY;
	nla->fContextReq |= ASC_REQ_REPLAY_DETECT;
	nla->fContextReq |= ASC_REQ_SEQUENCE_DETECT;
	nla->fContextReq |= ASC_REQ_EXTENDED_ERROR;
	return 1;
}

/**
 * Authenticate with client using CredSSP (server).
 * @param credssp
 * @return 1 if authentication is successful
 */

static int nla_server_authenticate(rdpNla* nla)
{
	if (nla_server_init(nla) < 1)
		return -1;

	while (TRUE)
	{
		/* receive authentication token */
		nla->inputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->inputBufferDesc.cBuffers = 1;
		nla->inputBufferDesc.pBuffers = &nla->inputBuffer;
		nla->inputBuffer.BufferType = SECBUFFER_TOKEN;

		if (nla_recv(nla, "Receiving Authentication Token") < 0)
			return -1;

		nla->inputBuffer.pvBuffer = nla->negoToken.pvBuffer;
		nla->inputBuffer.cbBuffer = nla->negoToken.cbBuffer;

		if (nla->negoToken.cbBuffer < 1)
		{
			WLog_ERR(TAG, "CredSSP: invalid negoToken!");
			return -1;
		}

		nla->outputBufferDesc.ulVersion = SECBUFFER_VERSION;
		nla->outputBufferDesc.cBuffers = 1;
		nla->outputBufferDesc.pBuffers = &nla->outputBuffer;
		nla->outputBuffer.BufferType = SECBUFFER_TOKEN;
		nla->outputBuffer.cbBuffer = nla->cbMaxToken;
		nla->outputBuffer.pvBuffer = malloc(nla->outputBuffer.cbBuffer);

		if (!nla->outputBuffer.pvBuffer)
			return -1;

		nla->status = nla->table->AcceptSecurityContext(
		    &nla->credentials, nla->haveContext ? &nla->context : NULL, &nla->inputBufferDesc,
		    nla->fContextReq, SECURITY_NATIVE_DREP, &nla->context, &nla->outputBufferDesc,
		    &nla->pfContextAttr, &nla->expiration);
		WLog_VRB(TAG, "AcceptSecurityContext status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		nla->negoToken.pvBuffer = nla->outputBuffer.pvBuffer;
		nla->negoToken.cbBuffer = nla->outputBuffer.cbBuffer;

		if ((nla->status == SEC_I_COMPLETE_AND_CONTINUE) || (nla->status == SEC_I_COMPLETE_NEEDED))
		{
			freerdp_peer* peer = nla->instance->context->peer;

			if (peer->ComputeNtlmHash)
			{
				SECURITY_STATUS status;
				status = nla->table->SetContextAttributes(
				    &nla->context, SECPKG_ATTR_AUTH_NTLM_HASH_CB, peer->ComputeNtlmHash, 0);

				if (status != SEC_E_OK)
				{
					WLog_ERR(TAG, "SetContextAttributesA(hash cb) status %s [0x%08" PRIX32 "]",
					         GetSecurityStatusString(status), status);
				}

				status = nla->table->SetContextAttributes(
				    &nla->context, SECPKG_ATTR_AUTH_NTLM_HASH_CB_DATA, peer, 0);

				if (status != SEC_E_OK)
				{
					WLog_ERR(TAG, "SetContextAttributesA(hash cb data) status %s [0x%08" PRIX32 "]",
					         GetSecurityStatusString(status), status);
				}
			}
			else if (nla->SamFile)
			{
				nla->table->SetContextAttributes(&nla->context, SECPKG_ATTR_AUTH_NTLM_SAM_FILE,
				                                 nla->SamFile, strlen(nla->SamFile) + 1);
			}

			if (nla->table->CompleteAuthToken)
			{
				SECURITY_STATUS status;
				status = nla->table->CompleteAuthToken(&nla->context, &nla->outputBufferDesc);

				if (status != SEC_E_OK)
				{
					WLog_WARN(TAG, "CompleteAuthToken status %s [0x%08" PRIX32 "]",
					          GetSecurityStatusString(status), status);
					return -1;
				}
			}

			if (nla->status == SEC_I_COMPLETE_NEEDED)
				nla->status = SEC_E_OK;
			else if (nla->status == SEC_I_COMPLETE_AND_CONTINUE)
				nla->status = SEC_I_CONTINUE_NEEDED;
		}

		if (nla->status == SEC_E_OK)
		{
			if (nla->outputBuffer.cbBuffer != 0)
			{
				if (!nla_send(nla, "Server: Sending response"))
				{
					nla_buffer_free(nla);
					return -1;
				}

				if (nla_recv(nla, "Receiving pubkey Token") < 0)
					return -1;
			}

			nla->havePubKeyAuth = TRUE;
			nla->status = nla->table->QueryContextAttributes(&nla->context, SECPKG_ATTR_SIZES,
			                                                 &nla->ContextSizes);

			if (nla->status != SEC_E_OK)
			{
				WLog_ERR(TAG,
				         "QueryContextAttributes SECPKG_ATTR_SIZES failure %s [0x%08" PRIX32 "]",
				         GetSecurityStatusString(nla->status), nla->status);
				return -1;
			}

			if (nla->peerVersion < 5)
				nla->status = nla_decrypt_public_key_echo(nla);
			else
				nla->status = nla_decrypt_public_key_hash(nla);

			if (nla->status != SEC_E_OK)
			{
				WLog_ERR(TAG,
				         "Error: could not verify client's public key echo %s [0x%08" PRIX32 "]",
				         GetSecurityStatusString(nla->status), nla->status);
				return -1;
			}

			sspi_SecBufferFree(&nla->negoToken);
			nla->negoToken.pvBuffer = NULL;
			nla->negoToken.cbBuffer = 0;

			if (nla->peerVersion < 5)
				nla->status = nla_encrypt_public_key_echo(nla);
			else
				nla->status = nla_encrypt_public_key_hash(nla);

			if (nla->status != SEC_E_OK)
				return -1;
		}

		if ((nla->status != SEC_E_OK) && (nla->status != SEC_I_CONTINUE_NEEDED))
		{
			/* Special handling of these specific error codes as NTSTATUS_FROM_WIN32
			   unfortunately does not map directly to the corresponding NTSTATUS values
			 */
			switch (GetLastError())
			{
				case ERROR_PASSWORD_MUST_CHANGE:
					nla->errorCode = STATUS_PASSWORD_MUST_CHANGE;
					break;

				case ERROR_PASSWORD_EXPIRED:
					nla->errorCode = STATUS_PASSWORD_EXPIRED;
					break;

				case ERROR_ACCOUNT_DISABLED:
					nla->errorCode = STATUS_ACCOUNT_DISABLED;
					break;

				default:
					nla->errorCode = NTSTATUS_FROM_WIN32(GetLastError());
					break;
			}

			WLog_ERR(TAG, "AcceptSecurityContext status %s [0x%08" PRIX32 "]",
			         GetSecurityStatusString(nla->status), nla->status);
			nla_send(nla, "Server: Sending AcceptSecurityContext error status");
			return -1; /* Access Denied */
		}

		/* send authentication token */
		if (!nla_send(nla, "Server: Sending Authentication Token"))
		{
			nla_buffer_free(nla);
			return -1;
		}

		nla_buffer_free(nla);

		if (nla->status != SEC_I_CONTINUE_NEEDED)
			break;

		nla->haveContext = TRUE;
	}

	/* Receive encrypted credentials */

	if (nla_recv(nla, "Receive Encryption Credentials") < 0)
		return -1;

	nla->status = nla_decrypt_ts_credentials(nla);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "Could not decrypt TSCredentials status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

	nla->status = nla->table->ImpersonateSecurityContext(&nla->context);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "ImpersonateSecurityContext status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}
	else
	{
		nla->status = nla->table->RevertSecurityContext(&nla->context);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "RevertSecurityContext status %s [0x%08" PRIX32 "]",
			         GetSecurityStatusString(nla->status), nla->status);
			return -1;
		}
	}

	nla->status = nla->table->FreeContextBuffer(nla->pPackageInfo);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "DeleteSecurityContext status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		return -1;
	}

	return 1;
}

/**
 * Authenticate using CredSSP.
 * @param credssp
 * @return 1 if authentication is successful
 */

int nla_authenticate(rdpNla* nla)
{
	if (nla->server)
		return nla_server_authenticate(nla);
	else
		return nla_client_authenticate(nla);
}

static void ap_integer_increment_le(BYTE* number, int size)
{
	int index;

	for (index = 0; index < size; index++)
	{
		if (number[index] < 0xFF)
		{
			number[index]++;
			break;
		}
		else
		{
			number[index] = 0;
			continue;
		}
	}
}

static void ap_integer_decrement_le(BYTE* number, int size)
{
	int index;

	for (index = 0; index < size; index++)
	{
		if (number[index] > 0)
		{
			number[index]--;
			break;
		}
		else
		{
			number[index] = 0xFF;
			continue;
		}
	}
}

SECURITY_STATUS nla_encrypt_public_key_echo(rdpNla* nla)
{
	SecBuffer Buffers[2] = { { 0 } };
	SecBufferDesc Message;
	SECURITY_STATUS status;
	ULONG public_key_length;
	const BOOL krb =
	    (_tcsncmp(nla->packageName, KERBEROS_SSP_NAME, ARRAYSIZE(KERBEROS_SSP_NAME)) == 0);
	const BOOL nego = (_tcsncmp(nla->packageName, NEGO_SSP_NAME, ARRAYSIZE(NEGO_SSP_NAME)) == 0);
	const BOOL ntlm = (_tcsncmp(nla->packageName, NTLM_SSP_NAME, ARRAYSIZE(NTLM_SSP_NAME)) == 0);
	public_key_length = nla->PublicKey.cbBuffer;

	sspi_SecBufferFree(&nla->pubKeyAuth);
	if (!sspi_SecBufferAlloc(&nla->pubKeyAuth,
	                         public_key_length + nla->ContextSizes.cbSecurityTrailer))
		return SEC_E_INSUFFICIENT_MEMORY;

	if (krb)
	{
		Message.cBuffers = 1;
		Buffers[0].BufferType = SECBUFFER_DATA; /* TLS Public Key */
		Buffers[0].cbBuffer = public_key_length;
		Buffers[0].pvBuffer = nla->pubKeyAuth.pvBuffer;
		CopyMemory(Buffers[0].pvBuffer, nla->PublicKey.pvBuffer, Buffers[0].cbBuffer);
	}
	else if (ntlm || nego)
	{
		Message.cBuffers = 2;
		Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
		Buffers[0].cbBuffer = nla->ContextSizes.cbSecurityTrailer;
		Buffers[0].pvBuffer = nla->pubKeyAuth.pvBuffer;
		Buffers[1].BufferType = SECBUFFER_DATA; /* TLS Public Key */
		Buffers[1].cbBuffer = public_key_length;
		Buffers[1].pvBuffer =
		    ((BYTE*)nla->pubKeyAuth.pvBuffer) + nla->ContextSizes.cbSecurityTrailer;
		CopyMemory(Buffers[1].pvBuffer, nla->PublicKey.pvBuffer, Buffers[1].cbBuffer);
	}

	if (!krb && nla->server)
	{
		/* server echos the public key +1 */
		ap_integer_increment_le((BYTE*)Buffers[1].pvBuffer, Buffers[1].cbBuffer);
	}

	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer)&Buffers;
	status = nla->table->EncryptMessage(&nla->context, 0, &Message, nla->sendSeqNum++);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage status %s [0x%08" PRIX32 "]", GetSecurityStatusString(status),
		         status);
		return status;
	}

	if (Message.cBuffers == 2 && Buffers[0].cbBuffer < nla->ContextSizes.cbSecurityTrailer)
	{
		/* IMPORTANT: EncryptMessage may not use all the signature space, so we need to shrink the
		 * excess between the buffers */
		MoveMemory(((BYTE*)Buffers[0].pvBuffer) + Buffers[0].cbBuffer, Buffers[1].pvBuffer,
		           Buffers[1].cbBuffer);
		nla->pubKeyAuth.cbBuffer = Buffers[0].cbBuffer + Buffers[1].cbBuffer;
	}

	return status;
}

SECURITY_STATUS nla_encrypt_public_key_hash(rdpNla* nla)
{
	SecBuffer Buffers[2] = { { 0 } };
	SecBufferDesc Message;
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	WINPR_DIGEST_CTX* sha256 = NULL;
	const BOOL krb =
	    (_tcsncmp(nla->packageName, KERBEROS_SSP_NAME, ARRAYSIZE(KERBEROS_SSP_NAME)) == 0);
	const ULONG auth_data_length =
	    (nla->ContextSizes.cbSecurityTrailer + WINPR_SHA256_DIGEST_LENGTH);
	const BYTE* hashMagic = nla->server ? ServerClientHashMagic : ClientServerHashMagic;
	const size_t hashSize =
	    nla->server ? sizeof(ServerClientHashMagic) : sizeof(ClientServerHashMagic);

	sspi_SecBufferFree(&nla->pubKeyAuth);
	if (!sspi_SecBufferAlloc(&nla->pubKeyAuth, auth_data_length))
	{
		status = SEC_E_INSUFFICIENT_MEMORY;
		goto out;
	}

	/* generate SHA256 of following data: ClientServerHashMagic, Nonce, SubjectPublicKey */
	if (!(sha256 = winpr_Digest_New()))
		goto out;

	if (!winpr_Digest_Init(sha256, WINPR_MD_SHA256))
		goto out;

	/* include trailing \0 from hashMagic */
	if (!winpr_Digest_Update(sha256, hashMagic, hashSize))
		goto out;

	if (!winpr_Digest_Update(sha256, nla->ClientNonce.pvBuffer, nla->ClientNonce.cbBuffer))
		goto out;

	/* SubjectPublicKey */
	if (!winpr_Digest_Update(sha256, nla->PublicKey.pvBuffer, nla->PublicKey.cbBuffer))
		goto out;

	if (krb)
	{
		Message.cBuffers = 1;
		Buffers[0].BufferType = SECBUFFER_DATA; /* SHA256 hash */
		Buffers[0].cbBuffer = WINPR_SHA256_DIGEST_LENGTH;
		Buffers[0].pvBuffer = nla->pubKeyAuth.pvBuffer;

		if (!winpr_Digest_Final(sha256, Buffers[0].pvBuffer, Buffers[0].cbBuffer))
			goto out;
	}
	else
	{
		Message.cBuffers = 2;
		Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
		Buffers[0].cbBuffer = nla->ContextSizes.cbSecurityTrailer;
		Buffers[0].pvBuffer = nla->pubKeyAuth.pvBuffer;
		Buffers[1].BufferType = SECBUFFER_DATA; /* SHA256 hash */
		Buffers[1].cbBuffer = WINPR_SHA256_DIGEST_LENGTH;
		Buffers[1].pvBuffer =
		    ((BYTE*)nla->pubKeyAuth.pvBuffer) + nla->ContextSizes.cbSecurityTrailer;

		if (!winpr_Digest_Final(sha256, Buffers[1].pvBuffer, Buffers[1].cbBuffer))
			goto out;
	}

	Message.pBuffers = (PSecBuffer)&Buffers;
	Message.ulVersion = SECBUFFER_VERSION;
	status = nla->table->EncryptMessage(&nla->context, 0, &Message, nla->sendSeqNum++);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage status %s [0x%08" PRIX32 "]", GetSecurityStatusString(status),
		         status);
		goto out;
	}

	if (Message.cBuffers == 2 && Buffers[0].cbBuffer < nla->ContextSizes.cbSecurityTrailer)
	{
		/* IMPORTANT: EncryptMessage may not use all the signature space, so we need to shrink the
		 * excess between the buffers */
		MoveMemory(((BYTE*)Buffers[0].pvBuffer) + Buffers[0].cbBuffer, Buffers[1].pvBuffer,
		           Buffers[1].cbBuffer);
		nla->pubKeyAuth.cbBuffer = Buffers[0].cbBuffer + Buffers[1].cbBuffer;
	}

out:
	winpr_Digest_Free(sha256);
	return status;
}

SECURITY_STATUS nla_decrypt_public_key_echo(rdpNla* nla)
{
	ULONG length;
	BYTE* buffer = NULL;
	ULONG pfQOP = 0;
	BYTE* public_key1 = NULL;
	BYTE* public_key2 = NULL;
	ULONG public_key_length = 0;
	int signature_length;
	SecBuffer Buffers[2] = { { 0 } };
	SecBufferDesc Message;
	BOOL krb, ntlm, nego;
	SECURITY_STATUS status = SEC_E_INVALID_TOKEN;

	if (!nla)
		goto fail;

	krb = (_tcsncmp(nla->packageName, KERBEROS_SSP_NAME, ARRAYSIZE(KERBEROS_SSP_NAME)) == 0);
	nego = (_tcsncmp(nla->packageName, NEGO_SSP_NAME, ARRAYSIZE(NEGO_SSP_NAME)) == 0);
	ntlm = (_tcsncmp(nla->packageName, NTLM_SSP_NAME, ARRAYSIZE(NTLM_SSP_NAME)) == 0);
	signature_length = nla->pubKeyAuth.cbBuffer - nla->PublicKey.cbBuffer;

	if ((signature_length < 0) || ((UINT32)signature_length > nla->ContextSizes.cbSecurityTrailer))
	{
		WLog_ERR(TAG, "unexpected pubKeyAuth buffer size: %" PRIu32 "", nla->pubKeyAuth.cbBuffer);
		goto fail;
	}

	length = nla->pubKeyAuth.cbBuffer;
	buffer = (BYTE*)malloc(length);

	if (!buffer)
	{
		status = SEC_E_INSUFFICIENT_MEMORY;
		goto fail;
	}

	if (krb)
	{
		CopyMemory(buffer, nla->pubKeyAuth.pvBuffer, length);
		Buffers[0].BufferType = SECBUFFER_DATA; /* Wrapped and encrypted TLS Public Key */
		Buffers[0].cbBuffer = length;
		Buffers[0].pvBuffer = buffer;
		Message.cBuffers = 1;
	}
	else if (ntlm || nego)
	{
		CopyMemory(buffer, nla->pubKeyAuth.pvBuffer, length);
		public_key_length = nla->PublicKey.cbBuffer;
		Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
		Buffers[0].cbBuffer = signature_length;
		Buffers[0].pvBuffer = buffer;
		Buffers[1].BufferType = SECBUFFER_DATA; /* Encrypted TLS Public Key */
		Buffers[1].cbBuffer = length - signature_length;
		Buffers[1].pvBuffer = buffer + signature_length;
		Message.cBuffers = 2;
	}

	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer)&Buffers;
	status = nla->table->DecryptMessage(&nla->context, &Message, nla->recvSeqNum++, &pfQOP);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "DecryptMessage failure %s [%08" PRIX32 "]", GetSecurityStatusString(status),
		         status);
		goto fail;
	}

	if (krb)
	{
		public_key1 = public_key2 = (BYTE*)nla->pubKeyAuth.pvBuffer;
		public_key_length = length;
	}
	else if (ntlm || nego)
	{
		public_key1 = (BYTE*)nla->PublicKey.pvBuffer;
		public_key2 = (BYTE*)Buffers[1].pvBuffer;
	}

	if (!nla->server)
	{
		/* server echos the public key +1 */
		ap_integer_decrement_le(public_key2, public_key_length);
	}

	if (!public_key1 || !public_key2 || memcmp(public_key1, public_key2, public_key_length) != 0)
	{
		WLog_ERR(TAG, "Could not verify server's public key echo");
#if defined(WITH_DEBUG_NLA)
		WLog_ERR(TAG, "Expected (length = %d):", public_key_length);
		winpr_HexDump(TAG, WLOG_ERROR, public_key1, public_key_length);
		WLog_ERR(TAG, "Actual (length = %d):", public_key_length);
		winpr_HexDump(TAG, WLOG_ERROR, public_key2, public_key_length);
#endif
		status = SEC_E_MESSAGE_ALTERED; /* DO NOT SEND CREDENTIALS! */
		goto fail;
	}

	status = SEC_E_OK;
fail:
	free(buffer);
	return status;
}

SECURITY_STATUS nla_decrypt_public_key_hash(rdpNla* nla)
{
	unsigned long length;
	BYTE* buffer = NULL;
	ULONG pfQOP = 0;
	int signature_length;
	SecBuffer Buffers[2] = { { 0 } };
	SecBufferDesc Message;
	WINPR_DIGEST_CTX* sha256 = NULL;
	BYTE serverClientHash[WINPR_SHA256_DIGEST_LENGTH];
	SECURITY_STATUS status = SEC_E_INVALID_TOKEN;
	const BOOL krb =
	    (_tcsncmp(nla->packageName, KERBEROS_SSP_NAME, ARRAYSIZE(KERBEROS_SSP_NAME)) == 0);
	const BYTE* hashMagic = nla->server ? ClientServerHashMagic : ServerClientHashMagic;
	const size_t hashSize =
	    nla->server ? sizeof(ClientServerHashMagic) : sizeof(ServerClientHashMagic);
	signature_length = nla->pubKeyAuth.cbBuffer - WINPR_SHA256_DIGEST_LENGTH;

	if ((signature_length < 0) || (signature_length > (int)nla->ContextSizes.cbSecurityTrailer))
	{
		WLog_ERR(TAG, "unexpected pubKeyAuth buffer size: %" PRIu32 "", nla->pubKeyAuth.cbBuffer);
		goto fail;
	}

	length = nla->pubKeyAuth.cbBuffer;
	buffer = (BYTE*)malloc(length);

	if (!buffer)
	{
		status = SEC_E_INSUFFICIENT_MEMORY;
		goto fail;
	}

	if (krb)
	{
		CopyMemory(buffer, nla->pubKeyAuth.pvBuffer, length);
		Buffers[0].BufferType = SECBUFFER_DATA; /* Encrypted Hash */
		Buffers[0].cbBuffer = length;
		Buffers[0].pvBuffer = buffer;
		Message.cBuffers = 1;
	}
	else
	{
		CopyMemory(buffer, nla->pubKeyAuth.pvBuffer, length);
		Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
		Buffers[0].cbBuffer = signature_length;
		Buffers[0].pvBuffer = buffer;
		Buffers[1].BufferType = SECBUFFER_DATA; /* Encrypted Hash */
		Buffers[1].cbBuffer = WINPR_SHA256_DIGEST_LENGTH;
		Buffers[1].pvBuffer = buffer + signature_length;
		Message.cBuffers = 2;
	}

	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer)&Buffers;
	status = nla->table->DecryptMessage(&nla->context, &Message, nla->recvSeqNum++, &pfQOP);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "DecryptMessage failure %s [%08" PRIX32 "]", GetSecurityStatusString(status),
		         status);
		goto fail;
	}

	/* generate SHA256 of following data: ServerClientHashMagic, Nonce, SubjectPublicKey */
	if (!(sha256 = winpr_Digest_New()))
		goto fail;

	if (!winpr_Digest_Init(sha256, WINPR_MD_SHA256))
		goto fail;

	/* include trailing \0 from hashMagic */
	if (!winpr_Digest_Update(sha256, hashMagic, hashSize))
		goto fail;

	if (!winpr_Digest_Update(sha256, nla->ClientNonce.pvBuffer, nla->ClientNonce.cbBuffer))
		goto fail;

	/* SubjectPublicKey */
	if (!winpr_Digest_Update(sha256, nla->PublicKey.pvBuffer, nla->PublicKey.cbBuffer))
		goto fail;

	if (!winpr_Digest_Final(sha256, serverClientHash, sizeof(serverClientHash)))
		goto fail;

	/* verify hash */
	if (memcmp(serverClientHash, Buffers[krb ? 0 : 1].pvBuffer, WINPR_SHA256_DIGEST_LENGTH) != 0)
	{
		WLog_ERR(TAG, "Could not verify server's hash");
		status = SEC_E_MESSAGE_ALTERED; /* DO NOT SEND CREDENTIALS! */
		goto fail;
	}

	status = SEC_E_OK;
fail:
	free(buffer);
	winpr_Digest_Free(sha256);
	return status;
}

static size_t nla_sizeof_ts_password_creds(rdpNla* nla)
{
	size_t length = 0;

	if (nla->identity)
	{
		length += ber_sizeof_sequence_octet_string(nla->identity->DomainLength * 2);
		length += ber_sizeof_sequence_octet_string(nla->identity->UserLength * 2);
		length += ber_sizeof_sequence_octet_string(nla->identity->PasswordLength * 2);
	}

	return length;
}

static size_t nla_sizeof_ts_credentials(rdpNla* nla)
{
	size_t size = 0;
	size += ber_sizeof_integer(1);
	size += ber_sizeof_contextual_tag(ber_sizeof_integer(1));
	size +=
	    ber_sizeof_sequence_octet_string(ber_sizeof_sequence(nla_sizeof_ts_password_creds(nla)));
	return size;
}

BOOL nla_read_ts_password_creds(rdpNla* nla, wStream* s)
{
	size_t length;

	if (!nla->identity)
	{
		WLog_ERR(TAG, "nla->identity is NULL!");
		return FALSE;
	}

	/* TSPasswordCreds (SEQUENCE)
	 * Initialise to default values. */
	nla->identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
	nla->identity->UserLength = (UINT32)0;
	nla->identity->User = NULL;
	nla->identity->DomainLength = (UINT32)0;
	nla->identity->Domain = NULL;
	nla->identity->Password = NULL;
	nla->identity->PasswordLength = (UINT32)0;

	if (!ber_read_sequence_tag(s, &length))
		return FALSE;

	/* The sequence is empty, return early,
	 * TSPasswordCreds (SEQUENCE) is optional. */
	if (length == 0)
		return TRUE;

	/* [0] domainName (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 0, &length, TRUE) || !ber_read_octet_string_tag(s, &length))
	{
		return FALSE;
	}

	nla->identity->DomainLength = (UINT32)length;

	if (nla->identity->DomainLength > 0)
	{
		nla->identity->Domain = (UINT16*)malloc(length);

		if (!nla->identity->Domain)
			return FALSE;

		CopyMemory(nla->identity->Domain, Stream_Pointer(s), nla->identity->DomainLength);
		Stream_Seek(s, nla->identity->DomainLength);
		nla->identity->DomainLength /= 2;
	}

	/* [1] userName (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 1, &length, TRUE) || !ber_read_octet_string_tag(s, &length))
	{
		return FALSE;
	}

	nla->identity->UserLength = (UINT32)length;

	if (nla->identity->UserLength > 0)
	{
		nla->identity->User = (UINT16*)malloc(length);

		if (!nla->identity->User)
			return FALSE;

		CopyMemory(nla->identity->User, Stream_Pointer(s), nla->identity->UserLength);
		Stream_Seek(s, nla->identity->UserLength);
		nla->identity->UserLength /= 2;
	}

	/* [2] password (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 2, &length, TRUE) || !ber_read_octet_string_tag(s, &length))
	{
		return FALSE;
	}

	nla->identity->PasswordLength = (UINT32)length;

	if (nla->identity->PasswordLength > 0)
	{
		nla->identity->Password = (UINT16*)malloc(length);

		if (!nla->identity->Password)
			return FALSE;

		CopyMemory(nla->identity->Password, Stream_Pointer(s), nla->identity->PasswordLength);
		Stream_Seek(s, nla->identity->PasswordLength);
		nla->identity->PasswordLength /= 2;
	}

	return TRUE;
}

static size_t nla_write_ts_password_creds(rdpNla* nla, wStream* s)
{
	size_t size = 0;
	size_t innerSize = nla_sizeof_ts_password_creds(nla);
	/* TSPasswordCreds (SEQUENCE) */
	size += ber_write_sequence_tag(s, innerSize);

	if (nla->identity)
	{
		/* [0] domainName (OCTET STRING) */
		size += ber_write_sequence_octet_string(s, 0, (BYTE*)nla->identity->Domain,
		                                        nla->identity->DomainLength * 2);
		/* [1] userName (OCTET STRING) */
		size += ber_write_sequence_octet_string(s, 1, (BYTE*)nla->identity->User,
		                                        nla->identity->UserLength * 2);
		/* [2] password (OCTET STRING) */
		size += ber_write_sequence_octet_string(s, 2, (BYTE*)nla->identity->Password,
		                                        nla->identity->PasswordLength * 2);
	}

	return size;
}

static BOOL nla_read_ts_credentials(rdpNla* nla, PSecBuffer ts_credentials)
{
	wStream* s;
	size_t length;
	size_t ts_password_creds_length = 0;
	BOOL ret;

	if (!ts_credentials || !ts_credentials->pvBuffer)
		return FALSE;

	s = Stream_New(ts_credentials->pvBuffer, ts_credentials->cbBuffer);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	/* TSCredentials (SEQUENCE) */
	ret = ber_read_sequence_tag(s, &length) &&
	      /* [0] credType (INTEGER) */
	      ber_read_contextual_tag(s, 0, &length, TRUE) && ber_read_integer(s, NULL) &&
	      /* [1] credentials (OCTET STRING) */
	      ber_read_contextual_tag(s, 1, &length, TRUE) &&
	      ber_read_octet_string_tag(s, &ts_password_creds_length) &&
	      nla_read_ts_password_creds(nla, s);
	Stream_Free(s, FALSE);
	return ret;
}

static size_t nla_write_ts_credentials(rdpNla* nla, wStream* s)
{
	size_t size = 0;
	size_t passwordSize;
	size_t innerSize = nla_sizeof_ts_credentials(nla);
	/* TSCredentials (SEQUENCE) */
	size += ber_write_sequence_tag(s, innerSize);
	/* [0] credType (INTEGER) */
	size += ber_write_contextual_tag(s, 0, ber_sizeof_integer(1), TRUE);
	size += ber_write_integer(s, 1);
	/* [1] credentials (OCTET STRING) */
	passwordSize = ber_sizeof_sequence(nla_sizeof_ts_password_creds(nla));
	size += ber_write_contextual_tag(s, 1, ber_sizeof_octet_string(passwordSize), TRUE);
	size += ber_write_octet_string_tag(s, passwordSize);
	size += nla_write_ts_password_creds(nla, s);
	return size;
}

/**
 * Encode TSCredentials structure.
 * @param credssp
 */

static BOOL nla_encode_ts_credentials(rdpNla* nla)
{
	wStream* s;
	size_t length;
	int DomainLength = 0;
	int UserLength = 0;
	int PasswordLength = 0;

	if (nla->identity)
	{
		/* TSPasswordCreds */
		DomainLength = nla->identity->DomainLength;
		UserLength = nla->identity->UserLength;
		PasswordLength = nla->identity->PasswordLength;
	}

	if (nla->settings->DisableCredentialsDelegation && nla->identity)
	{
		/* TSPasswordCreds */
		nla->identity->DomainLength = 0;
		nla->identity->UserLength = 0;
		nla->identity->PasswordLength = 0;
	}

	length = ber_sizeof_sequence(nla_sizeof_ts_credentials(nla));

	if (!sspi_SecBufferAlloc(&nla->tsCredentials, length))
	{
		WLog_ERR(TAG, "sspi_SecBufferAlloc failed!");
		return FALSE;
	}

	s = Stream_New((BYTE*)nla->tsCredentials.pvBuffer, length);

	if (!s)
	{
		sspi_SecBufferFree(&nla->tsCredentials);
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	nla_write_ts_credentials(nla, s);

	if (nla->settings->DisableCredentialsDelegation && nla->identity)
	{
		/* TSPasswordCreds */
		nla->identity->DomainLength = DomainLength;
		nla->identity->UserLength = UserLength;
		nla->identity->PasswordLength = PasswordLength;
	}

	Stream_Free(s, FALSE);
	return TRUE;
}

static SECURITY_STATUS nla_encrypt_ts_credentials(rdpNla* nla)
{
	SecBuffer Buffers[2] = { { 0 } };
	SecBufferDesc Message;
	SECURITY_STATUS status;
	const BOOL krb =
	    (_tcsncmp(nla->packageName, KERBEROS_SSP_NAME, ARRAYSIZE(KERBEROS_SSP_NAME)) == 0);
	const BOOL nego = (_tcsncmp(nla->packageName, NEGO_SSP_NAME, ARRAYSIZE(NEGO_SSP_NAME)) == 0);
	const BOOL ntlm = (_tcsncmp(nla->packageName, NTLM_SSP_NAME, ARRAYSIZE(NTLM_SSP_NAME)) == 0);

	if (!nla_encode_ts_credentials(nla))
		return SEC_E_INSUFFICIENT_MEMORY;

	if (!sspi_SecBufferAlloc(&nla->authInfo,
	                         nla->tsCredentials.cbBuffer + nla->ContextSizes.cbSecurityTrailer))
		return SEC_E_INSUFFICIENT_MEMORY;

	if (krb)
	{
		Buffers[0].BufferType = SECBUFFER_DATA; /* TSCredentials */
		Buffers[0].cbBuffer = nla->tsCredentials.cbBuffer;
		Buffers[0].pvBuffer = nla->authInfo.pvBuffer;
		CopyMemory(Buffers[0].pvBuffer, nla->tsCredentials.pvBuffer, Buffers[0].cbBuffer);
		Message.cBuffers = 1;
	}
	else if (ntlm || nego)
	{
		Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
		Buffers[0].cbBuffer = nla->ContextSizes.cbSecurityTrailer;
		Buffers[0].pvBuffer = nla->authInfo.pvBuffer;
		MoveMemory(Buffers[0].pvBuffer, nla->authInfo.pvBuffer, Buffers[0].cbBuffer);
		Buffers[1].BufferType = SECBUFFER_DATA; /* TSCredentials */
		Buffers[1].cbBuffer = nla->tsCredentials.cbBuffer;
		Buffers[1].pvBuffer = &((BYTE*)nla->authInfo.pvBuffer)[Buffers[0].cbBuffer];
		CopyMemory(Buffers[1].pvBuffer, nla->tsCredentials.pvBuffer, Buffers[1].cbBuffer);
		Message.cBuffers = 2;
	}

	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer)&Buffers;
	status = nla->table->EncryptMessage(&nla->context, 0, &Message, nla->sendSeqNum++);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage failure %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(status), status);
		return status;
	}

	if (Message.cBuffers == 2 && Buffers[0].cbBuffer < nla->ContextSizes.cbSecurityTrailer)
	{
		/* IMPORTANT: EncryptMessage may not use all the signature space, so we need to shrink the
		 * excess between the buffers */
		MoveMemory(((BYTE*)Buffers[0].pvBuffer) + Buffers[0].cbBuffer, Buffers[1].pvBuffer,
		           Buffers[1].cbBuffer);
		nla->authInfo.cbBuffer = Buffers[0].cbBuffer + Buffers[1].cbBuffer;
	}

	return SEC_E_OK;
}

static SECURITY_STATUS nla_decrypt_ts_credentials(rdpNla* nla)
{
	int length;
	BYTE* buffer;
	ULONG pfQOP;
	SecBuffer Buffers[2] = { { 0 } };
	SecBufferDesc Message;
	SECURITY_STATUS status;
	const BOOL krb =
	    (_tcsncmp(nla->packageName, KERBEROS_SSP_NAME, ARRAYSIZE(KERBEROS_SSP_NAME)) == 0);
	const BOOL nego = (_tcsncmp(nla->packageName, NEGO_SSP_NAME, ARRAYSIZE(NEGO_SSP_NAME)) == 0);
	const BOOL ntlm = (_tcsncmp(nla->packageName, NTLM_SSP_NAME, ARRAYSIZE(NTLM_SSP_NAME)) == 0);

	if (nla->authInfo.cbBuffer < 1)
	{
		WLog_ERR(TAG, "nla_decrypt_ts_credentials missing authInfo buffer");
		return SEC_E_INVALID_TOKEN;
	}

	length = nla->authInfo.cbBuffer;
	buffer = (BYTE*)malloc(length);

	if (!buffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (krb)
	{
		CopyMemory(buffer, nla->authInfo.pvBuffer, length);
		Buffers[0].BufferType = SECBUFFER_DATA; /* Wrapped and encrypted TSCredentials */
		Buffers[0].cbBuffer = length;
		Buffers[0].pvBuffer = buffer;
		Message.cBuffers = 1;
	}
	else if (ntlm || nego)
	{
		CopyMemory(buffer, nla->authInfo.pvBuffer, length);
		Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
		Buffers[0].cbBuffer = nla->ContextSizes.cbSecurityTrailer;
		Buffers[0].pvBuffer = buffer;
		Buffers[1].BufferType = SECBUFFER_DATA; /* TSCredentials */
		Buffers[1].cbBuffer = length - nla->ContextSizes.cbSecurityTrailer;
		Buffers[1].pvBuffer = &buffer[Buffers[0].cbBuffer];
		Message.cBuffers = 2;
	}

	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = (PSecBuffer)&Buffers;
	status = nla->table->DecryptMessage(&nla->context, &Message, nla->recvSeqNum++, &pfQOP);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "DecryptMessage failure %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(status), status);
		free(buffer);
		return status;
	}

	if (!nla_read_ts_credentials(nla, &Buffers[1]))
	{
		free(buffer);
		return SEC_E_INSUFFICIENT_MEMORY;
	}

	free(buffer);
	return SEC_E_OK;
}

static size_t nla_sizeof_nego_token(size_t length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

static size_t nla_sizeof_nego_tokens(size_t length)
{
	length = nla_sizeof_nego_token(length);
	length += ber_sizeof_sequence_tag(length);
	length += ber_sizeof_sequence_tag(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

static size_t nla_sizeof_pub_key_auth(size_t length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

static size_t nla_sizeof_auth_info(size_t length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

static size_t nla_sizeof_client_nonce(size_t length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

static size_t nla_sizeof_ts_request(size_t length)
{
	length += ber_sizeof_integer(2);
	length += ber_sizeof_contextual_tag(3);
	return length;
}

/**
 * Send CredSSP message.
 * @param credssp
 */

BOOL nla_send(rdpNla* nla, const char* msg)
{
	BOOL rc = TRUE;
	wStream* s;
	size_t length;
	size_t ts_request_length;
	size_t nego_tokens_length = 0;
	size_t pub_key_auth_length = 0;
	size_t auth_info_length = 0;
	size_t error_code_context_length = 0;
	size_t error_code_length = 0;
	size_t client_nonce_length = 0;
	WLog_DBG(TAG, "%s", msg);
	nla_buffer_print(nla);

	nego_tokens_length =
	    (nla->negoToken.cbBuffer > 0) ? nla_sizeof_nego_tokens(nla->negoToken.cbBuffer) : 0;
	pub_key_auth_length =
	    (nla->pubKeyAuth.cbBuffer > 0) ? nla_sizeof_pub_key_auth(nla->pubKeyAuth.cbBuffer) : 0;
	auth_info_length =
	    (nla->authInfo.cbBuffer > 0) ? nla_sizeof_auth_info(nla->authInfo.cbBuffer) : 0;
	client_nonce_length =
	    (nla->ClientNonce.cbBuffer > 0) ? nla_sizeof_client_nonce(nla->ClientNonce.cbBuffer) : 0;

	if (nla->peerVersion >= 3 && nla->peerVersion != 5 && nla->errorCode != 0)
	{
		error_code_length = ber_sizeof_integer(nla->errorCode);
		error_code_context_length = ber_sizeof_contextual_tag(error_code_length);
	}

	length = nego_tokens_length + pub_key_auth_length + auth_info_length +
	         error_code_context_length + error_code_length + client_nonce_length;
	ts_request_length = nla_sizeof_ts_request(length);
	s = Stream_New(NULL, ber_sizeof_sequence(ts_request_length));

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	/* TSRequest */
	ber_write_sequence_tag(s, ts_request_length); /* SEQUENCE */
	/* [0] version */
	ber_write_contextual_tag(s, 0, 3, TRUE);
	ber_write_integer(s, nla->version); /* INTEGER */

	/* [1] negoTokens (NegoData) */
	if (nego_tokens_length > 0)
	{
		length = ber_write_contextual_tag(
		    s, 1,
		    ber_sizeof_sequence(
		        ber_sizeof_sequence(ber_sizeof_sequence_octet_string(nla->negoToken.cbBuffer))),
		    TRUE); /* NegoData */
		length +=
		    ber_write_sequence_tag(s, ber_sizeof_sequence(ber_sizeof_sequence_octet_string(
		                                  nla->negoToken.cbBuffer))); /* SEQUENCE OF NegoDataItem */
		length += ber_write_sequence_tag(
		    s, ber_sizeof_sequence_octet_string(nla->negoToken.cbBuffer)); /* NegoDataItem */
		length += ber_write_sequence_octet_string(s, 0, (BYTE*)nla->negoToken.pvBuffer,
		                                          nla->negoToken.cbBuffer); /* OCTET STRING */

		if (length != nego_tokens_length)
		{
			Stream_Free(s, TRUE);
			return FALSE;
		}
	}

	/* [2] authInfo (OCTET STRING) */
	if (auth_info_length > 0)
	{
		if (ber_write_sequence_octet_string(s, 2, nla->authInfo.pvBuffer, nla->authInfo.cbBuffer) !=
		    auth_info_length)
		{
			Stream_Free(s, TRUE);
			return FALSE;
		}
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (pub_key_auth_length > 0)
	{
		if (ber_write_sequence_octet_string(s, 3, nla->pubKeyAuth.pvBuffer,
		                                    nla->pubKeyAuth.cbBuffer) != pub_key_auth_length)
		{
			Stream_Free(s, TRUE);
			return FALSE;
		}
	}

	/* [4] errorCode (INTEGER) */
	if (error_code_length > 0)
	{
		ber_write_contextual_tag(s, 4, error_code_length, TRUE);
		ber_write_integer(s, nla->errorCode);
	}

	/* [5] clientNonce (OCTET STRING) */
	if (client_nonce_length > 0)
	{
		if (ber_write_sequence_octet_string(s, 5, nla->ClientNonce.pvBuffer,
		                                    nla->ClientNonce.cbBuffer) != client_nonce_length)
		{
			Stream_Free(s, TRUE);
			return FALSE;
		}
	}

	Stream_SealLength(s);

	if (transport_write(nla->transport, s) < 0)
		rc = FALSE;

	Stream_Free(s, TRUE);
	return rc;
}

static int nla_decode_ts_request(rdpNla* nla, wStream* s)
{
	size_t length;
	UINT32 version = 0;

	/* TSRequest */
	if (!ber_read_sequence_tag(s, &length) || !ber_read_contextual_tag(s, 0, &length, TRUE) ||
	    !ber_read_integer(s, &version))
	{
		return -1;
	}

	if (nla->peerVersion == 0)
	{
		WLog_DBG(TAG, "CredSSP protocol support %" PRIu32 ", peer supports %" PRIu32, nla->version,
		         version);
		nla->peerVersion = version;
	}

	/* if the peer suddenly changed its version - kick it */
	if (nla->peerVersion != version)
	{
		WLog_ERR(TAG, "CredSSP peer changed protocol version from %" PRIu32 " to %" PRIu32,
		         nla->peerVersion, version);
		return -1;
	}

	/* [1] negoTokens (NegoData) */
	if (ber_read_contextual_tag(s, 1, &length, TRUE) != FALSE)
	{
		if (!ber_read_sequence_tag(s, &length) ||            /* SEQUENCE OF NegoDataItem */
		    !ber_read_sequence_tag(s, &length) ||            /* NegoDataItem */
		    !ber_read_contextual_tag(s, 0, &length, TRUE) || /* [0] negoToken */
		    !ber_read_octet_string_tag(s, &length) ||        /* OCTET STRING */
		    Stream_GetRemainingLength(s) < length)
		{
			return -1;
		}

		sspi_SecBufferFree(&nla->negoToken);
		if (!sspi_SecBufferAlloc(&nla->negoToken, length))
			return -1;

		Stream_Read(s, nla->negoToken.pvBuffer, length);
		nla->negoToken.cbBuffer = length;
	}

	/* [2] authInfo (OCTET STRING) */
	if (ber_read_contextual_tag(s, 2, &length, TRUE) != FALSE)
	{
		if (!ber_read_octet_string_tag(s, &length) || /* OCTET STRING */
		    Stream_GetRemainingLength(s) < length)
			return -1;

		if (!sspi_SecBufferAlloc(&nla->authInfo, length))
			return -1;

		Stream_Read(s, nla->authInfo.pvBuffer, length);
		nla->authInfo.cbBuffer = length;
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (ber_read_contextual_tag(s, 3, &length, TRUE) != FALSE)
	{
		if (!ber_read_octet_string_tag(s, &length) || /* OCTET STRING */
		    Stream_GetRemainingLength(s) < length)
			return -1;

		sspi_SecBufferFree(&nla->pubKeyAuth);
		if (!sspi_SecBufferAlloc(&nla->pubKeyAuth, length))
			return -1;

		Stream_Read(s, nla->pubKeyAuth.pvBuffer, length);
		nla->pubKeyAuth.cbBuffer = length;
	}

	/* [4] errorCode (INTEGER) */
	if (nla->peerVersion >= 3)
	{
		if (ber_read_contextual_tag(s, 4, &length, TRUE) != FALSE)
		{
			if (!ber_read_integer(s, &nla->errorCode))
				return -1;
			WLog_WARN(TAG, "SPNEGO received NTSTATUS: %s [0x%08" PRIX32 "] from server",
			          GetSecurityStatusString(nla->errorCode), nla->errorCode);
		}

		if (nla->peerVersion >= 5)
		{
			if (ber_read_contextual_tag(s, 5, &length, TRUE) != FALSE)
			{
				if (!ber_read_octet_string_tag(s, &length) || /* OCTET STRING */
				    Stream_GetRemainingLength(s) < length)
					return -1;

				sspi_SecBufferFree(&nla->ClientNonce);
				if (!sspi_SecBufferAlloc(&nla->ClientNonce, length))
					return -1;

				Stream_Read(s, nla->ClientNonce.pvBuffer, length);
				nla->ClientNonce.cbBuffer = length;
			}
		}
	}

	return 1;
}

int nla_recv_pdu(rdpNla* nla, wStream* s)
{
	if (nla_decode_ts_request(nla, s) < 1)
		return -1;

	if (nla->errorCode)
	{
		UINT32 code;

		switch (nla->errorCode)
		{
			case STATUS_PASSWORD_MUST_CHANGE:
				code = FREERDP_ERROR_CONNECT_PASSWORD_MUST_CHANGE;
				break;

			case STATUS_PASSWORD_EXPIRED:
				code = FREERDP_ERROR_CONNECT_PASSWORD_EXPIRED;
				break;

			case STATUS_ACCOUNT_DISABLED:
				code = FREERDP_ERROR_CONNECT_ACCOUNT_DISABLED;
				break;

			case STATUS_LOGON_FAILURE:
				code = FREERDP_ERROR_CONNECT_LOGON_FAILURE;
				break;

			case STATUS_WRONG_PASSWORD:
				code = FREERDP_ERROR_CONNECT_WRONG_PASSWORD;
				break;

			case STATUS_ACCESS_DENIED:
				code = FREERDP_ERROR_CONNECT_ACCESS_DENIED;
				break;

			case STATUS_ACCOUNT_RESTRICTION:
				code = FREERDP_ERROR_CONNECT_ACCOUNT_RESTRICTION;
				break;

			case STATUS_ACCOUNT_LOCKED_OUT:
				code = FREERDP_ERROR_CONNECT_ACCOUNT_LOCKED_OUT;
				break;

			case STATUS_ACCOUNT_EXPIRED:
				code = FREERDP_ERROR_CONNECT_ACCOUNT_EXPIRED;
				break;

			case STATUS_LOGON_TYPE_NOT_GRANTED:
				code = FREERDP_ERROR_CONNECT_LOGON_TYPE_NOT_GRANTED;
				break;

			default:
				WLog_ERR(TAG, "SPNEGO failed with NTSTATUS: %s [0x%08" PRIX32 "]",
				         GetSecurityStatusString(nla->errorCode), nla->errorCode);
				code = FREERDP_ERROR_AUTHENTICATION_FAILED;
				break;
		}

		freerdp_set_last_error_log(nla->instance->context, code);
		return -1;
	}

	if (nla_client_recv(nla) < 1)
		return -1;

	return 1;
}

int nla_recv(rdpNla* nla, const char* msg)
{
	wStream* s;
	int status;
	s = Stream_New(NULL, 4096);

	if (!s)
	{
		WLog_ERR(TAG, "[%s] Stream_New failed!", __FUNCTION__);
		return -1;
	}

	status = transport_read_pdu(nla->transport, s);

	if (status < 0)
	{
		WLog_ERR(TAG, "[%s] error: %d", __FUNCTION__, status);
		Stream_Free(s, TRUE);
		return -1;
	}

	if (nla_decode_ts_request(nla, s) < 1)
	{
		WLog_ERR(TAG, "[%s] Invalid data received, aborting", __FUNCTION__);
		Stream_Free(s, TRUE);
		return -1;
	}

	Stream_Free(s, TRUE);
	WLog_DBG(TAG, "[%s] %s", __FUNCTION__, msg);
	nla_buffer_print(nla);
	return 1;
}

void nla_buffer_print(rdpNla* nla)
{
	if (nla->negoToken.cbBuffer > 0)
	{
		WLog_DBG(TAG, "NLA.negoToken (length = %" PRIu32 "):", nla->negoToken.cbBuffer);
#if defined(WITH_DEBUG_NLA)
		winpr_HexDump(TAG, WLOG_DEBUG, nla->negoToken.pvBuffer, nla->negoToken.cbBuffer);
#endif
	}

	if (nla->pubKeyAuth.cbBuffer > 0)
	{
		WLog_DBG(TAG, "NLA.pubKeyAuth (length = %" PRIu32 "):", nla->pubKeyAuth.cbBuffer);
#if defined(WITH_DEBUG_NLA)
		winpr_HexDump(TAG, WLOG_DEBUG, nla->pubKeyAuth.pvBuffer, nla->pubKeyAuth.cbBuffer);
#endif
	}

	if (nla->authInfo.cbBuffer > 0)
	{
		WLog_DBG(TAG, "NLA.authInfo (length = %" PRIu32 "):", nla->authInfo.cbBuffer);
#if defined(WITH_DEBUG_NLA)
		winpr_HexDump(TAG, WLOG_DEBUG, nla->authInfo.pvBuffer, nla->authInfo.cbBuffer);
#endif
	}
}

void nla_buffer_free(rdpNla* nla)
{
	sspi_SecBufferFree(&nla->negoToken);
	sspi_SecBufferFree(&nla->pubKeyAuth);
	sspi_SecBufferFree(&nla->authInfo);
}

LPTSTR nla_make_spn(const char* ServiceClass, const char* hostname)
{
	DWORD status;
	DWORD SpnLength;
	LPTSTR hostnameX = NULL;
	LPTSTR ServiceClassX = NULL;
	LPTSTR ServicePrincipalName = NULL;
#ifdef UNICODE
	ConvertToUnicode(CP_UTF8, 0, hostname, -1, &hostnameX, 0);
	ConvertToUnicode(CP_UTF8, 0, ServiceClass, -1, &ServiceClassX, 0);
#else
	hostnameX = _strdup(hostname);
	ServiceClassX = _strdup(ServiceClass);
#endif

	if (!hostnameX || !ServiceClassX)
	{
		free(hostnameX);
		free(ServiceClassX);
		return NULL;
	}

	if (!ServiceClass)
	{
		ServicePrincipalName = (LPTSTR)_tcsdup(hostnameX);
		free(ServiceClassX);
		free(hostnameX);
		return ServicePrincipalName;
	}

	SpnLength = 0;
	status = DsMakeSpn(ServiceClassX, hostnameX, NULL, 0, NULL, &SpnLength, NULL);

	if (status != ERROR_BUFFER_OVERFLOW)
	{
		free(ServiceClassX);
		free(hostnameX);
		return NULL;
	}

	ServicePrincipalName = (LPTSTR)calloc(SpnLength, sizeof(TCHAR));

	if (!ServicePrincipalName)
	{
		free(ServiceClassX);
		free(hostnameX);
		return NULL;
	}

	status = DsMakeSpn(ServiceClassX, hostnameX, NULL, 0, NULL, &SpnLength, ServicePrincipalName);

	if (status != ERROR_SUCCESS)
	{
		free(ServicePrincipalName);
		free(ServiceClassX);
		free(hostnameX);
		return NULL;
	}

	free(ServiceClassX);
	free(hostnameX);
	return ServicePrincipalName;
}

/**
 * Create new CredSSP state machine.
 * @param transport
 * @return new CredSSP state machine.
 */

rdpNla* nla_new(freerdp* instance, rdpTransport* transport, rdpSettings* settings)
{
	rdpNla* nla = (rdpNla*)calloc(1, sizeof(rdpNla));

	if (!nla)
		return NULL;

	nla->identity = calloc(1, sizeof(SEC_WINNT_AUTH_IDENTITY));

	if (!nla->identity)
	{
		free(nla);
		return NULL;
	}

	nla->instance = instance;
	nla->settings = settings;
	nla->server = settings->ServerMode;
	nla->transport = transport;
	nla->sendSeqNum = 0;
	nla->recvSeqNum = 0;
	nla->version = 6;
	SecInvalidateHandle(&nla->context);

	if (settings->NtlmSamFile)
	{
		nla->SamFile = _strdup(settings->NtlmSamFile);

		if (!nla->SamFile)
			goto cleanup;
	}

	/* init to 0 or we end up freeing a bad pointer if the alloc fails */
	if (!sspi_SecBufferAlloc(&nla->ClientNonce, NonceLength))
		goto cleanup;

	/* generate random 32-byte nonce */
	if (winpr_RAND(nla->ClientNonce.pvBuffer, NonceLength) < 0)
		goto cleanup;

	if (nla->server)
	{
		LONG status;
		HKEY hKey;
		DWORD dwType;
		DWORD dwSize;
		status =
		    RegOpenKeyExA(HKEY_LOCAL_MACHINE, SERVER_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

		if (status != ERROR_SUCCESS)
			return nla;

		status = RegQueryValueEx(hKey, _T("SspiModule"), NULL, &dwType, NULL, &dwSize);

		if (status != ERROR_SUCCESS)
		{
			RegCloseKey(hKey);
			return nla;
		}

		nla->SspiModule = (LPTSTR)malloc(dwSize + sizeof(TCHAR));

		if (!nla->SspiModule)
		{
			RegCloseKey(hKey);
			goto cleanup;
		}

		status =
		    RegQueryValueEx(hKey, _T("SspiModule"), NULL, &dwType, (BYTE*)nla->SspiModule, &dwSize);

		if (status == ERROR_SUCCESS)
			WLog_INFO(TAG, "Using SSPI Module: %s", nla->SspiModule);

		RegCloseKey(hKey);
	}

	return nla;
cleanup:
	nla_free(nla);
	return NULL;
}

/**
 * Free CredSSP state machine.
 * @param credssp
 */

void nla_free(rdpNla* nla)
{
	if (!nla)
		return;

	if (nla->table)
	{
		SECURITY_STATUS status;

		if (SecIsValidHandle(&nla->credentials))
		{
			status = nla->table->FreeCredentialsHandle(&nla->credentials);

			if (status != SEC_E_OK)
			{
				WLog_WARN(TAG, "FreeCredentialsHandle status %s [0x%08" PRIX32 "]",
				          GetSecurityStatusString(status), status);
			}

			SecInvalidateHandle(&nla->credentials);
		}

		status = nla->table->DeleteSecurityContext(&nla->context);

		if (status != SEC_E_OK)
		{
			WLog_WARN(TAG, "DeleteSecurityContext status %s [0x%08" PRIX32 "]",
			          GetSecurityStatusString(status), status);
		}
	}

	free(nla->SamFile);
	nla->SamFile = NULL;
	sspi_SecBufferFree(&nla->ClientNonce);
	sspi_SecBufferFree(&nla->PublicKey);
	sspi_SecBufferFree(&nla->tsCredentials);
	free(nla->ServicePrincipalName);
	nla_identity_free(nla->identity);
	nla_buffer_free(nla);
	free(nla);
}

SEC_WINNT_AUTH_IDENTITY* nla_get_identity(rdpNla* nla)
{
	if (!nla)
		return NULL;

	return nla->identity;
}

NLA_STATE nla_get_state(rdpNla* nla)
{
	if (!nla)
		return NLA_STATE_FINAL;

	return nla->state;
}

BOOL nla_set_state(rdpNla* nla, NLA_STATE state)
{
	if (!nla)
		return FALSE;

	nla->state = state;
	return TRUE;
}

BOOL nla_set_service_principal(rdpNla* nla, LPSTR principal)
{
	if (!nla || !principal)
		return FALSE;

	nla->ServicePrincipalName = principal;
	return TRUE;
}

BOOL nla_impersonate(rdpNla* nla)
{
	if (!nla)
		return FALSE;

	if (!nla->table || !nla->table->ImpersonateSecurityContext)
		return FALSE;

	return (nla->table->ImpersonateSecurityContext(&nla->context) == SEC_E_OK);
}

BOOL nla_revert_to_self(rdpNla* nla)
{
	if (!nla)
		return FALSE;

	if (!nla->table || !nla->table->RevertSecurityContext)
		return FALSE;

	return (nla->table->RevertSecurityContext(&nla->context) == SEC_E_OK);
}
