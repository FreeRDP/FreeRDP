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

#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/api.h>

#include <string.h>

FREERDP_LOCAL BOOL freerdp_settings_set_default_order_support(rdpSettings* settings);
FREERDP_LOCAL BOOL freerdp_settings_clone_keys(rdpSettings* dst, const rdpSettings* src);
FREERDP_LOCAL void freerdp_settings_free_keys(rdpSettings* dst, BOOL cleanup);
FREERDP_LOCAL BOOL freerdp_settings_set_string_(rdpSettings* settings, size_t id, char* val,
                                                size_t len);
FREERDP_LOCAL BOOL freerdp_settings_set_string_copy_(rdpSettings* settings, size_t id,
                                                     const char* val, size_t len, BOOL cleanup);
FREERDP_LOCAL BOOL freerdp_capability_buffer_allocate(rdpSettings* settings, UINT32 count);

FREERDP_LOCAL BOOL identity_set_from_settings_with_pwd(SEC_WINNT_AUTH_IDENTITY_W* identity,
                                                       const rdpSettings* settings, size_t UserId,
                                                       size_t DomainId, const WCHAR* Password,
                                                       size_t pwdLen);
FREERDP_LOCAL BOOL identity_set_from_settings(SEC_WINNT_AUTH_IDENTITY_W* identity,
                                              const rdpSettings* settings, size_t UserId,
                                              size_t DomainId, size_t PwdId);
FREERDP_LOCAL BOOL identity_set_from_smartcard_hash(SEC_WINNT_AUTH_IDENTITY_W* identity,
                                                    const rdpSettings* settings, size_t userId,
                                                    size_t domainId, size_t pwdId,
                                                    const BYTE* certSha1, size_t sha1len);

#endif /* FREERDP_LIB_CORE_SETTINGS_H */
