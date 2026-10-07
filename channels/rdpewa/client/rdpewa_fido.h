/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * WebAuthn Virtual Channel Extension [MS-RDPEWA]
 * libfido2 authenticator integration
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

#ifndef FREERDP_CHANNEL_RDPEWA_CLIENT_FIDO_H
#define FREERDP_CHANNEL_RDPEWA_CLIENT_FIDO_H

#include <winpr/wtypes.h>
#include <winpr/platform.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>

#include "rdpewa_cbor.h"

/** @brief Handle a CTAPCBOR_RPC_COMMAND_WEB_AUTHN request (MakeCredential / GetAssertion).
 *
 *  Talks to local authenticators via libfido2. May block waiting for user interaction.
 *
 *  @param context   RDP context for user interaction (PIN prompt)
 *  @param request   decoded request message
 *  @return a new wStream on success (caller frees with Stream_Free(s, TRUE)), nullptr on failure
 */
WINPR_ATTR_MALLOC(Stream_Free, 1)
wStream* rdpewa_fido_webauthn(rdpContext* context, const RDPEWA_REQUEST* request);

/** @brief Handle CTAPCBOR_RPC_COMMAND_IUVPAA -- check for platform authenticator.
 *
 *  @return a new wStream on success (caller frees with Stream_Free(s, TRUE)), nullptr on failure
 */
WINPR_ATTR_MALLOC(Stream_Free, 1)
wStream* rdpewa_fido_is_uvpaa(void);

/** @brief Handle CTAPCBOR_RPC_COMMAND_API_VERSION.
 *
 *  @return a new wStream on success (caller frees with Stream_Free(s, TRUE)), nullptr on failure
 */
WINPR_ATTR_MALLOC(Stream_Free, 1)
wStream* rdpewa_fido_api_version(void);

/** @brief Handle CTAPCBOR_RPC_COMMAND_GET_AUTHENTICATOR_LIST.
 *
 *  @return a new wStream on success (caller frees with Stream_Free(s, TRUE)), nullptr on failure
 */
WINPR_ATTR_MALLOC(Stream_Free, 1)
wStream* rdpewa_fido_get_authenticator_list(void);

/** @brief Handle CTAPCBOR_RPC_COMMAND_GET_CREDENTIALS.
 *
 *  @param context   RDP context for user interaction (PIN prompt)
 *  @param rpId      relying party ID to query credentials for
 *  @return a new wStream on success (caller frees with Stream_Free(s, TRUE)), nullptr on failure
 */
WINPR_ATTR_MALLOC(Stream_Free, 1)
wStream* rdpewa_fido_get_credentials(rdpContext* context, const char* rpId);

#endif /* FREERDP_CHANNEL_RDPEWA_CLIENT_FIDO_H */
