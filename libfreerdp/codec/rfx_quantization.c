/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/primitives.h>
#include "rfx_quantization.h"

static void rfx_quantization_decode_block(const primitives_t *prims, INT16* buffer, int buffer_size, UINT32 factor)
{
	if (factor == 0)
		return;

	prims->lShiftC_16s(buffer, factor, buffer, buffer_size);
}

void rfx_quantization_decode(INT16* buffer, const UINT32* quantization_values)
{
	const primitives_t *prims = primitives_get();

	/* Scale the values so that they are represented as 11.5 fixed-point number */
	rfx_quantization_decode_block(prims, buffer, 4096, 5);

	rfx_quantization_decode_block(prims, buffer, 1024, quantization_values[8] - 6); /* HL1 */
	rfx_quantization_decode_block(prims, buffer + 1024, 1024, quantization_values[7] - 6); /* LH1 */
	rfx_quantization_decode_block(prims, buffer + 2048, 1024, quantization_values[9] - 6); /* HH1 */
	rfx_quantization_decode_block(prims, buffer + 3072, 256, quantization_values[5] - 6); /* HL2 */
	rfx_quantization_decode_block(prims, buffer + 3328, 256, quantization_values[4] - 6); /* LH2 */
	rfx_quantization_decode_block(prims, buffer + 3584, 256, quantization_values[6] - 6); /* HH2 */
	rfx_quantization_decode_block(prims, buffer + 3840, 64, quantization_values[2] - 6); /* HL3 */
	rfx_quantization_decode_block(prims, buffer + 3904, 64, quantization_values[1] - 6); /* LH3 */
	rfx_quantization_decode_block(prims, buffer + 3968, 64, quantization_values[3] - 6); /* HH3 */
	rfx_quantization_decode_block(prims, buffer + 4032, 64, quantization_values[0] - 6); /* LL3 */
}

static void rfx_quantization_encode_block(INT16* buffer, int buffer_size, UINT32 factor)
{
	INT16* dst;
	INT16 half;

	if (factor == 0)
		return;

	half = (1 << (factor - 1));
	/* Could probably use prims->rShiftC_16s(dst+half, factor, dst, buffer_size); */
	for (dst = buffer; buffer_size > 0; dst++, buffer_size--)
	{
		*dst = (*dst + half) >> factor;
	}
}

void rfx_quantization_encode(INT16* buffer, const UINT32* quantization_values)
{
	rfx_quantization_encode_block(buffer, 1024, quantization_values[8] - 6); /* HL1 */
	rfx_quantization_encode_block(buffer + 1024, 1024, quantization_values[7] - 6); /* LH1 */
	rfx_quantization_encode_block(buffer + 2048, 1024, quantization_values[9] - 6); /* HH1 */
	rfx_quantization_encode_block(buffer + 3072, 256, quantization_values[5] - 6); /* HL2 */
	rfx_quantization_encode_block(buffer + 3328, 256, quantization_values[4] - 6); /* LH2 */
	rfx_quantization_encode_block(buffer + 3584, 256, quantization_values[6] - 6); /* HH2 */
	rfx_quantization_encode_block(buffer + 3840, 64, quantization_values[2] - 6); /* HL3 */
	rfx_quantization_encode_block(buffer + 3904, 64, quantization_values[1] - 6); /* LH3 */
	rfx_quantization_encode_block(buffer + 3968, 64, quantization_values[3] - 6); /* HH3 */
	rfx_quantization_encode_block(buffer + 4032, 64, quantization_values[0] - 6); /* LL3 */

	/* The coefficients are scaled by << 5 at RGB->YCbCr phase, so we round it back here */
	rfx_quantization_encode_block(buffer, 4096, 5);
}
