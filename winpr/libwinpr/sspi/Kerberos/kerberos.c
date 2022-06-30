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
#include <winpr/tchar.h>
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
	sspi_gss_ctx_id_t gss_ctx;
	sspi_gss_name_t target_name;
	UINT32 trailerSize;
};

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

#ifdef WITH_GSSAPI
static sspi_gss_OID_desc g_SSPI_GSS_C_SPNEGO_KRB5 = {
	9, (void*)"\x2a\x86\x48\x86\xf7\x12\x01\x02\x02"
};
static sspi_gss_OID SSPI_GSS_C_SPNEGO_KRB5 = &g_SSPI_GSS_C_SPNEGO_KRB5;
#endif

static KRB_CONTEXT* kerberos_ContextNew(void)
{
	KRB_CONTEXT* context;

	context = (KRB_CONTEXT*)calloc(1, sizeof(KRB_CONTEXT));
	if (!context)
		return NULL;

	context->gss_ctx = SSPI_GSS_C_NO_CONTEXT;

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

	if (context->gss_ctx)
		sspi_gss_delete_sec_context(&minor_status, &context->gss_ctx, SSPI_GSS_C_NO_BUFFER);

	free(context);
}

#ifdef WITH_GSSAPI

static krb5_error_code krb5_prompter(krb5_context context, void* data, const char* name,
                                     const char* banner, int num_prompts, krb5_prompt prompts[])
{
	int i;
	krb5_prompt_type* ptypes = krb5_get_prompt_types(context);

	for (i = 0; i < num_prompts; i++)
	{
		if (ptypes && ptypes[i] == KRB5_PROMPT_TYPE_PREAUTH && data)
		{
			prompts[i].reply->data = _strdup((const char*)data);
			prompts[i].reply->length = strlen((const char*)data);
		}
	}
	return 0;
}

static void gss_log_status_messages(OM_uint32 major_status, OM_uint32 minor_status)
{
	OM_uint32 minor, msg_ctx = 0, status = major_status;
	int status_type = SSPI_GSS_C_GSS_CODE;

	/* If the failure was in the underlying mechanism log those messages */
	if (major_status == SSPI_GSS_S_FAILURE)
	{
		status_type = SSPI_GSS_C_MECH_CODE;
		status = minor_status;
	}

	do
	{
		sspi_gss_buffer_desc buffer = { 0 };
		sspi_gss_display_status(&minor, status, status_type, SSPI_GSS_C_NO_OID, &msg_ctx, &buffer);
		WLog_ERR(TAG, buffer.value);
		sspi_gss_release_buffer(&major_status, &buffer);
	} while (msg_ctx != 0);
}

/* taken from lib/gssapi/krb5/gssapi_err_krb5.h */
#define KG_EMPTY_CCACHE (39756044L)

static INLINE BOOL sspi_is_no_creds(OM_uint32 major, OM_uint32 minor)
{
	return (major == SSPI_GSS_S_NO_CRED) ||
	       (major == SSPI_GSS_S_FAILURE && minor == KG_EMPTY_CCACHE);
}
#endif /* WITH_GSSAPI */

