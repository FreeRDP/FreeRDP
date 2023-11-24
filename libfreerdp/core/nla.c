/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Level Authentication (NLA)
 *
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2016 Martin Fleisz <martin.fleisz@thincast.com>
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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

#include <freerdp/config.h>

#include "settings.h"

#include <time.h>
#include <ctype.h>

#include <freerdp/log.h>
#include <freerdp/build-config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/sam.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/tchar.h>
#include <winpr/ncrypt.h>
#include <winpr/cred.h>
#include <winpr/debug.h>
#include <winpr/asn1.h>
#include <winpr/secapi.h>

#include "../crypto/tls.h"
#include "nego.h"
#include "rdp.h"
#include "nla.h"
#include "utils.h"
#include "credssp_auth.h"
#include <freerdp/utils/smartcardlogon.h>

#define TAG FREERDP_TAG("core.nla")

#define SERVER_KEY "Software\\" FREERDP_VENDOR_STRING "\\" FREERDP_PRODUCT_STRING "\\Server"

#define NLA_AUTH_PKG NEGO_SSP_NAME

typedef enum
{
	AUTHZ_SUCCESS = 0x00000000,
	AUTHZ_ACCESS_DENIED = 0x00000005,
} AUTHZ_RESULT;

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

#define NLA_PKG_NAME CREDSSP_AUTH_PKG_SPNEGO

struct rdp_nla
{
	BOOL server;
	NLA_STATE state;
	ULONG sendSeqNum;
	ULONG recvSeqNum;
	rdpContext* rdpcontext;
	rdpTransport* transport;
	UINT32 version;
	UINT32 peerVersion;
	UINT32 errorCode;

	/* Lifetime of buffer nla_new -> nla_free */
	SecBuffer ClientNonce; /* Depending on protocol version a random nonce or a value read from the
	                          server. */

	SecBuffer negoToken;
	SecBuffer pubKeyAuth;
	SecBuffer authInfo;
	SecBuffer PublicKey;
	SecBuffer tsCredentials;

	SEC_WINNT_AUTH_IDENTITY* identity;

	rdpCredsspAuth* auth;
	char* pkinitArgs;
	SmartcardCertInfo* smartcardCert;
	BYTE certSha1[20];
	BOOL earlyUserAuth;
};

static BOOL nla_send(rdpNla* nla);
static int nla_server_recv(rdpNla* nla);
static BOOL nla_encrypt_public_key_echo(rdpNla* nla);
static BOOL nla_encrypt_public_key_hash(rdpNla* nla);
static BOOL nla_decrypt_public_key_echo(rdpNla* nla);
static BOOL nla_decrypt_public_key_hash(rdpNla* nla);
static BOOL nla_encrypt_ts_credentials(rdpNla* nla);
static BOOL nla_decrypt_ts_credentials(rdpNla* nla);

void nla_set_early_user_auth(rdpNla* nla, BOOL earlyUserAuth)
{
	WINPR_ASSERT(nla);
	WLog_DBG(TAG, "Early User Auth active: %s", earlyUserAuth ? "true" : "false");
	nla->earlyUserAuth = earlyUserAuth;
}

static void nla_buffer_free(rdpNla* nla)
{
	WINPR_ASSERT(nla);
	sspi_SecBufferFree(&nla->pubKeyAuth);
	sspi_SecBufferFree(&nla->authInfo);
	sspi_SecBufferFree(&nla->negoToken);
	sspi_SecBufferFree(&nla->ClientNonce);
	sspi_SecBufferFree(&nla->PublicKey);
}

static BOOL nla_Digest_Update_From_SecBuffer(WINPR_DIGEST_CTX* ctx, const SecBuffer* buffer)
{
	if (!buffer)
		return FALSE;
	return winpr_Digest_Update(ctx, buffer->pvBuffer, buffer->cbBuffer);
}

static BOOL nla_sec_buffer_alloc(SecBuffer* buffer, size_t size)
{
	WINPR_ASSERT(buffer);
	sspi_SecBufferFree(buffer);
	if (size > ULONG_MAX)
		return FALSE;
	if (!sspi_SecBufferAlloc(buffer, (ULONG)size))
		return FALSE;

	WINPR_ASSERT(buffer);
	buffer->BufferType = SECBUFFER_TOKEN;
	return TRUE;
}

