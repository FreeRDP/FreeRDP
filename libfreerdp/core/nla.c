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

#include <time.h>
#include <ctype.h>

#include <freerdp/log.h>
#include <freerdp/crypto/tls.h>
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

#include "nla.h"
#include "utils.h"
#include "credssp_auth.h"
#include <freerdp/utils/smartcardlogon.h>

#define TAG FREERDP_TAG("core.nla")

#define SERVER_KEY "Software\\" FREERDP_VENDOR_STRING "\\" FREERDP_PRODUCT_STRING "\\Server"

#define NLA_AUTH_PKG NEGO_SSP_NAME

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
};

static BOOL nla_send(rdpNla* nla);
static int nla_server_recv(rdpNla* nla);
static BOOL nla_encrypt_public_key_echo(rdpNla* nla);
static BOOL nla_encrypt_public_key_hash(rdpNla* nla);
static BOOL nla_decrypt_public_key_echo(rdpNla* nla);
static BOOL nla_decrypt_public_key_hash(rdpNla* nla);
static BOOL nla_encrypt_ts_credentials(rdpNla* nla);
static BOOL nla_decrypt_ts_credentials(rdpNla* nla);

static BOOL nla_Digest_Update_From_SecBuffer(WINPR_DIGEST_CTX* ctx, const SecBuffer* buffer)
{
	if (!buffer)
		return FALSE;
	return winpr_Digest_Update(ctx, buffer->pvBuffer, buffer->cbBuffer);
}

static BOOL nla_sec_buffer_alloc(SecBuffer* buffer, size_t size)
{
	sspi_SecBufferFree(buffer);
	if (!sspi_SecBufferAlloc(buffer, size))
		return FALSE;

	WINPR_ASSERT(buffer);
	buffer->BufferType = SECBUFFER_TOKEN;
	return TRUE;
}

static BOOL nla_sec_buffer_alloc_from_data(SecBuffer* buffer, const BYTE* data, size_t offset,
                                           size_t size)
{
	BYTE* pb;
	if (!nla_sec_buffer_alloc(buffer, offset + size))
		return FALSE;

	WINPR_ASSERT(buffer);
	pb = buffer->pvBuffer;
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
	rdpSettings* settings;
	BOOL ret = FALSE;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	settings = nla->rdpcontext->settings;
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
		else if (!freerdp_settings_set_string(settings, FreeRDP_CspName, MS_SCARD_PROV_A))
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
	WINPR_SAM* sam;
	WINPR_SAM_ENTRY* entry;
	BOOL PromptPassword = FALSE;
	rdpSettings* settings;
	freerdp* instance;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	settings = nla->rdpcontext->settings;
	WINPR_ASSERT(settings);

	instance = nla->rdpcontext->instance;
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
			if ((settings->PasswordHash) && (strlen(settings->PasswordHash) > 0))
				PromptPassword = FALSE;
		}
	}