static SECURITY_STATUS SEC_ENTRY kerberos_AcquireCredentialsHandleA(
    SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
#ifdef WITH_GSSAPI
	SEC_WINNT_AUTH_IDENTITY* identity = pAuthData;
	SEC_WINPR_KERBEROS_SETTINGS* krb_settings = NULL;
	krb5_error_code rv;
	krb5_context ctx = NULL;
	krb5_ccache ccache = NULL;
	krb5_keytab keytab = NULL;
	krb5_get_init_creds_opt* gic_opt = NULL;
	krb5_deltat start_time = 0;
	krb5_principal principal = NULL;
	krb5_principal cache_principal = NULL;
	krb5_creds creds;
	BOOL is_unicode = FALSE;
	char* domain = NULL;
	char* username = NULL;
	char* password = NULL;
	char* ccache_name = NULL;
	char keytab_name[PATH_MAX];
	sspi_gss_OID_set_desc desired_mechs = { 1, SSPI_GSS_C_SPNEGO_KRB5 };
	sspi_gss_key_value_element_desc cred_store_opts[2];
	sspi_gss_key_value_set_desc cred_store = { 2, cred_store_opts };
	sspi_gss_cred_id_t gss_creds = NULL;
	OM_uint32 major, minor;
	OM_uint32 time_rec;
	int cred_usage;

	switch (fCredentialUse)
	{
		case SECPKG_CRED_INBOUND:
			cred_usage = SSPI_GSS_C_ACCEPT;
			break;
		case SECPKG_CRED_OUTBOUND:
			cred_usage = SSPI_GSS_C_INITIATE;
			break;
		case SECPKG_CRED_BOTH:
			cred_usage = SSPI_GSS_C_BOTH;
			break;
	}

	if (identity)
	{
		if (identity->Flags & SEC_WINNT_AUTH_IDENTITY_EXTENDED)
			krb_settings = &((SEC_WINNT_AUTH_IDENTITY_WINPR*)identity)->kerberosSettings;

		if (identity->Flags & SEC_WINNT_AUTH_IDENTITY_UNICODE)
		{
			is_unicode = TRUE;
			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)identity->Domain, identity->DomainLength,
			                   &domain, 0, NULL, NULL);
			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)identity->User, identity->UserLength, &username,
			                   0, NULL, NULL);
			ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)identity->Password, identity->PasswordLength,
			                   &password, 0, NULL, NULL);
		}
		else
		{
			domain = (char*)identity->Domain;
			username = (char*)identity->User;
			password = (char*)identity->Password;
		}

		if (!pszPrincipal)
			pszPrincipal = username;
	}

	if ((rv = krb5_init_context(&ctx)))
		goto cleanup;

	if (domain)
	{
		CharUpperA(domain);
		/* Will use domain if realm is not specified in username */
		if ((rv = krb5_set_default_realm(ctx, domain)))
			goto cleanup;
	}

	if (pszPrincipal)
	{
		/* Find realm component if included and convert to uppercase */
		char* p = strchr(pszPrincipal, '@');
		CharUpperA(p);

		if ((rv = krb5_parse_name(ctx, pszPrincipal, &principal)))
			goto cleanup;
	}

	if (krb_settings && krb_settings->cache)
	{
		if ((rv = krb5_cc_resolve(ctx, krb_settings->cache, &ccache)))
			goto cleanup;

		/* Make sure the cache is initialized with the right principal */
		if (principal)
		{
			if ((rv = krb5_cc_get_principal(ctx, ccache, &cache_principal)))
				goto cleanup;
			if (!krb5_principal_compare(ctx, principal, cache_principal))
				if ((rv = krb5_cc_initialize(ctx, ccache, principal)))
					goto cleanup;
		}
	}
	else if (principal)
	{
		/* Use the default cache if it's initialized with the right principal */
		if (krb5_cc_cache_match(ctx, principal, &ccache) == KRB5_CC_NOTFOUND)
		{
			if ((rv = krb5_cc_resolve(ctx, "MEMORY:", &ccache)))
				goto cleanup;
			if ((rv = krb5_cc_initialize(ctx, ccache, principal)))
				goto cleanup;
		}
	}
	else if (fCredentialUse & SECPKG_CRED_OUTBOUND)
	{
		/* Use the default cache with it's default principal */
		if ((rv = krb5_cc_default(ctx, &ccache)))
			goto cleanup;
		if ((rv = krb5_cc_get_principal(ctx, ccache, &principal)))
			goto cleanup;
	}
	else
	{
		if ((rv = krb5_cc_resolve(ctx, "MEMORY:", &ccache)))
			goto cleanup;
	}

	if (krb_settings && krb_settings->keytab)
	{
		if ((rv = krb5_kt_resolve(ctx, krb_settings->keytab, &keytab)))
			goto cleanup;
	}
	else
	{
		if ((rv = krb5_kt_default(ctx, &keytab)))
			goto cleanup;
	}

	if (fCredentialUse & SECPKG_CRED_OUTBOUND)
	{
		if ((rv = krb5_get_init_creds_opt_alloc(ctx, &gic_opt)))
			goto cleanup;

		krb5_get_init_creds_opt_set_forwardable(gic_opt, 0);
		krb5_get_init_creds_opt_set_proxiable(gic_opt, 0);

		if (krb_settings)
		{
			if (krb_settings->startTime)
				start_time = krb_settings->startTime;
			if (krb_settings->lifeTime)
				krb5_get_init_creds_opt_set_tkt_life(gic_opt, krb_settings->lifeTime);
			if (krb_settings->renewLifeTime)
				krb5_get_init_creds_opt_set_renew_life(gic_opt, krb_settings->renewLifeTime);
			if (krb_settings->withPac)
				krb5_get_init_creds_opt_set_pac_request(ctx, gic_opt, TRUE);
			if (krb_settings->armorCache)
				krb5_get_init_creds_opt_set_fast_ccache_name(ctx, gic_opt,
				                                             krb_settings->armorCache);
			if (krb_settings->pkinitX509Identity)
				krb5_get_init_creds_opt_set_pa(ctx, gic_opt, "X509_user_identity",
				                               krb_settings->pkinitX509Identity);
			if (krb_settings->pkinitX509Anchors)
				krb5_get_init_creds_opt_set_pa(ctx, gic_opt, "X509_anchors",
				                               krb_settings->pkinitX509Anchors);
		}

#ifdef WITH_GSSAPI_MIT
		krb5_get_init_creds_opt_set_out_ccache(ctx, gic_opt, ccache);
		krb5_get_init_creds_opt_set_in_ccache(ctx, gic_opt, ccache);
#endif
	}

	krb5_cc_get_full_name(ctx, ccache, &ccache_name);
	krb5_kt_get_name(ctx, keytab, keytab_name, PATH_MAX);

	cred_store_opts[0].key = "ccache";
	cred_store_opts[0].value = ccache_name;
	cred_store_opts[1].key = "keytab";
	cred_store_opts[1].value = keytab_name;

	/* Check if there are initial creds already in the cache there's no need to request new ones */
	major = sspi_gss_acquire_cred_from(&minor, SSPI_GSS_C_NO_NAME, SSPI_GSS_C_INDEFINITE,
	                                   &desired_mechs, cred_usage, &cred_store, &gss_creds, NULL,
	                                   &time_rec);
	gss_log_status_messages(major, minor);

	/* No use getting initial creds for inbound creds */
	if (!(fCredentialUse & SECPKG_CRED_OUTBOUND))
		goto cleanup;

	if (!sspi_is_no_creds(major, minor) && time_rec > 0)
		goto cleanup;

	if ((rv = krb5_get_init_creds_password(ctx, &creds, principal, password, krb5_prompter,
	                                       password, start_time, NULL, gic_opt)))
		goto cleanup;