static BOOL nla_sec_buffer_alloc_from_data(SecBuffer* buffer, const BYTE* data, size_t offset,
                                           size_t size)
{
	if (!nla_sec_buffer_alloc(buffer, offset + size))
		return FALSE;

	WINPR_ASSERT(buffer);
	BYTE* pb = buffer->pvBuffer;
	memcpy(&pb[offset], data, size);
	return TRUE;
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

static BOOL nla_adjust_settings_from_smartcard(rdpNla* nla)
{
	BOOL ret = FALSE;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	rdpSettings* settings = nla->rdpcontext->settings;
	WINPR_ASSERT(settings);

	if (!settings->SmartcardLogon)
		return TRUE;

	smartcardCertInfo_Free(nla->smartcardCert);

	if (!smartcard_getCert(nla->rdpcontext, &nla->smartcardCert, FALSE))
	{
		WLog_ERR(TAG, "unable to get smartcard certificate for logon");
		return FALSE;
	}

	if (!settings->CspName)
	{
		if (nla->smartcardCert->csp && !freerdp_settings_set_string_from_utf16(
		                                   settings, FreeRDP_CspName, nla->smartcardCert->csp))
		{
			WLog_ERR(TAG, "unable to set CSP name");
			goto out;
		}
		if (!settings->CspName &&
		    !freerdp_settings_set_string(settings, FreeRDP_CspName, MS_SCARD_PROV_A))
		{
			WLog_ERR(TAG, "unable to set CSP name");
			goto out;
		}
	}

	if (!settings->ReaderName && nla->smartcardCert->reader)
	{
		if (!freerdp_settings_set_string_from_utf16(settings, FreeRDP_ReaderName,
		                                            nla->smartcardCert->reader))
		{
			WLog_ERR(TAG, "unable to copy reader name");
			goto out;
		}
	}

	if (!settings->ContainerName && nla->smartcardCert->containerName)
	{
		if (!freerdp_settings_set_string_from_utf16(settings, FreeRDP_ContainerName,
		                                            nla->smartcardCert->containerName))
		{
			WLog_ERR(TAG, "unable to copy container name");
			goto out;
		}
	}

	memcpy(nla->certSha1, nla->smartcardCert->sha1Hash, sizeof(nla->certSha1));

	if (nla->smartcardCert->pkinitArgs)
	{
		nla->pkinitArgs = _strdup(nla->smartcardCert->pkinitArgs);
		if (!nla->pkinitArgs)
		{
			WLog_ERR(TAG, "unable to copy pkinitArgs");
			goto out;
		}
	}

	ret = TRUE;
out:
	return ret;
}

static BOOL nla_client_setup_identity(rdpNla* nla)
{
	BOOL PromptPassword = FALSE;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	rdpSettings* settings = nla->rdpcontext->settings;
	WINPR_ASSERT(settings);

	freerdp* instance = nla->rdpcontext->instance;
	WINPR_ASSERT(instance);

	/* */
	if ((utils_str_is_empty(settings->Username) ||
	     (utils_str_is_empty(settings->Password) &&
	      utils_str_is_empty((const char*)settings->RedirectionPassword))))
	{
		PromptPassword = TRUE;
	}

	if (PromptPassword && !utils_str_is_empty(settings->Username))
	{
		WINPR_SAM* sam = SamOpen(NULL, TRUE);
		if (sam)
		{
			const size_t userLength = strlen(settings->Username);
			WINPR_SAM_ENTRY* entry = SamLookupUserA(
			    sam, settings->Username, userLength + 1 /* ensure '\0' is checked too */, NULL, 0);
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
			if ((settings->PasswordHash) && (strlen(settings->PasswordHash) > 0))
				PromptPassword = FALSE;
		}

		if (settings->RemoteCredentialGuard)
			PromptPassword = FALSE;
	}
#endif

	BOOL smartCardLogonWasDisabled = !settings->SmartcardLogon;
	if (PromptPassword)
	{
		switch (utils_authenticate(instance, AUTH_NLA, TRUE))
		{
			case AUTH_SKIP:
			case AUTH_SUCCESS:
				break;
			case AUTH_CANCELLED:
				freerdp_set_last_error_log(instance->context, FREERDP_ERROR_CONNECT_CANCELLED);
				return FALSE;
			case AUTH_NO_CREDENTIALS:
				WLog_INFO(TAG, "No credentials provided - using NULL identity");
				break;
			default:
				return FALSE;
		}
	}

	if (!settings->Username)
	{
		sspi_FreeAuthIdentity(nla->identity);
		nla->identity = NULL;
	}
	else if (settings->SmartcardLogon)
	{
		if (smartCardLogonWasDisabled)
		{
			if (!nla_adjust_settings_from_smartcard(nla))
				return FALSE;
		}

		if (!identity_set_from_smartcard_hash(nla->identity, settings, FreeRDP_Username,
		                                      FreeRDP_Domain, FreeRDP_Password, nla->certSha1,
		                                      sizeof(nla->certSha1)))
			return FALSE;
	}
	else
	{
		BOOL usePassword = TRUE;

		if (settings->RedirectionPassword && (settings->RedirectionPasswordLength > 0))
		{
			if (!identity_set_from_settings_with_pwd(
			        nla->identity, settings, FreeRDP_Username, FreeRDP_Domain,
			        (const WCHAR*)settings->RedirectionPassword,
			        settings->RedirectionPasswordLength / sizeof(WCHAR)))
				return FALSE;

			usePassword = FALSE;
		}

		if (settings->RestrictedAdminModeRequired)
		{
			if (settings->PasswordHash && strlen(settings->PasswordHash) == 32)
			{
				if (!identity_set_from_settings(nla->identity, settings, FreeRDP_Username,
				                                FreeRDP_Domain, FreeRDP_PasswordHash))
					return FALSE;

				/**
				 * Increase password hash length by LB_PASSWORD_MAX_LENGTH to obtain a
				 * length exceeding the maximum (LB_PASSWORD_MAX_LENGTH) and use it this for
				 * hash identification in WinPR.
				 */
				nla->identity->PasswordLength += LB_PASSWORD_MAX_LENGTH;
				usePassword = FALSE;
			}
		}

		if (usePassword)
		{
			if (!identity_set_from_settings(nla->identity, settings, FreeRDP_Username,
			                                FreeRDP_Domain, FreeRDP_Password))
				return FALSE;
		}
	}

	return TRUE;
}

static int nla_client_init(rdpNla* nla)
{
	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	rdpSettings* settings = nla->rdpcontext->settings;
	WINPR_ASSERT(settings);

	nla_set_state(nla, NLA_STATE_INITIAL);

	if (!nla_adjust_settings_from_smartcard(nla))
		return -1;

	if (!credssp_auth_init(nla->auth, NLA_AUTH_PKG, NULL))
		return -1;

	if (!nla_client_setup_identity(nla))
		return -1;

	const char* hostname = freerdp_settings_get_server_name(settings);

	if (!credssp_auth_setup_client(nla->auth, "TERMSRV", hostname, nla->identity, nla->pkinitArgs))
		return -1;

	rdpTls* tls = transport_get_tls(nla->transport);

	if (!tls)
	{
		WLog_ERR(TAG, "Unknown NLA transport layer");
		return -1;
	}

	if (!nla_sec_buffer_alloc_from_data(&nla->PublicKey, tls->PublicKey, 0, tls->PublicKeyLength))
	{
		WLog_ERR(TAG, "Failed to allocate sspi secBuffer");
		return -1;
	}

	return 1;
}

int nla_client_begin(rdpNla* nla)
{
	WINPR_ASSERT(nla);

	if (nla_client_init(nla) < 1)
		return -1;

	if (nla_get_state(nla) != NLA_STATE_INITIAL)
		return -1;

	/*
	 * from tspkg.dll: 0x00000132
	 * ISC_REQ_MUTUAL_AUTH
	 * ISC_REQ_CONFIDENTIALITY
	 * ISC_REQ_USE_SESSION_KEY
	 * ISC_REQ_ALLOCATE_MEMORY
	 */
	credssp_auth_set_flags(nla->auth, ISC_REQ_MUTUAL_AUTH | ISC_REQ_CONFIDENTIALITY);

	const int rc = credssp_auth_authenticate(nla->auth);

	switch (rc)
	{
		case 0:
			if (!nla_send(nla))
				return -1;
			nla_set_state(nla, NLA_STATE_NEGO_TOKEN);
			break;
		case 1:
			if (credssp_auth_have_output_token(nla->auth))
			{
				if (!nla_send(nla))
					return -1;
			}
			nla_set_state(nla, NLA_STATE_FINAL);
			break;
		default:
			return -1;
	}

	return 1;
}

static int nla_client_recv_nego_token(rdpNla* nla)
{
	credssp_auth_take_input_buffer(nla->auth, &nla->negoToken);
	const int rc = credssp_auth_authenticate(nla->auth);

	switch (rc)
	{
		case 0:
			if (!nla_send(nla))
				return -1;
			break;
		case 1: /* completed */
		{
			int res = -1;
			if (nla->peerVersion < 5)
				res = nla_encrypt_public_key_echo(nla);
			else
				res = nla_encrypt_public_key_hash(nla);

			if (!res)
				return -1;

			if (!nla_send(nla))
				return -1;

			nla_set_state(nla, NLA_STATE_PUB_KEY_AUTH);
		}
		break;

		default:
			return -1;
	}

	return 1;
}

static int nla_client_recv_pub_key_auth(rdpNla* nla)
{
	BOOL rc = FALSE;

	WINPR_ASSERT(nla);

	/* Verify Server Public Key Echo */
	if (nla->peerVersion < 5)
		rc = nla_decrypt_public_key_echo(nla);
	else
		rc = nla_decrypt_public_key_hash(nla);

	sspi_SecBufferFree(&nla->pubKeyAuth);

	if (!rc)
		return -1;

	/* Send encrypted credentials */
	rc = nla_encrypt_ts_credentials(nla);
	if (!rc)
		return -1;

	if (!nla_send(nla))
		return -1;

	if (nla->earlyUserAuth)
	{
		transport_set_early_user_auth_mode(nla->transport, TRUE);
		nla_set_state(nla, NLA_STATE_EARLY_USER_AUTH);
	}
	else
		nla_set_state(nla, NLA_STATE_AUTH_INFO);
	return 1;
}

static int nla_client_recv_early_user_auth(rdpNla* nla)
{
	WINPR_ASSERT(nla);

	transport_set_early_user_auth_mode(nla->transport, FALSE);
	nla_set_state(nla, NLA_STATE_AUTH_INFO);
	return 1;
}

static int nla_client_recv(rdpNla* nla)
{
	WINPR_ASSERT(nla);

	switch (nla_get_state(nla))
	{
		case NLA_STATE_NEGO_TOKEN:
			return nla_client_recv_nego_token(nla);

		case NLA_STATE_PUB_KEY_AUTH:
			return nla_client_recv_pub_key_auth(nla);

		case NLA_STATE_EARLY_USER_AUTH:
			return nla_client_recv_early_user_auth(nla);

		case NLA_STATE_FINAL:
		default:
			WLog_ERR(TAG, "NLA in invalid client receive state %s",
			         nla_get_state_str(nla_get_state(nla)));
			return -1;
	}
}

static int nla_client_authenticate(rdpNla* nla)
{
	int rc = -1;

	WINPR_ASSERT(nla);

	wStream* s = Stream_New(NULL, 4096);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return -1;
	}

	if (nla_client_begin(nla) < 1)
		goto fail;

	while (nla_get_state(nla) < NLA_STATE_AUTH_INFO)
	{
		Stream_SetPosition(s, 0);
		const int status = transport_read_pdu(nla->transport, s);

		if (status < 0)
		{
			WLog_ERR(TAG, "nla_client_authenticate failure");
			goto fail;
		}

		const int status2 = nla_recv_pdu(nla, s);

		if (status2 < 0)
			goto fail;
	}

	rc = 1;
fail:
	Stream_Free(s, TRUE);
	return rc;
}

