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
#include <freerdp/peer.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/sam.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/tchar.h>
#include <winpr/dsparse.h>
#include <winpr/library.h>
#include <winpr/registry.h>
#include <winpr/ncrypt.h>
#include <winpr/cred.h>
#include <winpr/debug.h>

#include "nla.h"
#include "utils.h"
#include "tscredentials.h"
#include <freerdp/utils/smartcardlogon.h>

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
	ULONG sendSeqNum;
	ULONG recvSeqNum;
	rdpContext* rdpcontext;
	CtxtHandle context;
	LPTSTR SspiModule;
	char* SamFile;
	rdpTransport* transport;
	UINT32 cbMaxToken;
	CHAR* packageName;
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

	/* Lifetime of buffer nla_new -> nla_free */
	SecBuffer ClientNonce; /* Depending on protocol version a random nonce or a value read from the
	                          server. */

	SecBuffer negoToken;
	SecBuffer pubKeyAuth;
	SecBuffer authInfo;
	SecBuffer PublicKey;
	SecBuffer tsCredentials;

	LPTSTR ServicePrincipalName;
	SEC_WINNT_AUTH_IDENTITY* identity;
	SEC_WINNT_AUTH_IDENTITY_WINPR identityWinPr;
	SEC_WINPR_KERBEROS_SETTINGS* kerberosSettings;
	PSecurityFunctionTable table;
	SecPkgContext_Sizes ContextSizes;
};

static BOOL nla_send(rdpNla* nla);
static int nla_server_recv(rdpNla* nla);
static void nla_buffer_free(rdpNla* nla);
static SECURITY_STATUS nla_encrypt_public_key_echo(rdpNla* nla);
static SECURITY_STATUS nla_encrypt_public_key_hash(rdpNla* nla);
static SECURITY_STATUS nla_decrypt_public_key_echo(rdpNla* nla);
static SECURITY_STATUS nla_decrypt_public_key_hash(rdpNla* nla);
static SECURITY_STATUS nla_encrypt_ts_credentials(rdpNla* nla);
static SECURITY_STATUS nla_decrypt_ts_credentials(rdpNla* nla);
static BOOL nla_read_ts_password_creds(rdpNla* nla, wStream* s);

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

static BOOL nla_sec_buffer_alloc_from_buffer(SecBuffer* buffer, const SecBuffer* data,
                                             size_t offset)
{
	return nla_sec_buffer_alloc_from_data(buffer, data->pvBuffer, offset, data->cbBuffer);
}

static BOOL nla_decode_to_buffer(wStream* s, SecBuffer* buffer)
{
	BOOL rc = FALSE;
	size_t length;
	if (!s || !buffer)
		return FALSE;
	if (!ber_read_octet_string_tag(s, &length) || /* OCTET STRING */
	    !Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	rc = nla_sec_buffer_alloc_from_data(buffer, Stream_Pointer(s), 0, length);
	Stream_Seek(s, length);
	return rc;
}

static BOOL nla_set_package_name(rdpNla* nla, const TCHAR* name)
{
	if (!nla)
		return FALSE;
	free(nla->packageName);
	nla->packageName = NULL;
	if (name)
	{
#if defined(UNICODE)
		ConvertFromUnicode(CP_UTF8, 0, name, -1, &nla->packageName, 0, NULL, NULL);
#else
		nla->packageName = _strdup(name);
#endif
		if (!nla->packageName)
			return FALSE;
	}
	return TRUE;
}

static SECURITY_STATUS nla_update_package_name(rdpNla* nla)
{
	BOOL rc;
	SECURITY_STATUS status;
	PSecPkgInfo pPackageInfo;

	if (!nla || !nla->table)
	{
		WLog_ERR(TAG, "[%s] nla->table=%p->%p", __FUNCTION__, nla, nla ? nla->table : NULL);
		return ERROR_INTERNAL_ERROR;
	}

	if (!nla->table->QuerySecurityPackageInfo || !nla->table->FreeContextBuffer)
	{
		WLog_ERR(TAG, "[%s] QuerySecurityPackageInfo=%p, FreeContextBuffer=%p", __FUNCTION__,
		         nla->table->QuerySecurityPackageInfo, nla->table->FreeContextBuffer);
		return ERROR_INTERNAL_ERROR;
	}
	status = nla->table->QuerySecurityPackageInfo(NLA_PKG_NAME, &pPackageInfo);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QuerySecurityPackageInfo status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(status), status);
		return status;
	}

	nla->cbMaxToken = pPackageInfo->cbMaxToken;
	rc = nla_set_package_name(nla, pPackageInfo->Name);
	status = nla->table->FreeContextBuffer(pPackageInfo);
	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "FreeContextBuffer status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(status), status);
		return status;
	}
	if (!rc)
		return ERROR_WINS_INTERNAL;

	return status;
}

static SECURITY_STATUS nla_query_context_sizes(rdpNla* nla)
{
	SECURITY_STATUS status;

	if (!nla || !nla->table)
	{
		WLog_ERR(TAG, "[%s] nla->table=%p->%p", __FUNCTION__, nla, nla ? nla->table : NULL);
		return SEC_E_INVALID_PARAMETER;
	}
	if (!nla->table->QueryContextAttributes)
	{
		WLog_ERR(TAG, "[%s] QueryContextAttributes=%p", __FUNCTION__,
		         nla->table->QuerySecurityPackageInfo);
		return ERROR_INTERNAL_ERROR;
	}
	status =
	    nla->table->QueryContextAttributes(&nla->context, SECPKG_ATTR_SIZES, &nla->ContextSizes);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "QueryContextAttributes SECPKG_ATTR_SIZES failure %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(status), status);
	}
	return status;
}

static SECURITY_STATUS nla_initialize_security_context(rdpNla* nla, BOOL initial,
                                                       SecBufferDesc* pInputBufferDesc,
                                                       SecBufferDesc* pOutputBufferDesc)
{
	SECURITY_STATUS status;
	PCtxtHandle ctx = NULL;

	if (!nla || !nla->table)
	{
		WLog_ERR(TAG, "[%s] nla->table=%p->%p", __FUNCTION__, nla, nla ? nla->table : NULL);
		return SEC_E_INVALID_PARAMETER;
	}
	if (!nla->table->InitializeSecurityContext)
	{
		WLog_ERR(TAG, "[%s] InitializeSecurityContext=%p", __FUNCTION__,
		         nla->table->InitializeSecurityContext);
		return ERROR_INTERNAL_ERROR;
	}
	if (!initial)
		ctx = &nla->context;

	status = nla->table->InitializeSecurityContext(
	    &nla->credentials, ctx, nla->ServicePrincipalName, nla->fContextReq, 0,
	    SECURITY_NATIVE_DREP, pInputBufferDesc, 0, &nla->context, pOutputBufferDesc,
	    &nla->pfContextAttr, &nla->expiration);

	switch (status)
	{
		case SEC_E_INCOMPLETE_MESSAGE:
		case SEC_E_OK:
		case SEC_I_COMPLETE_AND_CONTINUE:
		case SEC_I_COMPLETE_NEEDED:
		case SEC_I_CONTINUE_NEEDED:
		case SEC_I_INCOMPLETE_CREDENTIALS:
			WLog_DBG(TAG, " InitializeSecurityContext status %s [0x%08" PRIX32 "]",
			         GetSecurityStatusString(status), status);
			break;

		default:
			WLog_WARN(TAG, " InitializeSecurityContext failure status %s [0x%08" PRIX32 "]",
			          GetSecurityStatusString(status), status);
			break;
	}

	return status;
}

