/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: Copyright 2025 Siemens
 */

#include <sso-mib/sso-mib.h>
#include <freerdp/crypto/crypto.h>
#include <winpr/json.h>

#include "sso_mib_tokens.h"

BOOL sso_mib_get_avd_access_token(freerdp* instance, char** token)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	rdpClientContext* client_context = (rdpClientContext*)instance->context;
	WINPR_ASSERT(client_context->mibClientWrapper);
	WINPR_ASSERT(client_context->mibClientWrapper->app);
	WINPR_ASSERT(token);

	MIBAccount* account = NULL;
	GSList* scopes = NULL;

	BOOL rc = FALSE;
	*token = NULL;

	account = mib_client_app_get_account_by_upn(client_context->mibClientWrapper->app, NULL);
	if (!account)
	{
		goto cleanup;
	}

	scopes = g_slist_append(scopes, g_strdup("https://www.wvd.microsoft.com/.default"));

	MIBPrt* prt = mib_client_app_acquire_token_silent(client_context->mibClientWrapper->app,
	                                                  account, scopes, NULL, NULL, NULL);
	if (prt)
	{
		const char* access_token = mib_prt_get_access_token(prt);
		if (access_token)
		{
			*token = strdup(access_token);
		}
		g_object_unref(prt);
	}

	rc = TRUE && *token != NULL;
cleanup:
	if (account)
		g_object_unref(account);
	g_slist_free_full(scopes, g_free);
	return rc;
}

BOOL sso_mib_get_rdsaad_access_token(freerdp* instance, const char* scope, const char* req_cnf,
                                     char** token)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);
	rdpClientContext* client_context = (rdpClientContext*)instance->context;
	WINPR_ASSERT(client_context->mibClientWrapper);
	WINPR_ASSERT(client_context->mibClientWrapper->app);
	WINPR_ASSERT(scope);
	WINPR_ASSERT(token);
	WINPR_ASSERT(req_cnf);

	GSList* scopes = NULL;
	WINPR_JSON* json = NULL;
	MIBPopParams* params = NULL;

	BOOL rc = FALSE;
	*token = NULL;
	BYTE* req_cnf_dec = NULL;
	size_t req_cnf_dec_len = 0;

	scopes = g_slist_append(scopes, g_strdup(scope));

	// Parse the "kid" element from req_cnf
	crypto_base64_decode(req_cnf, strlen(req_cnf) + 1, &req_cnf_dec, &req_cnf_dec_len);
	if (!req_cnf_dec)
	{
		goto cleanup;
	}

	json = WINPR_JSON_Parse((const char*)req_cnf_dec);
	if (!json)
	{
		goto cleanup;
	}
	WINPR_JSON* prop = WINPR_JSON_GetObjectItem(json, "kid");
	if (!prop)
	{
		goto cleanup;
	}
	const char* kid = WINPR_JSON_GetStringValue(prop);
	if (!kid)
	{
		goto cleanup;
	}

	params = mib_pop_params_new(MIB_AUTH_SCHEME_POP, MIB_REQUEST_METHOD_GET, "");
	mib_pop_params_set_kid(params, kid);
	MIBPrt* prt = mib_client_app_acquire_token_interactive(
	    client_context->mibClientWrapper->app, scopes, MIB_PROMPT_NONE, NULL, NULL, NULL, params);
	if (prt)
	{
		*token = strdup(mib_prt_get_access_token(prt));
		rc = TRUE;
		g_object_unref(prt);
	}

cleanup:
	if (params)
		g_object_unref(params);
	WINPR_JSON_Delete(json);
	free(req_cnf_dec);
	g_slist_free_full(scopes, g_free);
	return rc;
}
