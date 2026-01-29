/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Winpr internal helper functions
 *
 * Copyright 2026 Armin Novak <armin.novak@gmail.com>
 * Copyright 2026 Thincast Technologies GmbH
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

WINPR_ATTR_MALLOC(free, 1)
WINPR_LOCAL WINPR_ATTR_NODISCARD char* winpr_getApplicatonDetailsRegKey(const char* fmt);

WINPR_ATTR_MALLOC(free, 1)
WINPR_LOCAL WINPR_ATTR_NODISCARD char* winpr_getApplicatonDetailsCombined(char separator);

WINPR_LOCAL WINPR_ATTR_NODISCARD BOOL winpr_areApplicationDetailsCustomized(void);
