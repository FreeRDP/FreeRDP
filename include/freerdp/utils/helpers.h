/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * common helper utilities
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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

#include <winpr/wtypes.h>
#include <freerdp/api.h>

/** @brief Return the absolute path of a configuration file (the path of the configuration
 * directory if \b filename is \b NULL)
 *
 *  @param system a boolean indicating the configuration base, \b TRUE for system configuration,
 * \b FALSE for user configuration
 *  @param filename an optional configuration file name to append.
 *
 *  @return The absolute path of the desired configuration or \b NULL in case of failure. Use \b
 * free to clean up the allocated string.
 *
 *
 *  @since version 3.9.0
 */
FREERDP_API char* freerdp_GetConfigFilePath(BOOL system, const char* filename);
