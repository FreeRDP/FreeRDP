/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Settings AAD cloud parser
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

#pragma once

#include <freerdp/api.h>
#include <freerdp/settings.h>

typedef struct freerdp_aad_cloud FreeRDP_AadCloud;

/** Look up an AVD cloud by its short name.
 *
 * @param name Cloud name (currently `commercial` or `usgov`).
 * @return The immutable cloud description, or \b nullptr if unknown.
 * @since version 3.34.0
 */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL const FreeRDP_AadCloud* freerdp_utils_aad_cloud_by_name(const char* name);

/** Select an AVD cloud from a gateway hostname.
 *
 * @param hostname Gateway hostname to inspect.
 * @return The matching immutable cloud description, or \b nullptr if unknown.
 * @since version 3.34.0
 */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL const FreeRDP_AadCloud* freerdp_utils_aad_cloud_for_gateway(const char* hostname);

/** Apply an AVD cloud's authority, scope and redirect settings.
 *
 * @param settings Settings instance to update.
 * @param cloud Cloud description returned by a lookup helper.
 * @return \b TRUE on success, \b FALSE on invalid input or allocation failure.
 * @since version 3.34.0
 */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL freerdp_utils_aad_apply_cloud(rdpSettings* settings,
                                                 const FreeRDP_AadCloud* cloud);
