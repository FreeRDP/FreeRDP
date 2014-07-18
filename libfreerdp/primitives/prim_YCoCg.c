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
pstatus_t general_YCoCgRToRGB_8u_AC4R(
	const BYTE *pSrc, INT32 srcStep,
	BYTE *pDst, INT32 dstStep,
	UINT32 width, UINT32 height,
	UINT8 shift,
	BOOL withAlpha,
	BOOL invert)
{
	const BYTE *sptr = pSrc;
	BYTE *dptr = pDst;
	int cll = shift - 1;  /* -1 builds in the /2's */
	int x,y;
	int srcRowBump = srcStep - width*sizeof(UINT32);
	int dstRowBump = dstStep - width*sizeof(UINT32);
	if (invert)
	{
		for (y=0; y<height; y++)
		{
			for (x=0; x<width; x++)
			{
				INT16 cg, co, y, t, r, g, b;
				BYTE a;

				/* Note: shifts must be done before sign-conversion. */
				cg = (INT16) ((INT8) ((*sptr++) << cll));
				co = (INT16) ((INT8) ((*sptr++) << cll));
				y = (INT16) (*sptr++);	/* UINT8->INT16 */
				a = *sptr++;
				if (!withAlpha) a = 0xFFU;
				t  = y - cg;
				r  = t + co;
				g  = y + cg;
				b  = t - co;
				*dptr++ = (BYTE) MINMAX(r, 0, 255);
				*dptr++ = (BYTE) MINMAX(g, 0, 255);
				*dptr++ = (BYTE) MINMAX(b, 0, 255);
				*dptr++ = a;
			}
			sptr += srcRowBump;
			dptr += dstRowBump;
		}
	}
	else
	{
		for (y=0; y<height; y++)
		{
			for (x=0; x<width; x++)
			{
				INT16 cg, co, y, t, r, g, b;
				BYTE a;

				/* Note: shifts must be done before sign-conversion. */
				cg = (INT16) ((INT8) ((*sptr++) << cll));
				co = (INT16) ((INT8) ((*sptr++) << cll));
				y = (INT16) (*sptr++);	/* UINT8->INT16 */
				a = *sptr++;
				if (!withAlpha) a = 0xFFU;
				t  = y - cg;
				r  = t + co;
				g  = y + cg;
				b  = t - co;
				*dptr++ = (BYTE) MINMAX(b, 0, 255);
				*dptr++ = (BYTE) MINMAX(g, 0, 255);
				*dptr++ = (BYTE) MINMAX(r, 0, 255);
				*dptr++ = a;
			}
			sptr += srcRowBump;
			dptr += dstRowBump;
		}
	}
	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_YCoCg(primitives_t* prims)
{
	prims->YCoCgRToRGB_8u_AC4R = general_YCoCgRToRGB_8u_AC4R;

	primitives_init_YCoCg_opt(prims);
}

/* ------------------------------------------------------------------------- */
void primitives_deinit_YCoCg(primitives_t* prims)
{
	/* Nothing to do. */
}