#ifdef WITH_GSSAPI_HEIMDAL
	if (rv = krb5_cc_store_cred(ctx, ccache, &creds))
		goto cleanup;
#endif

	major =
	    sspi_gss_acquire_cred_from(&minor, SSPI_GSS_C_NO_NAME, SSPI_GSS_C_INDEFINITE,
	                               &desired_mechs, cred_usage, &cred_store, &gss_creds, NULL, NULL);
	if (SSPI_GSS_ERROR(major))
		gss_log_status_messages(major, minor);

cleanup:

	if (rv)
	{
		const char* msg = krb5_get_error_message(ctx, rv);
		WLog_ERR(TAG, msg);
		krb5_free_error_message(ctx, msg);
	}

	if (is_unicode)
	{
		if (domain)
			free(domain);
		if (username)
			free(username);
		if (password)
			free(password);
	}
	if (keytab)
		krb5_kt_close(ctx, keytab);
	if (ccache_name)
		krb5_free_string(ctx, ccache_name);
	if (principal)
		krb5_free_principal(ctx, principal);
#ifdef HAVE_AT_LEAST_KRB_V1_13
	if (gic_opt)
		krb5_get_init_creds_opt_free(ctx, gic_opt);
#endif
	if (ccache)
		krb5_cc_close(ctx, ccache);
	if (ctx)
		krb5_free_context(ctx);

	/* If we managed to get gss credentials set the output */
	if (gss_creds)
	{
		sspi_SecureHandleSetLowerPointer(phCredential, (void*)gss_creds);
		sspi_SecureHandleSetUpperPointer(phCredential, (void*)KERBEROS_SSP_NAME);
		return SEC_E_OK;
	}
