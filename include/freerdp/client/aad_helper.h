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
#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief a running instance of the out-of-process AAD auth helper, spawned via
	 *  aad_auth_helper_start(). Reused across multiple navigate() calls for the lifetime of
	 *  one RDP connection so that browser cookies/session persist between them (e.g. AVD gateway
	 *  redirect followed by target host auth), avoiding a second login prompt.
	 *  @since version 3.32.0
	 */
	typedef struct AadAuthHelper AadAuthHelper;

	/** @brief outcome of aad_auth_helper_navigate(). */
	typedef enum
	{
		AAD_AUTH_HELPER_NAVIGATE_OK = 1,    /**< reached redirect_uri, *redirect_url is valid */
		AAD_AUTH_HELPER_NAVIGATE_CANCELLED, /**< the user closed the popup - callers must treat
		                                     *   this as an explicit abort, not a reason to fall
		                                     *   back to another auth method */
		AAD_AUTH_HELPER_NAVIGATE_TIMEOUT,   /**< timeout_ms elapsed with no matching navigation */
		AAD_AUTH_HELPER_NAVIGATE_ERROR      /**< any other failure: transport/protocol error, IdP
		                                     *   error, helper shutting down, ... */
	} AadAuthHelperNavigateStatus;

	/** @brief ask the helper to shut down cleanly and release all resources held for it.
	 *
	 * @param helper a helper started with aad_auth_helper_start(), may be NULL
	 * @since version 3.32.0
	 */
	FREERDP_API void aad_auth_helper_stop(AadAuthHelper* helper);

	/** @brief spawn the AAD auth helper process and perform the protocol handshake.
	 *
	 * @return a handle to the running helper, or NULL if it could not be spawned or the
	 *         handshake failed. Caller must release it with aad_auth_helper_stop().
	 * @since version 3.32.0
	 */
	WINPR_ATTR_MALLOC(aad_auth_helper_stop, 1)
	FREERDP_API AadAuthHelper* aad_auth_helper_start(rdpClientContext* context);

	/** @brief va_list-taking implementation, so a caller that's already inside its own variadic
	 *  function (see the SDL2/SDL3 GetAccessToken trampolines) can forward its va_list here
	 * directly
	 *  - the standard vprintf-style pattern - instead of needing to re-expose the raw "..." across
	 * a second function boundary, which C/C++ doesn't allow.
	 *
	 *  @param helper the caller's own per-connection storage slot (e.g. a member of its
	 * SdlContext); lazily filled in on first use and reused after that. This function never stores
	 * anything itself.
	 *
	 *  @param tokenType The token type to request
	 *  @param token A pointer to a result string, must not be NULL
	 *  @param count The number of arguments following
	 *  @param args a \ref va_list containing \ref count arguments
	 *
	 *  @return TRUE for successfully acquiring a token, FALSE otherwise
	 *
	 * @since version 3.32.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL aad_auth_helper_get_access_token_v(AadAuthHelper* helper,
	                                                    AccessTokenType tokenType, char** token,
	                                                    size_t count, va_list args);

	/** @brief convenience variadic wrapper around sdl_aad_helper_get_access_token_v(), for callers
	 *  that aren't themselves forwarding an existing va_list.
	 *
	 *  @param helper A pointer to the helper object
	 *  @param tokenType The token type to request
	 *  @param token A pointer to a result string, must not be NULL
	 *  @param count The number of arguments following
	 *  @return TRUE for successfully acquiring a token, FALSE otherwise
	 *  @since version 3.32.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL aad_auth_helper_get_access_token(AadAuthHelper* helper,
	                                                  AccessTokenType tokenType, char** token,
	                                                  size_t count, ...);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_COMMON_AAD_HELPER_H */
