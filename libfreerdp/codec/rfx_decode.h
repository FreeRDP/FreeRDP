/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - Decode
 *
 * Copyright 2011 Vic Lee
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

#ifndef FREERDP_LIB_CODEC_RFX_DECODE_H
#define FREERDP_LIB_CODEC_RFX_DECODE_H

#include <winpr/wtypes.h>

#include <freerdp/codec/rfx.h>
#include <freerdp/api.h>

/* stride is bytes between rows in the output buffer. */
FREERDP_LOCAL BOOL rfx_decode_rgb(RFX_CONTEXT* WINPR_RESTRICT context,
                                  const RFX_TILE* WINPR_RESTRICT tile,
                                  BYTE* WINPR_RESTRICT rgb_buffer, UINT32 stride);
FREERDP_LOCAL void rfx_decode_component(RFX_CONTEXT* WINPR_RESTRICT context,
                                        const UINT32* WINPR_RESTRICT quantization_values,
                                        const BYTE* WINPR_RESTRICT data, size_t size,
                                        INT16* WINPR_RESTRICT buffer);
#endif /* FREERDP_LIB_CODEC_RFX_DECODE_H */