#endif

	if (PromptPassword)
	{
		switch (utils_authenticate(instance, AUTH_NLA, TRUE))
		{
			case AUTH_SKIP:
			case AUTH_SUCCESS:
				break;
			case AUTH_NO_CREDENTIALS:
				freerdp_set_last_error_log(instance->context,
				                           FREERDP_ERROR_CONNECT_NO_OR_MISSING_CREDENTIALS);
				return FALSE;
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
#ifdef _WIN32
		{
			CERT_CREDENTIAL_INFO certInfo = { sizeof(CERT_CREDENTIAL_INFO), { 0 } };
			LPSTR marshalledCredentials;

			memcpy(certInfo.rgbHashOfCert, nla->certSha1, sizeof(certInfo.rgbHashOfCert));

			if (!CredMarshalCredentialA(CertCredential, &certInfo, &marshalledCredentials))
			{
				WLog_ERR(TAG, "error marshalling cert credentials");
				return FALSE;
			}

			if (sspi_SetAuthIdentityA(nla->identity, marshalledCredentials, NULL,
			                          settings->Password) < 0)
				return FALSE;

			CredFree(marshalledCredentials);
		}
#else
		if (sspi_SetAuthIdentityA(nla->identity, settings->Username, settings->Domain,
		                          settings->Password) < 0)
			return FALSE;
#endif /* _WIN32 */
	}
	else
	{
		BOOL usePassword = TRUE;

		if (settings->RedirectionPassword && (settings->RedirectionPasswordLength > 0))
		{
			if (sspi_SetAuthIdentityWithUnicodePassword(
			        nla->identity, settings->Username, settings->Domain,
			        (UINT16*)settings->RedirectionPassword,
			        settings->RedirectionPasswordLength / sizeof(WCHAR) - 1) < 0)
				return FALSE;
		}

		if (settings->RestrictedAdminModeRequired)
		{
			if (settings->PasswordHash && strlen(settings->PasswordHash) == 32)
			{
				if (sspi_SetAuthIdentityA(nla->identity, settings->Username, settings->Domain,
				                          settings->PasswordHash) < 0)
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
			if (sspi_SetAuthIdentityA(nla->identity, settings->Username, settings->Domain,
			                          settings->Password) < 0)
				return FALSE;
		}
	}

	return TRUE;
}

static int nla_client_init(rdpNla* nla)
{
	rdpTls* tls = NULL;
	rdpSettings* settings;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	settings = nla->rdpcontext->settings;
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

	tls = transport_get_tls(nla->transport);

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
	int rc;

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

	rc = credssp_auth_authenticate(nla->auth);

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
	int rc;

	credssp_auth_take_input_buffer(nla->auth, &nla->negoToken);
	rc = credssp_auth_authenticate(nla->auth);

	switch (rc)
	{
		case 0:
			if (!nla_send(nla))
				return -1;
			break;
		case 1: /* completed */
			if (nla->peerVersion < 5)
				rc = nla_encrypt_public_key_echo(nla);
			else
				rc = nla_encrypt_public_key_hash(nla);

			if (!rc)
				return -1;

			if (!nla_send(nla))
				return -1;

			nla_set_state(nla, NLA_STATE_PUB_KEY_AUTH);
			break;

		default:
			return -1;
	}

	return 1;
}

static int nla_client_recv_pub_key_auth(rdpNla* nla)
{
	BOOL rc;

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
	wStream* s;
	int status;

	WINPR_ASSERT(nla);

	s = Stream_New(NULL, 4096);

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
		status = transport_read_pdu(nla->transport, s);

		if (status < 0)
		{
			WLog_ERR(TAG, "nla_client_authenticate failure");
			goto fail;
		}

		status = nla_recv_pdu(nla, s);

		if (status < 0)
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
	rdpTls* tls;

	WINPR_ASSERT(nla);

	tls = transport_get_tls(nla->transport);
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
	int res = -1;

	WINPR_ASSERT(nla);

	if (nla_server_init(nla) < 1)
		return -1;

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
		if (nla_server_recv(nla) < 0)
			return -1;

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
			return -1;
		}

		if (res == 1)
		{
			/* Process final part of the nego token exchange */
			if (credssp_auth_have_output_token(nla->auth))
			{
				if (!nla_send(nla))
					return -1;

				if (nla_server_recv(nla) < 0)
					return -1;

				WLog_DBG(TAG, "Receiving pubkey Token");
			}

			if (nla->peerVersion < 5)
				res = nla_decrypt_public_key_echo(nla);
			else
				res = nla_decrypt_public_key_hash(nla);

			if (!res)
				return -1;

			/* Clear nego token buffer or we will send it again to the client */
			sspi_SecBufferFree(&nla->negoToken);

			if (nla->peerVersion < 5)
				res = nla_encrypt_public_key_echo(nla);
			else
				res = nla_encrypt_public_key_hash(nla);

			if (!res)
				return -1;
		}

		/* send authentication token */
		WLog_DBG(TAG, "Sending Authentication Token");

		if (!nla_send(nla))
			return -1;

		if (res == 1)
			break;
	}

	/* Receive encrypted credentials */
	if (!nla_server_recv_credentials(nla))
		return -1;

	return 1;
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
	size_t index;

	WINPR_ASSERT(number || (size == 0));

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