/**
 * Initialize NTLMSSP authentication module (server).
 */

static int nla_server_init(rdpNla* nla)
{
	WINPR_ASSERT(nla);

	rdpTls* tls = transport_get_tls(nla->transport);
	WINPR_ASSERT(tls);

	if (!nla_sec_buffer_alloc_from_data(&nla->PublicKey, tls->PublicKey, 0, tls->PublicKeyLength))
	{
		WLog_ERR(TAG, "Failed to allocate SecBuffer for public key");
		return -1;
	}

	if (!credssp_auth_init(nla->auth, NLA_AUTH_PKG, NULL))
		return -1;

	if (!credssp_auth_setup_server(nla->auth))
		return -1;

	nla_set_state(nla, NLA_STATE_INITIAL);
	return 1;
}

static wStream* nla_server_recv_stream(rdpNla* nla)
{
	wStream* s = NULL;
	int status = -1;

	WINPR_ASSERT(nla);

	s = Stream_New(NULL, 4096);

	if (!s)
		goto fail;

	status = transport_read_pdu(nla->transport, s);

fail:
	if (status < 0)
	{
		WLog_ERR(TAG, "nla_recv() error: %d", status);
		Stream_Free(s, TRUE);
		return NULL;
	}

	return s;
}

static BOOL nla_server_recv_credentials(rdpNla* nla)
{
	WINPR_ASSERT(nla);

	if (nla_server_recv(nla) < 0)
		return FALSE;

	if (!nla_decrypt_ts_credentials(nla))
		return FALSE;

	if (!nla_impersonate(nla))
		return FALSE;

	if (!nla_revert_to_self(nla))
		return FALSE;

	return TRUE;
}

/**
 * Authenticate with client using CredSSP (server).
 * @param nla The NLA instance to use
 *
 * @return 1 if authentication is successful
 */

static int nla_server_authenticate(rdpNla* nla)
{
	int ret = -1;

	WINPR_ASSERT(nla);

	if (nla_server_init(nla) < 1)
		goto fail;

	/*
	 * from tspkg.dll: 0x00000112
	 * ASC_REQ_MUTUAL_AUTH
	 * ASC_REQ_CONFIDENTIALITY
	 * ASC_REQ_ALLOCATE_MEMORY
	 */
	credssp_auth_set_flags(nla->auth, ASC_REQ_MUTUAL_AUTH | ASC_REQ_CONFIDENTIALITY |
	                                      ASC_REQ_CONNECTION | ASC_REQ_USE_SESSION_KEY |
	                                      ASC_REQ_SEQUENCE_DETECT | ASC_REQ_EXTENDED_ERROR);

	/* Client is starting, here es the state machine:
	 *
	 *  -- NLA_STATE_INITIAL	--> NLA_STATE_INITIAL
	 * ----->> sending...
	 *    ----->> protocol version 6
	 *    ----->> nego token
	 *    ----->> client nonce
	 * <<----- receiving...
	 *    <<----- protocol version 6
	 *    <<----- nego token
	 * ----->> sending...
	 *    ----->> protocol version 6
	 *    ----->> nego token
	 *    ----->> public key auth
	 *    ----->> client nonce
	 * -- NLA_STATE_NEGO_TOKEN	--> NLA_STATE_PUB_KEY_AUTH
	 * <<----- receiving...
	 *    <<----- protocol version 6
	 *    <<----- public key info
	 * ----->> sending...
	 *    ----->> protocol version 6
	 *    ----->> auth info
	 *    ----->> client nonce
	 * -- NLA_STATE_PUB_KEY_AUTH	--> NLA_STATE
	 */

	while (TRUE)
	{
		int res = -1;

		if (nla_server_recv(nla) < 0)
			goto fail;

		WLog_DBG(TAG, "Receiving Authentication Token");
		credssp_auth_take_input_buffer(nla->auth, &nla->negoToken);

		res = credssp_auth_authenticate(nla->auth);

		if (res == -1)
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

			nla_send(nla);
			/* Access Denied */
			goto fail;
		}

		if (res == 1)
		{
			/* Process final part of the nego token exchange */
			if (credssp_auth_have_output_token(nla->auth))
			{
				if (!nla_send(nla))
					goto fail;

				if (nla_server_recv(nla) < 0)
					goto fail;

				WLog_DBG(TAG, "Receiving pubkey Token");
			}

			if (nla->peerVersion < 5)
				res = nla_decrypt_public_key_echo(nla);
			else
				res = nla_decrypt_public_key_hash(nla);

			if (!res)
				goto fail;

			/* Clear nego token buffer or we will send it again to the client */
			sspi_SecBufferFree(&nla->negoToken);

			if (nla->peerVersion < 5)
				res = nla_encrypt_public_key_echo(nla);
			else
				res = nla_encrypt_public_key_hash(nla);

			if (!res)
				goto fail;
		}

		/* send authentication token */
		WLog_DBG(TAG, "Sending Authentication Token");

		if (!nla_send(nla))
			goto fail;

		if (res == 1)
		{
			ret = 1;
			break;
		}
	}

	/* Receive encrypted credentials */
	if (!nla_server_recv_credentials(nla))
		ret = -1;

fail:
	nla_buffer_free(nla);
	return ret;
}

/**
 * Authenticate using CredSSP.
 * @param nla The NLA instance to use
 *
 * @return 1 if authentication is successful
 */

int nla_authenticate(rdpNla* nla)
{
	WINPR_ASSERT(nla);

	if (nla->server)
		return nla_server_authenticate(nla);
	else
		return nla_client_authenticate(nla);
}

