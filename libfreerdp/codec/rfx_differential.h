/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library - Differential Encoding
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

#ifndef FREERDP_LIB_CODEC_RFX_DIFFERENTIAL_H
#define FREERDP_LIB_CODEC_RFX_DIFFERENTIAL_H

#include <freerdp/codec/rfx.h>
#include <freerdp/api.h>

static INLINE void rfx_differential_decode(INT16* buffer, int size)
{
	INT16* ptr = buffer;
	INT16* end = &buffer[size - 1];

	while (ptr != end)
	{
		ptr[1] += ptr[0];
		ptr++;
	}
}

static INLINE void rfx_differential_encode(INT16* buffer, int size)
{
	INT16 n1, n2;
	INT16* dst;

	for (n1 = *buffer, dst = buffer + 1; size > 1; dst++, size--)
	{
		n2 = *dst;
		*dst -= n1;
		n1 = n2;
	}
}

#endif /* FREERDP_LIB_CODEC_RFX_DIFFERENTIAL_H */
