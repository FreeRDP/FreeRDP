/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Internal settings header for functions not exported
 *
 * Copyright 2018 Armin Novak <armin.novak@thincast.com>
 * Copyright 2018 Thincast Technologies GmbH
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

#ifndef FREERDP_LIB_CORE_SETTINGS_H
#define FREERDP_LIB_CORE_SETTINGS_H

#include <winpr/string.h>
#include <winpr/sspi.h>

#include <freerdp/config.h>

#define FREERDP_SETTINGS_INTERNAL_USE
#include <freerdp/settings_types_private.h>

#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/api.h>

#include <string.h>

FREERDP_LOCAL BOOL freerdp_settings_enforce_monitor_exists(rdpSettings* settings);
FREERDP_LOCAL void freerdp_settings_print_warnings(const rdpSettings* settings);
FREERDP_LOCAL BOOL freerdp_settings_check_client_after_preconnect(const rdpSettings* settings);
FREERDP_LOCAL BOOL freerdp_settings_set_default_order_support(rdpSettings* settings);
FREERDP_LOCAL BOOL freerdp_settings_clone_keys(rdpSettings* dst, const rdpSettings* src);
FREERDP_LOCAL void freerdp_settings_free_keys(rdpSettings* dst, BOOL cleanup);
FREERDP_LOCAL BOOL freerdp_settings_set_string_(rdpSettings* settings,
                                                FreeRDP_Settings_Keys_String id, const char* val,
                                                size_t len);
FREERDP_LOCAL BOOL freerdp_settings_set_string_copy_(rdpSettings* settings,
                                                     FreeRDP_Settings_Keys_String id,
                                                     const char* val, size_t len, BOOL cleanup);
FREERDP_LOCAL BOOL freerdp_capability_buffer_allocate(rdpSettings* settings, UINT32 count);

FREERDP_LOCAL BOOL identity_set_from_settings_with_pwd(SEC_WINNT_AUTH_IDENTITY_W* identity,
                                                       const rdpSettings* settings,
                                                       FreeRDP_Settings_Keys_String UserId,
                                                       FreeRDP_Settings_Keys_String DomainId,
                                                       const WCHAR* Password, size_t pwdLen);
FREERDP_LOCAL BOOL identity_set_from_settings(SEC_WINNT_AUTH_IDENTITY_W* identity,
                                              const rdpSettings* settings,
                                              FreeRDP_Settings_Keys_String UserId,
                                              FreeRDP_Settings_Keys_String DomainId,
                                              FreeRDP_Settings_Keys_String PwdId);
FREERDP_LOCAL BOOL identity_set_from_smartcard_hash(SEC_WINNT_AUTH_IDENTITY_W* identity,
                                                    const rdpSettings* settings,
                                                    FreeRDP_Settings_Keys_String userId,
                                                    FreeRDP_Settings_Keys_String domainId,
                                                    FreeRDP_Settings_Keys_String pwdId,
                                                    const BYTE* certSha1, size_t sha1len);
FREERDP_LOCAL const char* freerdp_settings_glyph_level_string(UINT32 level, char* buffer,
                                                              size_t size);

FREERDP_LOCAL BOOL freerdp_settings_set_pointer_len_(rdpSettings* settings,
                                                     FreeRDP_Settings_Keys_Pointer id,
                                                     FreeRDP_Settings_Keys_UInt32 lenId,
                                                     const void* data, size_t len, size_t size);
FREERDP_LOCAL BOOL freerdp_target_net_adresses_reset(rdpSettings* settings, size_t size);

#endif /* FREERDP_LIB_CORE_SETTINGS_H */
