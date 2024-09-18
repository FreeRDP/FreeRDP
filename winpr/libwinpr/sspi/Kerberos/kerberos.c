/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Kerberos Auth Protocol
 *
 * Copyright 2015 ANSSI, Author Thomas Calderon
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
 * Copyright 2022 Isaac Klein <fifthdegree@protonmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <winpr/config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include <winpr/assert.h>
#include <winpr/asn1.h>
#include <winpr/crt.h>
#include <winpr/interlocked.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/tchar.h>
#include <winpr/sysinfo.h>
#include <winpr/registry.h>
#include <winpr/endian.h>
#include <winpr/crypto.h>
#include <winpr/path.h>
#include <winpr/wtypes.h>
#include <winpr/winsock.h>
#include <winpr/schannel.h>
#include <winpr/secapi.h>

#include "kerberos.h"

#ifdef WITH_KRB5_MIT
#include "krb5glue.h"
#include <profile.h>
#endif

#ifdef WITH_KRB5_HEIMDAL
#include "krb5glue.h"
#include <krb5-protos.h>
#endif

#include "../sspi.h"
#include "../../log.h"
#define TAG WINPR_TAG("sspi.Kerberos")

#define KRB_TGT_REQ 16
#define KRB_TGT_REP 17

const SecPkgInfoA KERBEROS_SecPkgInfoA = {
	0x000F3BBF,                 /* fCapabilities */
	1,                          /* wVersion */
	0x0010,                     /* wRPCID */
	0x0000BB80,                 /* cbMaxToken : 48k bytes maximum for Windows Server 2012 */
	"Kerberos",                 /* Name */
	"Kerberos Security Package" /* Comment */
};

static WCHAR KERBEROS_SecPkgInfoW_NameBuffer[32] = { 0 };
static WCHAR KERBEROS_SecPkgInfoW_CommentBuffer[32] = { 0 };

const SecPkgInfoW KERBEROS_SecPkgInfoW = {
	0x000F3BBF,                        /* fCapabilities */
	1,                                 /* wVersion */
	0x0010,                            /* wRPCID */
	0x0000BB80,                        /* cbMaxToken : 48k bytes maximum for Windows Server 2012 */
	KERBEROS_SecPkgInfoW_NameBuffer,   /* Name */
	KERBEROS_SecPkgInfoW_CommentBuffer /* Comment */
};

#ifdef WITH_KRB5

enum KERBEROS_STATE
{
	KERBEROS_STATE_INITIAL,
	KERBEROS_STATE_TGT_REQ,
	KERBEROS_STATE_TGT_REP,
	KERBEROS_STATE_AP_REQ,
	KERBEROS_STATE_AP_REP,
	KERBEROS_STATE_FINAL
};

typedef struct KRB_CREDENTIALS_st
{
	volatile LONG refCount;
	krb5_context ctx;
	char* kdc_url;
	krb5_ccache ccache;
	krb5_keytab keytab;
	krb5_keytab client_keytab;
	BOOL own_ccache; /**< Whether we created ccache, and must destroy it after use.  */
} KRB_CREDENTIALS;

struct s_KRB_CONTEXT
{
	enum KERBEROS_STATE state;
	KRB_CREDENTIALS* credentials;
	krb5_auth_context auth_ctx;
	BOOL acceptor;
	uint32_t flags;
	uint64_t local_seq;
	uint64_t remote_seq;
	struct krb5glue_keyset keyset;
	BOOL u2u;
	char* targetHost;
};

