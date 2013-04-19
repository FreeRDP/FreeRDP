/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - Encode
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

#ifndef __RFX_ENCODE_H
#define __RFX_ENCODE_H

#include <freerdp/codec/rfx.h>

void rfx_encode_rgb(RFX_CONTEXT* context, const BYTE* rgb_data, int width, int height, int rowstride,
	const UINT32* y_quants, const UINT32* cb_quants, const UINT32* cr_quants,
	wStream* data_out, int* y_size, int* cb_size, int* cr_size);

#endif