#endif /*WITH_GSSAPI*/
	return SEC_E_NO_CREDENTIALS;
}

static SECURITY_STATUS SEC_ENTRY kerberos_AcquireCredentialsHandleW(
    SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	char* principal = NULL;
	char* package = NULL;

	if (pszPrincipal)
		ConvertFromUnicode(CP_UTF8, 0, pszPrincipal, -1, &principal, 0, NULL, NULL);
	if (pszPackage)
		ConvertFromUnicode(CP_UTF8, 0, pszPackage, -1, &package, 0, NULL, NULL);

	status =
	    kerberos_AcquireCredentialsHandleA(principal, package, fCredentialUse, pvLogonID, pAuthData,
	                                       pGetKeyFn, pvGetKeyArgument, phCredential, ptsExpiry);

	if (principal)
		free(principal);
	if (package)
		free(package);

	return status;
}

static SECURITY_STATUS SEC_ENTRY kerberos_FreeCredentialsHandle(PCredHandle phCredential)
{
	UINT32 minor;
	sspi_gss_cred_id_t credentials;

	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	credentials = sspi_SecureHandleGetLowerPointer(phCredential);

	if (credentials)
		sspi_gss_release_cred(&minor, &credentials);

	sspi_SecureHandleInvalidate(phCredential);
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

static SECURITY_STATUS SEC_ENTRY kerberos_InitializeSecurityContextA(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, ULONG* pfContextAttr, PTimeStamp ptsExpiry)
{
#ifdef WITH_GSSAPI
	KRB_CONTEXT* context;
	PSecBuffer input_buffer = NULL;
	PSecBuffer output_buffer = NULL;
	sspi_gss_buffer_desc input_tok = { 0 };
	sspi_gss_buffer_desc output_tok = { 0 };
	sspi_gss_cred_id_t gss_creds = SSPI_GSS_C_NO_CREDENTIAL;
	sspi_gss_buffer_desc target_name;
	sspi_gss_OID actual_mech;
	UINT32 major, minor;
	UINT32 actual_time;

	gss_creds = sspi_SecureHandleGetLowerPointer(phCredential);
	context = sspi_SecureHandleGetLowerPointer(phContext);

	if (!context)
	{
		context = kerberos_ContextNew();

		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;

		/* Prepare to import the target name */
		target_name.value = strdup(pszTargetName);
		target_name.length = strlen(pszTargetName);
		/* gssapi expects the target name in the form service@hostanme */
		char* p = strchr(target_name.value, '/');
		if (p)
			*p = '@';

		major = sspi_gss_import_name(&minor, &target_name, SSPI_GSS_C_NT_HOSTBASED_SERVICE,
		                             &context->target_name);

		target_name.length = 0;
		free(target_name.value);

		if (SSPI_GSS_ERROR(major))
			return SEC_E_TARGET_UNKNOWN;
	}

	if (pInput)
	{
		input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);

		if (!input_buffer || input_buffer->cbBuffer < 1)
			return SEC_E_INVALID_TOKEN;

		input_tok.value = input_buffer->pvBuffer;
		input_tok.length = input_buffer->cbBuffer;
	}

	if (!pOutput || pOutput->cBuffers < 1)
		return SEC_E_INVALID_TOKEN;

	output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);
	if (!output_buffer || output_buffer->cbBuffer < 1)
		return SEC_E_INVALID_TOKEN;

	major = sspi_gss_init_sec_context(&minor, gss_creds, &context->gss_ctx, context->target_name,
	                                  SSPI_GSS_C_SPNEGO_KRB5, fContextReq, SSPI_GSS_C_INDEFINITE,
	                                  SSPI_GSS_C_NO_CHANNEL_BINDINGS, &input_tok, &actual_mech,
	                                  &output_tok, pfContextAttr, &actual_time);

	/* If the method failed and this is the first call, release the new context
	    otherwise set the ouput context */
	if (SSPI_GSS_ERROR(major) && !phContext)
	{
		kerberos_ContextFree(context);
	}
	else
	{
		sspi_SecureHandleSetUpperPointer(phNewContext, KERBEROS_SSP_NAME);
		sspi_SecureHandleSetLowerPointer(phNewContext, context);
	}

	if (output_buffer->cbBuffer < output_tok.length)
		return SEC_E_INVALID_TOKEN;

	output_buffer->cbBuffer = output_tok.length;
	if (output_tok.length != 0)
	{
		CopyMemory(output_buffer->pvBuffer, output_tok.value, output_tok.length);
		sspi_gss_release_buffer(&minor, &output_tok);
	}

	if (major & SSPI_GSS_S_CONTINUE_NEEDED)
		return SEC_I_CONTINUE_NEEDED;

	if (SSPI_GSS_ERROR(major))
	{
		gss_log_status_messages(major, minor);
		return SEC_E_INTERNAL_ERROR;
	}

	return SEC_E_OK;
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif /* WITH_GSSAPI */
}

