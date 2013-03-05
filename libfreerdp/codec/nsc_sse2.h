/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NSCodec Library - SSE2 Optimizations
 *
 * Copyright 2012 Vic Lee
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

#ifndef __NSC_SSE2_H
#define __NSC_SSE2_H

#include <freerdp/codec/nsc.h>

void nsc_init_sse2(NSC_CONTEXT* context);

#ifdef WITH_SSE2
 #ifndef NSC_INIT_SIMD
  #define NSC_INIT_SIMD(_context) nsc_init_sse2(_context)
 #endif
#endif

#endif /* __NSC_SSE2_H */