static void ap_integer_increment_le(BYTE* number, size_t size)
{
	WINPR_ASSERT(number || (size == 0));

	for (size_t index = 0; index < size; index++)
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

static void ap_integer_decrement_le(BYTE* number, size_t size)
{
	WINPR_ASSERT(number || (size == 0));

	for (size_t index = 0; index < size; index++)
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

BOOL nla_encrypt_public_key_echo(rdpNla* nla)
{
	BOOL status = FALSE;

	WINPR_ASSERT(nla);

	sspi_SecBufferFree(&nla->pubKeyAuth);
	if (nla->server)
	{
		SecBuffer buf = { 0 };
		if (!sspi_SecBufferAlloc(&buf, nla->PublicKey.cbBuffer))
			return FALSE;
		ap_integer_increment_le(buf.pvBuffer, buf.cbBuffer);
		status = credssp_auth_encrypt(nla->auth, &buf, &nla->pubKeyAuth, NULL, nla->sendSeqNum++);
		sspi_SecBufferFree(&buf);
	}
	else
	{
		status = credssp_auth_encrypt(nla->auth, &nla->PublicKey, &nla->pubKeyAuth, NULL,
		                              nla->sendSeqNum++);
	}

	return status;
}

BOOL nla_encrypt_public_key_hash(rdpNla* nla)
{
	BOOL status = FALSE;
	WINPR_DIGEST_CTX* sha256 = NULL;
	SecBuffer buf = { 0 };

	WINPR_ASSERT(nla);

	const BYTE* hashMagic = nla->server ? ServerClientHashMagic : ClientServerHashMagic;
	const size_t hashSize =
	    nla->server ? sizeof(ServerClientHashMagic) : sizeof(ClientServerHashMagic);

	if (!sspi_SecBufferAlloc(&buf, WINPR_SHA256_DIGEST_LENGTH))
		return FALSE;

	/* generate SHA256 of following data: ClientServerHashMagic, Nonce, SubjectPublicKey */
	if (!(sha256 = winpr_Digest_New()))
		goto out;

	if (!winpr_Digest_Init(sha256, WINPR_MD_SHA256))
		goto out;

	/* include trailing \0 from hashMagic */
	if (!winpr_Digest_Update(sha256, hashMagic, hashSize))
		goto out;

	if (!nla_Digest_Update_From_SecBuffer(sha256, &nla->ClientNonce))
		goto out;

	/* SubjectPublicKey */
	if (!nla_Digest_Update_From_SecBuffer(sha256, &nla->PublicKey))
		goto out;

	if (!winpr_Digest_Final(sha256, buf.pvBuffer, WINPR_SHA256_DIGEST_LENGTH))
		goto out;

	sspi_SecBufferFree(&nla->pubKeyAuth);
	if (!credssp_auth_encrypt(nla->auth, &buf, &nla->pubKeyAuth, NULL, nla->sendSeqNum++))
		goto out;

	status = TRUE;

out:
	winpr_Digest_Free(sha256);
	sspi_SecBufferFree(&buf);
	return status;
}

BOOL nla_decrypt_public_key_echo(rdpNla* nla)
{
	BOOL status = FALSE;
	SecBuffer public_key = { 0 };

	if (!nla)
		goto fail;

	if (!credssp_auth_decrypt(nla->auth, &nla->pubKeyAuth, &public_key, nla->recvSeqNum++))
		return FALSE;

	if (!nla->server)
	{
		/* server echos the public key +1 */
		ap_integer_decrement_le(public_key.pvBuffer, public_key.cbBuffer);
	}

	if (public_key.cbBuffer != nla->PublicKey.cbBuffer ||
	    memcmp(public_key.pvBuffer, nla->PublicKey.pvBuffer, public_key.cbBuffer) != 0)
	{
		WLog_ERR(TAG, "Could not verify server's public key echo");
#if defined(WITH_DEBUG_NLA)
		WLog_ERR(TAG, "Expected (length = %" PRIu32 "):", nla->PublicKey.cbBuffer);
		winpr_HexDump(TAG, WLOG_ERROR, nla->PublicKey.pvBuffer, nla->PublicKey.cbBuffer);
		WLog_ERR(TAG, "Actual (length = %" PRIu32 "):", public_key.cbBuffer);
		winpr_HexDump(TAG, WLOG_ERROR, public_key.pvBuffer, public_key.cbBuffer);
#endif
		/* DO NOT SEND CREDENTIALS! */
		goto fail;
	}

	status = TRUE;
fail:
	sspi_SecBufferFree(&public_key);
	return status;
}

BOOL nla_decrypt_public_key_hash(rdpNla* nla)
{
	WINPR_DIGEST_CTX* sha256 = NULL;
	BYTE serverClientHash[WINPR_SHA256_DIGEST_LENGTH] = { 0 };
	BOOL status = FALSE;

	WINPR_ASSERT(nla);

	const BYTE* hashMagic = nla->server ? ClientServerHashMagic : ServerClientHashMagic;
	const size_t hashSize =
	    nla->server ? sizeof(ClientServerHashMagic) : sizeof(ServerClientHashMagic);
	SecBuffer hash = { 0 };

	if (!credssp_auth_decrypt(nla->auth, &nla->pubKeyAuth, &hash, nla->recvSeqNum++))
		return FALSE;

	/* generate SHA256 of following data: ServerClientHashMagic, Nonce, SubjectPublicKey */
	if (!(sha256 = winpr_Digest_New()))
		goto fail;

	if (!winpr_Digest_Init(sha256, WINPR_MD_SHA256))
		goto fail;

	/* include trailing \0 from hashMagic */
	if (!winpr_Digest_Update(sha256, hashMagic, hashSize))
		goto fail;

	if (!nla_Digest_Update_From_SecBuffer(sha256, &nla->ClientNonce))
		goto fail;

	/* SubjectPublicKey */
	if (!nla_Digest_Update_From_SecBuffer(sha256, &nla->PublicKey))
		goto fail;

	if (!winpr_Digest_Final(sha256, serverClientHash, sizeof(serverClientHash)))
		goto fail;

	/* verify hash */
	if (hash.cbBuffer != WINPR_SHA256_DIGEST_LENGTH ||
	    memcmp(serverClientHash, hash.pvBuffer, WINPR_SHA256_DIGEST_LENGTH) != 0)
	{
		WLog_ERR(TAG, "Could not verify server's hash");
		/* DO NOT SEND CREDENTIALS! */
		goto fail;
	}

	status = TRUE;
fail:
	winpr_Digest_Free(sha256);
	sspi_SecBufferFree(&hash);
	return status;
}

static BOOL set_creds_octetstring_to_settings(WinPrAsn1Decoder* dec, WinPrAsn1_tagId tagId,
                                              BOOL optional, FreeRDP_Settings_Keys_String settingId,
                                              rdpSettings* settings)
{
	if (optional)
	{
		WinPrAsn1_tagId itemTag;
		if (!WinPrAsn1DecPeekTag(dec, &itemTag) || (itemTag != tagId))
			return TRUE;
	}

	BOOL error = FALSE;
	WinPrAsn1_OctetString value;
	/* note: not checking "error" value, as the not present optional item case is handled above
	 *       if the function fails it's because of a real error not because the item is not present
	 */
	if (!WinPrAsn1DecReadContextualOctetString(dec, tagId, &error, &value, FALSE))
		return FALSE;

	return freerdp_settings_set_string_from_utf16N(settings, settingId, (const WCHAR*)value.data,
	                                               value.len / sizeof(WCHAR));
}

static BOOL nla_read_TSCspDataDetail(WinPrAsn1Decoder* dec, rdpSettings* settings)
{
	BOOL error = FALSE;

	/* keySpec [0] INTEGER */
	WinPrAsn1_INTEGER keyspec;
	if (!WinPrAsn1DecReadContextualInteger(dec, 0, &error, &keyspec))
		return FALSE;
	settings->KeySpec = (UINT32)keyspec;

	/* cardName [1] OCTET STRING OPTIONAL */
	if (!set_creds_octetstring_to_settings(dec, 1, TRUE, FreeRDP_CardName, settings))
		return FALSE;

	/* readerName [2] OCTET STRING OPTIONAL */
	if (!set_creds_octetstring_to_settings(dec, 2, TRUE, FreeRDP_ReaderName, settings))
		return FALSE;

	/* containerName [3] OCTET STRING OPTIONAL */
	if (!set_creds_octetstring_to_settings(dec, 3, TRUE, FreeRDP_ContainerName, settings))
		return FALSE;

	/* cspName [4] OCTET STRING OPTIONAL */
	return set_creds_octetstring_to_settings(dec, 4, TRUE, FreeRDP_CspName, settings);
}

static BOOL nla_read_KERB_TICKET_LOGON(rdpNla* nla, wStream* s, KERB_TICKET_LOGON* ticket)
{
	/* mysterious extra 16 bytes before TGS/TGT content */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16 + 16))
		return FALSE;

	Stream_Read_UINT32(s, ticket->MessageType);
	Stream_Read_UINT32(s, ticket->Flags);
	Stream_Read_UINT32(s, ticket->ServiceTicketLength);
	Stream_Read_UINT32(s, ticket->TicketGrantingTicketLength);

	if (ticket->MessageType != KerbTicketLogon)
	{
		WLog_ERR(TAG, "Not a KerbTicketLogon");
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLength(
	        TAG, s, 16ull + ticket->ServiceTicketLength + ticket->TicketGrantingTicketLength))
		return FALSE;

	/* mysterious 16 bytes in the way, maybe they would need to be interpreted... */
	Stream_Seek(s, 16);

	/*WLog_INFO(TAG, "TGS");
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_PointerAs(s, const BYTE), ticket->ServiceTicketLength);*/
	ticket->ServiceTicket = Stream_PointerAs(s, UCHAR);
	Stream_Seek(s, ticket->ServiceTicketLength);

	/*WLog_INFO(TAG, "TGT");
	winpr_HexDump(TAG, WLOG_DEBUG, Stream_PointerAs(s, const BYTE),
	              ticket->TicketGrantingTicketLength);*/
	ticket->TicketGrantingTicket = Stream_PointerAs(s, UCHAR);
	return TRUE;
}

