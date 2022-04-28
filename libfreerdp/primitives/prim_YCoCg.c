/* FreeRDP: A Remote Desktop Protocol Client
 * YCoCg<->RGB Color conversion operations.
 * vi:ts=4 sw=4:
 *
 * (c) Copyright 2014 Hewlett-Packard Development Company, L.P.
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

#include <freerdp/config.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

/* helper function to convert raw 8 bit values to signed 16bit values.
 */
static INT16 convert(UINT8 raw, int shift)
{
	const int cll = shift - 1; /* -1 builds in the /2's */
	return (INT16)((INT8)(raw << cll));
}

/* ------------------------------------------------------------------------- */
static pstatus_t general_YCoCgToRGB_8u_AC4R(const BYTE* pSrc, INT32 srcStep, BYTE* pDst,
                                            UINT32 DstFormat, INT32 dstStep, UINT32 width,
                                            UINT32 height, UINT8 shift, BOOL withAlpha)
{
	UINT32 x, y;
	const DWORD formatSize = FreeRDPGetBytesPerPixel(DstFormat);
	fkt_writePixel writePixel = getPixelWriteFunction(DstFormat, TRUE);

	for (y = 0; y < height; y++)
	{
		const BYTE* sptr = &pSrc[srcStep * y];
		BYTE* dptr = &pDst[dstStep * y];
		for (x = 0; x < width; x++)
		{
			/* Note: shifts must be done before sign-conversion. */
			const INT16 Cg = convert(*sptr++, shift);
			const INT16 Co = convert(*sptr++, shift);
			const INT16 Y = *sptr++; /* UINT8->INT16 */
			const INT16 T = Y - Cg;
			const INT16 B = T + Co;
			const INT16 G = Y + Cg;
			const INT16 R = T - Co;
			BYTE A = *sptr++;

			if (!withAlpha)
				A = 0xFFU;

			dptr = writePixel(dptr, formatSize, DstFormat, CLIP(R), CLIP(G), CLIP(B), A);
		}
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_YCoCg(primitives_t* prims)
{
	prims->YCoCgToRGB_8u_AC4R = general_YCoCgToRGB_8u_AC4R;
}
