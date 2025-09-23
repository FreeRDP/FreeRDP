/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: Copyright 2025 Siemens
 */

#include <sso-mib/sso-mib.h>
#include <freerdp/crypto/crypto.h>
#include <winpr/json.h>

#include "sso_mib_tokens.h"

#include <freerdp/log.h>
#define TAG CLIENT_TAG("common.sso")

enum sso_mib_state
{
	SSO_MIB_STATE_INIT = 0,
	SSO_MIB_STATE_FAILED = 1,
	SSO_MIB_STATE_SUCCESS = 2,
};

struct MIBClientWrapper
{
	MIBClientApp* app;
	enum sso_mib_state state;
	pGetCommonAccessToken GetCommonAccessToken;
};

static BOOL sso_mib_get_avd_access_token(rdpClientContext* client_context, char** token)
{
	WINPR_ASSERT(client_context);
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

static BOOL sso_mib_get_rdsaad_access_token(rdpClientContext* client_context, const char* scope,
                                            const char* req_cnf, char** token)
{
	WINPR_ASSERT(client_context);
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
	WINPR_JSON* prop = WINPR_JSON_GetObjectItemCaseSensitive(json, "kid");
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

static BOOL sso_mib_get_access_token(rdpContext* context, AccessTokenType tokenType, char** token,
                                     size_t count, ...)
{
	BOOL rc = FALSE;
	rdpClientContext* client_context = (rdpClientContext*)context;
	WINPR_ASSERT(client_context);
	WINPR_ASSERT(client_context->mibClientWrapper);

	if (!client_context->mibClientWrapper->app)
	{
		const char* client_id =
		    freerdp_settings_get_string(context->settings, FreeRDP_GatewayAvdClientID);
		client_context->mibClientWrapper->app =
		    mib_public_client_app_new(client_id, MIB_AUTHORITY_COMMON, NULL, NULL);
	}

	if (!client_context->mibClientWrapper->app)
		return FALSE;

	const char* scope = NULL;
	const char* req_cnf = NULL;

	va_list ap;
	va_start(ap, count);

	if (tokenType == ACCESS_TOKEN_TYPE_AAD)
	{
		scope = va_arg(ap, const char*);
		req_cnf = va_arg(ap, const char*);
	}

	if ((client_context->mibClientWrapper->state == SSO_MIB_STATE_INIT) ||
	    (client_context->mibClientWrapper->state == SSO_MIB_STATE_SUCCESS))
	{
		switch (tokenType)
		{
			case ACCESS_TOKEN_TYPE_AVD:
			{
				rc = sso_mib_get_avd_access_token(client_context, token);
				if (rc)
					client_context->mibClientWrapper->state = SSO_MIB_STATE_SUCCESS;
				else
				{
					WLog_WARN(TAG, "Getting AVD token from identity broker failed, falling back to "
					               "browser-based authentication.");
					client_context->mibClientWrapper->state = SSO_MIB_STATE_FAILED;
				}
			}
			break;
			case ACCESS_TOKEN_TYPE_AAD:
			{
				// Setup scope without URL encoding for sso-mib
				char* scope_copy = winpr_str_url_decode(scope, strlen(scope));
				if (!scope_copy)
					WLog_ERR(TAG, "Failed to decode scope");
				else
				{
					rc =
					    sso_mib_get_rdsaad_access_token(client_context, scope_copy, req_cnf, token);
					free(scope_copy);
					if (rc)
						client_context->mibClientWrapper->state = SSO_MIB_STATE_SUCCESS;
					else
					{
						WLog_WARN(TAG,
						          "Getting RDS token from identity broker failed, falling back to "
						          "browser-based authentication.");
						client_context->mibClientWrapper->state = SSO_MIB_STATE_FAILED;
					}
				}
			}
			break;
			default:
				break;
		}
	}
	if (!rc && client_context->mibClientWrapper->GetCommonAccessToken)
		rc = client_context->mibClientWrapper->GetCommonAccessToken(context, tokenType, token,
		                                                            count, scope, req_cnf);
	va_end(ap);

	return rc;
}

MIBClientWrapper* sso_mib_new(rdpContext* context)
{

	MIBClientWrapper* mibClientWrapper = (MIBClientWrapper*)calloc(1, sizeof(MIBClientWrapper));
	if (!mibClientWrapper)
		return NULL;

	mibClientWrapper->GetCommonAccessToken = freerdp_get_common_access_token(context);
	if (!freerdp_set_common_access_token(context, sso_mib_get_access_token))
	{
		sso_mib_free(mibClientWrapper);
		return NULL;
	}
	mibClientWrapper->state = SSO_MIB_STATE_INIT;
	return mibClientWrapper;
}

void sso_mib_free(MIBClientWrapper* sso)
{
	if (!sso)
		return;

	if (sso->app)
		g_object_unref(sso->app);

	free(sso);
}
