/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Out-of-process AAD auth helper integration
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#include <cstring>
#include <memory>
#include <sstream>
#include <string>

#include <winpr/library.h>
#include <winpr/path.h>
#include <winpr/string.h>
#include <freerdp/client.h>
#include <freerdp/client/aad_helper.h>
#include <freerdp/log.h>
#include <freerdp/utils/aad.h>

#include "sdl_aad_helper.hpp"

#define TAG CLIENT_TAG("SDL.aadhelper")

/* special /azure:auth-helper: value requesting the auto-probe order below, instead of a literal
 * path - also what's used when the option is omitted entirely. */
#define AAD_AUTH_HELPER_AUTODETECT "autodetect"

/* one out-of-process AAD auth helper per client process, lazily started on first use and
 * reused for every AAD auth request of the current connection (so an AVD gateway auth and a
 * following host auth share the same browser profile/cookies and only prompt once). Stopped from
 * sdl_aad_helper_stop() on disconnect; a following reconnect starts a fresh one. */
static AadAuthHelper* g_aadAuthHelper = nullptr;

namespace
{
	/* auto-pick order for /azure:auth-helper:autodetect (or the option omitted entirely): webview
	 * first (lighter, native OS look), then Qt. */
	constexpr const char* kHelperCandidates[] = {
		"freerdp-webview-aad-helper",
		"freerdp-qt-aad-helper",
	};
} // namespace

/* directory this client binary itself lives in - where an installed (or freshly built) helper
 * binary is expected to sit alongside it. */
static std::string sdl_aad_helper_binary_dir()
{
	char path[4096] = {};
	if (GetModuleFileNameA(nullptr, path, sizeof(path)) == 0)
	{
		WLog_ERR(TAG, "[aad-auth] GetModuleFileNameA failed");
		return "";
	}

	char* sep = strrchr(path, '/');
#ifdef _WIN32
	char* sepWin = strrchr(path, '\\');
	if (!sep || (sepWin && (sepWin > sep)))
		sep = sepWin;
#endif
	if (!sep)
		return "";
	*sep = '\0';
	return path;
}

static std::string sdl_aad_helper_path_for_binary(const std::string& dir, const char* binaryName)
{
	std::string path = dir;
	path += "/";
	path += binaryName;
#ifdef _WIN32
	path += ".exe";
#endif
	return path;
}

/* /azure:auth-helper:autodetect (or the option omitted entirely): probe the well-known binaries
 * in kHelperCandidates order and use whichever is actually present. */
static std::string sdl_aad_helper_auto_locate()
{
	auto dir = sdl_aad_helper_binary_dir();
	if (dir.empty())
		return "";

	for (const auto& binaryName : kHelperCandidates)
	{
		auto path = sdl_aad_helper_path_for_binary(dir, binaryName);
		if (PathFileExistsA(path.c_str()))
			return path;
	}
	return "";
}

static AadAuthHelper* sdl_aad_helper_get(const rdpSettings* settings)
{
	if (g_aadAuthHelper)
		return g_aadAuthHelper;

	std::string path;
	const char* source = nullptr;

	const char* fromSettings =
	    settings ? freerdp_settings_get_string(settings, FreeRDP_AadAuthHelper) : nullptr;
	if (fromSettings && fromSettings[0] && (strcmp(fromSettings, AAD_AUTH_HELPER_AUTODETECT) != 0))
	{
		path = fromSettings;
		source = "/azure:auth-helper:";
	}

	if (path.empty())
	{
		path = sdl_aad_helper_auto_locate();
		source = "auto-detected";
	}

	if (path.empty())
	{
		WLog_ERR(TAG, "[aad-auth] could not determine expected helper binary location");
		return nullptr;
	}

	if (!PathFileExistsA(path.c_str()))
	{
		WLog_ERR(
		    TAG,
		    "[aad-auth] helper binary not found at '%s' (from %s) - was FreeRDP built and "
		    "installed with -DWITH_WEBVIEW_AAD_AUTH_HELPER=ON or -DWITH_QT_AAD_AUTH_HELPER=ON? "
		    "Falling back to manual copy/paste login",
		    path.c_str(), source);
		return nullptr;
	}

	WLog_DBG(TAG, "[aad-auth] using helper path from %s: %s", source, path.c_str());
	g_aadAuthHelper = aad_auth_helper_start(path.c_str());
	if (!g_aadAuthHelper)
		WLog_ERR(TAG, "[aad-auth] failed to start '%s'", path.c_str());
	return g_aadAuthHelper;
}