static BOOL nla_complete_auth(rdpNla* nla, PSecBufferDesc pOutputBufferDesc)
{
	SECURITY_STATUS status = SEC_E_INVALID_TOKEN;

	if (!nla || !nla->table)
	{
		WLog_ERR(TAG, "[%s] nla->table=%p->%p", __FUNCTION__, nla, nla ? nla->table : NULL);
		return FALSE;
	}
	if (!nla->table->CompleteAuthToken)
	{
		WLog_ERR(TAG, "[%s] CompleteAuthToken=%p", __FUNCTION__, nla->table->CompleteAuthToken);
		return FALSE;
	}

	nla->status = nla->table->CompleteAuthToken(&nla->context, pOutputBufferDesc);

	if (nla->status != SEC_E_OK)
	{
		WLog_WARN(TAG, "CompleteAuthToken status %s [0x%08" PRIX32 "]",
		          GetSecurityStatusString(status), status);
		return FALSE;
	}

	return TRUE;
}

static SECURITY_STATUS nla_decrypt(rdpNla* nla, SecBuffer* buffer, size_t headerLength)
{
	ULONG pfQOP = 0;
	SECURITY_STATUS status = SEC_E_INVALID_PARAMETER;
	SecBuffer Buffers[2];
	SecBufferDesc Message;

	if (!nla || !nla->table)
	{
		WLog_ERR(TAG, "[%s] nla->table=%p->%p", __FUNCTION__, nla, nla ? nla->table : NULL);
		return SEC_E_INVALID_PARAMETER;
	}
	if (!nla->table->DecryptMessage)
	{
		WLog_ERR(TAG, "[%s] DecryptMessage=%p", __FUNCTION__, nla->table->DecryptMessage);
		return ERROR_INTERNAL_ERROR;
	}

	if (!buffer || (headerLength > buffer->cbBuffer))
	{
		return SEC_E_INVALID_PARAMETER;
	}

	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[0].cbBuffer = (ULONG)headerLength;
	Buffers[0].pvBuffer = &((BYTE*)buffer->pvBuffer)[0];

	Buffers[1].BufferType = SECBUFFER_DATA; /* Encrypted TLS Public Key */
	Buffers[1].cbBuffer = (ULONG)(buffer->cbBuffer - headerLength);
	Buffers[1].pvBuffer = &((BYTE*)buffer->pvBuffer)[headerLength];
	Message.cBuffers = 2;

	Message.ulVersion = SECBUFFER_VERSION;
	Message.pBuffers = Buffers;
	status = nla->table->DecryptMessage(&nla->context, &Message, nla->recvSeqNum++, &pfQOP);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "DecryptMessage failure %s [%08" PRIX32 "]", GetSecurityStatusString(status),
		         status);
	}

	return status;
}

static SECURITY_STATUS nla_encrypt(rdpNla* nla, SecBuffer* buffer, size_t headerLength)
{
	SECURITY_STATUS status = SEC_E_INVALID_PARAMETER;
	SecBuffer Buffers[2];
	SecBufferDesc Message;

	if (!nla || !nla->table)
	{
		WLog_ERR(TAG, "[%s] nla->table=%p->%p", __FUNCTION__, nla, nla ? nla->table : NULL);
		return SEC_E_INVALID_PARAMETER;
	}
	if (!nla->table->EncryptMessage)
	{
		WLog_ERR(TAG, "[%s] EncryptMessage=%p", __FUNCTION__, nla->table->EncryptMessage);
		return ERROR_INTERNAL_ERROR;
	}

	if (!buffer || (headerLength > buffer->cbBuffer))
	{
		return SEC_E_INVALID_PARAMETER;
	}

	Message.cBuffers = 2;
	Buffers[0].BufferType = SECBUFFER_TOKEN; /* Signature */
	Buffers[0].cbBuffer = (ULONG)headerLength;
	Buffers[0].pvBuffer = &((BYTE*)buffer->pvBuffer)[0];

	Buffers[1].BufferType = SECBUFFER_DATA;
	Buffers[1].cbBuffer = (ULONG)(buffer->cbBuffer - headerLength);
	Buffers[1].pvBuffer = &((BYTE*)buffer->pvBuffer)[headerLength];

	Message.pBuffers = Buffers;
	Message.ulVersion = SECBUFFER_VERSION;
	status = nla->table->EncryptMessage(&nla->context, 0, &Message, nla->sendSeqNum++);

	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "EncryptMessage status %s [0x%08" PRIX32 "]", GetSecurityStatusString(status),
		         status);
		return status;
	}

	if ((Message.cBuffers == 2) && (Buffers[0].cbBuffer < nla->ContextSizes.cbSecurityTrailer))
	{
		/* IMPORTANT: EncryptMessage may not use all the signature space, so we need to shrink the
		 * excess between the buffers */
		MoveMemory(((BYTE*)Buffers[0].pvBuffer) + Buffers[0].cbBuffer, Buffers[1].pvBuffer,
		           Buffers[1].cbBuffer);
		buffer->cbBuffer = Buffers[0].cbBuffer + Buffers[1].cbBuffer;
	}

	return status;
}

static size_t ber_sizeof_sequence_octet_string(size_t length)
{
	size_t rc = ber_sizeof_contextual_tag(ber_sizeof_octet_string(length));
	rc += ber_sizeof_octet_string(length);
	return rc;
}

static size_t ber_write_sequence_octet_string(wStream* stream, BYTE context, const BYTE* value,
                                              size_t length)
{
	size_t rc = ber_write_contextual_tag(stream, context, ber_sizeof_octet_string(length), TRUE);
	rc += ber_write_octet_string(stream, value, length);
	return rc;
}

static size_t ber_write_sequence_octet_string_from_secbuffer(wStream* stream, BYTE context,
                                                             const SecBuffer* buffer)
{
	return ber_write_sequence_octet_string(stream, context, buffer->pvBuffer, buffer->cbBuffer);
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
	SmartcardCerts* certs = NULL;
	const SmartcardCertInfo* info = NULL;
	DWORD count;
	rdpSettings* settings;
	SEC_WINPR_KERBEROS_SETTINGS* kerbSettings;
	BOOL ret = FALSE;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	settings = nla->rdpcontext->settings;
	WINPR_ASSERT(settings);

	if (!settings->SmartcardLogon)
		return TRUE;

	kerbSettings = nla->kerberosSettings;
	WINPR_ASSERT(kerbSettings);

	if (!settings->CspName)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_CspName, MS_SCARD_PROV_A))
		{
			WLog_ERR(TAG, "unable to set CSP name");
			return FALSE;
		}
	}

	if (settings->PkinitAnchors)
	{
		kerbSettings->pkinitX509Anchors = _strdup(settings->PkinitAnchors);
		if (!kerbSettings->pkinitX509Anchors)
		{
			WLog_ERR(TAG, "error setting X509 anchors");
			return FALSE;
		}
	}

	if (!smartcard_enumerateCerts(settings, &certs, &count))
	{
		WLog_ERR(TAG, "unable to list smartcard certificates");
		return FALSE;
	}

	if (count < 1)
	{
		WLog_ERR(TAG, "no smartcard certificates found");
		goto out;
	}

	if (count != 1)
		goto setup_pin;

	info = smartcard_getCertInfo(certs, 0);
	if (!info)
		goto out;

	/*
	 * just one result let's try to fill missing parameters
	 */
	if (!settings->Username && info->userHint)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_Username, info->userHint))
		{
			WLog_ERR(TAG, "unable to copy certificate username");
			goto out;
		}
	}

	if (!settings->Domain && info->domainHint)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_Domain, info->domainHint))
		{
			WLog_ERR(TAG, "unable to copy certificate domain");
			goto out;
		}
	}

	if (!settings->ReaderName && info->reader)
	{
		if (ConvertFromUnicode(CP_UTF8, 0, info->reader, -1, &settings->ReaderName, 0, NULL, NULL) <
		    0)
		{
			WLog_ERR(TAG, "unable to copy reader name");
			goto out;
		}
	}

	if (!settings->ContainerName && info->containerName)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_ContainerName, info->containerName))
		{
			WLog_ERR(TAG, "unable to copy container name");
			goto out;
		}
	}

	memcpy(kerbSettings->certSha1, info->sha1Hash, sizeof(kerbSettings->certSha1));

	if (info->pkinitArgs)
	{
		kerbSettings->pkinitX509Identity = _strdup(info->pkinitArgs);
		if (!kerbSettings->pkinitX509Identity)
		{
			WLog_ERR(TAG, "unable to copy pkinitArgs");
			goto out;
		}
	}

