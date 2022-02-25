/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Kerberos Auth Protocol
 *
 * Copyright 2015 ANSSI, Author Thomas Calderon
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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
#include <string.h>
#include <ctype.h>

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/sspi.h>
#include <winpr/print.h>
#include <winpr/sysinfo.h>
#include <winpr/registry.h>

#include "kerberos.h"

#ifdef WITH_GSSAPI_HEIMDAL
#include <krb5-protos.h>
#endif

#include "../sspi.h"
#include "../../log.h"
#define TAG WINPR_TAG("sspi.Kerberos")

struct s_KRB_CONTEXT
{
	CtxtHandle context;
	SSPI_CREDENTIALS* credentials;
	SEC_WINNT_AUTH_IDENTITY identity;

	/* GSSAPI */
	UINT32 major_status;
	UINT32 minor_status;
	UINT32 actual_time;
	sspi_gss_cred_id_t cred;
	sspi_gss_ctx_id_t gss_ctx;
	sspi_gss_name_t target_name;
	UINT32 trailerSize;
};

static const char* KRB_PACKAGE_NAME = "Kerberos";

const SecPkgInfoA KERBEROS_SecPkgInfoA = {
	0x000F3BBF,                 /* fCapabilities */
	1,                          /* wVersion */
	0x0010,                     /* wRPCID */
	0x0000BB80,                 /* cbMaxToken : 48k bytes maximum for Windows Server 2012 */
	"Kerberos",                 /* Name */
	"Kerberos Security Package" /* Comment */
};

static WCHAR KERBEROS_SecPkgInfoW_Name[] = { 'K', 'e', 'r', 'b', 'e', 'r', 'o', 's', '\0' };

static WCHAR KERBEROS_SecPkgInfoW_Comment[] = { 'K', 'e', 'r', 'b', 'e', 'r', 'o', 's', ' ',
	                                            'S', 'e', 'c', 'u', 'r', 'i', 't', 'y', ' ',
	                                            'P', 'a', 'c', 'k', 'a', 'g', 'e', '\0' };

const SecPkgInfoW KERBEROS_SecPkgInfoW = {
	0x000F3BBF,                  /* fCapabilities */
	1,                           /* wVersion */
	0x0010,                      /* wRPCID */
	0x0000BB80,                  /* cbMaxToken : 48k bytes maximum for Windows Server 2012 */
	KERBEROS_SecPkgInfoW_Name,   /* Name */
	KERBEROS_SecPkgInfoW_Comment /* Comment */
};

static sspi_gss_OID_desc g_SSPI_GSS_C_SPNEGO_KRB5 = {
	9, (void*)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02"
};
static sspi_gss_OID SSPI_GSS_C_SPNEGO_KRB5 = &g_SSPI_GSS_C_SPNEGO_KRB5;

static KRB_CONTEXT* kerberos_ContextNew(void)
{
	KRB_CONTEXT* context;

	context = (KRB_CONTEXT*)calloc(1, sizeof(KRB_CONTEXT));
	if (!context)
		return NULL;

	context->minor_status = 0;
	context->major_status = 0;
	context->gss_ctx = SSPI_GSS_C_NO_CONTEXT;
	context->cred = SSPI_GSS_C_NO_CREDENTIAL;

	return context;
}

static void kerberos_ContextFree(KRB_CONTEXT* context)
{
	UINT32 minor_status;

	if (!context)
		return;

	if (context->target_name)
	{
		sspi_gss_release_name(&minor_status, &context->target_name);
		context->target_name = NULL;
	}

	sspi_gss_release_cred(&minor_status, &context->cred);

	if (context->gss_ctx)
	{
		sspi_gss_delete_sec_context(&minor_status, &context->gss_ctx, SSPI_GSS_C_NO_BUFFER);
		context->gss_ctx = SSPI_GSS_C_NO_CONTEXT;
	}

	free(context);
}