static SECURITY_STATUS SEC_ENTRY kerberos_InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, ULONG* pfContextAttr, PTimeStamp ptsExpiry)
{
	SECURITY_STATUS status;
	char* target_name = NULL;

	if (pszTargetName)
		ConvertFromUnicode(CP_UTF8, 0, pszTargetName, -1, &target_name, 0, NULL, NULL);

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
#ifdef WITH_GSSAPI
	KRB_CONTEXT* context;
	sspi_gss_cred_id_t creds;
	sspi_gss_ctx_id_t gss_ctx = SSPI_GSS_C_NO_CONTEXT;
	PSecBuffer input_buffer;
	PSecBuffer output_buffer = NULL;
	sspi_gss_buffer_desc input_token;
	sspi_gss_buffer_desc output_token;
	UINT32 major, minor;
	UINT32 time_rec;

	context = sspi_SecureHandleGetLowerPointer(phContext);
	creds = sspi_SecureHandleGetLowerPointer(phCredential);

	if (pInput)
		input_buffer = sspi_FindSecBuffer(pInput, SECBUFFER_TOKEN);
	if (pOutput)
		output_buffer = sspi_FindSecBuffer(pOutput, SECBUFFER_TOKEN);

	if (!input_buffer)
		return SEC_E_INVALID_TOKEN;

	if (context)
		gss_ctx = context->gss_ctx;

	input_token.length = input_buffer->cbBuffer;
	input_token.value = input_buffer->pvBuffer;

	major = sspi_gss_accept_sec_context(&minor, &gss_ctx, creds, &input_token,
	                                    SSPI_GSS_C_NO_CHANNEL_BINDINGS, NULL, NULL, &output_token,
	                                    pfContextAttr, &time_rec, NULL);

	if (SSPI_GSS_ERROR(major))
	{
		gss_log_status_messages(major, minor);
		return SEC_E_INTERNAL_ERROR;
	}

	if (output_token.length > 0)
	{
		if (output_buffer && output_buffer->cbBuffer >= output_token.length)
		{
			output_buffer->cbBuffer = output_token.length;
			CopyMemory(output_buffer->pvBuffer, output_token.value, output_token.length);
			sspi_gss_release_buffer(&minor, &output_token);
		}
		else
		{
			sspi_gss_release_buffer(&minor, &output_token);
			return SEC_E_INVALID_TOKEN;
		}
	}

	if (!context)
	{
		context = kerberos_ContextNew();
		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;
	}

	context->gss_ctx = gss_ctx;
	sspi_SecureHandleSetUpperPointer(phNewContext, KERBEROS_SSP_NAME);
	sspi_SecureHandleSetLowerPointer(phNewContext, context);

	if (major & SSPI_GSS_S_CONTINUE_NEEDED)
		return SEC_I_CONTINUE_NEEDED;

	return SEC_E_OK;
#else
	return SEC_E_UNSUPPORTED_FUNCTION;
#endif /* WITH_GSSAPI */
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

static SECURITY_STATUS SEC_ENTRY kerberos_QueryContextAttributesW(PCtxtHandle phContext,
                                                                  ULONG ulAttribute, void* pBuffer)
{
	return kerberos_QueryContextAttributesA(phContext, ulAttribute, pBuffer);
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
	KRB_CONTEXT* context;
	PSecBuffer data_buffer = NULL;
	PSecBuffer sig_buffer = NULL;
	sspi_gss_buffer_desc message;
	sspi_gss_buffer_desc mic;
	UINT32 major, minor;

	context = sspi_SecureHandleGetLowerPointer(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	for (int i = 0; i < pMessage->cBuffers; i++)
	{
		if (pMessage->pBuffers[i].BufferType == SECBUFFER_DATA)
			data_buffer = &pMessage->pBuffers[i];
		else if (pMessage->pBuffers[i].BufferType == SECBUFFER_TOKEN)
			sig_buffer = &pMessage->pBuffers[i];
	}

	if (!data_buffer || !sig_buffer)
		return SEC_E_INVALID_TOKEN;

	message.length = data_buffer->cbBuffer;
	message.value = data_buffer->pvBuffer;
	major = sspi_gss_get_mic(&minor, context->gss_ctx, fQOP, &message, &mic);

	if (SSPI_GSS_ERROR(major))
		return SEC_E_INTERNAL_ERROR;

	if (sig_buffer->cbBuffer < mic.length)
	{
		sspi_gss_release_buffer(&minor, &mic);
		return SEC_E_INSUFFICIENT_MEMORY;
	}

	CopyMemory(sig_buffer->pvBuffer, mic.value, mic.length);
	sig_buffer->cbBuffer = mic.length;
	sspi_gss_release_buffer(&minor, &mic);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY kerberos_VerifySignature(PCtxtHandle phContext,
                                                          PSecBufferDesc pMessage,
                                                          ULONG MessageSeqNo, ULONG* pfQOP)
{
	KRB_CONTEXT* context;
	PSecBuffer data_buffer = NULL;
	PSecBuffer sig_buffer = NULL;
	sspi_gss_buffer_desc message;
	sspi_gss_buffer_desc mic;
	UINT32 major, minor;

	context = sspi_SecureHandleGetLowerPointer(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	for (int i = 0; i < pMessage->cBuffers; i++)
	{
		if (pMessage->pBuffers[i].BufferType == SECBUFFER_DATA)
			data_buffer = &pMessage->pBuffers[i];
		else if (pMessage->pBuffers[i].BufferType == SECBUFFER_TOKEN)
			sig_buffer = &pMessage->pBuffers[i];
	}

	if (!data_buffer || !sig_buffer)
		return SEC_E_INVALID_TOKEN;

	message.length = data_buffer->cbBuffer;
	message.value = data_buffer->pvBuffer;
	mic.length = sig_buffer->cbBuffer;
	mic.value = sig_buffer->pvBuffer;

	major = sspi_gss_verify_mic(&minor, context->gss_ctx, &message, &mic, pfQOP);

	if (SSPI_GSS_ERROR(major))
		return SEC_E_INTERNAL_ERROR;

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
	NULL,                                 /* SetContextAttributes */
};