setup_pin:

	ret = TRUE;
out:
	smartcardCerts_Free(certs);

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
				return 0;
			default:
				return 0;
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

			memcpy(certInfo.rgbHashOfCert, nla->kerberosSettings->certSha1,
			       sizeof(certInfo.rgbHashOfCert));

			if (!CredMarshalCredentialA(CertCredential, &certInfo, &marshalledCredentials))
			{
				WLog_ERR(TAG, "error marshalling cert credentials");
				return FALSE;
			}

			if (sspi_SetAuthIdentityA(nla->identity, marshalledCredentials, NULL,
			                          settings->Password) < 0)
				return -1;

			CredFree(marshalledCredentials);
		}
#else
		if (sspi_SetAuthIdentityA(nla->identity, settings->Username, settings->Domain,
		                          settings->Password) < 0)
			return -1;
#endif /* _WIN32 */

		return TRUE;
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
				return -1;
		}

		if (settings->RestrictedAdminModeRequired)
		{
			if (settings->PasswordHash && strlen(settings->PasswordHash) == 32)
			{
				if (sspi_SetAuthIdentityA(nla->identity, settings->Username, settings->Domain,
				                          settings->PasswordHash) < 0)
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

		if (usePassword)
		{
			if (sspi_SetAuthIdentityA(nla->identity, settings->Username, settings->Domain,
			                          settings->Password) < 0)
				return -1;
		}
	}

	return TRUE;
}

static const char* parseInt(const char* v, INT32* r)
{
	*r = 0;

	/* check that we have at least a digit */
	if (!*v || !((*v >= '0') && (*v <= '9')))
		return NULL;

	for (; *v && (*v >= '0') && (*v <= '9'); v++)
	{
		*r = (*r * 10) + (*v - '0');
	}

	return v;
}

static BOOL parseKerberosDeltat(const char* value, INT32* dest, const char* message)
{
	INT32 v;
	const char* ptr;

	WINPR_ASSERT(value);
	WINPR_ASSERT(dest);
	WINPR_ASSERT(message);

	/* determine the format :
	 *   h:m[:s] (3:00:02) 			deltat in hours/minutes
	 *   <n>d<n>h<n>m<n>s 1d4h	    deltat in day/hours/minutes/seconds
	 *   <n>						deltat in seconds
	 */
	ptr = strchr(value, ':');
	if (ptr)
	{
		/* format like h:m[:s] */
		*dest = 0;
		value = parseInt(value, &v);
		if (!value || *value != ':')
		{
			WLog_ERR(TAG, "Invalid value for %s", message);
			return FALSE;
		}

		*dest = v * 3600;

		value = parseInt(value + 1, &v);
		if (!value || (*value != 0 && *value != ':') || (v > 60))
		{
			WLog_ERR(TAG, "Invalid value for %s", message);
			return FALSE;
		}
		*dest += v * 60;

		if (*value == ':')
		{
			/* have optional seconds */
			value = parseInt(value + 1, &v);
			if (!value || (*value != 0) || (v > 60))
			{
				WLog_ERR(TAG, "Invalid value for %s", message);
				return FALSE;
			}
			*dest += v;
		}
		return TRUE;
	}

	/* <n> or <n>d<n>h<n>m<n>s format */
	value = parseInt(value, &v);
	if (!value)
	{
		WLog_ERR(TAG, "Invalid value for %s", message);
		return FALSE;
	}

	if (!*value || isspace(*value))
	{
		/* interpret that as a value in seconds */
		*dest = v;
		return TRUE;
	}

	*dest = 0;
	do
	{
		INT32 factor = 0;
		INT32 maxValue = 0;

		switch (*value)
		{
			case 'd':
				factor = 3600 * 24;
				maxValue = 0;
				break;
			case 'h':
				factor = 3600;
				maxValue = 0;
				break;
			case 'm':
				factor = 60;
				maxValue = 60;
				break;
			case 's':
				factor = 1;
				maxValue = 60;
				break;
			default:
				WLog_ERR(TAG, "invalid value for unit %c when parsing %s", *value, message);
				return FALSE;
		}

		if ((maxValue > 0) && (v > maxValue))
		{
			WLog_ERR(TAG, "invalid value for unit %c when parsing %s", *value, message);
			return FALSE;
		}

		*dest += (v * factor);
		value++;
		if (!*value)
			return TRUE;

		value = parseInt(value, &v);
		if (!value || !*value)
		{
			WLog_ERR(TAG, "Invalid value for %s", message);
			return FALSE;
		}

	} while (TRUE);

	return TRUE;
}