static SECURITY_STATUS SEC_ENTRY kerberos_AcquireCredentialsHandleW(
    SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY kerberos_AcquireCredentialsHandleA(
    SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY kerberos_FreeCredentialsHandle(PCredHandle phCredential)
{
	SSPI_CREDENTIALS* credentials;

	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	credentials = (SSPI_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);

	if (!credentials)
		return SEC_E_INVALID_HANDLE;

	sspi_CredentialsFree(credentials);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY kerberos_QueryCredentialsAttributesW(PCredHandle phCredential,
                                                                      ULONG ulAttribute,
                                                                      void* pBuffer)
{
	if (ulAttribute == SECPKG_CRED_ATTR_NAMES)
	{
		return SEC_E_OK;
	}

	WLog_ERR(TAG, "[%s]: TODO: Implement ulAttribute=%08" PRIx32, __FUNCTION__, ulAttribute);
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY kerberos_QueryCredentialsAttributesA(PCredHandle phCredential,
                                                                      ULONG ulAttribute,
                                                                      void* pBuffer)
{
	return kerberos_QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);
}

static SECURITY_STATUS SEC_ENTRY kerberos_InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, ULONG* pfContextAttr, PTimeStamp ptsExpiry)
{
	WLog_ERR(TAG, "[%s]: TODO: Implement", __FUNCTION__);
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static int kerberos_SetContextServicePrincipalNameA(KRB_CONTEXT* context,
                                                    SEC_CHAR* ServicePrincipalName)
{
	char* p;
	UINT32 major_status;
	UINT32 minor_status;
	char* gss_name = NULL;
	sspi_gss_buffer_desc name_buffer;

	if (!ServicePrincipalName)
	{
		context->target_name = NULL;
		return 1;
	}

	/* GSSAPI expects a SPN of type <service>@FQDN, let's construct it */
	gss_name = _strdup(ServicePrincipalName);

	if (!gss_name)
		return -1;

	p = strchr(gss_name, '/');

	if (p)
		*p = '@';

	name_buffer.value = gss_name;
	name_buffer.length = strlen(gss_name) + 1;
	major_status = sspi_gss_import_name(&minor_status, &name_buffer,
	                                    SSPI_GSS_C_NT_HOSTBASED_SERVICE, &(context->target_name));
	free(gss_name);

	if (SSPI_GSS_ERROR(major_status))
	{
		WLog_ERR(TAG, "error: gss_import_name failed");
		return -1;
	}

	return 1;
}

#ifdef WITH_GSSAPI

static krb5_error_code krb5_prompter(krb5_context context, void* data, const char* name,
                                     const char* banner, int num_prompts, krb5_prompt prompts[])
{
	int i;
	krb5_prompt_type* ptypes = krb5_get_prompt_types(context);

	for (i = 0; i < num_prompts; i++)
	{
		if (ptypes && ptypes[i] == KRB5_PROMPT_TYPE_PREAUTH)
		{
			prompts[i].reply->data = _strdup((const char*)data);
			prompts[i].reply->length = strlen((const char*)data);
		}
	}
	return 0;
}

static char* allocate_print(const char* fmt, va_list ap)
{
	char* buffer = NULL;
	size_t size = 0;
	do
	{
		if (size > 0)
		{
			char* tmp = realloc(buffer, size);
			if (!tmp)
				break;
			buffer = tmp;
		}

		int rc = vsnprintf(buffer, size, fmt, ap);
		if (rc <= 0)
			break;
		size = rc;
	} while (!buffer);
	return buffer;
}

static void krb5_log_error_message(krb5_context ctx, krb5_error_code ret, DWORD level,
                                   const char* fmt, ...)
{
	va_list ap;
	char* buffer = NULL;
	const char* msg = krb5_get_error_message(ctx, ret);

	va_start(ap, fmt);
	buffer = allocate_print(fmt, ap);
	va_end(ap);

	WLog_Print_tag(TAG, level, "%s [%s]", buffer, msg);
	free(buffer);
	krb5_free_error_message(ctx, msg);
}

static krb5_error_code acquire_cred(krb5_context ctx, SEC_WINPR_KERBEROS_SETTINGS* optionsBlock,
                                    krb5_principal client, const char* password)
{
	krb5_error_code ret;
	krb5_creds creds;
	krb5_deltat starttime = 0;
	krb5_get_init_creds_opt* options = NULL;
	krb5_ccache ccache;
	krb5_init_creds_context init_ctx = NULL;
	const char* cacheType;

	/* Get ccache */
	if (optionsBlock && optionsBlock->cache)
	{
		ret = krb5_cc_resolve(ctx, optionsBlock->cache, &ccache);
		cacheType = "user set cache";
	}
	else
	{
		ret = krb5_cc_default(ctx, &ccache);
		cacheType = "default";
	}

	if (ret)
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR, "error while getting %s ccache", cacheType);
		goto cleanup;
	}

	if ((ret = krb5_cc_initialize(ctx, ccache, client)))
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR, "error: could not initialize ccache");
		goto cleanup;
	}

	memset(&creds, 0, sizeof(creds));

	if ((ret = krb5_get_init_creds_opt_alloc(ctx, &options)))
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR, "error while allocating options");
		goto cleanup;
	}

	/* Set default options */
	krb5_get_init_creds_opt_set_forwardable(options, 0);
	krb5_get_init_creds_opt_set_proxiable(options, 0);

	if (optionsBlock && optionsBlock->startTime)
		starttime = optionsBlock->startTime;

	if (optionsBlock && optionsBlock->lifeTime)
		krb5_get_init_creds_opt_set_tkt_life(options, optionsBlock->lifeTime);

	if (optionsBlock && optionsBlock->renewLifeTime)
		krb5_get_init_creds_opt_set_renew_life(options, optionsBlock->renewLifeTime);

	if (optionsBlock && optionsBlock->withPac)
		krb5_get_init_creds_opt_set_pac_request(ctx, options, TRUE);

	if (optionsBlock && optionsBlock->armorCache)
		krb5_get_init_creds_opt_set_fast_ccache_name(ctx, options, optionsBlock->armorCache);

	if (optionsBlock && optionsBlock->pkinitX509Identity)
		krb5_get_init_creds_opt_set_pa(ctx, options, "X509_user_identity",
		                               optionsBlock->pkinitX509Identity);

	if (optionsBlock && optionsBlock->pkinitX509Anchors)
		krb5_get_init_creds_opt_set_pa(ctx, options, "X509_anchors",
		                               optionsBlock->pkinitX509Anchors);

