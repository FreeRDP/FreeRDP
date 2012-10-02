/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#ifndef __RFX_DECODE_H
#define __RFX_DECODE_H

#include <freerdp/codec/rfx.h>

void rfx_decode_ycbcr_to_rgb(sint16* y_r_buf, sint16* cb_g_buf, sint16* cr_b_buf);

void rfx_decode_rgb(RFX_CONTEXT* context, STREAM* data_in,
	int y_size, const uint32 * y_quants,
	int cb_size, const uint32 * cb_quants,
	int cr_size, const uint32 * cr_quants, uint8* rgb_buffer);

#endif /* __RFX_DECODE_H */