/** @brief kind of RCG credentials */
typedef enum
{
	RCG_TYPE_KERB,
	RCG_TYPE_NTLM
} RemoteGuardPackageCredType;

static BOOL nla_read_TSRemoteGuardPackageCred(rdpNla* nla, WinPrAsn1Decoder* dec,
                                              RemoteGuardPackageCredType* credsType,
                                              wStream* payload)
{
	WinPrAsn1_OctetString packageName = { 0 };
	WinPrAsn1_OctetString credBuffer = { 0 };
	BOOL error = FALSE;
	char packageNameStr[100] = { 0 };

	/* packageName [0] OCTET STRING */
	if (!WinPrAsn1DecReadContextualOctetString(dec, 0, &error, &packageName, FALSE) || error)
		return TRUE;

	ConvertMszWCharNToUtf8((WCHAR*)packageName.data, packageName.len / sizeof(WCHAR),
	                       packageNameStr, 100);
	WLog_DBG(TAG, "TSRemoteGuardPackageCred(%s)", packageNameStr);

	/* credBuffer [1] OCTET STRING, */
	if (!WinPrAsn1DecReadContextualOctetString(dec, 1, &error, &credBuffer, FALSE) || error)
		return TRUE;

	if (_stricmp(packageNameStr, "Kerberos") == 0)
	{
		*credsType = RCG_TYPE_KERB;
	}
	else if (_stricmp(packageNameStr, "NTLM") == 0)
	{
		*credsType = RCG_TYPE_NTLM;
	}
	else
	{
		WLog_INFO(TAG, "TSRemoteGuardPackageCred package %s not handled", packageNameStr);
		return FALSE;
	}

	Stream_StaticInit(payload, credBuffer.data, credBuffer.len);
	return TRUE;
}

/** @brief kind of TSCreds */
typedef enum
{
	TSCREDS_USER_PASSWD = 1,
	TSCREDS_SMARTCARD = 2,
	TSCREDS_REMOTEGUARD = 6
} TsCredentialsType;

static BOOL nla_read_ts_credentials(rdpNla* nla, SecBuffer* data)
{
	WinPrAsn1Decoder dec = { 0 };
	WinPrAsn1Decoder dec2 = { 0 };
	WinPrAsn1_OctetString credentials = { 0 };
	BOOL error = FALSE;
	WinPrAsn1_INTEGER credType = -1;
	BOOL ret = true;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(data);

	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, (BYTE*)data->pvBuffer, data->cbBuffer);

	/* TSCredentials */
	if (!WinPrAsn1DecReadSequence(&dec, &dec2))
		return FALSE;
	dec = dec2;

	/* credType [0] INTEGER */
	if (!WinPrAsn1DecReadContextualInteger(&dec, 0, &error, &credType))
		return FALSE;

	/* credentials [1] OCTET STRING */
	if (!WinPrAsn1DecReadContextualOctetString(&dec, 1, &error, &credentials, FALSE))
		return FALSE;

	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, credentials.data, credentials.len);

	rdpSettings* settings = nla->rdpcontext->settings;
	if (nego_get_remoteCredentialGuard(nla->rdpcontext->rdp->nego) &&
	    credType != TSCREDS_REMOTEGUARD)
	{
		WLog_ERR(TAG, "connecting with RCG but it's not TSRemoteGuard credentials");
		return FALSE;
	}

	switch (credType)
	{
		case TSCREDS_USER_PASSWD:
		{
			/* TSPasswordCreds */
			if (!WinPrAsn1DecReadSequence(&dec, &dec2))
				return FALSE;
			dec = dec2;

			/* domainName [0] OCTET STRING */
			if (!set_creds_octetstring_to_settings(&dec, 0, FALSE, FreeRDP_Domain, settings))
				return FALSE;

			/* userName [1] OCTET STRING */
			if (!set_creds_octetstring_to_settings(&dec, 1, FALSE, FreeRDP_Username, settings))
				return FALSE;

			/* password [2] OCTET STRING */
			return set_creds_octetstring_to_settings(&dec, 2, FALSE, FreeRDP_Password, settings);
		}
		case TSCREDS_SMARTCARD:
		{
			/* TSSmartCardCreds */
			if (!WinPrAsn1DecReadSequence(&dec, &dec2))
				return FALSE;
			dec = dec2;

			/* pin [0] OCTET STRING, */
			if (!set_creds_octetstring_to_settings(&dec, 0, FALSE, FreeRDP_Password, settings))
				return FALSE;
			settings->PasswordIsSmartcardPin = TRUE;

			/* cspData [1] TSCspDataDetail */
			WinPrAsn1Decoder cspDetails = { 0 };
			if (!WinPrAsn1DecReadContextualSequence(&dec, 1, &error, &cspDetails) && error)
				return FALSE;
			if (!nla_read_TSCspDataDetail(&cspDetails, settings))
				return FALSE;

			/* userHint [2] OCTET STRING OPTIONAL */
			if (!set_creds_octetstring_to_settings(&dec, 2, TRUE, FreeRDP_Username, settings))
				return FALSE;

			/* domainHint [3] OCTET STRING OPTIONAL */
			return set_creds_octetstring_to_settings(&dec, 3, TRUE, FreeRDP_Domain, settings);
		}
		case TSCREDS_REMOTEGUARD:
		{
			/* TSRemoteGuardCreds */
			if (!WinPrAsn1DecReadSequence(&dec, &dec2))
				return FALSE;

			/* logonCred[0] TSRemoteGuardPackageCred */
			KERB_TICKET_LOGON kerbLogon = { 0 };
			WinPrAsn1Decoder logonCredsSeq = { 0 };
			if (!WinPrAsn1DecReadContextualSequence(&dec2, 0, &error, &logonCredsSeq) || error)
				return FALSE;

			RemoteGuardPackageCredType logonCredsType = { 0 };
			wStream logonPayload = { 0 };
			if (!nla_read_TSRemoteGuardPackageCred(nla, &logonCredsSeq, &logonCredsType,
			                                       &logonPayload))
				return FALSE;
			if (logonCredsType != RCG_TYPE_KERB)
			{
				WLog_ERR(TAG, "logonCred must be some Kerberos creds");
				return FALSE;
			}

			if (!nla_read_KERB_TICKET_LOGON(nla, &logonPayload, &kerbLogon))
			{
				WLog_ERR(TAG, "invalid KERB_TICKET_LOGON");
				return FALSE;
			}

			/* supplementalCreds [1] SEQUENCE OF TSRemoteGuardPackageCred OPTIONAL, */
			MSV1_0_SUPPLEMENTAL_CREDENTIAL ntlmCreds = { 0 };
			MSV1_0_SUPPLEMENTAL_CREDENTIAL* suppCreds = NULL;
			WinPrAsn1Decoder suppCredsSeq = { 0 };

			if (WinPrAsn1DecReadContextualSequence(&dec2, 1, &error, &suppCredsSeq))
			{
				WinPrAsn1Decoder ntlmCredsSeq = { 0 };
				if (!WinPrAsn1DecReadSequence(&suppCredsSeq, &ntlmCredsSeq))
					return FALSE;

				RemoteGuardPackageCredType suppCredsType = { 0 };
				wStream ntlmPayload = { 0 };
				if (!nla_read_TSRemoteGuardPackageCred(nla, &ntlmCredsSeq, &suppCredsType,
				                                       &ntlmPayload))
					return FALSE;

				if (suppCredsType != RCG_TYPE_NTLM)
				{
					WLog_ERR(TAG, "supplementalCreds must be some NTLM creds");
					return FALSE;
				}

				/* TODO: suppCreds = &ntlmCreds; and parse NTLM creds */
			}
			else if (error)
			{
				WLog_ERR(TAG, "invalid supplementalCreds");
				return FALSE;
			}

			freerdp_peer* peer = nla->rdpcontext->peer;
			ret = IFCALLRESULT(TRUE, peer->RemoteCredentials, peer, &kerbLogon, suppCreds);
			break;
		}
		default:
			WLog_DBG(TAG, "TSCredentials type " PRIu32 " not supported for now", credType);
			ret = FALSE;
			break;
	}

	return ret;
}

