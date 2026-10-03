/**
 * WinPR: Windows Portable Runtime
 * JSON configuration read helpers
 *
 * Copyright 2026 Armin Novak <anovak@thincast.com>
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

#include <winpr/json.h>

/** @brief helper to read and apply a boolean from a configuration JSON
 *
 *  @param config The name of the configuration file
 *  @param json   The parsed configuration data
 *  @param name   The name of the BOOLEAN option in the JSON object
 *  @param dst    A pointer to a BOOL that will hold the result. Must not be NULL
 *
 *  @return TRUE if the configuration was found and applied, FALSE otherwise
 *  @since version 3.33.0
 */
WINPR_API BOOL winpr_config_apply_bool(const char* config, WINPR_JSON* json, const char* name,
                                       BOOL* dst);

/** @brief helper to read and apply a UINT32 from a configuration JSON
 *
 *  @param config The name of the configuration file
 *  @param json   The parsed configuration data
 *  @param name   The name of the UINT32 option in the JSON object
 *  @param dst    A pointer to a UINT32 that will hold the result. Must not be NULL
 *
 *  @return TRUE if the configuration was found and applied, FALSE otherwise
 *  @since version 3.33.0
 */
WINPR_API BOOL winpr_config_apply_uint32(const char* config, WINPR_JSON* json, const char* name,
                                         UINT32* dst);

/** @brief helper to read and apply a string from a configuration JSON
 *
 *  @param config The name of the configuration file
 *  @param json   The parsed configuration data
 *  @param name   The name of the string option in the JSON object
 *  @param dst    A pointer to a string that will hold the result. Must not be NULL
 *
 *  @return TRUE if the configuration was found and applied, FALSE otherwise
 *  @since version 3.33.0
 */
WINPR_API BOOL winpr_config_apply_string(const char* config, WINPR_JSON* json, const char* name,
                                         char** dst);
