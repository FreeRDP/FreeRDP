/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Popup browser for AAD authentication
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
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

#include <string>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <memory>

#include <winpr/crt.h>
#include <winpr/string.h>
#include <freerdp/log.h>
#include <freerdp/utils/aad.h>

#include "sdl_webview.hpp"
#include "webview_impl.hpp"

#define TAG CLIENT_TAG("SDL.webview")

/** @brief Release a string that held credential material
 *
 *  A token request body carries the authorization code and the PKCE code verifier, so it is
 *  scrubbed before it is released, like the bodies client/common builds for the paste flow.
 *
 *  @param str The string to scrub and release, may be \b nullptr
 */
static void free_secret(char* str)
{
	if (str)
		SecureZeroMemory(str, strlen(str));
	free(str);
}

static std::string from_settings(const rdpSettings* settings, FreeRDP_Settings_Keys_String id)
{
	auto val = freerdp_settings_get_string(settings, id);
	if (!val)
	{
		WLog_WARN(TAG, "Settings key %s is nullptr", freerdp_settings_get_name_for_key(id));
		return "";
	}
	return val;
}

static std::string from_aad_wellknown(rdpContext* context, AAD_WELLKNOWN_VALUES which)
{
	auto val = freerdp_utils_aad_get_wellknown_string(context, which);

	if (!val)
	{
		WLog_WARN(TAG, "[wellknown] key %s is nullptr",
		          freerdp_utils_aad_wellknwon_value_name(which));
		return "";
	}
	return val;
}

static BOOL sdl_webview_get_rdsaad_access_token(freerdp* instance, const char* scope,
                                                const char* req_cnf, char** token)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(scope);
	WINPR_ASSERT(req_cnf);
	WINPR_ASSERT(token);

	WINPR_UNUSED(instance);

	auto context = instance->context;
	WINPR_UNUSED(context);

	auto settings = context->settings;
	WINPR_ASSERT(settings);

	auto cctx = reinterpret_cast<rdpClientContext*>(instance->context);
	std::shared_ptr<char> request(
	    freerdp_client_get_aad_url(cctx, FREERDP_CLIENT_AAD_AUTH_REQUEST, scope), free);
	if (!request)
	{
		WLog_ERR(TAG, "failed to build the AAD authorization request");
		return FALSE;
	}

	const std::string title = "FreeRDP WebView - AAD access token";
	std::string code;
	auto rc = webview_impl_run(title, request.get(), cctx, code);
	if (!rc || code.empty())
		return FALSE;

	std::shared_ptr<char> token_request(freerdp_client_get_aad_url(cctx,
	                                                               FREERDP_CLIENT_AAD_TOKEN_REQUEST,
	                                                               scope, code.c_str(), req_cnf),
	                                    free_secret);
	if (!token_request)
		return FALSE;

	return client_common_get_access_token(instance, token_request.get(), token);
}

static BOOL sdl_webview_get_avd_access_token(freerdp* instance, char** token)
{
	WINPR_ASSERT(token);
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	auto cctx = reinterpret_cast<rdpClientContext*>(instance->context);
	std::shared_ptr<char> request(
	    freerdp_client_get_aad_url(cctx, FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST), free);
	if (!request)
	{
		WLog_ERR(TAG, "failed to build the AVD authorization request");
		return FALSE;
	}

	const std::string title = "FreeRDP WebView - AVD access token";
	std::string code;
	auto rc = webview_impl_run(title, request.get(), cctx, code);
	if (!rc || code.empty())
		return FALSE;

	std::shared_ptr<char> token_request(
	    freerdp_client_get_aad_url(cctx, FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST, code.c_str()),
	    free_secret);
	if (!token_request)
		return FALSE;

	return client_common_get_access_token(instance, token_request.get(), token);
}

BOOL sdl_webview_get_access_token(freerdp* instance, AccessTokenType tokenType, char** token,
                                  size_t count, ...)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(token);
	switch (tokenType)
	{
		case ACCESS_TOKEN_TYPE_AAD:
		{
			if (count < 2)
			{
				WLog_ERR(TAG,
				         "ACCESS_TOKEN_TYPE_AAD expected 2 additional arguments, but got %" PRIuz
				         ", aborting",
				         count);
				return FALSE;
			}
			else if (count > 2)
				WLog_WARN(TAG,
				          "ACCESS_TOKEN_TYPE_AAD expected 2 additional arguments, but got %" PRIuz
				          ", ignoring",
				          count);
			va_list ap = {};
			va_start(ap, count);
			const char* scope = va_arg(ap, const char*);
			const char* req_cnf = va_arg(ap, const char*);
			const BOOL rc = sdl_webview_get_rdsaad_access_token(instance, scope, req_cnf, token);
			va_end(ap);
			return rc;
		}
		case ACCESS_TOKEN_TYPE_AVD:
			if (count != 0)
				WLog_WARN(TAG,
				          "ACCESS_TOKEN_TYPE_AVD expected 0 additional arguments, but got %" PRIuz
				          ", ignoring",
				          count);
			return sdl_webview_get_avd_access_token(instance, token);
		default:
			WLog_ERR(TAG, "Unexpected value for AccessTokenType [%" PRIu32 "], aborting",
			         tokenType);
			return FALSE;
	}
}
