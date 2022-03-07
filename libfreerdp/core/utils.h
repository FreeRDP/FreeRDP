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

BOOL utils_reset_abort(rdpContext* context);
BOOL utils_abort_connect(rdpContext* context);

#endif /* FREERDP_LIB_CORE_UTILS_H */