/**
 * Encode TSCredentials structure.
 * @param nla A pointer to the NLA to use
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

static BOOL nla_encode_ts_credentials(rdpNla* nla)
{
	BOOL ret = FALSE;
	WinPrAsn1Encoder* enc = NULL;
	size_t length = 0;
	wStream s = { 0 };
	TsCredentialsType credType;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	rdpSettings* settings = nla->rdpcontext->settings;
	WINPR_ASSERT(settings);

	if (settings->RemoteCredentialGuard)
		credType = TSCREDS_REMOTEGUARD;
	else if (settings->SmartcardLogon)
		credType = TSCREDS_SMARTCARD;
	else
		credType = TSCREDS_USER_PASSWD;

	enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return FALSE;

	/* TSCredentials */
	if (!WinPrAsn1EncSeqContainer(enc))
		goto out;

	/* credType [0] INTEGER */
	if (!WinPrAsn1EncContextualInteger(enc, 0, (WinPrAsn1_INTEGER)credType))
		goto out;

	/* credentials [1] OCTET STRING */
	if (!WinPrAsn1EncContextualOctetStringContainer(enc, 1))
		goto out;

	switch (credType)
	{
		case TSCREDS_SMARTCARD:
		{
			struct
			{
				WinPrAsn1_tagId tag;
				size_t setting_id;
			} cspData_fields[] = { { 1, FreeRDP_CardName },
				                   { 2, FreeRDP_ReaderName },
				                   { 3, FreeRDP_ContainerName },
				                   { 4, FreeRDP_CspName } };
			WinPrAsn1_OctetString octet_string = { 0 };

			/* TSSmartCardCreds */
			if (!WinPrAsn1EncSeqContainer(enc))
				goto out;

			/* pin [0] OCTET STRING */
			size_t ss;
			octet_string.data =
			    (BYTE*)freerdp_settings_get_string_as_utf16(settings, FreeRDP_Password, &ss);
			octet_string.len = ss * sizeof(WCHAR);
			const BOOL res = WinPrAsn1EncContextualOctetString(enc, 0, &octet_string) > 0;
			free(octet_string.data);
			if (!res)
				goto out;

			/* cspData [1] SEQUENCE */
			if (!WinPrAsn1EncContextualSeqContainer(enc, 1))
				goto out;

			/* keySpec [0] INTEGER */
			if (!WinPrAsn1EncContextualInteger(
			        enc, 0, freerdp_settings_get_uint32(settings, FreeRDP_KeySpec)))
				goto out;

			for (size_t i = 0; i < ARRAYSIZE(cspData_fields); i++)
			{
				size_t len;

				octet_string.data = (BYTE*)freerdp_settings_get_string_as_utf16(
				    settings, cspData_fields[i].setting_id, &len);
				octet_string.len = len * sizeof(WCHAR);
				if (octet_string.len)
				{
					const BOOL res2 = WinPrAsn1EncContextualOctetString(enc, cspData_fields[i].tag,
					                                                    &octet_string) > 0;
					free(octet_string.data);
					if (!res2)
						goto out;
				}
			}

			/* End cspData */
			if (!WinPrAsn1EncEndContainer(enc))
				goto out;

			/* End TSSmartCardCreds */
			if (!WinPrAsn1EncEndContainer(enc))
				goto out;
			break;
		}
		case TSCREDS_USER_PASSWD:
		{
			WinPrAsn1_OctetString username = { 0 };
			WinPrAsn1_OctetString domain = { 0 };
			WinPrAsn1_OctetString password = { 0 };

			/* TSPasswordCreds */
			if (!WinPrAsn1EncSeqContainer(enc))
				goto out;

			if (!settings->DisableCredentialsDelegation && nla->identity)
			{
				username.len = nla->identity->UserLength * sizeof(WCHAR);
				username.data = (BYTE*)nla->identity->User;

				domain.len = nla->identity->DomainLength * sizeof(WCHAR);
				domain.data = (BYTE*)nla->identity->Domain;

				password.len = nla->identity->PasswordLength * sizeof(WCHAR);
				password.data = (BYTE*)nla->identity->Password;
			}

			if (WinPrAsn1EncContextualOctetString(enc, 0, &domain) == 0)
				goto out;
			if (WinPrAsn1EncContextualOctetString(enc, 1, &username) == 0)
				goto out;
			if (WinPrAsn1EncContextualOctetString(enc, 2, &password) == 0)
				goto out;

			/* End TSPasswordCreds */
			if (!WinPrAsn1EncEndContainer(enc))
				goto out;
			break;
		}
		case TSCREDS_REMOTEGUARD:
		default:
			goto out;
	}

	/* End credentials | End TSCredentials */
	if (!WinPrAsn1EncEndContainer(enc) || !WinPrAsn1EncEndContainer(enc))
		goto out;

	if (!WinPrAsn1EncStreamSize(enc, &length))
		goto out;

	if (!nla_sec_buffer_alloc(&nla->tsCredentials, length))
	{
		WLog_ERR(TAG, "sspi_SecBufferAlloc failed!");
		goto out;
	}

	Stream_StaticInit(&s, (BYTE*)nla->tsCredentials.pvBuffer, length);

	ret = WinPrAsn1EncToStream(enc, &s);

