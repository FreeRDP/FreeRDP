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
#include "prim_YCoCg.h"

#ifndef MINMAX
#define MINMAX(_v_, _l_, _h_) \
	((_v_) < (_l_) ? (_l_) : ((_v_) > (_h_) ? (_h_) : (_v_)))
#endif /* !MINMAX */

/* ------------------------------------------------------------------------- */
pstatus_t general_YCoCgToRGB_8u_AC4R(
	const BYTE *pSrc, INT32 srcStep,
	BYTE *pDst, INT32 dstStep,
	UINT32 width, UINT32 height,
	UINT8 shift,
	BOOL withAlpha,
	BOOL invert)
{
	BYTE A;
	int x, y;
	BYTE *dptr = pDst;
	const BYTE *sptr = pSrc;
	INT16 Cg, Co, Y, T, R, G, B;
	int cll = shift - 1;  /* -1 builds in the /2's */
	int srcPad = srcStep - (width * 4);
	int dstPad = dstStep - (width * 4);

	if (invert)
	{
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < width; x++)
			{
				/* Note: shifts must be done before sign-conversion. */
				Cg = (INT16) ((INT8) ((*sptr++) << cll));
				Co = (INT16) ((INT8) ((*sptr++) << cll));
				Y = (INT16) (*sptr++);	/* UINT8->INT16 */

				A = *sptr++;

				if (!withAlpha)
					A = 0xFFU;

				T  = Y - Cg;
				R  = T + Co;
				G  = Y + Cg;
				B  = T - Co;

				*dptr++ = (BYTE) MINMAX(R, 0, 255);
				*dptr++ = (BYTE) MINMAX(G, 0, 255);
				*dptr++ = (BYTE) MINMAX(B, 0, 255);
				*dptr++ = A;
			}

			sptr += srcPad;
			dptr += dstPad;
		}
	}
	else
	{
		for (y = 0; y < height; y++)
		{
			for (x = 0; x < width; x++)
			{
				/* Note: shifts must be done before sign-conversion. */
				Cg = (INT16) ((INT8) ((*sptr++) << cll));
				Co = (INT16) ((INT8) ((*sptr++) << cll));
				Y = (INT16) (*sptr++);	/* UINT8->INT16 */

				A = *sptr++;

				if (!withAlpha)
					A = 0xFFU;

				T  = Y - Cg;
				R  = T + Co;
				G  = Y + Cg;
				B  = T - Co;

				*dptr++ = (BYTE) MINMAX(B, 0, 255);
				*dptr++ = (BYTE) MINMAX(G, 0, 255);
				*dptr++ = (BYTE) MINMAX(R, 0, 255);
				*dptr++ = A;
			}

			sptr += srcPad;
			dptr += dstPad;
		}
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_YCoCg(primitives_t* prims)
{
	prims->YCoCgToRGB_8u_AC4R = general_YCoCgToRGB_8u_AC4R;

	primitives_init_YCoCg_opt(prims);
}

/* ------------------------------------------------------------------------- */
void primitives_deinit_YCoCg(primitives_t* prims)
{
	/* Nothing to do. */
}
