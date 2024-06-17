/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Primitives colors
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

#ifndef FREERDP_LIB_PRIM_COLORS_H
#define FREERDP_LIB_PRIM_COLORS_H

#include <winpr/wtypes.h>
#include <freerdp/config.h>
#include <freerdp/primitives.h>

void primitives_init_colors_sse2(primitives_t* prims);
void primitives_init_colors_neon(primitives_t* prims);

#endif