#ifdef WITH_GSSAPI_MIT
	/* for MIT we specify ccache output using an option */
	if ((ret = krb5_get_init_creds_opt_set_out_ccache(ctx, options, ccache)))
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR, "error while setting ccache output");
		goto cleanup;
	}

#endif

	if ((ret = krb5_init_creds_init(ctx, client, krb5_prompter, (void*)password, starttime, options,
	                                &init_ctx)))
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR, "error krb5_init_creds_init failed");
		goto cleanup;
	}

	if ((ret = krb5_init_creds_set_password(ctx, init_ctx, password)))
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR, "error krb5_init_creds_set_password failed");
		goto cleanup;
	}

	/* Get credentials */
	if ((ret = krb5_init_creds_get(ctx, init_ctx)))
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR, "error while getting credentials");
		goto cleanup;
	}

	/* Retrieve credentials */
	if ((ret = krb5_init_creds_get_creds(ctx, init_ctx, &creds)))
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR, "error while retrieving credentials");
		goto cleanup;
	}

#ifdef WITH_GSSAPI_HEIMDAL

	/* For Heimdal, we use this function to store credentials */
	if ((ret = krb5_init_creds_store(ctx, init_ctx, ccache)))
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR, "error while storing credentials");
		goto cleanup;
	}

#endif
cleanup:
	krb5_free_cred_contents(ctx, &creds);