out:
	WinPrAsn1Encoder_Free(&enc);
	return ret;
}

static BOOL nla_encrypt_ts_credentials(rdpNla* nla)
{
	WINPR_ASSERT(nla);

	if (!nla_encode_ts_credentials(nla))
		return FALSE;

	sspi_SecBufferFree(&nla->authInfo);
	if (!credssp_auth_encrypt(nla->auth, &nla->tsCredentials, &nla->authInfo, NULL,
	                          nla->sendSeqNum++))
		return FALSE;

	return TRUE;
}

static BOOL nla_decrypt_ts_credentials(rdpNla* nla)
{
	WINPR_ASSERT(nla);

	if (nla->authInfo.cbBuffer < 1)
	{
		WLog_ERR(TAG, "nla_decrypt_ts_credentials missing authInfo buffer");
		return FALSE;
	}

	sspi_SecBufferFree(&nla->tsCredentials);
	if (!credssp_auth_decrypt(nla->auth, &nla->authInfo, &nla->tsCredentials, nla->recvSeqNum++))
		return FALSE;

	if (!nla_read_ts_credentials(nla, &nla->tsCredentials))
		return FALSE;

	return TRUE;
}

static BOOL nla_write_octet_string(WinPrAsn1Encoder* enc, const SecBuffer* buffer,
                                   WinPrAsn1_tagId tagId, const char* msg)
{
	BOOL res = FALSE;

	WINPR_ASSERT(enc);
	WINPR_ASSERT(buffer);
	WINPR_ASSERT(msg);

	if (buffer->cbBuffer > 0)
	{
		size_t rc = 0;
		WinPrAsn1_OctetString octet_string = { 0 };

		WLog_DBG(TAG, "   ----->> %s", msg);
		octet_string.data = buffer->pvBuffer;
		octet_string.len = buffer->cbBuffer;
		rc = WinPrAsn1EncContextualOctetString(enc, tagId, &octet_string);
		if (rc != 0)
			res = TRUE;
	}

	return res;
}

static BOOL nla_write_octet_string_free(WinPrAsn1Encoder* enc, SecBuffer* buffer,
                                        WinPrAsn1_tagId tagId, const char* msg)
{
	const BOOL rc = nla_write_octet_string(enc, buffer, tagId, msg);
	sspi_SecBufferFree(buffer);
	return rc;
}

/**
 * Send CredSSP message.
 *
 * @param nla A pointer to the NLA to use
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL nla_send(rdpNla* nla)
{
	BOOL rc = FALSE;
	wStream* s = NULL;
	size_t length = 0;
	WinPrAsn1Encoder* enc = NULL;

	WINPR_ASSERT(nla);

	enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return FALSE;

	/* TSRequest */
	WLog_DBG(TAG, "----->> sending...");
	if (!WinPrAsn1EncSeqContainer(enc))
		goto fail;

	/* version [0] INTEGER */
	WLog_DBG(TAG, "   ----->> protocol version %" PRIu32, nla->version);
	if (!WinPrAsn1EncContextualInteger(enc, 0, nla->version))
		goto fail;

	/* negoTokens [1] SEQUENCE OF SEQUENCE */
	if (nla_get_state(nla) <= NLA_STATE_NEGO_TOKEN && credssp_auth_have_output_token(nla->auth))
	{
		const SecBuffer* buffer = credssp_auth_get_output_buffer(nla->auth);

		if (!WinPrAsn1EncContextualSeqContainer(enc, 1) || !WinPrAsn1EncSeqContainer(enc))
			goto fail;

		/* negoToken [0] OCTET STRING */
		if (!nla_write_octet_string(enc, buffer, 0, "negoToken"))
			goto fail;

		/* End negoTokens (SEQUENCE OF SEQUENCE) */
		if (!WinPrAsn1EncEndContainer(enc) || !WinPrAsn1EncEndContainer(enc))
			goto fail;
	}

	/* authInfo [2] OCTET STRING */
	if (nla->authInfo.cbBuffer > 0)
	{
		if (!nla_write_octet_string_free(enc, &nla->authInfo, 2, "auth info"))
			goto fail;
	}

	/* pubKeyAuth [3] OCTET STRING */
	if (nla->pubKeyAuth.cbBuffer > 0)
	{
		if (!nla_write_octet_string_free(enc, &nla->pubKeyAuth, 3, "public key auth"))
			goto fail;
	}

	/* errorCode [4] INTEGER */
	if (nla->errorCode && nla->peerVersion >= 3 && nla->peerVersion != 5)
	{
		WLog_DBG(TAG, "   ----->> error code %s 0x%08" PRIx32, NtStatus2Tag(nla->errorCode),
		         nla->errorCode);
		if (!WinPrAsn1EncContextualInteger(enc, 4, nla->errorCode))
			goto fail;
	}

	/* clientNonce [5] OCTET STRING */
	if (!nla->server && nla->ClientNonce.cbBuffer > 0)
	{
		if (!nla_write_octet_string(enc, &nla->ClientNonce, 5, "client nonce"))
			goto fail;
	}

	/* End TSRequest */
	if (!WinPrAsn1EncEndContainer(enc))
		goto fail;

	if (!WinPrAsn1EncStreamSize(enc, &length))
		goto fail;

	s = Stream_New(NULL, length);
	if (!s)
		goto fail;

	if (!WinPrAsn1EncToStream(enc, s))
		goto fail;

	WLog_DBG(TAG, "[%" PRIuz " bytes]", length);
	if (transport_write(nla->transport, s) < 0)
		goto fail;
	rc = TRUE;

fail:
	Stream_Free(s, TRUE);
	WinPrAsn1Encoder_Free(&enc);
	return rc;
}