static void ap_integer_decrement_le(BYTE* number, size_t size)
{
	size_t index;

	WINPR_ASSERT(number || (size == 0));

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

BOOL nla_encrypt_public_key_echo(rdpNla* nla)
{
	BOOL status;

	WINPR_ASSERT(nla);

	if (nla->server)
	{
		SecBuffer buf;
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
	SecBuffer buf;

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

	if (credssp_auth_encrypt(nla->auth, &buf, &nla->pubKeyAuth, NULL, nla->sendSeqNum++))
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
	BYTE serverClientHash[WINPR_SHA256_DIGEST_LENGTH];
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

static BOOL nla_read_ts_credentials(rdpNla* nla, SecBuffer* data)
{
	WinPrAsn1Decoder dec, dec2;
	WinPrAsn1_OctetString credentials;
	BOOL error;
	WinPrAsn1_INTEGER credType;

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

	if (credType == 1)
	{
		WinPrAsn1_OctetString domain;
		WinPrAsn1_OctetString username;
		WinPrAsn1_OctetString password;

		/* TSPasswordCreds */
		if (!WinPrAsn1DecReadSequence(&dec, &dec2))
			return FALSE;
		dec = dec2;

		/* domainName [0] OCTET STRING */
		if (!WinPrAsn1DecReadContextualOctetString(&dec, 0, &error, &domain, FALSE) && error)
			return FALSE;

		/* userName [1] OCTET STRING */
		if (!WinPrAsn1DecReadContextualOctetString(&dec, 1, &error, &username, FALSE) && error)
			return FALSE;

		/* password [2] OCTET STRING */
		if (!WinPrAsn1DecReadContextualOctetString(&dec, 2, &error, &password, FALSE))
			return FALSE;

		if (sspi_SetAuthIdentityWithLengthW(nla->identity, (WCHAR*)username.data,
		                                    username.len / sizeof(WCHAR), (WCHAR*)domain.data,
		                                    domain.len / sizeof(WCHAR), (WCHAR*)password.data,
		                                    password.len / sizeof(WCHAR)) < 0)
			return FALSE;
	}

	return TRUE;
}

/**
 * Encode TSCredentials structure.
 * @param nla A pointer to the NLA to use
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

static BOOL nla_encode_ts_credentials(rdpNla* nla)
{
	rdpSettings* settings;
	BOOL ret = FALSE;
	WinPrAsn1Encoder* enc;
	size_t length;
	wStream s;
	int credType;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	settings = nla->rdpcontext->settings;
	WINPR_ASSERT(settings);

	enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return FALSE;

	/* TSCredentials */
	if (!WinPrAsn1EncSeqContainer(enc))
		goto out;

	/* credType [0] INTEGER */
	credType = settings->SmartcardLogon ? 2 : 1;
	if (!WinPrAsn1EncContextualInteger(enc, 0, credType))
		goto out;

	/* credentials [1] OCTET STRING */
	if (!WinPrAsn1EncContextualOctetStringContainer(enc, 1))
		goto out;

	if (settings->SmartcardLogon)
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
		if (!WinPrAsn1EncContextualInteger(enc, 0,
		                                   freerdp_settings_get_uint32(settings, FreeRDP_KeySpec)))
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
	}
	else
	{
		WinPrAsn1_OctetString username = { 0 };
		WinPrAsn1_OctetString domain = { 0 };
		WinPrAsn1_OctetString password = { 0 };

		/* TSPasswordCreds */
		if (!WinPrAsn1EncSeqContainer(enc))
			goto out;

		if (!settings->DisableCredentialsDelegation && nla->identity)
		{
			username.len = nla->identity->UserLength * 2;
			username.data = (BYTE*)nla->identity->User;

			domain.len = nla->identity->DomainLength * 2;
			domain.data = (BYTE*)nla->identity->Domain;

			password.len = nla->identity->PasswordLength * 2;
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
	if (WinPrAsn1EncToStream(enc, &s))
		ret = TRUE;

out:
	WinPrAsn1Encoder_Free(&enc);
	return ret;
}

static BOOL nla_encrypt_ts_credentials(rdpNla* nla)
{
	WINPR_ASSERT(nla);

	if (!nla_encode_ts_credentials(nla))
		return FALSE;

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

	if (!credssp_auth_decrypt(nla->auth, &nla->authInfo, &nla->tsCredentials, nla->recvSeqNum++))
		return FALSE;

	if (!nla_read_ts_credentials(nla, &nla->tsCredentials))
		return FALSE;

	return TRUE;
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
	size_t length;
	WinPrAsn1Encoder* enc;
	WinPrAsn1_OctetString octet_string;

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

		WLog_DBG(TAG, "   ----->> nego token");
		if (!WinPrAsn1EncContextualSeqContainer(enc, 1) || !WinPrAsn1EncSeqContainer(enc))
			goto fail;

		/* negoToken [0] OCTET STRING */
		octet_string.data = buffer->pvBuffer;
		octet_string.len = buffer->cbBuffer;
		if (WinPrAsn1EncContextualOctetString(enc, 0, &octet_string) == 0)
			goto fail;

		/* End negoTokens (SEQUENCE OF SEQUENCE) */
		if (!WinPrAsn1EncEndContainer(enc) || !WinPrAsn1EncEndContainer(enc))
			goto fail;
	}

	/* authInfo [2] OCTET STRING */
	if (nla->authInfo.cbBuffer > 0)
	{
		WLog_DBG(TAG, "   ----->> auth info");
		octet_string.data = nla->authInfo.pvBuffer;
		octet_string.len = nla->authInfo.cbBuffer;
		if (WinPrAsn1EncContextualOctetString(enc, 2, &octet_string) == 0)
			goto fail;
		sspi_SecBufferFree(&nla->authInfo);
	}

	/* pubKeyAuth [3] OCTET STRING */
	if (nla->pubKeyAuth.cbBuffer > 0)
	{
		WLog_DBG(TAG, "   ----->> public key auth");
		octet_string.data = nla->pubKeyAuth.pvBuffer;
		octet_string.len = nla->pubKeyAuth.cbBuffer;
		if (WinPrAsn1EncContextualOctetString(enc, 3, &octet_string) == 0)
			goto fail;
		sspi_SecBufferFree(&nla->pubKeyAuth);
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
		WLog_DBG(TAG, "   ----->> client nonce");
		octet_string.data = nla->ClientNonce.pvBuffer;
		octet_string.len = nla->ClientNonce.cbBuffer;
		if (WinPrAsn1EncContextualOctetString(enc, 5, &octet_string) == 0)
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
	WinPrAsn1Decoder dec3 = { 0 };
	BOOL error = FALSE;
	WinPrAsn1_tagId tag = { 0 };
	WinPrAsn1_OctetString octet_string = { 0 };
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
				WLog_ERR(TAG, "SPNEGO failed with NTSTATUS: 0x%08" PRIX32 "", nla->errorCode);
				code = FREERDP_ERROR_AUTHENTICATION_FAILED;
				break;
		}

		freerdp_set_last_error_log(nla->rdpcontext, code);
		return -1;
	}

	return nla_client_recv(nla);
}

int nla_server_recv(rdpNla* nla)
{
	int status = -1;
	wStream* s;

	WINPR_ASSERT(nla);

	s = nla_server_recv_stream(nla);
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
	rdpNla* nla = (rdpNla*)calloc(1, sizeof(rdpNla));
	rdpSettings* settings;

	WINPR_ASSERT(transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!nla)
		return NULL;

	nla->rdpcontext = context;
	nla->server = settings->ServerMode;
	nla->transport = transport;
	nla->sendSeqNum = 0;
	nla->recvSeqNum = 0;
	nla->version = 6;

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
	sspi_SecBufferFree(&nla->pubKeyAuth);
	sspi_SecBufferFree(&nla->authInfo);
	sspi_SecBufferFree(&nla->negoToken);
	sspi_SecBufferFree(&nla->ClientNonce);
	sspi_SecBufferFree(&nla->PublicKey);
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