static const WinPrAsn1_OID kerberos_OID = { 9, (void*)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02" };
static const WinPrAsn1_OID kerberos_u2u_OID = { 10,
	                                            (void*)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02\x03" };

#define krb_log_exec(fkt, ctx, ...) \
	kerberos_log_msg(ctx, fkt(ctx, ##__VA_ARGS__), #fkt, __FILE__, __func__, __LINE__)
#define krb_log_exec_ptr(fkt, ctx, ...) \
	kerberos_log_msg(*ctx, fkt(ctx, ##__VA_ARGS__), #fkt, __FILE__, __func__, __LINE__)
static krb5_error_code kerberos_log_msg(krb5_context ctx, krb5_error_code code, const char* what,
                                        const char* file, const char* fkt, size_t line)
{
	switch (code)
	{
		case 0:
		case KRB5_KT_END:
			break;
		default:
		{
			const DWORD level = WLOG_ERROR;

			wLog* log = WLog_Get(TAG);
			if (WLog_IsLevelActive(log, level))
			{
				const char* msg = krb5_get_error_message(ctx, code);
				WLog_PrintMessage(log, WLOG_MESSAGE_TEXT, level, line, file, fkt, "%s (%s [%d])",
				                  what, msg, code);
				krb5_free_error_message(ctx, msg);
			}
		}
		break;
	}
	return code;
}

static void credentials_unref(KRB_CREDENTIALS* credentials);

static void kerberos_ContextFree(KRB_CONTEXT* ctx, BOOL allocated)
{
	if (!ctx)
		return;

	free(ctx->targetHost);
	ctx->targetHost = NULL;

	if (ctx->credentials)
	{
		krb5_context krbctx = ctx->credentials->ctx;
		if (krbctx)
		{
			if (ctx->auth_ctx)
				krb5_auth_con_free(krbctx, ctx->auth_ctx);

			krb5glue_keys_free(krbctx, &ctx->keyset);
		}

		credentials_unref(ctx->credentials);
	}

	if (allocated)
		free(ctx);
}

static KRB_CONTEXT* kerberos_ContextNew(KRB_CREDENTIALS* credentials)
{
	KRB_CONTEXT* context = NULL;

	context = (KRB_CONTEXT*)calloc(1, sizeof(KRB_CONTEXT));
	if (!context)
		return NULL;

	context->credentials = credentials;
	InterlockedIncrement(&credentials->refCount);
	return context;
}

static krb5_error_code krb5_prompter(krb5_context context, void* data, const char* name,
                                     const char* banner, int num_prompts, krb5_prompt prompts[])
{
	for (int i = 0; i < num_prompts; i++)
	{
		krb5_prompt_type type = krb5glue_get_prompt_type(context, prompts, i);
		if (type && (type == KRB5_PROMPT_TYPE_PREAUTH || type == KRB5_PROMPT_TYPE_PASSWORD) && data)
		{
			prompts[i].reply->data = _strdup((const char*)data);
			prompts[i].reply->length = strlen((const char*)data);
		}
	}
	return 0;
}

static INLINE krb5glue_key get_key(struct krb5glue_keyset* keyset)
{
	return keyset->acceptor_key    ? keyset->acceptor_key
	       : keyset->initiator_key ? keyset->initiator_key
	                               : keyset->session_key;
}

static BOOL isValidIPv4(const char* ipAddress)
{
	struct sockaddr_in sa = { 0 };
	int result = inet_pton(AF_INET, ipAddress, &(sa.sin_addr));
	return result != 0;
}

static BOOL isValidIPv6(const char* ipAddress)
{
	struct sockaddr_in6 sa = { 0 };
	int result = inet_pton(AF_INET6, ipAddress, &(sa.sin6_addr));
	return result != 0;
}

static BOOL isValidIP(const char* ipAddress)
{
	return isValidIPv4(ipAddress) || isValidIPv6(ipAddress);
}

static int build_krbtgt(krb5_context ctx, krb5_data* realm, krb5_principal* ptarget)
{
	/* "krbtgt/" + realm + "@" + realm */
	size_t len = 0;
	char* name = NULL;
	krb5_error_code rv = KRB5_CC_NOMEM;

	(void)winpr_asprintf(&name, &len, "krbtgt/%s@%s", realm->data, realm->data);
	if (!name || (len == 0))
		goto fail;

	krb5_principal target = { 0 };
	rv = krb5_parse_name(ctx, name, &target);
	*ptarget = target;
fail:
	free(name);
	return rv;
}

#endif /* WITH_KRB5 */

static SECURITY_STATUS SEC_ENTRY kerberos_AcquireCredentialsHandleA(
    SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
#ifdef WITH_KRB5
	SEC_WINPR_KERBEROS_SETTINGS* krb_settings = NULL;
	KRB_CREDENTIALS* credentials = NULL;
	krb5_context ctx = NULL;
	krb5_ccache ccache = NULL;
	krb5_keytab keytab = NULL;
	krb5_principal principal = NULL;
	char* domain = NULL;
	char* username = NULL;
	char* password = NULL;
	BOOL own_ccache = FALSE;
	const char* const default_ccache_type = "MEMORY";

	if (pAuthData)
	{
		UINT32 identityFlags = sspi_GetAuthIdentityFlags(pAuthData);

		if (identityFlags & SEC_WINNT_AUTH_IDENTITY_EXTENDED)
			krb_settings = (((SEC_WINNT_AUTH_IDENTITY_WINPR*)pAuthData)->kerberosSettings);

		if (!sspi_CopyAuthIdentityFieldsA((const SEC_WINNT_AUTH_IDENTITY_INFO*)pAuthData, &username,
		                                  &domain, &password))
		{
			WLog_ERR(TAG, "Failed to copy auth identity fields");
			goto cleanup;
		}

		if (!pszPrincipal)
			pszPrincipal = username;
	}

	if (krb_log_exec_ptr(krb5_init_context, &ctx))
		goto cleanup;

	if (domain)
	{
		char* udomain = _strdup(domain);
		if (!udomain)
			goto cleanup;

		CharUpperA(udomain);
		/* Will use domain if realm is not specified in username */
		krb5_error_code rv = krb_log_exec(krb5_set_default_realm, ctx, udomain);
		free(udomain);

		if (rv)
			goto cleanup;
	}

	if (pszPrincipal)
	{
		char* cpszPrincipal = _strdup(pszPrincipal);
		if (!cpszPrincipal)
			goto cleanup;

		/* Find realm component if included and convert to uppercase */
		char* p = strchr(cpszPrincipal, '@');
		if (p)
			CharUpperA(p);

		krb5_error_code rv = krb_log_exec(krb5_parse_name, ctx, cpszPrincipal, &principal);
		free(cpszPrincipal);

		if (rv)
			goto cleanup;
	}

	if (krb_settings && krb_settings->cache)
	{
		if ((krb_log_exec(krb5_cc_set_default_name, ctx, krb_settings->cache)))
			goto cleanup;
	}
	else
		own_ccache = TRUE;

	if (principal)
	{
		/* Use the default cache if it's initialized with the right principal */
		if (krb5_cc_cache_match(ctx, principal, &ccache) == KRB5_CC_NOTFOUND)
		{
			if (own_ccache)
			{
				if (krb_log_exec(krb5_cc_new_unique, ctx, default_ccache_type, 0, &ccache))
					goto cleanup;
			}
			else
			{
				if (krb_log_exec(krb5_cc_resolve, ctx, krb_settings->cache, &ccache))
					goto cleanup;
			}

			if (krb_log_exec(krb5_cc_initialize, ctx, ccache, principal))
				goto cleanup;
		}
		else
			own_ccache = FALSE;
	}
	else if (fCredentialUse & SECPKG_CRED_OUTBOUND)
	{
		/* Use the default cache with it's default principal */
		if (krb_log_exec(krb5_cc_default, ctx, &ccache))
			goto cleanup;
		if (krb_log_exec(krb5_cc_get_principal, ctx, ccache, &principal))
			goto cleanup;
		own_ccache = FALSE;
	}
	else
	{
		if (own_ccache)
		{
			if (krb_log_exec(krb5_cc_new_unique, ctx, default_ccache_type, 0, &ccache))
				goto cleanup;
		}
		else
		{
			if (krb_log_exec(krb5_cc_resolve, ctx, krb_settings->cache, &ccache))
				goto cleanup;
		}
	}

	if (krb_settings && krb_settings->keytab)
	{
		if (krb_log_exec(krb5_kt_resolve, ctx, krb_settings->keytab, &keytab))
			goto cleanup;
	}
	else
	{
		if (fCredentialUse & SECPKG_CRED_INBOUND)
			if (krb_log_exec(krb5_kt_default, ctx, &keytab))
				goto cleanup;
	}

	/* Get initial credentials if required */
	if (fCredentialUse & SECPKG_CRED_OUTBOUND)
	{
		krb5_creds creds = { 0 };
		krb5_creds matchCreds = { 0 };
		int matchFlags = KRB5_TC_MATCH_TIMES;

		krb5_timeofday(ctx, &matchCreds.times.endtime);
		matchCreds.times.endtime += 60;
		matchCreds.client = principal;

		if (krb_log_exec(build_krbtgt, ctx, &principal->realm, &matchCreds.server))
			goto cleanup;

		int rv = krb5_cc_retrieve_cred(ctx, ccache, matchFlags, &matchCreds, &creds);
		krb5_free_principal(ctx, matchCreds.server);
		krb5_free_cred_contents(ctx, &creds);
		if (rv)
		{
			if (krb_log_exec(krb5glue_get_init_creds, ctx, principal, ccache, krb5_prompter,
			                 password, krb_settings))
				goto cleanup;
		}
	}

	credentials = calloc(1, sizeof(KRB_CREDENTIALS));
	if (!credentials)
		goto cleanup;
	credentials->refCount = 1;
	credentials->ctx = ctx;
	credentials->ccache = ccache;
	credentials->keytab = keytab;
	credentials->own_ccache = own_ccache;

cleanup:

	free(domain);
	free(username);
	free(password);

	if (principal)
		krb5_free_principal(ctx, principal);
	if (ctx)
	{
		if (!credentials)
		{
			if (ccache)
			{
				if (own_ccache)
					krb5_cc_destroy(ctx, ccache);
				else
					krb5_cc_close(ctx, ccache);
			}
			if (keytab)
				krb5_kt_close(ctx, keytab);

			krb5_free_context(ctx);
		}
	}

	/* If we managed to get credentials set the output */
	if (credentials)
	{
		sspi_SecureHandleSetLowerPointer(phCredential, (void*)credentials);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*)KERBEROS_SSP_NAME);
		return SEC_E_OK;
	}

	return SEC_E_NO_CREDENTIALS;
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif
}

static SECURITY_STATUS SEC_ENTRY kerberos_AcquireCredentialsHandleW(
    SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status = SEC_E_INSUFFICIENT_MEMORY;
	char* principal = NULL;
	char* package = NULL;

	if (pszPrincipal)
	{
		principal = ConvertWCharToUtf8Alloc(pszPrincipal, NULL);
		if (!principal)
			goto fail;
	}
	if (pszPackage)
	{
		package = ConvertWCharToUtf8Alloc(pszPackage, NULL);
		if (!package)
			goto fail;
	}

	status =
	    kerberos_AcquireCredentialsHandleA(principal, package, fCredentialUse, pvLogonID, pAuthData,
	                                       pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

fail:
	free(principal);
	free(package);

	return status;
}

#ifdef WITH_KRB5
static void credentials_unref(KRB_CREDENTIALS* credentials)
{
	WINPR_ASSERT(credentials);

	if (InterlockedDecrement(&credentials->refCount))
		return;

	free(credentials->kdc_url);

	if (credentials->ccache)
	{
		if (credentials->own_ccache)
			krb5_cc_destroy(credentials->ctx, credentials->ccache);
		else
			krb5_cc_close(credentials->ctx, credentials->ccache);
	}
	if (credentials->keytab)
		krb5_kt_close(credentials->ctx, credentials->keytab);

	krb5_free_context(credentials->ctx);
	free(credentials);
}
#endif

static SECURITY_STATUS SEC_ENTRY kerberos_FreeCredentialsHandle(PCredHandle phCredential)
{
#ifdef WITH_KRB5
	KRB_CREDENTIALS* credentials = sspi_SecureHandleGetLowerPointer(phCredential);
	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	credentials_unref(credentials);

	sspi_SecureHandleInvalidate(phCredential);
	return SEC_E_OK;
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif
}

static SECURITY_STATUS SEC_ENTRY kerberos_QueryCredentialsAttributesW(PCredHandle phCredential,
                                                                      ULONG ulAttribute,
                                                                      void* pBuffer)
{
#ifdef WITH_KRB5
	switch (ulAttribute)
	{
		case SECPKG_CRED_ATTR_NAMES:
			return SEC_E_OK;
		default:
			WLog_ERR(TAG, "TODO: QueryCredentialsAttributesW, implement ulAttribute=%08" PRIx32,
			         ulAttribute);
			return SEC_E_UNSUPPORTED_FUNCTION;
	}

#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif
}

static SECURITY_STATUS SEC_ENTRY kerberos_QueryCredentialsAttributesA(PCredHandle phCredential,
                                                                      ULONG ulAttribute,
                                                                      void* pBuffer)
{
	return kerberos_QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);
}

#ifdef WITH_KRB5

static BOOL kerberos_mk_tgt_token(SecBuffer* buf, int msg_type, char* sname, char* host,
                                  const krb5_data* ticket)
{
	WinPrAsn1Encoder* enc = NULL;
	WinPrAsn1_MemoryChunk data;
	wStream s;
	size_t len = 0;
	sspi_gss_data token;
	BOOL ret = FALSE;

	WINPR_ASSERT(buf);

	if (msg_type != KRB_TGT_REQ && msg_type != KRB_TGT_REP)
		return FALSE;
	if (msg_type == KRB_TGT_REP && !ticket)
		return FALSE;

	enc = WinPrAsn1Encoder_New(WINPR_ASN1_DER);
	if (!enc)
		return FALSE;

	/* KERB-TGT-REQUEST (SEQUENCE) */
	if (!WinPrAsn1EncSeqContainer(enc))
		goto cleanup;

	/* pvno [0] INTEGER */
	if (!WinPrAsn1EncContextualInteger(enc, 0, 5))
		goto cleanup;

	/* msg-type [1] INTEGER */
	if (!WinPrAsn1EncContextualInteger(enc, 1, msg_type))
		goto cleanup;

	if (msg_type == KRB_TGT_REQ && sname)
	{
		/* server-name [2] PrincipalName (SEQUENCE) */
		if (!WinPrAsn1EncContextualSeqContainer(enc, 2))
			goto cleanup;

		/* name-type [0] INTEGER */
		if (!WinPrAsn1EncContextualInteger(enc, 0, KRB5_NT_SRV_HST))
			goto cleanup;

		/* name-string [1] SEQUENCE OF GeneralString */
		if (!WinPrAsn1EncContextualSeqContainer(enc, 1))
			goto cleanup;

		if (!WinPrAsn1EncGeneralString(enc, sname))
			goto cleanup;

		if (host && !WinPrAsn1EncGeneralString(enc, host))
			goto cleanup;

		if (!WinPrAsn1EncEndContainer(enc) || !WinPrAsn1EncEndContainer(enc))
			goto cleanup;
	}
	else if (msg_type == KRB_TGT_REP)
	{
		/* ticket [2] Ticket */
		data.data = (BYTE*)ticket->data;
		data.len = ticket->length;
		if (!WinPrAsn1EncContextualRawContent(enc, 2, &data))
			goto cleanup;
	}

	if (!WinPrAsn1EncEndContainer(enc))
		goto cleanup;

	if (!WinPrAsn1EncStreamSize(enc, &len) || len > buf->cbBuffer)
		goto cleanup;

	Stream_StaticInit(&s, buf->pvBuffer, len);
	if (!WinPrAsn1EncToStream(enc, &s))
		goto cleanup;

	token.data = buf->pvBuffer;
	token.length = (UINT)len;
	if (sspi_gss_wrap_token(buf, &kerberos_u2u_OID,
	                        msg_type == KRB_TGT_REQ ? TOK_ID_TGT_REQ : TOK_ID_TGT_REP, &token))
		ret = TRUE;

cleanup:
	WinPrAsn1Encoder_Free(&enc);
	return ret;
}

static BOOL kerberos_rd_tgt_token(const sspi_gss_data* token, char** target, krb5_data* ticket)
{
	WinPrAsn1Decoder dec;
	WinPrAsn1Decoder dec2;
	BOOL error = 0;
	WinPrAsn1_tagId tag = 0;
	WinPrAsn1_INTEGER val = 0;
	size_t len = 0;
	wStream s;
	char* buf = NULL;
	char* str = NULL;

	WINPR_ASSERT(token);

	WinPrAsn1Decoder_InitMem(&dec, WINPR_ASN1_DER, (BYTE*)token->data, token->length);

	/* KERB-TGT-REQUEST (SEQUENCE) */
	if (!WinPrAsn1DecReadSequence(&dec, &dec2))
		return FALSE;
	dec = dec2;

	/* pvno [0] INTEGER */
	if (!WinPrAsn1DecReadContextualInteger(&dec, 0, &error, &val) || val != 5)
		return FALSE;

	/* msg-type [1] INTEGER */
	if (!WinPrAsn1DecReadContextualInteger(&dec, 1, &error, &val))
		return FALSE;

	if (val == KRB_TGT_REQ)
	{
		if (!target)
			return FALSE;
		*target = NULL;

		s = WinPrAsn1DecGetStream(&dec);
		len = Stream_Length(&s);
		if (len == 0)
			return TRUE;

		buf = malloc(len);
		if (!buf)
			return FALSE;

		*buf = 0;
		*target = buf;

		if (!WinPrAsn1DecReadContextualTag(&dec, &tag, &dec2))
			goto fail;

		if (tag == 2)
		{
			WinPrAsn1Decoder seq;
			/* server-name [2] PrincipalName (SEQUENCE) */
			if (!WinPrAsn1DecReadSequence(&dec2, &seq))
				goto fail;

			/* name-type [0] INTEGER */
			if (!WinPrAsn1DecReadContextualInteger(&seq, 0, &error, &val))
				goto fail;

			/* name-string [1] SEQUENCE OF GeneralString */
			if (!WinPrAsn1DecReadContextualSequence(&seq, 1, &error, &dec2))
				goto fail;

			while (WinPrAsn1DecPeekTag(&dec2, &tag))
			{
				if (!WinPrAsn1DecReadGeneralString(&dec2, &str))
					goto fail;

				if (buf != *target)
					*buf++ = '/';
				buf = stpcpy(buf, str);
				free(str);
			}

			if (!WinPrAsn1DecReadContextualTag(&dec, &tag, &dec2))
				return TRUE;
		}

		/* realm [3] Realm */
		if (tag != 3 || !WinPrAsn1DecReadGeneralString(&dec2, &str))
			goto fail;

		*buf++ = '@';
		strcpy(buf, str);
		free(str);
		return TRUE;
	}
	else if (val == KRB_TGT_REP)
	{
		if (!ticket)
			return FALSE;

		/* ticket [2] Ticket */
		if (!WinPrAsn1DecReadContextualTag(&dec, &tag, &dec2) || tag != 2)
			return FALSE;

		s = WinPrAsn1DecGetStream(&dec2);
		ticket->data = Stream_BufferAs(&s, char);
		ticket->length = Stream_Length(&s);
		return TRUE;
	}
	else
		return FALSE;

fail:
	free(buf);
	if (target)
		*target = NULL;
	return FALSE;
}

#endif /* WITH_KRB5 */

static BOOL kerberos_hash_channel_bindings(WINPR_DIGEST_CTX* md5, SEC_CHANNEL_BINDINGS* bindings)
{
	BYTE buf[4];

	Data_Write_UINT32(buf, bindings->dwInitiatorAddrType);
	if (!winpr_Digest_Update(md5, buf, 4))
		return FALSE;

	Data_Write_UINT32(buf, bindings->cbInitiatorLength);
	if (!winpr_Digest_Update(md5, buf, 4))
		return FALSE;

	if (bindings->cbInitiatorLength &&
	    !winpr_Digest_Update(md5, (BYTE*)bindings + bindings->dwInitiatorOffset,
	                         bindings->cbInitiatorLength))
		return FALSE;

	Data_Write_UINT32(buf, bindings->dwAcceptorAddrType);
	if (!winpr_Digest_Update(md5, buf, 4))
		return FALSE;

	Data_Write_UINT32(buf, bindings->cbAcceptorLength);
	if (!winpr_Digest_Update(md5, buf, 4))
		return FALSE;

	if (bindings->cbAcceptorLength &&
	    !winpr_Digest_Update(md5, (BYTE*)bindings + bindings->dwAcceptorOffset,
	                         bindings->cbAcceptorLength))
		return FALSE;

	Data_Write_UINT32(buf, bindings->cbApplicationDataLength);
	if (!winpr_Digest_Update(md5, buf, 4))
		return FALSE;

	if (bindings->cbApplicationDataLength &&
	    !winpr_Digest_Update(md5, (BYTE*)bindings + bindings->dwApplicationDataOffset,
	                         bindings->cbApplicationDataLength))
		return FALSE;

	return TRUE;
}

static SECURITY_STATUS SEC_ENTRY kerberos_InitializeSecurityContextA(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, ULONG* pfContextAttr, PTimeStamp ptsExpiry)
{
#ifdef WITH_KRB5
	PSecBuffer input_buffer = NULL;
	PSecBuffer output_buffer = NULL;
	PSecBuffer bindings_buffer = NULL;
	WINPR_DIGEST_CTX* md5 = NULL;
	char* target = NULL;
	char* sname = NULL;
	char* host = NULL;
	krb5_data input_token = { 0 };
	krb5_data output_token = { 0 };
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	WinPrAsn1_OID oid = { 0 };
	uint16_t tok_id = 0;
	krb5_ap_rep_enc_part* reply = NULL;
	krb5_flags ap_flags = AP_OPTS_USE_SUBKEY;
	char cksum_contents[24] = { 0 };
	krb5_data cksum = { 0 };
	krb5_creds in_creds = { 0 };
	krb5_creds* creds = NULL;
	BOOL isNewContext = FALSE;
	KRB_CONTEXT* context = NULL;
	KRB_CREDENTIALS* credentials = sspi_SecureHandleGetLowerPointer(phCredential);

	/* behave like windows SSPIs that don't want empty context */
	if (phContext && !phContext->dwLower && !phContext->dwUpper)
		return SEC_E_INVALID_HANDLE;

	context = sspi_SecureHandleGetLowerPointer(phContext);

	if (!credentials)
		return SEC_E_NO_CREDENTIALS;

	if (pInput)
	{
		input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);
		bindings_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_CHANNEL_BINDINGS);
	}
	if (pOutput)
		output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

	if (fContextReq & ISC_REQ_MUTUAL_AUTH)
		ap_flags |= AP_OPTS_MUTUAL_REQUIRED;

	if (fContextReq & ISC_REQ_USE_SESSION_KEY)
		ap_flags |= AP_OPTS_USE_SESSION_KEY;

	/* Split target name into service/hostname components */
	if (pszTargetName)
	{
		target = _strdup(pszTargetName);
		if (!target)
		{
			status = SEC_E_INSUFFICIENT_MEMORY;
			goto cleanup;
		}
		host = strchr(target, '/');
		if (host)
		{
			*host++ = 0;
			sname = target;
		}
		else
			host = target;
		if (isValidIP(host))
		{
			status = SEC_E_NO_CREDENTIALS;
			goto cleanup;
		}
	}

	if (!context)
	{
		context = kerberos_ContextNew(credentials);
		if (!context)
		{
			status = SEC_E_INSUFFICIENT_MEMORY;
			goto cleanup;
		}

		isNewContext = TRUE;

		if (host)
			context->targetHost = _strdup(host);
		if (!context->targetHost)
		{
			status = SEC_E_INSUFFICIENT_MEMORY;
			goto cleanup;
		}

		if (fContextReq & ISC_REQ_USE_SESSION_KEY)
		{
			context->state = KERBEROS_STATE_TGT_REQ;
			context->u2u = TRUE;
		}
		else
			context->state = KERBEROS_STATE_AP_REQ;
	}
	else
	{
		if (!input_buffer || !sspi_gss_unwrap_token(input_buffer, &oid, &tok_id, &input_token))
			goto bad_token;
		if ((context->u2u && !sspi_gss_oid_compare(&oid, &kerberos_u2u_OID)) ||
		    (!context->u2u && !sspi_gss_oid_compare(&oid, &kerberos_OID)))
			goto bad_token;
	}

	/* SSPI flags are compatible with GSS flags except INTEG_FLAG */
	context->flags |= (fContextReq & 0x1F);
	if ((fContextReq & ISC_REQ_INTEGRITY) && !(fContextReq & ISC_REQ_NO_INTEGRITY))
		context->flags |= SSPI_GSS_C_INTEG_FLAG;

	switch (context->state)
	{
		case KERBEROS_STATE_TGT_REQ:

			if (!kerberos_mk_tgt_token(output_buffer, KRB_TGT_REQ, sname, host, NULL))
				goto cleanup;

			context->state = KERBEROS_STATE_TGT_REP;
			status = SEC_I_CONTINUE_NEEDED;
			break;

		case KERBEROS_STATE_TGT_REP:

			if (tok_id != TOK_ID_TGT_REP)
				goto bad_token;

			if (!kerberos_rd_tgt_token(&input_token, NULL, &in_creds.second_ticket))
				goto bad_token;

			/* Continue to AP-REQ */
			/* fallthrough */
			WINPR_FALLTHROUGH

		case KERBEROS_STATE_AP_REQ:

			/* Set auth_context options */
			if (krb_log_exec(krb5_auth_con_init, credentials->ctx, &context->auth_ctx))
				goto cleanup;
			if (krb_log_exec(krb5_auth_con_setflags, credentials->ctx, context->auth_ctx,
			                 KRB5_AUTH_CONTEXT_DO_SEQUENCE | KRB5_AUTH_CONTEXT_USE_SUBKEY))
				goto cleanup;
			if (krb_log_exec(krb5glue_auth_con_set_cksumtype, credentials->ctx, context->auth_ctx,
			                 GSS_CHECKSUM_TYPE))
				goto cleanup;

			/* Get a service ticket */
			if (krb_log_exec(krb5_sname_to_principal, credentials->ctx, host, sname,
			                 KRB5_NT_SRV_HST, &in_creds.server))
				goto cleanup;

			if (krb_log_exec(krb5_cc_get_principal, credentials->ctx, credentials->ccache,
			                 &in_creds.client))
			{
				status = SEC_E_WRONG_PRINCIPAL;
				goto cleanup;
			}

			if (krb_log_exec(krb5_get_credentials, credentials->ctx,
			                 context->u2u ? KRB5_GC_USER_USER : 0, credentials->ccache, &in_creds,
			                 &creds))
			{
				status = SEC_E_NO_CREDENTIALS;
				goto cleanup;
			}

			/* Write the checksum (delegation not implemented) */
			cksum.data = cksum_contents;
			cksum.length = sizeof(cksum_contents);
			Data_Write_UINT32(cksum_contents, 16);
			Data_Write_UINT32((cksum_contents + 20), context->flags);

			if (bindings_buffer)
			{
				SEC_CHANNEL_BINDINGS* bindings = bindings_buffer->pvBuffer;

				/* Sanity checks */
				if (bindings_buffer->cbBuffer < sizeof(SEC_CHANNEL_BINDINGS) ||
				    (bindings->cbInitiatorLength + bindings->dwInitiatorOffset) >
				        bindings_buffer->cbBuffer ||
				    (bindings->cbAcceptorLength + bindings->dwAcceptorOffset) >
				        bindings_buffer->cbBuffer ||
				    (bindings->cbApplicationDataLength + bindings->dwApplicationDataOffset) >
				        bindings_buffer->cbBuffer)
				{
					status = SEC_E_BAD_BINDINGS;
					goto cleanup;
				}

				md5 = winpr_Digest_New();
				if (!md5)
					goto cleanup;

				if (!winpr_Digest_Init(md5, WINPR_MD_MD5))
					goto cleanup;

				if (!kerberos_hash_channel_bindings(md5, bindings))
					goto cleanup;

				if (!winpr_Digest_Final(md5, (BYTE*)cksum_contents + 4, 16))
					goto cleanup;
			}

			/* Make the AP_REQ message */
			if (krb_log_exec(krb5_mk_req_extended, credentials->ctx, &context->auth_ctx, ap_flags,
			                 &cksum, creds, &output_token))
				goto cleanup;

			if (!sspi_gss_wrap_token(output_buffer,
			                         context->u2u ? &kerberos_u2u_OID : &kerberos_OID,
			                         TOK_ID_AP_REQ, &output_token))
				goto cleanup;

			if (context->flags & SSPI_GSS_C_SEQUENCE_FLAG)
			{
				if (krb_log_exec(krb5_auth_con_getlocalseqnumber, credentials->ctx,
				                 context->auth_ctx, (INT32*)&context->local_seq))
					goto cleanup;
				context->remote_seq ^= context->local_seq;
			}

			if (krb_log_exec(krb5glue_update_keyset, credentials->ctx, context->auth_ctx, FALSE,
			                 &context->keyset))
				goto cleanup;

			context->state = KERBEROS_STATE_AP_REP;

			if (context->flags & SSPI_GSS_C_MUTUAL_FLAG)
				status = SEC_I_CONTINUE_NEEDED;
			else
				status = SEC_E_OK;
			break;

		case KERBEROS_STATE_AP_REP:

			if (tok_id == TOK_ID_AP_REP)
			{
				if (krb_log_exec(krb5_rd_rep, credentials->ctx, context->auth_ctx, &input_token,
				                 &reply))
					goto cleanup;
				krb5_free_ap_rep_enc_part(credentials->ctx, reply);
			}
			else if (tok_id == TOK_ID_ERROR)
			{
				krb5glue_log_error(credentials->ctx, &input_token, TAG);
				goto cleanup;
			}
			else
				goto bad_token;

			if (context->flags & SSPI_GSS_C_SEQUENCE_FLAG)
			{
				if (krb_log_exec(krb5_auth_con_getremoteseqnumber, credentials->ctx,
				                 context->auth_ctx, (INT32*)&context->remote_seq))
					goto cleanup;
			}

			if (krb_log_exec(krb5glue_update_keyset, credentials->ctx, context->auth_ctx, FALSE,
			                 &context->keyset))
				goto cleanup;

			context->state = KERBEROS_STATE_FINAL;

			if (output_buffer)
				output_buffer->cbBuffer = 0;
			status = SEC_E_OK;
			break;

		case KERBEROS_STATE_FINAL:
		default:
			WLog_ERR(TAG, "Kerberos in invalid state!");
			goto cleanup;
	}

cleanup:
{
	/* second_ticket is not allocated */
	krb5_data edata = { 0 };
	in_creds.second_ticket = edata;
	krb5_free_cred_contents(credentials->ctx, &in_creds);
}

	krb5_free_creds(credentials->ctx, creds);
	if (output_token.data)
		krb5glue_free_data_contents(credentials->ctx, &output_token);

	winpr_Digest_Free(md5);

	free(target);

	if (isNewContext)
	{
		switch (status)
		{
			case SEC_E_OK:
			case SEC_I_CONTINUE_NEEDED:
				sspi_SecureHandleSetLowerPointer(phNewContext, context);
				sspi_SecureHandleSetUpperPointer(phNewContext, KERBEROS_SSP_NAME);
				break;
			default:
				kerberos_ContextFree(context, TRUE);
				break;
		}
	}

	return status;

bad_token:
	status = SEC_E_INVALID_TOKEN;
	goto cleanup;
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif /* WITH_KRB5 */
}

static SECURITY_STATUS SEC_ENTRY kerberos_InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, ULONG* pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status = 0;
	char* target_name = NULL;

	if (pszTargetName)
	{
		target_name = ConvertWCharToUtf8Alloc(pszTargetName, NULL);
		if (!target_name)
			return SEC_E_INSUFFICIENT_MEMORY;
	}

	status = kerberos_InitializeSecurityContextA(phCredential, phContext, target_name, fContextReq,
	                                             Reserved1, TargetDataRep, pInput, Reserved2,
	                                             phNewContext, pOutput, pfContextAttr, ptsExpiry);

	if (target_name)
		free(target_name);

	return status;
}

static SECURITY_STATUS SEC_ENTRY kerberos_AcceptSecurityContext(
    PCredHandle phCredential, PCtxtHandle phContext, PSecBufferDesc pInput, ULONG fContextReq,
    ULONG TargetDataRep, PCtxtHandle phNewContext, PSecBufferDesc pOutput, ULONG* pfContextAttr,
    PTimeStamp ptsExpity)
{
#ifdef WITH_KRB5
	BOOL isNewContext = FALSE;
	PSecBuffer input_buffer = NULL;
	PSecBuffer output_buffer = NULL;
	WinPrAsn1_OID oid = { 0 };
	uint16_t tok_id = 0;
	krb5_data input_token = { 0 };
	krb5_data output_token = { 0 };
	SECURITY_STATUS status = SEC_E_INTERNAL_ERROR;
	krb5_flags ap_flags = 0;
	krb5glue_authenticator authenticator = NULL;
	char* target = NULL;
	char* sname = NULL;
	char* realm = NULL;
	krb5_kt_cursor cur = { 0 };
	krb5_keytab_entry entry = { 0 };
	krb5_principal principal = NULL;
	krb5_creds creds = { 0 };

	/* behave like windows SSPIs that don't want empty context */
	if (phContext && !phContext->dwLower && !phContext->dwUpper)
		return SEC_E_INVALID_HANDLE;

	KRB_CONTEXT* context = sspi_SecureHandleGetLowerPointer(phContext);
	KRB_CREDENTIALS* credentials = sspi_SecureHandleGetLowerPointer(phCredential);

	if (pInput)
		input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);
	if (pOutput)
		output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

	if (!input_buffer)
		return SEC_E_INVALID_TOKEN;

	if (!sspi_gss_unwrap_token(input_buffer, &oid, &tok_id, &input_token))
		return SEC_E_INVALID_TOKEN;

	if (!context)
	{
		isNewContext = TRUE;
		context = kerberos_ContextNew(credentials);
		context->acceptor = TRUE;

		if (sspi_gss_oid_compare(&oid, &kerberos_u2u_OID))
		{
			context->u2u = TRUE;
			context->state = KERBEROS_STATE_TGT_REQ;
		}
		else if (sspi_gss_oid_compare(&oid, &kerberos_OID))
			context->state = KERBEROS_STATE_AP_REQ;
		else
			goto bad_token;
	}
	else
	{
		if ((context->u2u && !sspi_gss_oid_compare(&oid, &kerberos_u2u_OID)) ||
		    (!context->u2u && !sspi_gss_oid_compare(&oid, &kerberos_OID)))
			goto bad_token;
	}

	if (context->state == KERBEROS_STATE_TGT_REQ && tok_id == TOK_ID_TGT_REQ)
	{
		if (!kerberos_rd_tgt_token(&input_token, &target, NULL))
			goto bad_token;

		if (target)
		{
			if (*target != 0 && *target != '@')
				sname = target;
			realm = strchr(target, '@');
			if (realm)
				realm++;
		}

		if (krb_log_exec(krb5_parse_name_flags, credentials->ctx, sname ? sname : "",
		                 KRB5_PRINCIPAL_PARSE_NO_REALM, &principal))
			goto cleanup;

		if (realm)
		{
			if (krb_log_exec(krb5glue_set_principal_realm, credentials->ctx, principal, realm))
				goto cleanup;
		}

		if (krb_log_exec(krb5_kt_start_seq_get, credentials->ctx, credentials->keytab, &cur))
			goto cleanup;

		do
		{
			krb5_error_code rv = krb_log_exec(krb5_kt_next_entry, credentials->ctx,
			                                  credentials->keytab, &entry, &cur);
			if (rv == KRB5_KT_END)
				break;
			if (rv != 0)
				goto cleanup;

			if ((!sname || krb_log_exec(krb5_principal_compare_any_realm, credentials->ctx,
			                            principal, entry.principal)) &&
			    (!realm ||
			     krb_log_exec(krb5_realm_compare, credentials->ctx, principal, entry.principal)))
				break;
			if (krb_log_exec(krb5glue_free_keytab_entry_contents, credentials->ctx, &entry))
				goto cleanup;
		} while (1);

		if (krb_log_exec(krb5_kt_end_seq_get, credentials->ctx, credentials->keytab, &cur))
			goto cleanup;

		if (!entry.principal)
			goto cleanup;

		/* Get the TGT */
		if (krb_log_exec(krb5_get_init_creds_keytab, credentials->ctx, &creds, entry.principal,
		                 credentials->keytab, 0, NULL, NULL))
			goto cleanup;

		if (!kerberos_mk_tgt_token(output_buffer, KRB_TGT_REP, NULL, NULL, &creds.ticket))
			goto cleanup;

		if (krb_log_exec(krb5_auth_con_init, credentials->ctx, &context->auth_ctx))
			goto cleanup;

		if (krb_log_exec(krb5glue_auth_con_setuseruserkey, credentials->ctx, context->auth_ctx,
		                 &krb5glue_creds_getkey(creds)))
			goto cleanup;

		context->state = KERBEROS_STATE_AP_REQ;
	}
	else if (context->state == KERBEROS_STATE_AP_REQ && tok_id == TOK_ID_AP_REQ)
	{
		if (krb_log_exec(krb5_rd_req, credentials->ctx, &context->auth_ctx, &input_token, NULL,
		                 credentials->keytab, &ap_flags, NULL))
			goto cleanup;

		if (krb_log_exec(krb5_auth_con_setflags, credentials->ctx, context->auth_ctx,
		                 KRB5_AUTH_CONTEXT_DO_SEQUENCE | KRB5_AUTH_CONTEXT_USE_SUBKEY))
			goto cleanup;

		/* Retrieve and validate the checksum */
		if (krb_log_exec(krb5_auth_con_getauthenticator, credentials->ctx, context->auth_ctx,
		                 &authenticator))
			goto cleanup;
		if (!krb5glue_authenticator_validate_chksum(authenticator, GSS_CHECKSUM_TYPE,
		                                            &context->flags))
			goto bad_token;

		if ((ap_flags & AP_OPTS_MUTUAL_REQUIRED) && (context->flags & SSPI_GSS_C_MUTUAL_FLAG))
		{
			if (!output_buffer)
				goto bad_token;
			if (krb_log_exec(krb5_mk_rep, credentials->ctx, context->auth_ctx, &output_token))
				goto cleanup;
			if (!sspi_gss_wrap_token(output_buffer,
			                         context->u2u ? &kerberos_u2u_OID : &kerberos_OID,
			                         TOK_ID_AP_REP, &output_token))
				goto cleanup;
		}
		else
		{
			if (output_buffer)
				output_buffer->cbBuffer = 0;
		}

		*pfContextAttr = (context->flags & 0x1F);
		if (context->flags & SSPI_GSS_C_INTEG_FLAG)
			*pfContextAttr |= ASC_RET_INTEGRITY;

		if (context->flags & SSPI_GSS_C_SEQUENCE_FLAG)
		{
			if (krb_log_exec(krb5_auth_con_getlocalseqnumber, credentials->ctx, context->auth_ctx,
			                 (INT32*)&context->local_seq))
				goto cleanup;
			if (krb_log_exec(krb5_auth_con_getremoteseqnumber, credentials->ctx, context->auth_ctx,
			                 (INT32*)&context->remote_seq))
				goto cleanup;
		}

		if (krb_log_exec(krb5glue_update_keyset, credentials->ctx, context->auth_ctx, TRUE,
		                 &context->keyset))
			goto cleanup;

		context->state = KERBEROS_STATE_FINAL;
	}
	else
		goto bad_token;

	/* On first call allocate new context */
	if (context->state == KERBEROS_STATE_FINAL)
		status = SEC_E_OK;
	else
		status = SEC_I_CONTINUE_NEEDED;

cleanup:
	free(target);
	if (output_token.data)
		krb5glue_free_data_contents(credentials->ctx, &output_token);
	if (entry.principal)
		krb5glue_free_keytab_entry_contents(credentials->ctx, &entry);

	if (isNewContext)
	{
		switch (status)
		{
			case SEC_E_OK:
			case SEC_I_CONTINUE_NEEDED:
				sspi_SecureHandleSetLowerPointer(phNewContext, context);
				sspi_SecureHandleSetUpperPointer(phNewContext, KERBEROS_SSP_NAME);
				break;
			default:
				kerberos_ContextFree(context, TRUE);
				break;
		}
	}

	return status;

bad_token:
	status = SEC_E_INVALID_TOKEN;
	goto cleanup;
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif /* WITH_KRB5 */
}

