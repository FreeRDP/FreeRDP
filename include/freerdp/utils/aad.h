/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Level Authentication (NLA)
 *
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#ifndef FREERDP_UTILS_AAD_H
#define FREERDP_UTILS_AAD_H

/** \defgroup AAD AAD related helper utilities
 *  \since version 3.0.0
 */
#include <winpr/wlog.h>

#include <freerdp/api.h>
#include <freerdp/config.h>

#ifdef WITH_AAD

/** Helper to retrieve the AAD access token from JSON input
 *
 *  @param data The JSON to parse
 *  @param length The number of bytes of the JSON data
 *
 *  @since version 3.0.0
 *
 * @return The token string or \b NULL
 */
WINPR_ATTR_MALLOC(free, 1)
FREERDP_API char* freerdp_utils_aad_get_access_token(wLog* log, const char* data, size_t length);

#endif

#endif /* FREERDP_UTILS_AAD_H */