void sdl_aad_helper_stop(void)
{
	if (g_aadAuthHelper)
	{
		aad_auth_helper_stop(g_aadAuthHelper);
		g_aadAuthHelper = nullptr;
	}
}

static std::string sdl_aad_helper_extract_query_param(const std::string& url,
                                                      const std::string& name)
{
	auto qpos = url.find('?');
	if (qpos == std::string::npos)
		return "";

	std::istringstream stream(url.substr(qpos + 1));
	std::string pair;
	while (std::getline(stream, pair, '&'))
	{
		auto eq = pair.find('=');
		if (eq == std::string::npos)
			continue;
		if (pair.compare(0, eq, name) != 0)
			continue;

		auto value = pair.substr(eq + 1);
		auto decoded = winpr_str_url_decode(value.c_str(), value.length());
		std::string result = decoded ? decoded : "";
		free(decoded);
		return result;
	}
	return "";
}

/** drives the out-of-process helper to show \b url and waits for the OAuth2 redirect. On
 * AAD_AUTH_HELPER_NAVIGATE_ERROR (helper unreachable/unusable), callers fall back to the
 * terminal copy/paste flow rather than hard-failing the connection - but NOT on
 * AAD_AUTH_HELPER_NAVIGATE_CANCELLED (the user closed the popup) or _TIMEOUT (the user didn't
 * complete the flow in time): falling back in either of those cases would silently override an
 * outcome the user already determined, by prompting them to do it all over again via the
 * terminal instead of respecting that the attempt is over. */
static AadAuthHelperNavigateStatus sdl_aad_helper_navigate(const rdpSettings* settings,
                                                           const std::string& title,
                                                           const std::string& url,
                                                           std::string& redirectUrl)
{
	auto redirectUri = sdl_aad_helper_extract_query_param(url, "redirect_uri");
	if (redirectUri.empty())
	{
		WLog_ERR(TAG, "[aad-auth] url %s has no redirect_uri parameter", url.c_str());
		return AAD_AUTH_HELPER_NAVIGATE_ERROR;
	}

	auto helper = sdl_aad_helper_get(settings);
	if (!helper)
		return AAD_AUTH_HELPER_NAVIGATE_ERROR;

	char* out = nullptr;
	const AadAuthHelperNavigateStatus status = aad_auth_helper_navigate(
	    helper, title.c_str(), url.c_str(), redirectUri.c_str(), 180000, &out);
	if (status != AAD_AUTH_HELPER_NAVIGATE_OK)
		return status;

	redirectUrl = out;
	free(out);
	return AAD_AUTH_HELPER_NAVIGATE_OK;
}

