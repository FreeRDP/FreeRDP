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

#include <freerdp/config.h>

#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/api.h>

#include <string.h>

FREERDP_LOCAL BOOL freerdp_settings_set_default_order_support(rdpSettings* settings);
FREERDP_LOCAL BOOL freerdp_settings_clone_keys(rdpSettings* dst, const rdpSettings* src);
FREERDP_LOCAL void freerdp_settings_free_keys(rdpSettings* dst, BOOL cleanup);
FREERDP_LOCAL BOOL freerdp_settings_set_string_(rdpSettings* settings, size_t id, const char* val,
                                                size_t len, BOOL copy, BOOL cleanup);
FREERDP_LOCAL BOOL freerdp_capability_buffer_allocate(rdpSettings* settings, UINT32 count);

#endif /* FREERDP_LIB_CORE_SETTINGS_H */
