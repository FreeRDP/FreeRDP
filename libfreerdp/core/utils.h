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

WINPR_ATTR_NODISCARD FREERDP_LOCAL auth_status utils_authenticate_gateway(freerdp* instance,
                                                                          rdp_auth_reason reason);

WINPR_ATTR_NODISCARD FREERDP_LOCAL auth_status utils_authenticate(freerdp* instance,
                                                                  rdp_auth_reason reason,
                                                                  BOOL override);

WINPR_ATTR_NODISCARD FREERDP_LOCAL HANDLE utils_get_abort_event(rdpRdp* rdp);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL utils_abort_event_is_set(const rdpRdp* rdp);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL utils_reset_abort(rdpRdp* rdp);

FREERDP_LOCAL BOOL utils_abort_connect(rdpRdp* rdp);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL utils_sync_credentials(rdpSettings* settings,
                                                               BOOL toGateway);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL utils_persist_credentials(rdpSettings* settings,
                                                                  const rdpSettings* current);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL utils_str_is_empty(const char* str);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL utils_str_copy(const char* value, char** dst);

WINPR_ATTR_NODISCARD FREERDP_LOCAL const char* utils_is_vsock(const char* hostname);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL utils_apply_gateway_policy(wLog* log, rdpContext* context,
                                                                   UINT32 flags,
                                                                   const char* module);

WINPR_ATTR_NODISCARD FREERDP_LOCAL char* utils_redir_flags_to_string(UINT32 flags, char* buffer,
                                                                     size_t size);

WINPR_ATTR_NODISCARD FREERDP_LOCAL BOOL utils_reload_channels(rdpContext* context);

/** @brief generate a registry key string of format 'someting\\%s\\foo'
 *
 *  @param fmt A format string that must contain a single '%s' being replaced by the
 * 'vendor\\product` values.
 *
 *  @return A registry key to use or \b nullptr if failed.
 *  @version since 3.23.0
 */
WINPR_ATTR_MALLOC(free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL
char* freerdp_getApplicatonDetailsRegKey(WINPR_FORMAT_ARG const char* fmt);

/** @brief generate a 'vendor/product' string with desired separator
 *
 *  @param separator the separator character to use
 *
 *  @return A 'vendor/product' string to use or \b nullptr if failed.
 *  @version since 3.23.0
 */
WINPR_ATTR_MALLOC(free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL
char* freerdp_getApplicatonDetailsCombined(char separator);

/** @brief returns if we are using default compile time 'vendor' and 'product' settings or an
 * application provided one.
 *
 *  @return \b TRUE if \b freerdp_setApplicationDetails was called, \b FALSE otherwise.
 *  @version since 3.23.0
 */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL
BOOL freerdp_areApplicationDetailsCustomized(void);

#endif /* FREERDP_LIB_CORE_UTILS_H */