static BOOL sdl_aad_helper_get_rdsaad_access_token(freerdp* instance, const char* scope,
                                                   const char* req_cnf, char** token)
{
	WINPR_ASSERT(instance);
	WINPR_ASSERT(scope);
	WINPR_ASSERT(req_cnf);
	WINPR_ASSERT(token);

	auto context = instance->context;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->settings);

	std::shared_ptr<char> request(
	    freerdp_client_get_aad_url(reinterpret_cast<rdpClientContext*>(instance->context),
	                               FREERDP_CLIENT_AAD_AUTH_REQUEST, scope),
	    free);

	std::string redirectUrl;
	const AadAuthHelperNavigateStatus status = sdl_aad_helper_navigate(
	    context->settings, "FreeRDP WebView - AAD access token", request.get(), redirectUrl);
	if (status == AAD_AUTH_HELPER_NAVIGATE_CANCELLED)
	{
		WLog_INFO(TAG, "[aad-auth] user cancelled the authentication");
		return FALSE;
	}
	if (status == AAD_AUTH_HELPER_NAVIGATE_TIMEOUT)
	{
		WLog_ERR(TAG, "[aad-auth] authentication timed out");
		return FALSE;
	}
	if (status != AAD_AUTH_HELPER_NAVIGATE_OK)
		return client_cli_get_access_token(instance, ACCESS_TOKEN_TYPE_AAD, token, 2, scope,
		                                   req_cnf);

	auto code = sdl_aad_helper_extract_query_param(redirectUrl, "code");
	if (code.empty())
		return client_cli_get_access_token(instance, ACCESS_TOKEN_TYPE_AAD, token, 2, scope,
		                                   req_cnf);

	std::shared_ptr<char> token_request(
	    freerdp_client_get_aad_url(reinterpret_cast<rdpClientContext*>(instance->context),
	                               FREERDP_CLIENT_AAD_TOKEN_REQUEST, scope, code.c_str(), req_cnf),
	    free);
	return client_common_get_access_token(instance, token_request.get(), token);
}

static BOOL sdl_aad_helper_get_avd_access_token(freerdp* instance, char** token)
{
	WINPR_ASSERT(token);
	WINPR_ASSERT(instance);
	WINPR_ASSERT(instance->context);

	std::shared_ptr<char> request(
	    freerdp_client_get_aad_url(reinterpret_cast<rdpClientContext*>(instance->context),
	                               FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST),
	    free);

	std::string redirectUrl;
	const AadAuthHelperNavigateStatus status =
	    sdl_aad_helper_navigate(instance->context->settings, "FreeRDP WebView - AVD access token",
	                            request.get(), redirectUrl);
	if (status == AAD_AUTH_HELPER_NAVIGATE_CANCELLED)
	{
		WLog_INFO(TAG, "[aad-auth] user cancelled the authentication");
		return FALSE;
	}
	if (status == AAD_AUTH_HELPER_NAVIGATE_TIMEOUT)
	{
		WLog_ERR(TAG, "[aad-auth] authentication timed out");
		return FALSE;
	}
	if (status != AAD_AUTH_HELPER_NAVIGATE_OK)
		return client_cli_get_access_token(instance, ACCESS_TOKEN_TYPE_AVD, token, 0);

	auto code = sdl_aad_helper_extract_query_param(redirectUrl, "code");
	if (code.empty())
		return client_cli_get_access_token(instance, ACCESS_TOKEN_TYPE_AVD, token, 0);

	std::shared_ptr<char> token_request(
	    freerdp_client_get_aad_url(reinterpret_cast<rdpClientContext*>(instance->context),
	                               FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST, code.c_str()),
	    free);
	return client_common_get_access_token(instance, token_request.get(), token);
}

BOOL sdl_aad_helper_get_access_token(freerdp* instance, AccessTokenType tokenType, char** token,
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
			const BOOL rc = sdl_aad_helper_get_rdsaad_access_token(instance, scope, req_cnf, token);
			va_end(ap);
			return rc;
		}
		case ACCESS_TOKEN_TYPE_AVD:
			if (count != 0)
				WLog_WARN(TAG,
				          "ACCESS_TOKEN_TYPE_AVD expected 0 additional arguments, but got %" PRIuz
				          ", ignoring",
				          count);
			return sdl_aad_helper_get_avd_access_token(instance, token);
		default:
			WLog_ERR(TAG, "Unexpected value for AccessTokenType [%" PRIu32 "], aborting",
			         tokenType);
			return FALSE;
	}
}
