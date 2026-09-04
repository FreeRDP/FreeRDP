/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client-driven out-of-process browser helper (JSON-RPC over dedicated pipes)
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CLIENT_COMMON_AAD_HELPER_H
#define FREERDP_CLIENT_COMMON_AAD_HELPER_H

#include <winpr/wtypes.h>
#include <freerdp/api.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief a running instance of the out-of-process AAD auth helper, spawned via
	 *  aad_auth_helper_start(). Reused across multiple navigate() calls for the lifetime of
	 *  one RDP connection so that browser cookies/session persist between them (e.g. AVD gateway
	 *  redirect followed by target host auth), avoiding a second login prompt. */
	typedef struct AadAuthHelper AadAuthHelper;

	/** @brief outcome of aad_auth_helper_navigate(). */
	typedef enum
	{
		AAD_AUTH_HELPER_NAVIGATE_OK = 0,    /**< reached redirect_uri, *redirect_url is valid */
		AAD_AUTH_HELPER_NAVIGATE_CANCELLED, /**< the user closed the popup - callers must treat
		                                     *   this as an explicit abort, not a reason to fall
		                                     *   back to another auth method */
		AAD_AUTH_HELPER_NAVIGATE_TIMEOUT,   /**< timeout_ms elapsed with no matching navigation */
		AAD_AUTH_HELPER_NAVIGATE_ERROR      /**< any other failure: transport/protocol error, IdP
		                                     *   error, helper shutting down, ... */
	} AadAuthHelperNavigateStatus;

	/** @brief spawn the AAD auth helper process and perform the protocol handshake.
	 *
	 * @param helper_path path to a helper executable (e.g. freerdp-webview-aad-helper or
	 *        freerdp-qt-aad-helper)
	 * @return a handle to the running helper, or NULL if it could not be spawned or the
	 *         handshake failed. Caller must release it with aad_auth_helper_stop().
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API AadAuthHelper* aad_auth_helper_start(const char* helper_path);

	/** @brief drive the helper's browser to \b url and wait for it to navigate to a URI prefixed
	 *  with \b redirect_uri (the OAuth2 authorization-code redirect).
	 *
	 * @param helper a helper started with aad_auth_helper_start()
	 * @param title window title to show
	 * @param url the initial URL to navigate to (typically an AAD /authorize URL)
	 * @param redirect_uri the redirect URI prefix to watch for
	 * @param timeout_ms how long the helper should wait before giving up
	 * @param redirect_url on AAD_AUTH_HELPER_NAVIGATE_OK, receives the full redirect URL the
	 *        browser navigated to (caller must free() it). Left untouched otherwise.
	 * @return AAD_AUTH_HELPER_NAVIGATE_OK if the browser reached the redirect URI, or a specific
	 *         failure reason otherwise (see AadAuthHelperNavigateStatus).
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API AadAuthHelperNavigateStatus
	aad_auth_helper_navigate(AadAuthHelper* helper, const char* title, const char* url,
	                         const char* redirect_uri, UINT32 timeout_ms, char** redirect_url);

	/** @brief ask the helper to shut down cleanly and release all resources held for it.
	 *
	 * @param helper a helper started with aad_auth_helper_start(), may be NULL
	 */
	FREERDP_API void aad_auth_helper_stop(AadAuthHelper* helper);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_COMMON_AAD_HELPER_H */
