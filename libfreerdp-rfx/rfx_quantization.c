/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - Quantization
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

#include "rfx_quantization.h"

static void rfx_quantization_decode_block(sint16* buffer, int buffer_size, uint32 factor)
{
	sint16* dst;

	if (factor <= 6)
		return;

	factor -= 6;
	for (dst = buffer; buffer_size > 0; dst++, buffer_size--)
	{
		*dst <<= factor;
	}
}

void rfx_quantization_decode(sint16* buffer, const uint32* quantization_values)
{
	rfx_quantization_decode_block(buffer, 1024, quantization_values[8]); /* HL1 */
	rfx_quantization_decode_block(buffer + 1024, 1024, quantization_values[7]); /* LH1 */
	rfx_quantization_decode_block(buffer + 2048, 1024, quantization_values[9]); /* HH1 */
	rfx_quantization_decode_block(buffer + 3072, 256, quantization_values[5]); /* HL2 */
	rfx_quantization_decode_block(buffer + 3328, 256, quantization_values[4]); /* LH2 */
	rfx_quantization_decode_block(buffer + 3584, 256, quantization_values[6]); /* HH2 */
	rfx_quantization_decode_block(buffer + 3840, 64, quantization_values[2]); /* HL3 */
	rfx_quantization_decode_block(buffer + 3904, 64, quantization_values[1]); /* LH3 */
	rfx_quantization_decode_block(buffer + 3868, 64, quantization_values[3]); /* HH3 */
	rfx_quantization_decode_block(buffer + 4032, 64, quantization_values[0]); /* LL3 */
}

static void rfx_quantization_encode_block(sint16* buffer, int buffer_size, uint32 factor)
{
	sint16* dst;

	if (factor <= 6)
		return;

	factor -= 6;
	for (dst = buffer; buffer_size > 0; dst++, buffer_size--)
	{
		*dst >>= factor;
	}
}

void rfx_quantization_encode(sint16* buffer, const uint32* quantization_values)
{
	rfx_quantization_encode_block(buffer, 1024, quantization_values[8]); /* HL1 */
	rfx_quantization_encode_block(buffer + 1024, 1024, quantization_values[7]); /* LH1 */
	rfx_quantization_encode_block(buffer + 2048, 1024, quantization_values[9]); /* HH1 */
	rfx_quantization_encode_block(buffer + 3072, 256, quantization_values[5]); /* HL2 */
	rfx_quantization_encode_block(buffer + 3328, 256, quantization_values[4]); /* LH2 */
	rfx_quantization_encode_block(buffer + 3584, 256, quantization_values[6]); /* HH2 */
	rfx_quantization_encode_block(buffer + 3840, 64, quantization_values[2]); /* HL3 */
	rfx_quantization_encode_block(buffer + 3904, 64, quantization_values[1]); /* LH3 */
	rfx_quantization_encode_block(buffer + 3868, 64, quantization_values[3]); /* HH3 */
	rfx_quantization_encode_block(buffer + 4032, 64, quantization_values[0]); /* LL3 */
}