static BOOL nla_setup_kerberos(rdpNla* nla)
{
	SEC_WINPR_KERBEROS_SETTINGS* kerbSettings;
	rdpSettings* settings;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	settings = nla->rdpcontext->settings;
	WINPR_ASSERT(settings);

	kerbSettings = nla->kerberosSettings;
	WINPR_ASSERT(kerbSettings);

	if (settings->KerberosLifeTime &&
	    !parseKerberosDeltat(settings->KerberosLifeTime, &kerbSettings->lifeTime, "lifetime"))
		return FALSE;

	if (settings->KerberosStartTime &&
	    !parseKerberosDeltat(settings->KerberosStartTime, &kerbSettings->startTime, "starttime"))
		return FALSE;

	if (settings->KerberosRenewableLifeTime &&
	    !parseKerberosDeltat(settings->KerberosRenewableLifeTime, &kerbSettings->renewLifeTime,
	                         "renewLifetime"))
		return FALSE;

	if (settings->KerberosCache)
	{
		kerbSettings->cache = _strdup(settings->KerberosCache);
		if (!kerbSettings->cache)
		{
			WLog_ERR(TAG, "unable to copy cache name");
			return FALSE;
		}
	}

	if (settings->KerberosKeytab)
	{
		kerbSettings->keytab = _strdup(settings->KerberosKeytab);
		if (!kerbSettings->keytab)
		{
			WLog_ERR(TAG, "unable to copy keytab name");
			return FALSE;
		}
	}

	if (settings->KerberosArmor)
	{
		kerbSettings->armorCache = _strdup(settings->KerberosArmor);
		if (!kerbSettings->armorCache)
		{
			WLog_ERR(TAG, "unable to copy armorCache");
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * Initialize NTLM/Kerberos SSP authentication module (client).
 * @param credssp
 */

static int nla_client_init(rdpNla* nla)
{
	char* spn;
	size_t length;
	rdpTls* tls = NULL;
	rdpSettings* settings;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	settings = nla->rdpcontext->settings;
	WINPR_ASSERT(settings);

	nla_set_state(nla, NLA_STATE_INITIAL);

	if (settings->RestrictedAdminModeRequired)
		settings->DisableCredentialsDelegation = TRUE;

	if (!nla_setup_kerberos(nla))
		return -1;

	if (!nla_adjust_settings_from_smartcard(nla))
		return -1;

	if (!nla_client_setup_identity(nla))
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

	if (!nla_sspi_module_init(nla))
		return -1;

	nla->status = nla_update_package_name(nla);
	if (nla->status != SEC_E_OK)
		return -1;

	WLog_DBG(TAG, "%s %" PRIu32 " : packageName=%s ; cbMaxToken=%d", __FUNCTION__, __LINE__,
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

static BOOL nla_client_send_token(rdpNla* nla, SecBufferDesc* token)
{
	BOOL rc = FALSE;

	if (!nla || !token)
	{
		WLog_ERR(TAG, "[%s] nla=%p, token=%p", __FUNCTION__, nla, token);
		goto fail;
	}

	if (token->cBuffers != 1)
	{
		WLog_ERR(TAG, "[%s] token->cBuffers=%" PRIu32, __FUNCTION__, token->cBuffers);
		goto fail;
	}

	if (!nla_sec_buffer_alloc_from_buffer(&nla->negoToken, &token->pBuffers[0], 0))
		goto fail;

	if (!nla_send(nla))
		goto fail;

	rc = TRUE;

fail:

	return rc;
}

int nla_client_begin(rdpNla* nla)
{
	int rc = -1;
	SecBuffer outputBuffer = { 0 };
	SecBufferDesc outputBufferDesc = { 0 };

	WINPR_ASSERT(nla);

	if (nla_client_init(nla) < 1)
		goto fail;

	if (nla_get_state(nla) != NLA_STATE_INITIAL)
		goto fail;

	outputBufferDesc.ulVersion = SECBUFFER_VERSION;
	outputBufferDesc.cBuffers = 1;
	outputBufferDesc.pBuffers = &outputBuffer;

	if (!nla_sec_buffer_alloc(&outputBuffer, nla->cbMaxToken))
		goto fail;

	nla->status = nla_initialize_security_context(nla, TRUE, NULL, &outputBufferDesc);
	switch (nla->status)
	{
		case SEC_E_OK: /* Complete, send output token if >0 */
			if (!nla_client_send_token(nla, &outputBufferDesc))
				goto fail;
			nla_set_state(nla, NLA_STATE_FINAL);
			break;
		case SEC_I_COMPLETE_AND_CONTINUE: /* CompleteAuthToken -> server ->
		                                     InitializeSecurityContext again */
			if (!nla_complete_auth(nla, &outputBufferDesc))
				goto fail;
			nla_set_state(nla, NLA_STATE_NEGO_TOKEN);
			break;
		case SEC_I_COMPLETE_NEEDED: /* CompleteAuthToken -> finish */
			if (!nla_complete_auth(nla, &outputBufferDesc))
				goto fail;
			nla_set_state(nla, NLA_STATE_FINAL);
			break;
		case SEC_I_CONTINUE_NEEDED: /* out token -> server -> InitializeSecurityContext again */
			if (!nla_client_send_token(nla, &outputBufferDesc))
				goto fail;
			nla_set_state(nla, NLA_STATE_NEGO_TOKEN);
			break;
		case SEC_E_NO_CREDENTIALS:
		case SEC_I_INCOMPLETE_CREDENTIALS:
		case SEC_E_INCOMPLETE_MESSAGE:
		default:
			goto fail;
	}

	rc = 1;
fail:
	sspi_SecBufferFree(&outputBuffer);
	return rc;
}

static int nla_client_recv_nego_token(rdpNla* nla)
{
	int rc = -1;
	SECURITY_STATUS status;
	SecBuffer inputBuffer = { 0 };
	SecBuffer outputBuffer = { 0 };
	SecBufferDesc inputBufferDesc = { 0 };
	SecBufferDesc outputBufferDesc = { 0 };

	WINPR_ASSERT(nla);

	inputBufferDesc.ulVersion = SECBUFFER_VERSION;
	inputBufferDesc.cBuffers = 1;
	inputBufferDesc.pBuffers = &inputBuffer;
	if (!nla_sec_buffer_alloc_from_buffer(&inputBuffer, &nla->negoToken, 0))
		goto fail;

	outputBufferDesc.ulVersion = SECBUFFER_VERSION;
	outputBufferDesc.cBuffers = 1;
	outputBufferDesc.pBuffers = &outputBuffer;
	if (!nla_sec_buffer_alloc(&outputBuffer, nla->cbMaxToken))
		goto fail;

	nla->status = nla_initialize_security_context(nla, FALSE, &inputBufferDesc, &outputBufferDesc);

	switch (nla->status)
	{
		case SEC_I_COMPLETE_AND_CONTINUE:
			if (!nla_complete_auth(nla, &outputBufferDesc))
				goto fail;
			if (!nla_client_send_token(nla, &outputBufferDesc))
				goto fail;
			break;
		case SEC_I_COMPLETE_NEEDED: /* CompleteAuthToken -> finish */
			if (!nla_complete_auth(nla, &outputBufferDesc))
				goto fail;
			nla_set_state(nla, NLA_STATE_NEGO_TOKEN);
			break;
		case SEC_E_OK: /* completed */
			nla->havePubKeyAuth = TRUE;
			status = nla_query_context_sizes(nla);

			if (status != SEC_E_OK)
				goto fail;

			if (nla->peerVersion < 5)
				status = nla_encrypt_public_key_echo(nla);
			else
				status = nla_encrypt_public_key_hash(nla);

			if (status != SEC_E_OK)
				goto fail;

			if (!nla_client_send_token(nla, &outputBufferDesc))
				goto fail;
			nla_set_state(nla, NLA_STATE_PUB_KEY_AUTH);
			break;

		case SEC_I_CONTINUE_NEEDED: /* send -> retry */
			if (!nla_client_send_token(nla, &outputBufferDesc))
				goto fail;
			nla_set_state(nla, NLA_STATE_NEGO_TOKEN);
			break;

		default:
			WLog_ERR(TAG, "Unexpected NLA return %s", GetSecurityStatusString(nla->status));
			goto fail;
	}

	rc = 1;
fail:
	sspi_SecBufferFree(&inputBuffer);
	sspi_SecBufferFree(&outputBuffer);
	return rc;
}

static int nla_client_recv_pub_key_auth(rdpNla* nla)
{
	WINPR_ASSERT(nla);

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
		return -1;

	if (!nla_send(nla))
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

	if (nla->status != SEC_E_OK)
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
 * @param credssp
 */

static int nla_server_init(rdpNla* nla)
{
	rdpTls* tls;
	freerdp_peer* peer;
	SEC_WINNT_AUTH_IDENTITY_WINPR* identity;

	WINPR_ASSERT(nla);

	tls = transport_get_tls(nla->transport);
	WINPR_ASSERT(tls);

	if (!nla_sec_buffer_alloc_from_data(&nla->PublicKey, tls->PublicKey, 0, tls->PublicKeyLength))
	{
		WLog_ERR(TAG, "Failed to allocate SecBuffer for public key");
		return -1;
	}

	if (!nla_sspi_module_init(nla))
		return -1;

	if (!nla_setup_kerberos(nla))
		return -1;

	WINPR_ASSERT(nla->rdpcontext);
	WINPR_ASSERT(nla->rdpcontext->peer);

	peer = nla->rdpcontext->peer;
	WINPR_ASSERT(peer);

	identity = &nla->identityWinPr;
	identity->ntlmSettings.hashCallback = peer->SspiNtlmHashCallback;
	identity->ntlmSettings.hashCallbackArg = peer;
	identity->ntlmSettings.samFile = nla->SamFile;

	nla->status = nla_update_package_name(nla);

	if (nla->status != SEC_E_OK)
		return -1;

	nla->status = nla->table->AcquireCredentialsHandle(NULL, NLA_PKG_NAME, SECPKG_CRED_INBOUND,
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
	BOOL rc = FALSE;
	WINPR_ASSERT(nla);

	if (nla_server_recv(nla) < 0)
		goto fail;

	nla->status = nla_decrypt_ts_credentials(nla);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "Could not decrypt TSCredentials status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		goto fail;
	}

	WINPR_ASSERT(nla->table);
	WINPR_ASSERT(nla->table->ImpersonateSecurityContext);
	nla->status = nla->table->ImpersonateSecurityContext(&nla->context);

	if (nla->status != SEC_E_OK)
	{
		WLog_ERR(TAG, "ImpersonateSecurityContext status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);
		goto fail;
	}
	else
	{
		WINPR_ASSERT(nla->table->RevertSecurityContext);
		nla->status = nla->table->RevertSecurityContext(&nla->context);

		if (nla->status != SEC_E_OK)
		{
			WLog_ERR(TAG, "RevertSecurityContext status %s [0x%08" PRIX32 "]",
			         GetSecurityStatusString(nla->status), nla->status);
			goto fail;
		}
	}

	rc = TRUE;
fail:
	return rc;
}

/**
 * Authenticate with client using CredSSP (server).
 * @param credssp
 * @return 1 if authentication is successful
 */

static int nla_server_authenticate(rdpNla* nla)
{
	int res = -1;

	WINPR_ASSERT(nla);

	if (nla_server_init(nla) < 1)
		goto fail_auth;

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
		int rc = -1;
		SecBuffer inputBuffer = { 0 };
		SecBuffer outputBuffer = { 0 };
		SecBufferDesc inputBufferDesc = { 0 };
		SecBufferDesc outputBufferDesc = { 0 };

		/* receive authentication token */
		inputBufferDesc.ulVersion = SECBUFFER_VERSION;
		inputBufferDesc.cBuffers = 1;
		inputBufferDesc.pBuffers = &inputBuffer;

		if (nla_server_recv(nla) < 0)
			goto fail_auth;

		WLog_DBG(TAG, "Receiving Authentication Token");
		if (!nla_sec_buffer_alloc_from_buffer(&inputBuffer, &nla->negoToken, 0))
		{
			WLog_ERR(TAG, "CredSSP: invalid negoToken!");
			goto fail;
		}

		outputBufferDesc.ulVersion = SECBUFFER_VERSION;
		outputBufferDesc.cBuffers = 1;
		outputBufferDesc.pBuffers = &outputBuffer;

		if (!nla_sec_buffer_alloc(&outputBuffer, nla->cbMaxToken))
			goto fail;

		nla->status = nla->table->AcceptSecurityContext(
		    &nla->credentials, nla->haveContext ? &nla->context : NULL, &inputBufferDesc,
		    nla->fContextReq, SECURITY_NATIVE_DREP, &nla->context, &outputBufferDesc,
		    &nla->pfContextAttr, &nla->expiration);
		WLog_VRB(TAG, "AcceptSecurityContext status %s [0x%08" PRIX32 "]",
		         GetSecurityStatusString(nla->status), nla->status);

		if (!nla_sec_buffer_alloc_from_buffer(&nla->negoToken, &outputBuffer, 0))
			goto fail;

		if (nla->status == SEC_E_OK)
		{
			if (outputBuffer.cbBuffer != 0)
			{
				if (!nla_send(nla))
				{
					nla_buffer_free(nla);
					goto fail;
				}

				if (nla_server_recv(nla) < 0)
					goto fail;

				WLog_DBG(TAG, "Receiving pubkey Token");
			}

			nla->havePubKeyAuth = TRUE;
			nla->status = nla_query_context_sizes(nla);

			if (nla->status != SEC_E_OK)
				goto fail;

			if (nla->peerVersion < 5)
				nla->status = nla_decrypt_public_key_echo(nla);
			else
				nla->status = nla_decrypt_public_key_hash(nla);

			if (nla->status != SEC_E_OK)
			{
				WLog_ERR(TAG,
				         "Error: could not verify client's public key echo %s [0x%08" PRIX32 "]",
				         GetSecurityStatusString(nla->status), nla->status);
				goto fail;
			}

			sspi_SecBufferFree(&nla->negoToken);

			if (nla->peerVersion < 5)
				nla->status = nla_encrypt_public_key_echo(nla);
			else
				nla->status = nla_encrypt_public_key_hash(nla);

			if (nla->status != SEC_E_OK)
				goto fail;

			rc = 1;
		}
		else
		{
			rc = 0;
		}
	fail:
		sspi_SecBufferFree(&inputBuffer);
		sspi_SecBufferFree(&outputBuffer);
		if (rc < 0)
		{
			res = rc;
			goto fail_auth;
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
			nla_send(nla);
			goto fail_auth; /* Access Denied */
		}

		/* send authentication token */
		WLog_DBG(TAG, "Sending Authentication Token");

		if (!nla_send(nla))
		{
			nla_buffer_free(nla);
			goto fail_auth;
		}

		if (nla->status != SEC_I_CONTINUE_NEEDED)
			break;

		nla->haveContext = TRUE;
	}

	/* Receive encrypted credentials */
	if (!nla_server_recv_credentials(nla))
		goto fail_auth;

	res = 1;
fail_auth:
	nla_buffer_free(nla);
	return res;
}

/**
 * Authenticate using CredSSP.
 * @param credssp
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

SECURITY_STATUS nla_encrypt_public_key_echo(rdpNla* nla)
{
	SECURITY_STATUS status;

	WINPR_ASSERT(nla);

	if (!nla_sec_buffer_alloc_from_buffer(&nla->pubKeyAuth, &nla->PublicKey,
	                                      nla->ContextSizes.cbSecurityTrailer))
		return SEC_E_INSUFFICIENT_MEMORY;

	if (!nla->server)
	{
		BYTE* data = &((BYTE*)nla->PublicKey.pvBuffer)[nla->ContextSizes.cbSecurityTrailer];
		/* server echos the public key +1 */
		ap_integer_increment_le(data, nla->PublicKey.cbBuffer);
	}

	status = nla_encrypt(nla, &nla->pubKeyAuth, nla->ContextSizes.cbSecurityTrailer);
	if (status != SEC_E_OK)
		return status;

	return status;
}

SECURITY_STATUS nla_encrypt_public_key_hash(rdpNla* nla)
{
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	WINPR_DIGEST_CTX* sha256 = NULL;
	BYTE* hash;

	WINPR_ASSERT(nla);

	const ULONG auth_data_length =
	    (nla->ContextSizes.cbSecurityTrailer + WINPR_SHA256_DIGEST_LENGTH);
	const BYTE* hashMagic = nla->server ? ServerClientHashMagic : ClientServerHashMagic;
	const size_t hashSize =
	    nla->server ? sizeof(ServerClientHashMagic) : sizeof(ClientServerHashMagic);

	if (!nla_sec_buffer_alloc(&nla->pubKeyAuth, auth_data_length))
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

	if (!nla_Digest_Update_From_SecBuffer(sha256, &nla->ClientNonce))
		goto out;

	/* SubjectPublicKey */
	if (!nla_Digest_Update_From_SecBuffer(sha256, &nla->PublicKey))
		goto out;

	hash = &((BYTE*)nla->pubKeyAuth.pvBuffer)[nla->ContextSizes.cbSecurityTrailer];
	if (!winpr_Digest_Final(sha256, hash, WINPR_SHA256_DIGEST_LENGTH))
		goto out;

	status = nla_encrypt(nla, &nla->pubKeyAuth, nla->ContextSizes.cbSecurityTrailer);
	if (status != SEC_E_OK)
		goto out;

out:
	winpr_Digest_Free(sha256);
	return status;
}

SECURITY_STATUS nla_decrypt_public_key_echo(rdpNla* nla)
{
	BYTE* public_key1 = NULL;
	BYTE* public_key2 = NULL;
	ULONG public_key_length = 0;
	size_t signature_length;
	SECURITY_STATUS status = SEC_E_INVALID_TOKEN;
	PSecBuffer pubKeyAuth;

	if (!nla)
		goto fail;

	pubKeyAuth = &nla->pubKeyAuth;

	signature_length = pubKeyAuth->cbBuffer - nla->PublicKey.cbBuffer;
	if ((pubKeyAuth->cbBuffer < nla->PublicKey.cbBuffer) ||
	    (signature_length > nla->ContextSizes.cbSecurityTrailer))
	{
		WLog_ERR(TAG, "unexpected pubKeyAuth buffer size: %" PRIu32 "", pubKeyAuth->cbBuffer);
		goto fail;
	}

	status = nla_decrypt(nla, pubKeyAuth, signature_length);
	if (status != SEC_E_OK)
		goto fail;

	public_key1 = &((BYTE*)nla->PublicKey.pvBuffer)[0];
	public_key2 = &((BYTE*)pubKeyAuth->pvBuffer)[pubKeyAuth->cbBuffer];

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

	return status;
}

SECURITY_STATUS nla_decrypt_public_key_hash(rdpNla* nla)
{
	size_t signature_length;
	WINPR_DIGEST_CTX* sha256 = NULL;
	BYTE serverClientHash[WINPR_SHA256_DIGEST_LENGTH];
	SECURITY_STATUS status = SEC_E_INVALID_TOKEN;

	WINPR_ASSERT(nla);

	const BYTE* hashMagic = nla->server ? ClientServerHashMagic : ServerClientHashMagic;
	const BYTE* decryptedHash;
	const size_t hashSize =
	    nla->server ? sizeof(ClientServerHashMagic) : sizeof(ServerClientHashMagic);
	PSecBuffer pubKeyAuth = &nla->pubKeyAuth;

	signature_length = pubKeyAuth->cbBuffer - WINPR_SHA256_DIGEST_LENGTH;

	if ((pubKeyAuth->cbBuffer < WINPR_SHA256_DIGEST_LENGTH) ||
	    (signature_length > nla->ContextSizes.cbSecurityTrailer))
	{
		WLog_ERR(TAG, "unexpected pubKeyAuth buffer size: %" PRIu32 "", pubKeyAuth->cbBuffer);
		goto fail;
	}

	status = nla_decrypt(nla, pubKeyAuth, signature_length);
	if (status != SEC_E_OK)
		goto fail;

	status = SEC_E_INTERNAL_ERROR;
	decryptedHash = &((const BYTE*)pubKeyAuth->pvBuffer)[signature_length];

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
	if (memcmp(serverClientHash, decryptedHash, WINPR_SHA256_DIGEST_LENGTH) != 0)
	{
		WLog_ERR(TAG, "Could not verify server's hash");
		status = SEC_E_MESSAGE_ALTERED; /* DO NOT SEND CREDENTIALS! */
		goto fail;
	}

	status = SEC_E_OK;
fail:
	winpr_Digest_Free(sha256);
	return status;
}

BOOL nla_read_ts_password_creds(rdpNla* nla, wStream* s)
{
	size_t length;
	size_t userLen = 0;
	size_t domainLen = 0;
	size_t passwordLen = 0;
	const WCHAR* user = NULL;
	const WCHAR* domain = NULL;
	const WCHAR* password = NULL;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(s);

	if (!nla->identity)
	{
		WLog_ERR(TAG, "nla->identity is NULL!");
		return FALSE;
	}

	/* TSPasswordCreds (SEQUENCE)
	 * Initialise to default values. */

	sspi_FreeAuthIdentity(nla->identity);

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

	domainLen = length / sizeof(WCHAR);
	if (length > 0)
		domain = Stream_PointerAs(s, const WCHAR);

	if (!Stream_SafeSeek(s, length))
		return FALSE;

	/* [1] userName (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 1, &length, TRUE) || !ber_read_octet_string_tag(s, &length))
		return FALSE;

	userLen = length / sizeof(WCHAR);
	if (length > 0)
		user = Stream_PointerAs(s, const WCHAR);

	if (!Stream_SafeSeek(s, length))
		return FALSE;

	/* [2] password (OCTET STRING) */
	if (!ber_read_contextual_tag(s, 2, &length, TRUE) || !ber_read_octet_string_tag(s, &length))
		return FALSE;

	passwordLen = length / sizeof(WCHAR);
	if (length > 0)
		password = Stream_PointerAs(s, const WCHAR);

	if (!Stream_SafeSeek(s, length))
		return FALSE;

	return sspi_SetAuthIdentityWithLengthW(nla->identity, user, userLen, domain, domainLen,
	                                       password, passwordLen) > 0;
}

static BOOL nla_read_ts_credentials(rdpNla* nla, SecBuffer* data, size_t offset)
{
	wStream* s;
	size_t length;
	size_t ts_password_creds_length = 0;
	BOOL ret = FALSE;

	WINPR_ASSERT(nla);
	if (!data)
		return FALSE;

	s = Stream_New(data->pvBuffer, data->cbBuffer);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	if (!Stream_SafeSeek(s, offset))
		goto fail;

	/* TSCredentials (SEQUENCE) */
	ret = ber_read_sequence_tag(s, &length) &&
	      /* [0] credType (INTEGER) */
	      ber_read_contextual_tag(s, 0, &length, TRUE) && ber_read_integer(s, NULL) &&
	      /* [1] credentials (OCTET STRING) */
	      ber_read_contextual_tag(s, 1, &length, TRUE) &&
	      ber_read_octet_string_tag(s, &ts_password_creds_length) &&
	      nla_read_ts_password_creds(nla, s);
fail:
	Stream_Free(s, FALSE);
	return ret;
}

/**
 * Encode TSCredentials structure.
 * @param credssp
 */

static BOOL nla_encode_ts_credentials(rdpNla* nla)
{
	wStream* credsContentStream;
	wStream staticRetStream;
	wStream* s;
	size_t length;
	rdpSettings* settings;
	BOOL ret = FALSE;
	TSCredentials_t cr = { 0 };

	WINPR_ASSERT(nla);
	WINPR_ASSERT(nla->rdpcontext);

	settings = nla->rdpcontext->settings;
	WINPR_ASSERT(settings);

	if (settings->SmartcardLogon)
	{
		TSSmartCardCreds_t smartcardCreds = { 0 };
		TSCspDataDetail_t cspData = { 0 };
		char* Password = freerdp_settings_get_string_writable(settings, FreeRDP_Password);

		smartcardCreds.pin = Password ? Password : "";

		/*smartcardCreds.userHint = settings->UserHint;
		smartcardCreds.domainHint = settings->DomainHint;*/
		smartcardCreds.cspData = &cspData;

		cspData.keySpec = freerdp_settings_get_uint32(settings, FreeRDP_KeySpec);
		cspData.cspName = freerdp_settings_get_string_writable(settings, FreeRDP_CspName);
		cspData.readerName = freerdp_settings_get_string_writable(settings, FreeRDP_ReaderName);
		cspData.cardName = freerdp_settings_get_string_writable(settings, FreeRDP_CardName);
		cspData.containerName =
		    freerdp_settings_get_string_writable(settings, FreeRDP_ContainerName);

		length = ber_sizeof_nla_TSSmartCardCreds(&smartcardCreds);
		credsContentStream = Stream_New(NULL, length);
		if (!credsContentStream)
			return FALSE;

		if (ber_write_nla_TSSmartCardCreds(credsContentStream, &smartcardCreds) == 0)
			return FALSE;

		cr.credType = 2;
	}
	else
	{
		TSPasswordCreds_t passCreds = { 0 };

		if (!settings->DisableCredentialsDelegation && nla->identity)
		{
			passCreds.userNameLen = nla->identity->UserLength * 2;
			passCreds.userName = (BYTE*)nla->identity->User;

			passCreds.domainNameLen = nla->identity->DomainLength * 2;
			passCreds.domainName = (BYTE*)nla->identity->Domain;

			passCreds.passwordLen = nla->identity->PasswordLength * 2;
			passCreds.password = (BYTE*)nla->identity->Password;
		}

		length = ber_sizeof_nla_TSPasswordCreds(&passCreds);
		credsContentStream = Stream_New(NULL, length);
		if (!credsContentStream)
			return FALSE;

		ber_write_nla_TSPasswordCreds(credsContentStream, &passCreds);

		cr.credType = 1;
	}

	cr.credentialsLen = length;
	cr.credentials = Stream_Buffer(credsContentStream);

	length = ber_sizeof_nla_TSCredentials(&cr);
	if (!nla_sec_buffer_alloc(&nla->tsCredentials, length))
	{
		WLog_ERR(TAG, "sspi_SecBufferAlloc failed!");
		goto out;
	}

	s = Stream_StaticInit(&staticRetStream, (BYTE*)nla->tsCredentials.pvBuffer, length);
	ber_write_nla_TSCredentials(s, &cr);
	ret = TRUE;

out:
	Stream_Free(credsContentStream, TRUE);
	return ret;
}

static SECURITY_STATUS nla_encrypt_ts_credentials(rdpNla* nla)
{
	SECURITY_STATUS status;

	WINPR_ASSERT(nla);

	if (!nla_encode_ts_credentials(nla))
		return SEC_E_INSUFFICIENT_MEMORY;

	if (!nla_sec_buffer_alloc_from_buffer(&nla->authInfo, &nla->tsCredentials,
	                                      nla->ContextSizes.cbSecurityTrailer))
		return SEC_E_INSUFFICIENT_MEMORY;

	status = nla_encrypt(nla, &nla->authInfo, nla->ContextSizes.cbSecurityTrailer);
	if (status != SEC_E_OK)
		return status;

	return SEC_E_OK;
}

static SECURITY_STATUS nla_decrypt_ts_credentials(rdpNla* nla)
{
	SECURITY_STATUS status;

	WINPR_ASSERT(nla);

	if (nla->authInfo.cbBuffer < 1)
	{
		WLog_ERR(TAG, "nla_decrypt_ts_credentials missing authInfo buffer");
		return SEC_E_INVALID_TOKEN;
	}

	status = nla_decrypt(nla, &nla->authInfo, nla->ContextSizes.cbSecurityTrailer);
	if (status != SEC_E_OK)
		return status;

	if (!nla_read_ts_credentials(nla, &nla->authInfo, nla->ContextSizes.cbSecurityTrailer))
		return SEC_E_INSUFFICIENT_MEMORY;

	return SEC_E_OK;
}

static size_t nla_sizeof_nego_token(size_t length)
{
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

static size_t nla_sizeof_nego_tokens(const SecBuffer* buffer)
{
	WINPR_ASSERT(buffer);

	size_t length = buffer->cbBuffer;
	if (length == 0)
		return 0;
	length = nla_sizeof_nego_token(length);
	length += ber_sizeof_sequence_tag(length);
	length += ber_sizeof_sequence_tag(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

static size_t nla_sizeof_pub_key_auth(const SecBuffer* buffer)
{
	WINPR_ASSERT(buffer);

	size_t length = buffer->cbBuffer;
	if (length == 0)
		return 0;
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

static size_t nla_sizeof_auth_info(const SecBuffer* buffer)
{
	WINPR_ASSERT(buffer);

	size_t length = buffer->cbBuffer;
	if (length == 0)
		return 0;
	length = ber_sizeof_octet_string(length);
	length += ber_sizeof_contextual_tag(length);
	return length;
}

static size_t nla_sizeof_client_nonce(const SecBuffer* buffer)
{
	WINPR_ASSERT(buffer);

	size_t length = buffer->cbBuffer;
	if (length == 0)
		return 0;
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

static BOOL nla_client_write_nego_token(wStream* s, const SecBuffer* negoToken)
{
	const size_t nego_tokens_length = nla_sizeof_nego_tokens(negoToken);

	WINPR_ASSERT(s);

	if (Stream_GetRemainingCapacity(s) < nego_tokens_length)
		return FALSE;

	if (nego_tokens_length > 0)
	{
		size_t length;

		WLog_DBG(TAG, "   ----->> nego token");
		length =
		    ber_write_contextual_tag(s, 1,
		                             ber_sizeof_sequence(ber_sizeof_sequence(
		                                 ber_sizeof_sequence_octet_string(negoToken->cbBuffer))),
		                             TRUE); /* NegoData */
		length +=
		    ber_write_sequence_tag(s, ber_sizeof_sequence(ber_sizeof_sequence_octet_string(
		                                  negoToken->cbBuffer))); /* SEQUENCE OF NegoDataItem */
		length += ber_write_sequence_tag(
		    s, ber_sizeof_sequence_octet_string(negoToken->cbBuffer)); /* NegoDataItem */
		length +=
		    ber_write_sequence_octet_string_from_secbuffer(s, 0, negoToken); /* OCTET STRING */

		if (length != nego_tokens_length)
			return FALSE;
	}

	return TRUE;
}
/**
 * Send CredSSP message.
 * @param credssp
 */

BOOL nla_send(rdpNla* nla)
{
	BOOL rc = FALSE;
	wStream* s;
	size_t length;
	size_t ts_request_length;
	size_t error_code_context_length = 0;
	size_t error_code_length = 0;

	WINPR_ASSERT(nla);

	const size_t nego_tokens_length = nla_sizeof_nego_tokens(&nla->negoToken);
	const size_t pub_key_auth_length = nla_sizeof_pub_key_auth(&nla->pubKeyAuth);
	const size_t auth_info_length = nla_sizeof_auth_info(&nla->authInfo);
	const size_t client_nonce_length = nla_sizeof_client_nonce(&nla->ClientNonce);

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

	WLog_DBG(TAG, "----->> sending...");
	/* TSRequest */
	ber_write_sequence_tag(s, ts_request_length); /* SEQUENCE */
	/* [0] version */
	ber_write_contextual_tag(s, 0, 3, TRUE);

	WLog_DBG(TAG, "   ----->> protocol version %" PRIu32, nla->version);
	ber_write_integer(s, nla->version); /* INTEGER */

	/* [1] negoTokens (NegoData) */
	if (!nla_client_write_nego_token(s, &nla->negoToken))
		goto fail;

	/* [2] authInfo (OCTET STRING) */
	if (auth_info_length > 0)
	{
		WLog_DBG(TAG, "   ----->> auth info");
		if (ber_write_sequence_octet_string_from_secbuffer(s, 2, &nla->authInfo) !=
		    auth_info_length)
			goto fail;
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (pub_key_auth_length > 0)
	{
		WLog_DBG(TAG, "   ----->> public key auth");
		if (ber_write_sequence_octet_string_from_secbuffer(s, 3, &nla->pubKeyAuth) !=
		    pub_key_auth_length)
			goto fail;
	}

	/* [4] errorCode (INTEGER) */
	if (error_code_length > 0)
	{
		char buffer[1024];
		WLog_DBG(TAG, "   ----->> error code %s 0x%08" PRIx32,
		         winpr_strerror(nla->errorCode, buffer, sizeof(buffer)), nla->errorCode);
		ber_write_contextual_tag(s, 4, error_code_length, TRUE);
		ber_write_integer(s, nla->errorCode);
	}

	/* [5] clientNonce (OCTET STRING) */
	if (client_nonce_length > 0)
	{
		WLog_DBG(TAG, "   ----->> client nonce");
		if (ber_write_sequence_octet_string_from_secbuffer(s, 5, &nla->ClientNonce) !=
		    client_nonce_length)
			goto fail;
	}

	WLog_DBG(TAG, "[%" PRIuz " bytes]", Stream_GetPosition(s));
	if (transport_write(nla->transport, s) < 0)
		goto fail;
	rc = TRUE;

fail:
	Stream_Free(s, TRUE);
	return rc;
}

static int nla_decode_ts_request(rdpNla* nla, wStream* s)
{
	int rc = -1;
	size_t length;
	UINT32 version = 0;

	WINPR_ASSERT(nla);
	WINPR_ASSERT(s);

	WLog_DBG(TAG, "<<----- receiving...");

	/* TSRequest */
	if (!ber_read_sequence_tag(s, &length) || !ber_read_contextual_tag(s, 0, &length, TRUE) ||
	    !ber_read_integer(s, &version))
	{
		goto fail;
	}

	WLog_DBG(TAG, "   <<----- protocol version %" PRIu32, version);
	if (nla->peerVersion == 0)
		nla->peerVersion = version;

	/* if the peer suddenly changed its version - kick it */
	if (nla->peerVersion != version)
	{
		WLog_ERR(TAG, "CredSSP peer changed protocol version from %" PRIu32 " to %" PRIu32,
		         nla->peerVersion, version);
		goto fail;
	}

	/* [1] negoTokens (NegoData) */
	if (ber_read_contextual_tag(s, 1, &length, TRUE) != FALSE)
	{
		WLog_DBG(TAG, "   <<----- nego token");
		if (!ber_read_sequence_tag(s, &length) || /* SEQUENCE OF NegoDataItem */
		    !ber_read_sequence_tag(s, &length) || /* NegoDataItem */
		    !ber_read_contextual_tag(s, 0, &length, TRUE))
		{
			goto fail;
		}

		if (!nla_decode_to_buffer(s, &nla->negoToken))
			goto fail;
	}

	/* [2] authInfo (OCTET STRING) */
	if (ber_read_contextual_tag(s, 2, &length, TRUE) != FALSE)
	{
		WLog_DBG(TAG, "   <<----- auth info");
		if (!nla_decode_to_buffer(s, &nla->authInfo))
			goto fail;
	}

	/* [3] pubKeyAuth (OCTET STRING) */
	if (ber_read_contextual_tag(s, 3, &length, TRUE) != FALSE)
	{
		WLog_DBG(TAG, "   <<----- public key info");
		if (!nla_decode_to_buffer(s, &nla->pubKeyAuth))
			goto fail;
	}

	/* [4] errorCode (INTEGER) */
	if (nla->peerVersion >= 3)
	{
		if (ber_read_contextual_tag(s, 4, &length, TRUE) != FALSE)
		{
			char buffer[1024];
			if (!ber_read_integer(s, &nla->errorCode))
				goto fail;
			WLog_DBG(TAG, "   <<----- error code %s 0x%08" PRIx32,
			         winpr_strerror(nla->errorCode, buffer, sizeof(buffer)), nla->errorCode);
		}

		if (nla->peerVersion >= 5)
		{
			if (ber_read_contextual_tag(s, 5, &length, TRUE) != FALSE)
			{
				WLog_DBG(TAG, "   <<----- client nonce");
				if (!nla_decode_to_buffer(s, &nla->ClientNonce))
					goto fail;
			}
		}
	}
	rc = 1;
fail:

	return rc;
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

void nla_buffer_free(rdpNla* nla)
{
	WINPR_ASSERT(nla);
	sspi_SecBufferFree(&nla->negoToken);
	sspi_SecBufferFree(&nla->pubKeyAuth);
	sspi_SecBufferFree(&nla->authInfo);
}

LPTSTR nla_make_spn(const char* ServiceClass, const char* hostname)
{
	DWORD status = ERROR_INTERNAL_ERROR;
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
		goto fail;

	if (!ServiceClass)
	{
		ServicePrincipalName = (LPTSTR)_tcsdup(hostnameX);
		status = ERROR_SUCCESS;
		goto fail;
	}

	SpnLength = 0;
	status = DsMakeSpn(ServiceClassX, hostnameX, NULL, 0, NULL, &SpnLength, NULL);

	if (status != ERROR_BUFFER_OVERFLOW)
		goto fail;

	ServicePrincipalName = (LPTSTR)calloc(SpnLength, sizeof(TCHAR));

	if (!ServicePrincipalName)
		goto fail;

	status = DsMakeSpn(ServiceClassX, hostnameX, NULL, 0, NULL, &SpnLength, ServicePrincipalName);

	if (status != ERROR_SUCCESS)
		goto fail;

fail:
	if (status != ERROR_SUCCESS)
	{
		free(ServicePrincipalName);
		ServicePrincipalName = NULL;
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

	nla->identity = (SEC_WINNT_AUTH_IDENTITY*)&nla->identityWinPr;
	nla->identity->Flags = SEC_WINNT_AUTH_IDENTITY_EXTENDED;
	nla->rdpcontext = context;
	nla->server = settings->ServerMode;
	nla->transport = transport;
	nla->sendSeqNum = 0;
	nla->recvSeqNum = 0;
	nla->version = 6;
	nla->kerberosSettings = &nla->identityWinPr.kerberosSettings;
	SecInvalidateHandle(&nla->context);

	if (settings->NtlmSamFile)
	{
		nla->SamFile = _strdup(settings->NtlmSamFile);

		if (!nla->SamFile)
			goto cleanup;
	}

	if (settings->SspiModule)
	{
		nla->SspiModule = _strdup(settings->SspiModule);

		if (!nla->SspiModule)
			goto cleanup;
	}

	/* init to 0 or we end up freeing a bad pointer if the alloc fails */
	if (!nla_sec_buffer_alloc(&nla->ClientNonce, NonceLength))
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

	free(nla->SspiModule);
	nla->SspiModule = NULL;

	nla_buffer_free(nla);
	sspi_SecBufferFree(&nla->ClientNonce);
	sspi_SecBufferFree(&nla->PublicKey);
	sspi_SecBufferFree(&nla->tsCredentials);

	free(nla->ServicePrincipalName);
	free(nla->kerberosSettings->armorCache);
	free(nla->kerberosSettings->cache);
	free(nla->kerberosSettings->pkinitX509Anchors);
	free(nla->kerberosSettings->pkinitX509Identity);
	sspi_FreeAuthIdentity(nla->identity);
	nla_set_package_name(nla, NULL);
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

BOOL nla_set_service_principal(rdpNla* nla, LPTSTR principal)
{
	if (!nla || !principal)
		return FALSE;

	nla->ServicePrincipalName = principal;
	return TRUE;
}

BOOL nla_set_sspi_module(rdpNla* nla, const char* sspiModule)
{
	if (!nla)
		return FALSE;

	if (nla->SspiModule)
	{
		free(nla->SspiModule);
		nla->SspiModule = NULL;
	}

	if (!sspiModule)
		return TRUE;

#ifdef UNICODE
	ConvertToUnicode(CP_UTF8, 0, sspiModule, -1, &nla->SspiModule, 0);
#else
	nla->SspiModule = _strdup(sspiModule);
#endif

	if (!nla->SspiModule)
		return FALSE;

	return TRUE;
}

BOOL nla_sspi_module_init(rdpNla* nla)
{
	if (!nla)
		return FALSE;

	if (nla->SspiModule)
	{
		INIT_SECURITY_INTERFACE pInitSecurityInterface;
		HMODULE hSSPI = LoadLibraryX(nla->SspiModule);

		if (!hSSPI)
		{
			WLog_ERR(TAG, "Failed to load SSPI module: %s", nla->SspiModule);
			return FALSE;
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
