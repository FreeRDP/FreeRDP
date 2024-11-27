/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SIMD support detection header
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

#include <freerdp/config.h>

/* https://sourceforge.net/p/predef/wiki/Architectures/
 *
 * contains a list of defined symbols for each compiler
 */
#if defined(WITH_SIMD)
#if defined(_M_IX86) || defined(_M_AMD64) || defined(_M_IA64) || defined(_M_IX86_AMD64) || \
    defined(__amd64__) || defined(__amd64) || defined(__x86_64__) || defined(__x86_64) ||  \
    defined(__i686__) || defined(__ia64__)
#define SSE_AVX_INTRINSICS_ENABLED
#endif

#if defined(_M_ARM64) || defined(_M_ARM) || defined(__arm) || defined(__aarch64__)
#define NEON_INTRINSICS_ENABLED
#endif
#endif