static int nla_decode_ts_request(rdpNla* nla, wStream* s)
{
	WinPrAsn1Decoder dec = { 0 };
	WinPrAsn1Decoder dec2 = { 0 };
	BOOL error = FALSE;
	WinPrAsn1_tagId tag = { 0 };
	WinPrAsn1_INTEGER val = { 0 };
	UINT32 version = 0;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(s);

	WinPrAsn1Decoder_Init(&dec, WINPR_ASN1_DER, s);

	WLog_DBG(TAG, "<<----- receiving...");

	/* TSRequest */
	const size_t offset = WinPrAsn1DecReadSequence(&dec, &dec2);
	if (offset == 0)
		return -1;
	dec = dec2;

	/* version [0] INTEGER */
	if (WinPrAsn1DecReadContextualInteger(&dec, 0, &error, &val) == 0)
		return -1;

	if (!Stream_SafeSeek(s, offset))
		return -1;

	version = (UINT)val;
	WLog_DBG(TAG, "   <<----- protocol version %" PRIu32, version);

	if (nla->peerVersion == 0)
		nla->peerVersion = version;

	/* if the peer suddenly changed its version - kick it */
	if (nla->peerVersion != version)
	{
		WLog_ERR(TAG, "CredSSP peer changed protocol version from %" PRIu32 " to %" PRIu32,
		         nla->peerVersion, version);
		return -1;
	}

	while (WinPrAsn1DecReadContextualTag(&dec, &tag, &dec2) != 0)
	{
		WinPrAsn1Decoder dec3 = { 0 };
		WinPrAsn1_OctetString octet_string = { 0 };

		switch (tag)
		{
			case 1:
				WLog_DBG(TAG, "   <<----- nego token");
				/* negoTokens [1] SEQUENCE OF SEQUENCE */
				if ((WinPrAsn1DecReadSequence(&dec2, &dec3) == 0) ||
				    (WinPrAsn1DecReadSequence(&dec3, &dec2) == 0))
					return -1;
				/* negoToken [0] OCTET STRING */
				if ((WinPrAsn1DecReadContextualOctetString(&dec2, 0, &error, &octet_string,
				                                           FALSE) == 0) &&
				    error)
					return -1;
				if (!nla_sec_buffer_alloc_from_data(&nla->negoToken, octet_string.data, 0,
				                                    octet_string.len))
					return -1;
				break;
			case 2:
				WLog_DBG(TAG, "   <<----- auth info");
				/* authInfo [2] OCTET STRING */
				if (WinPrAsn1DecReadOctetString(&dec2, &octet_string, FALSE) == 0)
					return -1;
				if (!nla_sec_buffer_alloc_from_data(&nla->authInfo, octet_string.data, 0,
				                                    octet_string.len))
					return -1;
				break;
			case 3:
				WLog_DBG(TAG, "   <<----- public key auth");
				/* pubKeyAuth [3] OCTET STRING */
				if (WinPrAsn1DecReadOctetString(&dec2, &octet_string, FALSE) == 0)
					return -1;
				if (!nla_sec_buffer_alloc_from_data(&nla->pubKeyAuth, octet_string.data, 0,
				                                    octet_string.len))
					return -1;
				break;
			case 4:
				/* errorCode [4] INTEGER */
				if (WinPrAsn1DecReadInteger(&dec2, &val) == 0)
					return -1;
				nla->errorCode = (UINT)val;
				WLog_DBG(TAG, "   <<----- error code %s 0x%08" PRIx32, NtStatus2Tag(nla->errorCode),
				         nla->errorCode);
				break;
			case 5:
				WLog_DBG(TAG, "   <<----- client nonce");
				/* clientNonce [5] OCTET STRING */
				if (WinPrAsn1DecReadOctetString(&dec2, &octet_string, FALSE) == 0)
					return -1;
				if (!nla_sec_buffer_alloc_from_data(&nla->ClientNonce, octet_string.data, 0,
				                                    octet_string.len))
					return -1;
				break;
			default:
				return -1;
		}
	}

	return 1;
}

int nla_recv_pdu(rdpNla* nla, wStream* s)
{
	WINPR_ASSERT(nla);
	WINPR_ASSERT(s);

	if (nla_get_state(nla) == NLA_STATE_EARLY_USER_AUTH)
	{
		UINT32 code;
		Stream_Read_UINT32(s, code);
		if (code != AUTHZ_SUCCESS)
		{
			WLog_DBG(TAG, "Early User Auth active: FAILURE code 0x%08" PRIX32 "", code);
			code = FREERDP_ERROR_AUTHENTICATION_FAILED;
			freerdp_set_last_error_log(nla->rdpcontext, code);
			return -1;
		}
		else
			WLog_DBG(TAG, "Early User Auth active: SUCCESS");
	}
	else
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
					         NtStatus2Tag(nla->errorCode), nla->errorCode);
					code = FREERDP_ERROR_AUTHENTICATION_FAILED;
					break;
			}

			freerdp_set_last_error_log(nla->rdpcontext, code);
			return -1;
		}
	}

	return nla_client_recv(nla);
}

int nla_server_recv(rdpNla* nla)
{
	int status = -1;

	WINPR_ASSERT(nla);

	wStream* s = nla_server_recv_stream(nla);
	if (!s)
		goto fail;
	status = nla_decode_ts_request(nla, s);

fail:
	Stream_Free(s, TRUE);
	return status;
}

/**
 * Create new CredSSP state machine.
 *
 * @param context A pointer to the rdp context to use
 * @param transport A pointer to the transport to use
 *
 * @return new CredSSP state machine.
 */

rdpNla* nla_new(rdpContext* context, rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	WINPR_ASSERT(context);

	rdpSettings* settings = context->settings;
	WINPR_ASSERT(settings);

	rdpNla* nla = (rdpNla*)calloc(1, sizeof(rdpNla));

	if (!nla)
		return NULL;

	nla->rdpcontext = context;
	nla->server = settings->ServerMode;
	nla->transport = transport;
	nla->sendSeqNum = 0;
	nla->recvSeqNum = 0;
	nla->version = 6;
	nla->earlyUserAuth = FALSE;

	nla->identity = calloc(1, sizeof(SEC_WINNT_AUTH_IDENTITY));
	if (!nla->identity)
		goto cleanup;

	nla->auth = credssp_auth_new(context);
	if (!nla->auth)
		goto cleanup;

	/* init to 0 or we end up freeing a bad pointer if the alloc fails */
	if (!nla_sec_buffer_alloc(&nla->ClientNonce, NonceLength))
		goto cleanup;

	/* generate random 32-byte nonce */
	if (winpr_RAND(nla->ClientNonce.pvBuffer, NonceLength) < 0)
		goto cleanup;

	return nla;
cleanup:
	credssp_auth_free(nla->auth);
	free(nla->identity);
	nla_free(nla);
	return NULL;
}

/**
 * Free CredSSP state machine.
 * @param nla The NLA instance to free
 */

void nla_free(rdpNla* nla)
{
	if (!nla)
		return;

	smartcardCertInfo_Free(nla->smartcardCert);
	nla_buffer_free(nla);
	sspi_SecBufferFree(&nla->tsCredentials);
	credssp_auth_free(nla->auth);

	sspi_FreeAuthIdentity(nla->identity);
	free(nla->pkinitArgs);
	free(nla->identity);
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

	WLog_DBG(TAG, "-- %s\t--> %s", nla_get_state_str(nla->state), nla_get_state_str(state));
	nla->state = state;
	return TRUE;
}

BOOL nla_set_service_principal(rdpNla* nla, const char* service, const char* hostname)
{
	if (!credssp_auth_set_spn(nla->auth, service, hostname))
		return FALSE;
	return TRUE;
}

BOOL nla_impersonate(rdpNla* nla)
{
	return credssp_auth_impersonate(nla->auth);
}

BOOL nla_revert_to_self(rdpNla* nla)
{
	return credssp_auth_revert_to_self(nla->auth);
}

const char* nla_get_state_str(NLA_STATE state)
{
	switch (state)
	{
		case NLA_STATE_INITIAL:
			return "NLA_STATE_INITIAL";
		case NLA_STATE_NEGO_TOKEN:
			return "NLA_STATE_NEGO_TOKEN";
		case NLA_STATE_PUB_KEY_AUTH:
			return "NLA_STATE_PUB_KEY_AUTH";
		case NLA_STATE_AUTH_INFO:
			return "NLA_STATE_AUTH_INFO";
		case NLA_STATE_POST_NEGO:
			return "NLA_STATE_POST_NEGO";
		case NLA_STATE_EARLY_USER_AUTH:
			return "NLA_STATE_EARLY_USER_AUTH";
		case NLA_STATE_FINAL:
			return "NLA_STATE_FINAL";
		default:
			return "UNKNOWN";
	}
}

DWORD nla_get_error(rdpNla* nla)
{
	if (!nla)
		return ERROR_INTERNAL_ERROR;
	return nla->errorCode;
}

UINT32 nla_get_sspi_error(rdpNla* nla)
{
	WINPR_ASSERT(nla);
	return credssp_auth_sspi_error(nla->auth);
}