#ifdef WITH_KRB5
static KRB_CONTEXT* get_context(PCtxtHandle phContext)
{
	if (!phContext)
		return NULL;

	TCHAR* name = sspi_SecureHandleGetUpperPointer(phContext);
	if (!name)
		return NULL;

	if (_tcsncmp(KERBEROS_SSP_NAME, name, ARRAYSIZE(KERBEROS_SSP_NAME)) != 0)
		return NULL;
	return sspi_SecureHandleGetLowerPointer(phContext);
}

static BOOL copy_krb5_data(krb5_data* data, PUCHAR* ptr, ULONG* psize)
{
	WINPR_ASSERT(data);
	WINPR_ASSERT(ptr);
	WINPR_ASSERT(psize);

	*ptr = (PUCHAR)malloc(data->length);
	if (!*ptr)
		return FALSE;

	*psize = data->length;
	memcpy(*ptr, data->data, data->length);
	return TRUE;
}
#endif

static SECURITY_STATUS SEC_ENTRY kerberos_DeleteSecurityContext(PCtxtHandle phContext)
{
#ifdef WITH_KRB5
	KRB_CONTEXT* context = get_context(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	kerberos_ContextFree(context, TRUE);

	return SEC_E_OK;
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif
}

#ifdef WITH_KRB5

static SECURITY_STATUS krb5_error_to_SECURITY_STATUS(krb5_error_code code)
{
	switch (code)
	{
		case 0:
			return SEC_E_OK;
		default:
			return SEC_E_INTERNAL_ERROR;
	}
}

static SECURITY_STATUS kerberos_ATTR_SIZES(KRB_CONTEXT* context, KRB_CREDENTIALS* credentials,
                                           SecPkgContext_Sizes* ContextSizes)
{
	UINT header = 0;
	UINT pad = 0;
	UINT trailer = 0;
	krb5glue_key key = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(context->auth_ctx);

	/* The MaxTokenSize by default is 12,000 bytes. This has been the default value
	 * since Windows 2000 SP2 and still remains in Windows 7 and Windows 2008 R2.
	 *  For Windows Server 2012, the default value of the MaxTokenSize registry
	 *  entry is 48,000 bytes.*/
	ContextSizes->cbMaxToken = KERBEROS_SecPkgInfoA.cbMaxToken;
	ContextSizes->cbMaxSignature = 0;
	ContextSizes->cbBlockSize = 1;
	ContextSizes->cbSecurityTrailer = 0;

	key = get_key(&context->keyset);

	if (context->flags & SSPI_GSS_C_CONF_FLAG)
	{
		krb5_error_code rv = krb_log_exec(krb5glue_crypto_length, credentials->ctx, key,
		                                  KRB5_CRYPTO_TYPE_HEADER, &header);
		if (rv)
			return krb5_error_to_SECURITY_STATUS(rv);

		rv = krb_log_exec(krb5glue_crypto_length, credentials->ctx, key, KRB5_CRYPTO_TYPE_PADDING,
		                  &pad);
		if (rv)
			return krb5_error_to_SECURITY_STATUS(rv);

		rv = krb_log_exec(krb5glue_crypto_length, credentials->ctx, key, KRB5_CRYPTO_TYPE_TRAILER,
		                  &trailer);
		if (rv)
			return krb5_error_to_SECURITY_STATUS(rv);

		/* GSS header (= 16 bytes) + encrypted header = 32 bytes */
		ContextSizes->cbSecurityTrailer = header + pad + trailer + 32;
	}

	if (context->flags & SSPI_GSS_C_INTEG_FLAG)
	{
		krb5_error_code rv = krb_log_exec(krb5glue_crypto_length, credentials->ctx, key,
		                                  KRB5_CRYPTO_TYPE_CHECKSUM, &ContextSizes->cbMaxSignature);
		if (rv)
			return krb5_error_to_SECURITY_STATUS(rv);

		ContextSizes->cbMaxSignature += 16;
	}

	return SEC_E_OK;
}

static SECURITY_STATUS kerberos_ATTR_TICKET_LOGON(KRB_CONTEXT* context,
                                                  KRB_CREDENTIALS* credentials,
                                                  KERB_TICKET_LOGON* ticketLogon)
{
	krb5_creds matchCred = { 0 };
	krb5_auth_context authContext = NULL;
	int getCredsFlags = KRB5_GC_CACHED;
	BOOL firstRun = TRUE;
	krb5_creds* hostCred = NULL;
	SECURITY_STATUS ret = SEC_E_INSUFFICIENT_MEMORY;
	int rv = krb_log_exec(krb5_sname_to_principal, credentials->ctx, context->targetHost, "HOST",
	                      KRB5_NT_SRV_HST, &matchCred.server);
	if (rv)
		goto out;

	rv = krb_log_exec(krb5_cc_get_principal, credentials->ctx, credentials->ccache,
	                  &matchCred.client);
	if (rv)
		goto out;

	/* try from the cache first, and then do a new request */
again:
	rv = krb_log_exec(krb5_get_credentials, credentials->ctx, getCredsFlags, credentials->ccache,
	                  &matchCred, &hostCred);
	switch (rv)
	{
		case 0:
			break;
		case KRB5_CC_NOTFOUND:
			getCredsFlags = 0;
			if (firstRun)
			{
				firstRun = FALSE;
				goto again;
			}
			WINPR_FALLTHROUGH
		default:
			WLog_ERR(TAG, "krb5_get_credentials(hostCreds), rv=%d", rv);
			goto out;
	}

	if (krb_log_exec(krb5_auth_con_init, credentials->ctx, &authContext))
		goto out;

	krb5_data derOut = { 0 };
	if (krb_log_exec(krb5_fwd_tgt_creds, credentials->ctx, authContext, context->targetHost,
	                 matchCred.client, matchCred.server, credentials->ccache, 1, &derOut))
	{
		ret = SEC_E_LOGON_DENIED;
		goto out;
	}

	ticketLogon->MessageType = KerbTicketLogon;
	ticketLogon->Flags = KERB_LOGON_FLAG_REDIRECTED;

	if (!copy_krb5_data(&hostCred->ticket, &ticketLogon->ServiceTicket,
	                    &ticketLogon->ServiceTicketLength))
	{
		krb5_free_data(credentials->ctx, &derOut);
		goto out;
	}

	ticketLogon->TicketGrantingTicketLength = derOut.length;
	ticketLogon->TicketGrantingTicket = (PUCHAR)derOut.data;

	ret = SEC_E_OK;
out:
	krb5_auth_con_free(credentials->ctx, authContext);
	krb5_free_creds(credentials->ctx, hostCred);
	krb5_free_cred_contents(credentials->ctx, &matchCred);
	return ret;
}

#endif /* WITH_KRB5 */

static SECURITY_STATUS SEC_ENTRY kerberos_QueryContextAttributesA(PCtxtHandle phContext,
                                                                  ULONG ulAttribute, void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INVALID_PARAMETER;

#ifdef WITH_KRB5
	KRB_CONTEXT* context = get_context(phContext);
	if (!context)
		return SEC_E_INVALID_PARAMETER;

	KRB_CREDENTIALS* credentials = context->credentials;

	switch (ulAttribute)
	{
		case SECPKG_ATTR_SIZES:
			return kerberos_ATTR_SIZES(context, credentials, (SecPkgContext_Sizes*)pBuffer);

		case SECPKG_CRED_ATTR_TICKET_LOGON:
			return kerberos_ATTR_TICKET_LOGON(context, credentials, (KERB_TICKET_LOGON*)pBuffer);

		default:
			WLog_ERR(TAG, "TODO: QueryContextAttributes implement ulAttribute=0x%08" PRIx32,
			         ulAttribute);
			return SEC_E_UNSUPPORTED_FUNCTION;
	}
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif
}

static SECURITY_STATUS SEC_ENTRY kerberos_QueryContextAttributesW(PCtxtHandle phContext,
                                                                  ULONG ulAttribute, void* pBuffer)
{
	return kerberos_QueryContextAttributesA(phContext, ulAttribute, pBuffer);
}

static SECURITY_STATUS SEC_ENTRY kerberos_SetContextAttributesW(PCtxtHandle phContext,
                                                                ULONG ulAttribute, void* pBuffer,
                                                                ULONG cbBuffer)
{
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY kerberos_SetContextAttributesA(PCtxtHandle phContext,
                                                                ULONG ulAttribute, void* pBuffer,
                                                                ULONG cbBuffer)
{
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY kerberos_SetCredentialsAttributesX(PCredHandle phCredential,
                                                                    ULONG ulAttribute,
                                                                    void* pBuffer, ULONG cbBuffer,
                                                                    BOOL unicode)
{
#ifdef WITH_KRB5
	KRB_CREDENTIALS* credentials = NULL;

	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	credentials = sspi_SecureHandleGetLowerPointer(phCredential);

	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	switch (ulAttribute)
	{
		case SECPKG_CRED_ATTR_KDC_PROXY_SETTINGS:
		{
			SecPkgCredentials_KdcProxySettingsW* kdc_settings = pBuffer;

			/* Sanity checks */
			if (cbBuffer < sizeof(SecPkgCredentials_KdcProxySettingsW) ||
			    kdc_settings->Version != KDC_PROXY_SETTINGS_V1 ||
			    kdc_settings->ProxyServerOffset < sizeof(SecPkgCredentials_KdcProxySettingsW) ||
			    cbBuffer < sizeof(SecPkgCredentials_KdcProxySettingsW) +
			                   kdc_settings->ProxyServerOffset + kdc_settings->ProxyServerLength)
				return SEC_E_INVALID_TOKEN;

			if (credentials->kdc_url)
			{
				free(credentials->kdc_url);
				credentials->kdc_url = NULL;
			}

			if (kdc_settings->ProxyServerLength > 0)
			{
				WCHAR* proxy = (WCHAR*)((BYTE*)pBuffer + kdc_settings->ProxyServerOffset);

				credentials->kdc_url = ConvertWCharNToUtf8Alloc(
				    proxy, kdc_settings->ProxyServerLength / sizeof(WCHAR), NULL);
				if (!credentials->kdc_url)
					return SEC_E_INSUFFICIENT_MEMORY;
			}

			return SEC_E_OK;
		}
		case SECPKG_CRED_ATTR_NAMES:
		case SECPKG_ATTR_SUPPORTED_ALGS:
		default:
			WLog_ERR(TAG, "TODO: SetCredentialsAttributesX implement ulAttribute=0x%08" PRIx32,
			         ulAttribute);
			return SEC_E_UNSUPPORTED_FUNCTION;
	}

#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif
}

static SECURITY_STATUS SEC_ENTRY kerberos_SetCredentialsAttributesW(PCredHandle phCredential,
                                                                    ULONG ulAttribute,
                                                                    void* pBuffer, ULONG cbBuffer)
{
	return kerberos_SetCredentialsAttributesX(phCredential, ulAttribute, pBuffer, cbBuffer, TRUE);
}

static SECURITY_STATUS SEC_ENTRY kerberos_SetCredentialsAttributesA(PCredHandle phCredential,
                                                                    ULONG ulAttribute,
                                                                    void* pBuffer, ULONG cbBuffer)
{
	return kerberos_SetCredentialsAttributesX(phCredential, ulAttribute, pBuffer, cbBuffer, FALSE);
}

static SECURITY_STATUS SEC_ENTRY kerberos_EncryptMessage(PCtxtHandle phContext, ULONG fQOP,
                                                         PSecBufferDesc pMessage,
                                                         ULONG MessageSeqNo)
{
#ifdef WITH_KRB5
	KRB_CONTEXT* context = get_context(phContext);
	PSecBuffer sig_buffer = NULL;
	PSecBuffer data_buffer = NULL;
	char* header = NULL;
	BYTE flags = 0;
	krb5glue_key key = NULL;
	krb5_keyusage usage = 0;
	krb5_crypto_iov encrypt_iov[] = { { KRB5_CRYPTO_TYPE_HEADER, { 0 } },
		                              { KRB5_CRYPTO_TYPE_DATA, { 0 } },
		                              { KRB5_CRYPTO_TYPE_DATA, { 0 } },
		                              { KRB5_CRYPTO_TYPE_PADDING, { 0 } },
		                              { KRB5_CRYPTO_TYPE_TRAILER, { 0 } } };

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (!(context->flags & SSPI_GSS_C_CONF_FLAG))
		return SEC_E_UNSUPPORTED_FUNCTION;

	KRB_CREDENTIALS* creds = context->credentials;

	sig_buffer = sspi_FindSecBuffer(pMessage, SECBUFFER_TOKEN);
	data_buffer = sspi_FindSecBuffer(pMessage, SECBUFFER_DATA);

	if (!sig_buffer || !data_buffer)
		return SEC_E_INVALID_TOKEN;

	if (fQOP)
		return SEC_E_QOP_NOT_SUPPORTED;

	flags |= context->acceptor ? FLAG_SENDER_IS_ACCEPTOR : 0;
	flags |= FLAG_WRAP_CONFIDENTIAL;

	key = get_key(&context->keyset);
	if (!key)
		return SEC_E_INTERNAL_ERROR;

	flags |= context->keyset.acceptor_key == key ? FLAG_ACCEPTOR_SUBKEY : 0;

	usage = context->acceptor ? KG_USAGE_ACCEPTOR_SEAL : KG_USAGE_INITIATOR_SEAL;

	/* Set the lengths of the data (plaintext + header) */
	encrypt_iov[1].data.length = data_buffer->cbBuffer;
	encrypt_iov[2].data.length = 16;

	/* Get the lengths of the header, trailer, and padding and ensure sig_buffer is large enough */
	if (krb_log_exec(krb5glue_crypto_length_iov, creds->ctx, key, encrypt_iov,
	                 ARRAYSIZE(encrypt_iov)))
		return SEC_E_INTERNAL_ERROR;
	if (sig_buffer->cbBuffer <
	    encrypt_iov[0].data.length + encrypt_iov[3].data.length + encrypt_iov[4].data.length + 32)
		return SEC_E_INSUFFICIENT_MEMORY;

	/* Set up the iov array in sig_buffer */
	header = sig_buffer->pvBuffer;
	encrypt_iov[2].data.data = header + 16;
	encrypt_iov[3].data.data = encrypt_iov[2].data.data + encrypt_iov[2].data.length;
	encrypt_iov[4].data.data = encrypt_iov[3].data.data + encrypt_iov[3].data.length;
	encrypt_iov[0].data.data = encrypt_iov[4].data.data + encrypt_iov[4].data.length;
	encrypt_iov[1].data.data = data_buffer->pvBuffer;

	/* Write the GSS header with 0 in RRC */
	Data_Write_UINT16_BE(header, TOK_ID_WRAP);
	header[2] = flags;
	header[3] = (char)0xFF;
	Data_Write_UINT32(header + 4, 0);
	Data_Write_UINT64_BE(header + 8, (context->local_seq + MessageSeqNo));

	/* Copy header to be encrypted */
	CopyMemory(encrypt_iov[2].data.data, header, 16);

	/* Set the correct RRC */
	Data_Write_UINT16_BE(header + 6, 16 + encrypt_iov[3].data.length + encrypt_iov[4].data.length);

	if (krb_log_exec(krb5glue_encrypt_iov, creds->ctx, key, usage, encrypt_iov,
	                 ARRAYSIZE(encrypt_iov)))
		return SEC_E_INTERNAL_ERROR;

	return SEC_E_OK;
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif
}

static SECURITY_STATUS SEC_ENTRY kerberos_DecryptMessage(PCtxtHandle phContext,
                                                         PSecBufferDesc pMessage,
                                                         ULONG MessageSeqNo, ULONG* pfQOP)
{
#ifdef WITH_KRB5
	KRB_CONTEXT* context = get_context(phContext);
	PSecBuffer sig_buffer = NULL;
	PSecBuffer data_buffer = NULL;
	krb5glue_key key = NULL;
	krb5_keyusage usage = 0;
	char* header = NULL;
	uint16_t tok_id = 0;
	BYTE flags = 0;
	uint16_t ec = 0;
	uint16_t rrc = 0;
	uint64_t seq_no = 0;
	krb5_crypto_iov iov[] = { { KRB5_CRYPTO_TYPE_HEADER, { 0 } },
		                      { KRB5_CRYPTO_TYPE_DATA, { 0 } },
		                      { KRB5_CRYPTO_TYPE_DATA, { 0 } },
		                      { KRB5_CRYPTO_TYPE_PADDING, { 0 } },
		                      { KRB5_CRYPTO_TYPE_TRAILER, { 0 } } };

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (!(context->flags & SSPI_GSS_C_CONF_FLAG))
		return SEC_E_UNSUPPORTED_FUNCTION;

	KRB_CREDENTIALS* creds = context->credentials;

	sig_buffer = sspi_FindSecBuffer(pMessage, SECBUFFER_TOKEN);
	data_buffer = sspi_FindSecBuffer(pMessage, SECBUFFER_DATA);

	if (!sig_buffer || !data_buffer || sig_buffer->cbBuffer < 16)
		return SEC_E_INVALID_TOKEN;

	/* Read in header information */
	header = sig_buffer->pvBuffer;
	Data_Read_UINT16_BE(header, tok_id);
	flags = header[2];
	Data_Read_UINT16_BE((header + 4), ec);
	Data_Read_UINT16_BE((header + 6), rrc);
	Data_Read_UINT64_BE((header + 8), seq_no);

	/* Check that the header is valid */
	if (tok_id != TOK_ID_WRAP || (BYTE)header[3] != 0xFF)
		return SEC_E_INVALID_TOKEN;

	if ((flags & FLAG_SENDER_IS_ACCEPTOR) == context->acceptor)
		return SEC_E_INVALID_TOKEN;

	if ((context->flags & ISC_REQ_SEQUENCE_DETECT) &&
	    (seq_no != context->remote_seq + MessageSeqNo))
		return SEC_E_OUT_OF_SEQUENCE;

	if (!(flags & FLAG_WRAP_CONFIDENTIAL))
		return SEC_E_INVALID_TOKEN;

	/* We don't expect a trailer buffer; the encrypted header must be rotated */
	if (rrc < 16)
		return SEC_E_INVALID_TOKEN;

	/* Find the proper key and key usage */
	key = get_key(&context->keyset);
	if (!key || ((flags & FLAG_ACCEPTOR_SUBKEY) && (context->keyset.acceptor_key != key)))
		return SEC_E_INTERNAL_ERROR;
	usage = context->acceptor ? KG_USAGE_INITIATOR_SEAL : KG_USAGE_ACCEPTOR_SEAL;

	/* Fill in the lengths of the iov array */
	iov[1].data.length = data_buffer->cbBuffer;
	iov[2].data.length = 16;
	if (krb_log_exec(krb5glue_crypto_length_iov, creds->ctx, key, iov, ARRAYSIZE(iov)))
		return SEC_E_INTERNAL_ERROR;

	/* We don't expect a trailer buffer; everything must be in sig_buffer */
	if (rrc != 16 + iov[3].data.length + iov[4].data.length)
		return SEC_E_INVALID_TOKEN;
	if (sig_buffer->cbBuffer != 16 + rrc + iov[0].data.length)
		return SEC_E_INVALID_TOKEN;

	/* Locate the parts of the message */
	iov[0].data.data = header + 16 + rrc + ec;
	iov[1].data.data = data_buffer->pvBuffer;
	iov[2].data.data = header + 16 + ec;
	iov[3].data.data = iov[2].data.data + iov[2].data.length;
	iov[4].data.data = iov[3].data.data + iov[3].data.length;

	if (krb_log_exec(krb5glue_decrypt_iov, creds->ctx, key, usage, iov, ARRAYSIZE(iov)))
		return SEC_E_INTERNAL_ERROR;

	/* Validate the encrypted header */
	Data_Write_UINT16_BE(iov[2].data.data + 4, ec);
	Data_Write_UINT16_BE(iov[2].data.data + 6, rrc);
	if (memcmp(iov[2].data.data, header, 16) != 0)
		return SEC_E_MESSAGE_ALTERED;

	*pfQOP = 0;

	return SEC_E_OK;
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif
}

static SECURITY_STATUS SEC_ENTRY kerberos_MakeSignature(PCtxtHandle phContext, ULONG fQOP,
                                                        PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
#ifdef WITH_KRB5
	KRB_CONTEXT* context = get_context(phContext);
	PSecBuffer sig_buffer = NULL;
	PSecBuffer data_buffer = NULL;
	krb5glue_key key = NULL;
	krb5_keyusage usage = 0;
	char* header = NULL;
	BYTE flags = 0;
	krb5_crypto_iov iov[] = { { KRB5_CRYPTO_TYPE_DATA, { 0 } },
		                      { KRB5_CRYPTO_TYPE_DATA, { 0 } },
		                      { KRB5_CRYPTO_TYPE_CHECKSUM, { 0 } } };

	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (!(context->flags & SSPI_GSS_C_INTEG_FLAG))
		return SEC_E_UNSUPPORTED_FUNCTION;

	KRB_CREDENTIALS* creds = context->credentials;

	sig_buffer = sspi_FindSecBuffer(pMessage, SECBUFFER_TOKEN);
	data_buffer = sspi_FindSecBuffer(pMessage, SECBUFFER_DATA);

	if (!sig_buffer || !data_buffer)
		return SEC_E_INVALID_TOKEN;

	flags |= context->acceptor ? FLAG_SENDER_IS_ACCEPTOR : 0;

	key = get_key(&context->keyset);
	if (!key)
		return SEC_E_INTERNAL_ERROR;
	usage = context->acceptor ? KG_USAGE_ACCEPTOR_SIGN : KG_USAGE_INITIATOR_SIGN;

	flags |= context->keyset.acceptor_key == key ? FLAG_ACCEPTOR_SUBKEY : 0;

	/* Fill in the lengths of the iov array */
	iov[0].data.length = data_buffer->cbBuffer;
	iov[1].data.length = 16;
	if (krb_log_exec(krb5glue_crypto_length_iov, creds->ctx, key, iov, ARRAYSIZE(iov)))
		return SEC_E_INTERNAL_ERROR;

	/* Ensure the buffer is big enough */
	if (sig_buffer->cbBuffer < iov[2].data.length + 16)
		return SEC_E_INSUFFICIENT_MEMORY;

	/* Write the header */
	header = sig_buffer->pvBuffer;
	Data_Write_UINT16_BE(header, TOK_ID_MIC);
	header[2] = flags;
	memset(header + 3, 0xFF, 5);
	Data_Write_UINT64_BE(header + 8, (context->local_seq + MessageSeqNo));

	/* Set up the iov array */
	iov[0].data.data = data_buffer->pvBuffer;
	iov[1].data.data = header;
	iov[2].data.data = header + 16;

	if (krb_log_exec(krb5glue_make_checksum_iov, creds->ctx, key, usage, iov, ARRAYSIZE(iov)))
		return SEC_E_INTERNAL_ERROR;

	sig_buffer->cbBuffer = iov[2].data.length + 16;

	return SEC_E_OK;
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif
}

static SECURITY_STATUS SEC_ENTRY kerberos_VerifySignature(PCtxtHandle phContext,
                                                          PSecBufferDesc pMessage,
                                                          ULONG MessageSeqNo, ULONG* pfQOP)
{
#ifdef WITH_KRB5
	PSecBuffer sig_buffer = NULL;
	PSecBuffer data_buffer = NULL;
	krb5glue_key key = NULL;
	krb5_keyusage usage = 0;
	char* header = NULL;
	BYTE flags = 0;
	uint16_t tok_id = 0;
	uint64_t seq_no = 0;
	krb5_boolean is_valid = 0;
	krb5_crypto_iov iov[] = { { KRB5_CRYPTO_TYPE_DATA, { 0 } },
		                      { KRB5_CRYPTO_TYPE_DATA, { 0 } },
		                      { KRB5_CRYPTO_TYPE_CHECKSUM, { 0 } } };
	BYTE cmp_filler[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

	KRB_CONTEXT* context = get_context(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (!(context->flags & SSPI_GSS_C_INTEG_FLAG))
		return SEC_E_UNSUPPORTED_FUNCTION;

	sig_buffer = sspi_FindSecBuffer(pMessage, SECBUFFER_TOKEN);
	data_buffer = sspi_FindSecBuffer(pMessage, SECBUFFER_DATA);

	if (!sig_buffer || !data_buffer || sig_buffer->cbBuffer < 16)
		return SEC_E_INVALID_TOKEN;

	/* Read in header info */
	header = sig_buffer->pvBuffer;
	Data_Read_UINT16_BE(header, tok_id);
	flags = header[2];
	Data_Read_UINT64_BE((header + 8), seq_no);

	/* Validate header */
	if (tok_id != TOK_ID_MIC)
		return SEC_E_INVALID_TOKEN;

	if ((flags & FLAG_SENDER_IS_ACCEPTOR) == context->acceptor || flags & FLAG_WRAP_CONFIDENTIAL)
		return SEC_E_INVALID_TOKEN;

	if (memcmp(header + 3, cmp_filler, sizeof(cmp_filler)) != 0)
		return SEC_E_INVALID_TOKEN;

	if (context->flags & ISC_REQ_SEQUENCE_DETECT && seq_no != context->remote_seq + MessageSeqNo)
		return SEC_E_OUT_OF_SEQUENCE;

	/* Find the proper key and usage */
	key = get_key(&context->keyset);
	if (!key || (flags & FLAG_ACCEPTOR_SUBKEY && context->keyset.acceptor_key != key))
		return SEC_E_INTERNAL_ERROR;
	usage = context->acceptor ? KG_USAGE_INITIATOR_SIGN : KG_USAGE_ACCEPTOR_SIGN;

	/* Fill in the iov array lengths */
	KRB_CREDENTIALS* creds = context->credentials;
	iov[0].data.length = data_buffer->cbBuffer;
	iov[1].data.length = 16;
	if (krb_log_exec(krb5glue_crypto_length_iov, creds->ctx, key, iov, ARRAYSIZE(iov)))
		return SEC_E_INTERNAL_ERROR;

	if (sig_buffer->cbBuffer != iov[2].data.length + 16)
		return SEC_E_INTERNAL_ERROR;

	/* Set up the iov array */
	iov[0].data.data = data_buffer->pvBuffer;
	iov[1].data.data = header;
	iov[2].data.data = header + 16;

	if (krb_log_exec(krb5glue_verify_checksum_iov, creds->ctx, key, usage, iov, ARRAYSIZE(iov),
	                 &is_valid))
		return SEC_E_INTERNAL_ERROR;

	if (!is_valid)
		return SEC_E_MESSAGE_ALTERED;

	return SEC_E_OK;
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif
}

const SecurityFunctionTableA KERBEROS_SecurityFunctionTableA = {
	3,                                    /* dwVersion */
	NULL,                                 /* EnumerateSecurityPackages */
	kerberos_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	kerberos_AcquireCredentialsHandleA,   /* AcquireCredentialsHandle */
	kerberos_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                                 /* Reserved2 */
	kerberos_InitializeSecurityContextA,  /* InitializeSecurityContext */
	kerberos_AcceptSecurityContext,       /* AcceptSecurityContext */
	NULL,                                 /* CompleteAuthToken */
	kerberos_DeleteSecurityContext,       /* DeleteSecurityContext */
	NULL,                                 /* ApplyControlToken */
	kerberos_QueryContextAttributesA,     /* QueryContextAttributes */
	NULL,                                 /* ImpersonateSecurityContext */
	NULL,                                 /* RevertSecurityContext */
	kerberos_MakeSignature,               /* MakeSignature */
	kerberos_VerifySignature,             /* VerifySignature */
	NULL,                                 /* FreeContextBuffer */
	NULL,                                 /* QuerySecurityPackageInfo */
	NULL,                                 /* Reserved3 */
	NULL,                                 /* Reserved4 */
	NULL,                                 /* ExportSecurityContext */
	NULL,                                 /* ImportSecurityContext */
	NULL,                                 /* AddCredentials */
	NULL,                                 /* Reserved8 */
	NULL,                                 /* QuerySecurityContextToken */
	kerberos_EncryptMessage,              /* EncryptMessage */
	kerberos_DecryptMessage,              /* DecryptMessage */
	kerberos_SetContextAttributesA,       /* SetContextAttributes */
	kerberos_SetCredentialsAttributesA,   /* SetCredentialsAttributes */
};

const SecurityFunctionTableW KERBEROS_SecurityFunctionTableW = {
	3,                                    /* dwVersion */
	NULL,                                 /* EnumerateSecurityPackages */
	kerberos_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	kerberos_AcquireCredentialsHandleW,   /* AcquireCredentialsHandle */
	kerberos_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                                 /* Reserved2 */
	kerberos_InitializeSecurityContextW,  /* InitializeSecurityContext */
	kerberos_AcceptSecurityContext,       /* AcceptSecurityContext */
	NULL,                                 /* CompleteAuthToken */
	kerberos_DeleteSecurityContext,       /* DeleteSecurityContext */
	NULL,                                 /* ApplyControlToken */
	kerberos_QueryContextAttributesW,     /* QueryContextAttributes */
	NULL,                                 /* ImpersonateSecurityContext */
	NULL,                                 /* RevertSecurityContext */
	kerberos_MakeSignature,               /* MakeSignature */
	kerberos_VerifySignature,             /* VerifySignature */
	NULL,                                 /* FreeContextBuffer */
	NULL,                                 /* QuerySecurityPackageInfo */
	NULL,                                 /* Reserved3 */
	NULL,                                 /* Reserved4 */
	NULL,                                 /* ExportSecurityContext */
	NULL,                                 /* ImportSecurityContext */
	NULL,                                 /* AddCredentials */
	NULL,                                 /* Reserved8 */
	NULL,                                 /* QuerySecurityContextToken */
	kerberos_EncryptMessage,              /* EncryptMessage */
	kerberos_DecryptMessage,              /* DecryptMessage */
	kerberos_SetContextAttributesW,       /* SetContextAttributes */
	kerberos_SetCredentialsAttributesW,   /* SetCredentialsAttributes */
};

BOOL KERBEROS_init(void)
{
	InitializeConstWCharFromUtf8(KERBEROS_SecPkgInfoA.Name, KERBEROS_SecPkgInfoW_NameBuffer,
	                             ARRAYSIZE(KERBEROS_SecPkgInfoW_NameBuffer));
	InitializeConstWCharFromUtf8(KERBEROS_SecPkgInfoA.Comment, KERBEROS_SecPkgInfoW_CommentBuffer,
	                             ARRAYSIZE(KERBEROS_SecPkgInfoW_CommentBuffer));
	return TRUE;
}