#ifdef HAVE_AT_LEAST_KRB_V1_13

	/* MIT Kerberos version 1.13 at minimum.
	 * For releases 1.12 and previous, krb5_get_init_creds_opt structure
	 * is freed in krb5_init_creds_free() */
	if (options)
		krb5_get_init_creds_opt_free(ctx, options);

#endif

	if (init_ctx)
		krb5_init_creds_free(ctx, init_ctx);

	if (ccache)
		krb5_cc_close(ctx, ccache);

	return ret;
}

static int init_creds(SEC_WINPR_KERBEROS_SETTINGS* options, LPCWSTR username, size_t username_len,
                      LPCWSTR domain, size_t domain_len, LPCWSTR password, size_t password_len)
{
	krb5_error_code ret = 0;
	krb5_context ctx = NULL;
	krb5_principal principal = NULL;
	char* krb_name = NULL;
	char* lusername = NULL;
	char* lrealm = NULL;
	char* lpassword = NULL;
	int flags = 0;
	char* pstr = NULL;
	size_t krb_name_len = 0;
	size_t lrealm_len = 0;
	size_t lusername_len = 0;
	int status = 0;
	BOOL isDefaultRealm = FALSE;

	status = ConvertFromUnicode(CP_UTF8, 0, username, username_len, &lusername, 0, NULL, NULL);

	if (status <= 0)
	{
		WLog_ERR(TAG, "Failed to convert username");
		goto cleanup;
	}

	status = ConvertFromUnicode(CP_UTF8, 0, password, password_len, &lpassword, 0, NULL, NULL);
	if (status <= 0)
	{
		WLog_ERR(TAG, "Failed to convert password");
		goto cleanup;
	}

	/* Could call krb5_init_secure_context, but it disallows user overrides */
	ret = krb5_init_context(&ctx);

	if (ret)
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR,
		                       "error: while initializing Kerberos 5 library");
		goto cleanup;
	}

	if (domain && domain_len)
	{
		char* tmp;
		if (ConvertFromUnicode(CP_UTF8, 0, domain, domain_len, &lrealm, 0, NULL, NULL) <= 0)
		{
			WLog_ERR(TAG, "error: converting domain");
		}

		/* convert to upper case */
		for (tmp = lrealm; *tmp; tmp++)
			*tmp = toupper(*tmp);

		isDefaultRealm = FALSE;
	}
	else
	{
		ret = krb5_get_default_realm(ctx, &lrealm);
		if (ret)
		{
			krb5_log_error_message(ctx, ret, WLOG_WARN, "could not get Kerberos default realm");
			goto cleanup;
		}
		isDefaultRealm = TRUE;
	}

	lrealm_len = strlen(lrealm);
	lusername_len = strlen(lusername);
	krb_name_len = lusername_len + lrealm_len + 1; // +1 for '@'
	krb_name = calloc(krb_name_len + 1, sizeof(char));

	if (!krb_name)
	{
		WLog_ERR(TAG, "could not allocate memory for string rep of principal\n");
		ret = -1;
		goto cleanup;
	}

	/* Set buffer */
	_snprintf(krb_name, krb_name_len + 1, "%s@%s", lusername, lrealm);
#ifdef WITH_DEBUG_NLA
	WLog_DBG(TAG, "copied string is %s\n", krb_name);
#endif
	pstr = strchr(lusername, '@');

	if (pstr != NULL)
		flags = KRB5_PRINCIPAL_PARSE_ENTERPRISE;

	/* Use the specified principal name. */
	ret = krb5_parse_name_flags(ctx, krb_name, flags, &principal);

	if (ret)
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR, "could not convert %s to principal", krb_name);
		goto cleanup;
	}

	ret = acquire_cred(ctx, options, principal, lpassword);

	if (ret)
	{
		krb5_log_error_message(ctx, ret, WLOG_ERROR,
		                       "Kerberos credentials not found and could not be acquired");
		goto cleanup;
	}

