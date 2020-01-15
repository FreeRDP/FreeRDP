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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

/* ------------------------------------------------------------------------- */
static pstatus_t general_YCoCgToRGB_8u_AC4R(const BYTE* pSrc, INT32 srcStep, BYTE* pDst,
                                            UINT32 DstFormat, INT32 dstStep, UINT32 width,
                                            UINT32 height, UINT8 shift, BOOL withAlpha)
{
	BYTE A;
	UINT32 x, y;
	BYTE* dptr = pDst;
	const BYTE* sptr = pSrc;
	INT16 Cg, Co, Y, T, R, G, B;
	const DWORD formatSize = GetBytesPerPixel(DstFormat);
	fkt_writePixel writePixel = getPixelWriteFunction(DstFormat);
	int cll = shift - 1; /* -1 builds in the /2's */
	UINT32 srcPad = srcStep - (width * 4);
	UINT32 dstPad = dstStep - (width * formatSize);

	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			/* Note: shifts must be done before sign-conversion. */
			Cg = (INT16)((INT8)((*sptr++) << cll));
			Co = (INT16)((INT8)((*sptr++) << cll));
			Y = (INT16)(*sptr++); /* UINT8->INT16 */
			A = *sptr++;

			if (!withAlpha)
				A = 0xFFU;

			T = Y - Cg;
			B = T + Co;
			G = Y + Cg;
			R = T - Co;
			dptr = (*writePixel)(dptr, formatSize, DstFormat, CLIP(R), CLIP(G), CLIP(B), A);
		}

		sptr += srcPad;
		dptr += dstPad;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_YCoCg(primitives_t* prims)
{
	prims->YCoCgToRGB_8u_AC4R = general_YCoCgToRGB_8u_AC4R;
}
