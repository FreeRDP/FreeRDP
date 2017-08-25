/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - SSE2 Optimizations
 *
 * Copyright 2011 Stephen Erisman
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

#ifndef FREERDP_LIB_CODEC_RFX_SSE2_H
#define FREERDP_LIB_CODEC_RFX_SSE2_H

#include <freerdp/codec/rfx.h>
#include <freerdp/api.h>

FREERDP_LOCAL void rfx_init_sse2(RFX_CONTEXT* context);

#ifdef WITH_SSE2
#ifndef RFX_INIT_SIMD
#define RFX_INIT_SIMD(_rfx_context) rfx_init_sse2(_rfx_context)
#endif
#endif

#endif /* FREERDP_LIB_CODEC_RFX_SSE2_H */