cleanup:
	free(lusername);
	free(lpassword);

	if (krb_name)
		free(krb_name);

	if (lrealm)
	{
		if (isDefaultRealm)
			krb5_free_default_realm(ctx, lrealm);
		else
			free(lrealm);
	}

	if (principal)
		krb5_free_principal(ctx, principal);

	if (ctx)
		krb5_free_context(ctx);

	return ret;
}

static SECURITY_STATUS setupCreds(KRB_CONTEXT* context, SSPI_CREDENTIALS* credentials)
{
	SEC_WINPR_KERBEROS_SETTINGS* kerbSettings = credentials->kerbSettings;
	sspi_gss_OID_set_desc krb5_set = { 1, SSPI_GSS_C_SPNEGO_KRB5 };
	sspi_gss_key_value_element_desc ccache_setting = { "ccache", NULL };
	sspi_gss_key_value_set_desc store = { 0, &ccache_setting };
	char* file_colon_cache = NULL;
	OM_uint32 ret, minor;

	if (kerbSettings && kerbSettings->cache)
	{
		size_t len = _snprintf(NULL, 0, "FILE:%s", kerbSettings->cache);
		file_colon_cache = malloc(len + 1);
		_snprintf(file_colon_cache, len + 1, "FILE:%s", kerbSettings->cache);
		store.count = 1;
		ccache_setting.value = file_colon_cache;
	}

	WLog_DBG(TAG, "acquiring credentials");
	ret = sspi_gss_acquire_cred_from(&minor, SSPI_GSS_C_NO_NAME, SSPI_GSS_C_INDEFINITE, &krb5_set,
	                                 SSPI_GSS_C_INITIATE, &store, &(context->cred), NULL, NULL);
	if (SSPI_GSS_ERROR(ret))
	{

		BOOL isNoCreds = !!(ret & SSPI_GSS_S_NO_CRED);
		if (isNoCreds)
		{
			/* Then let's try to acquire credentials using login and password,
			 * and only those two, means not with a smartcard.
			 * If we use smartcard-logon, the credentials have already
			 * been acquired by pkinit process. If not, returned error previously.
			 */
			if (init_creds(credentials->kerbSettings, credentials->identity.User,
			               credentials->identity.UserLength, credentials->identity.Domain,
			               credentials->identity.DomainLength, credentials->identity.Password,
			               credentials->identity.PasswordLength))
				return SEC_E_NO_CREDENTIALS;

			WLog_INFO(TAG, "Authenticated to Kerberos v5 via login/password");
			ret = sspi_gss_acquire_cred_from(&minor, SSPI_GSS_C_NO_NAME, SSPI_GSS_C_INDEFINITE, &krb5_set,
			                               SSPI_GSS_C_INITIATE, &store, &(context->cred), NULL, NULL);

		}

	}
	free(file_colon_cache);

	return ret;
}

#endif

static SECURITY_STATUS SEC_ENTRY kerberos_InitializeSecurityContextA(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, ULONG* pfContextAttr, PTimeStamp ptsExpiry)
{
	KRB_CONTEXT* context;
	SSPI_CREDENTIALS* credentials;
	PSecBuffer input_buffer = NULL;
	PSecBuffer output_buffer = NULL;
	sspi_gss_buffer_desc input_tok = { 0 };
	sspi_gss_buffer_desc output_tok = { 0 };
	sspi_gss_OID actual_mech;
	sspi_gss_OID desired_mech = SSPI_GSS_C_SPNEGO_KRB5;
	UINT32 actual_services;

	context = (KRB_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!context)
	{
		context = kerberos_ContextNew();

		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;

		credentials = (SSPI_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);
		context->credentials = credentials;

		if (kerberos_SetContextServicePrincipalNameA(context, pszTargetName) < 0)
		{
			kerberos_ContextFree(context);
			return SEC_E_INTERNAL_ERROR;
		}

		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetUpperPointer(phNewContext, (void*)KRB_PACKAGE_NAME);
	}
	else
		credentials = context->credentials;

	if (pInput)
	{
		input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);

		if (!input_buffer)
			return SEC_E_INVALID_TOKEN;

		if (input_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		input_tok.value = input_buffer->pvBuffer;
		input_tok.length = input_buffer->cbBuffer;
	}

	if (!pOutput)
		return SEC_E_INVALID_TOKEN;

	if (pOutput->cBuffers < 1)
		return SEC_E_INVALID_TOKEN;

	output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);
	if (!output_buffer)
		return SEC_E_INVALID_TOKEN;

	if (output_buffer->cbBuffer < 1)
		return SEC_E_INVALID_TOKEN;

