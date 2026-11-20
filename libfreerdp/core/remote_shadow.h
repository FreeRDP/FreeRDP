/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote session shadowing ([MS-TSTS] RpcShadow2)
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

#ifndef FREERDP_LIB_CORE_REMOTE_SHADOW_H
#define FREERDP_LIB_CORE_REMOTE_SHADOW_H

#include <winpr/wtypes.h>
#include <winpr/stream.h>
#include <winpr/wlog.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

/** SHADOW_REQUEST_RESPONSE values returned by RpcShadow2 ([MS-TSTS] 2.2.2.16.3) */
typedef enum
{
	REMOTE_SHADOW_RESPONSE_ALLOW = 0,
	REMOTE_SHADOW_RESPONSE_DECLINE = 1,
	REMOTE_SHADOW_RESPONSE_POLICY_PERMISSION_REQUIRED = 2,
	REMOTE_SHADOW_RESPONSE_POLICY_DISABLED = 3,
	REMOTE_SHADOW_RESPONSE_POLICY_VIEW_ONLY = 4,
	REMOTE_SHADOW_RESPONSE_POLICY_VIEW_ONLY_PERMISSION_REQUIRED = 5,
	REMOTE_SHADOW_RESPONSE_SESSION_ALREADY_CONTROLLED = 6
} REMOTE_SHADOW_RESPONSE;

/**
 * If FreeRDP_RemoteShadowMode is set, asks the server named by FreeRDP_ServerHostname for a
 * shadow invitation of session FreeRDP_RemoteShadowSessionId and points the connection at the
 * listener it names (remote assistance mode, NLA with the same credentials, the listener
 * certificate pinned). Called before every connect, as each invitation is single use.
 *
 * @return TRUE on success or if shadowing is not requested.
 */
WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL remote_shadow_prepare_connect(rdpContext* context);

#if defined(WITH_LIBSMB2)
/**
 * Calls RpcShadow2 on FreeRDP_ServerHostname over SMB (\\pipe\\SessEnvPublicRpc) with the
 * connection's credentials, NTLM at RPC_C_AUTHN_LEVEL_PKT_PRIVACY.
 *
 * @param invitation on success the MS-RAI connection string 2, to be freed by the caller.
 * @return TRUE if the call completed; check permission and hresult for the server's answer.
 */
WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL remote_shadow_rpc_shadow2(
    rdpContext* context, UINT32 timeout, UINT16* permission, UINT32* hresult, char** invitation);

/**
 * Points settings at the listener an RpcShadow2 invitation names: its target net addresses
 * without IPv6 link-local ones, remote assistance mode, NLA, and the listener certificate
 * pinned. FreeRDP_ServerHostname is kept for the next RpcShadow2 call.
 */
WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL remote_shadow_apply_invitation(rdpSettings* settings,
                                                                       const char* invitation);
#endif

/** Writes the NDR stub of the RpcShadow2 in parameters. */
WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL remote_shadow_write_shadow2_stub(wStream* s,
                                                                         UINT32 sessionId,
                                                                         BOOL control,
                                                                         BOOL noConsentPrompt);

/** Reads the NDR stub of the RpcShadow2 out parameters. */
WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL remote_shadow_read_shadow2_stub(wLog* log, wStream* s,
                                                                        UINT16* permission,
                                                                        UINT32* hresult,
                                                                        char** invitation);

#endif /* FREERDP_LIB_CORE_REMOTE_SHADOW_H */
