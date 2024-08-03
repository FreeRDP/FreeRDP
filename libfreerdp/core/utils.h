/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Terminal Server Gateway (utils)
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#ifndef FREERDP_LIB_CORE_UTILS_H
#define FREERDP_LIB_CORE_UTILS_H

#include <winpr/winpr.h>
#include <freerdp/freerdp.h>

/* HTTP tunnel redir flags. */
#define HTTP_TUNNEL_REDIR_ENABLE_ALL 0x80000000
#define HTTP_TUNNEL_REDIR_DISABLE_ALL 0x40000000
#define HTTP_TUNNEL_REDIR_DISABLE_DRIVE 0x1
#define HTTP_TUNNEL_REDIR_DISABLE_PRINTER 0x2
#define HTTP_TUNNEL_REDIR_DISABLE_PORT 0x4
#define HTTP_TUNNEL_REDIR_DISABLE_CLIPBOARD 0x8
#define HTTP_TUNNEL_REDIR_DISABLE_PNP 0x10

typedef enum
{
	AUTH_SUCCESS,
	AUTH_SKIP,
	AUTH_NO_CREDENTIALS,
	AUTH_CANCELLED,
	AUTH_FAILED
} auth_status;

auth_status utils_authenticate_gateway(freerdp* instance, rdp_auth_reason reason);
auth_status utils_authenticate(freerdp* instance, rdp_auth_reason reason, BOOL override);

HANDLE utils_get_abort_event(rdpRdp* rdp);
BOOL utils_abort_event_is_set(const rdpRdp* rdp);
BOOL utils_reset_abort(rdpRdp* rdp);
BOOL utils_abort_connect(rdpRdp* rdp);
BOOL utils_sync_credentials(rdpSettings* settings, BOOL toGateway);

BOOL utils_str_is_empty(const char* str);
BOOL utils_str_copy(const char* value, char** dst);

const char* utils_is_vsock(const char* hostname);

BOOL utils_apply_gateway_policy(wLog* log, rdpContext* context, UINT32 flags, const char* module);
char* utils_redir_flags_to_string(UINT32 flags, char* buffer, size_t size);

BOOL utils_reload_channels(rdpContext* context);

#endif /* FREERDP_LIB_CORE_UTILS_H */