#if defined(WITH_GSSAPI)
	if (context->cred == SSPI_GSS_C_NO_CREDENTIAL)
	{
		context->major_status = setupCreds(context, credentials);
		if (SSPI_GSS_ERROR(context->major_status))
			return context->major_status;
	}

	context->major_status = sspi_gss_init_sec_context(
		&(context->minor_status), context->cred, &(context->gss_ctx), context->target_name,
		desired_mech, SSPI_GSS_C_MUTUAL_FLAG | SSPI_GSS_C_DELEG_FLAG, SSPI_GSS_C_INDEFINITE,
		SSPI_GSS_C_NO_CHANNEL_BINDINGS, &input_tok, &actual_mech, &output_tok, &actual_services,
		&(context->actual_time));
#endif

	if (output_buffer->cbBuffer < output_tok.length)
		return SEC_E_INVALID_TOKEN;

	output_buffer->cbBuffer = output_tok.length;
	if (output_tok.length != 0)
	{
		CopyMemory(output_buffer->pvBuffer, output_tok.value, output_tok.length);
		sspi_gss_release_buffer(&(context->minor_status), &output_tok);
	}

	if (context->major_status & SSPI_GSS_S_CONTINUE_NEEDED)
		return SEC_I_CONTINUE_NEEDED;

	if (SSPI_GSS_ERROR(context->major_status))
		return SEC_E_INTERNAL_ERROR;

	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY kerberos_DeleteSecurityContext(PCtxtHandle phContext)
{
	KRB_CONTEXT* context;
	context = (KRB_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	kerberos_ContextFree(context);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY kerberos_QueryContextAttributesW(PCtxtHandle phContext,
                                                                  ULONG ulAttribute, void* pBuffer)
{
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY kerberos_QueryContextAttributesA(PCtxtHandle phContext,
                                                                  ULONG ulAttribute, void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	if (!pBuffer)
		return SEC_E_INSUFFICIENT_MEMORY;

	if (ulAttribute == SECPKG_ATTR_SIZES)
	{
		UINT32 major_status, minor_status, max_unwrapped;
		KRB_CONTEXT* context = (KRB_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

		SecPkgContext_Sizes* ContextSizes = (SecPkgContext_Sizes*)pBuffer;
		/* The MaxTokenSize by default is 12,000 bytes. This has been the default value
		 * since Windows 2000 SP2 and still remains in Windows 7 and Windows 2008 R2.
		 *  For Windows Server 2012, the default value of the MaxTokenSize registry
		 *  entry is 48,000 bytes.*/
		ContextSizes->cbMaxToken = KERBEROS_SecPkgInfoA.cbMaxToken;
		ContextSizes->cbMaxSignature = 0; /* means verify not supported */
		ContextSizes->cbBlockSize = 0;    /* padding not used */

		WINPR_ASSERT(context);

		major_status = sspi_gss_wrap_size_limit(&minor_status, context->gss_ctx, TRUE,
		                                        SSPI_GSS_C_QOP_DEFAULT, 12000, &max_unwrapped);
		if (SSPI_GSS_ERROR(major_status))
		{
			WLog_ERR(TAG, "unable to compute the trailer size with gss_wrap_size_limit()");
			return SEC_E_INTERNAL_ERROR;
		}

		context->trailerSize = ContextSizes->cbSecurityTrailer = (12000 - max_unwrapped);
		return SEC_E_OK;
	}

	WLog_ERR(TAG, "[%s]: TODO: Implement ulAttribute=%08" PRIx32, __FUNCTION__, ulAttribute);
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY kerberos_EncryptMessage(PCtxtHandle phContext, ULONG fQOP,
                                                         PSecBufferDesc pMessage,
                                                         ULONG MessageSeqNo)
{
	int index;
	int conf_state;
	UINT32 major_status;
	UINT32 minor_status;
	KRB_CONTEXT* context;
	sspi_gss_buffer_desc input;
	sspi_gss_buffer_desc output;
	BYTE* ptr;
	size_t toCopy;
	PSecBuffer data_buffer = NULL;
	PSecBuffer sig_buffer = NULL;
	SECURITY_STATUS ret = SEC_E_OK;

	context = (KRB_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	for (index = 0; index < (int)pMessage->cBuffers; index++)
	{
		if (pMessage->pBuffers[index].BufferType == SECBUFFER_DATA)
			data_buffer = &pMessage->pBuffers[index];
		else if (pMessage->pBuffers[index].BufferType == SECBUFFER_TOKEN)
			sig_buffer = &pMessage->pBuffers[index];
	}

	if (!data_buffer || !sig_buffer)
	{
		WLog_ERR(TAG, "kerberos_EncryptMessage: our winPR emulation can only handle calls with a "
		              "token AND a data buffer");
		return SEC_E_INVALID_TOKEN;
	}

	if ((BYTE*)sig_buffer->pvBuffer + sig_buffer->cbBuffer != data_buffer->pvBuffer)
	{
		WLog_ERR(TAG, "kerberos_EncryptMessage: our winPR emulation expects token and data buffer "
		              "to be contiguous");
		return SEC_E_INVALID_TOKEN;
	}

	input.value = data_buffer->pvBuffer;
	input.length = data_buffer->cbBuffer;
	major_status = sspi_gss_wrap(&minor_status, context->gss_ctx, TRUE, SSPI_GSS_C_QOP_DEFAULT,
	                             &input, &conf_state, &output);

	if (SSPI_GSS_ERROR(major_status))
		return SEC_E_INTERNAL_ERROR;

	if (conf_state == 0)
	{
		WLog_ERR(TAG, "error: gss_wrap confidentiality was not applied");
		ret = SEC_E_INTERNAL_ERROR;
		goto out;
	}

	if (output.length < context->trailerSize)
	{
		WLog_ERR(TAG, "error: output is smaller than expected trailerSize");
		ret = SEC_E_INTERNAL_ERROR;
		goto out;
	}

	/*
	 * we artificially fill the sig buffer and data_buffer, even if gss_wrap() puts the
	 * mic and message in the same place
	 */
	ptr = output.value;
	CopyMemory(sig_buffer->pvBuffer, ptr, context->trailerSize);
	ptr += context->trailerSize;

	toCopy = output.length - context->trailerSize;
	if (data_buffer->cbBuffer < toCopy)
	{
		ret = SEC_E_INTERNAL_ERROR;
		goto out;
	}
	CopyMemory(data_buffer->pvBuffer, ptr, toCopy);

out:
	sspi_gss_release_buffer(&minor_status, &output);
	return ret;
}

static SECURITY_STATUS SEC_ENTRY kerberos_DecryptMessage(PCtxtHandle phContext,
                                                         PSecBufferDesc pMessage,
                                                         ULONG MessageSeqNo, ULONG* pfQOP)
{
	int index;
	int conf_state;
	UINT32 major_status;
	UINT32 minor_status;
	KRB_CONTEXT* context;
	sspi_gss_buffer_desc input_data;
	sspi_gss_buffer_desc output;
	PSecBuffer data_buffer_to_unwrap = NULL, sig_buffer = NULL;
	context = (KRB_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
		return SEC_E_INVALID_HANDLE;

	for (index = 0; index < (int)pMessage->cBuffers; index++)
	{
		if (pMessage->pBuffers[index].BufferType == SECBUFFER_DATA)
			data_buffer_to_unwrap = &pMessage->pBuffers[index];
		else if (pMessage->pBuffers[index].BufferType == SECBUFFER_TOKEN)
			sig_buffer = &pMessage->pBuffers[index];
	}

	if (!data_buffer_to_unwrap || !sig_buffer)
	{
		WLog_ERR(TAG, "kerberos_DecryptMessage: our winPR emulation can only handle calls with a "
		              "token AND a data buffer");
		return SEC_E_INVALID_TOKEN;
	}

	if ((BYTE*)sig_buffer->pvBuffer + sig_buffer->cbBuffer != data_buffer_to_unwrap->pvBuffer)
	{
		WLog_ERR(TAG, "kerberos_DecryptMessage: our winPR emulation expects token and data buffer "
		              "to be contiguous");
		return SEC_E_INVALID_TOKEN;
	}

	/* unwrap encrypted TLS key AND its signature */
	input_data.value = sig_buffer->pvBuffer;
	input_data.length = sig_buffer->cbBuffer + data_buffer_to_unwrap->cbBuffer;
	major_status =
	    sspi_gss_unwrap(&minor_status, context->gss_ctx, &input_data, &output, &conf_state, NULL);

	if (SSPI_GSS_ERROR(major_status))
		return SEC_E_INTERNAL_ERROR;

	if (conf_state == 0)
	{
		WLog_ERR(TAG, "error: gss_unwrap confidentiality was not applied");
		sspi_gss_release_buffer(&minor_status, &output);
		return SEC_E_INTERNAL_ERROR;
	}

	CopyMemory(data_buffer_to_unwrap->pvBuffer, output.value, output.length);
	sspi_gss_release_buffer(&minor_status, &output);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY kerberos_MakeSignature(PCtxtHandle phContext, ULONG fQOP,
                                                        PSecBufferDesc pMessage, ULONG MessageSeqNo)
{
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY kerberos_VerifySignature(PCtxtHandle phContext,
                                                          PSecBufferDesc pMessage,
                                                          ULONG MessageSeqNo, ULONG* pfQOP)
{
	return SEC_E_OK;
}

const SecurityFunctionTableA KERBEROS_SecurityFunctionTableA = {
	1,                                    /* dwVersion */
	NULL,                                 /* EnumerateSecurityPackages */
	kerberos_QueryCredentialsAttributesA, /* QueryCredentialsAttributes */
	kerberos_AcquireCredentialsHandleA,   /* AcquireCredentialsHandle */
	kerberos_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                                 /* Reserved2 */
	kerberos_InitializeSecurityContextA,  /* InitializeSecurityContext */
	NULL,                                 /* AcceptSecurityContext */
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
	NULL,                                 /* SetContextAttributes */
};

const SecurityFunctionTableW KERBEROS_SecurityFunctionTableW = {
	1,                                    /* dwVersion */
	NULL,                                 /* EnumerateSecurityPackages */
	kerberos_QueryCredentialsAttributesW, /* QueryCredentialsAttributes */
	kerberos_AcquireCredentialsHandleW,   /* AcquireCredentialsHandle */
	kerberos_FreeCredentialsHandle,       /* FreeCredentialsHandle */
	NULL,                                 /* Reserved2 */
	kerberos_InitializeSecurityContextW,  /* InitializeSecurityContext */
	NULL,                                 /* AcceptSecurityContext */
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
	NULL,                                 /* SetContextAttributes */
};
