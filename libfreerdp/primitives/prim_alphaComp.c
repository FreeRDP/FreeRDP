/* FreeRDP: A Remote Desktop Protocol Client
 * Alpha blending routines.
 * vi:ts=4 sw=4:
 *
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 *
 * Note: this code assumes the second operand is fully opaque,
 * e.g.
 *   newval = alpha1*val1 + (1-alpha1)*val2
 * rather than
 *   newval = alpha1*val1 + (1-alpha1)*alpha2*val2
 * The IPP gives other options.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

#define ALPHA(_k_) (((_k_)&0xFF000000U) >> 24)
#define RED(_k_) (((_k_)&0x00FF0000U) >> 16)
#define GRN(_k_) (((_k_)&0x0000FF00U) >> 8)
#define BLU(_k_) (((_k_)&0x000000FFU))

/* ------------------------------------------------------------------------- */
static pstatus_t general_alphaComp_argb(const BYTE* pSrc1, UINT32 src1Step, const BYTE* pSrc2,
                                        UINT32 src2Step, BYTE* pDst, UINT32 dstStep, UINT32 width,
                                        UINT32 height)
{
	UINT32 y;

	for (y = 0; y < height; y++)
	{
		const UINT32* sptr1 = (const UINT32*)(pSrc1 + y * src1Step);
		const UINT32* sptr2 = (const UINT32*)(pSrc2 + y * src2Step);
		UINT32* dptr = (UINT32*)(pDst + y * dstStep);
		UINT32 x;

		for (x = 0; x < width; x++)
		{
			const UINT32 src1 = *sptr1++;
			const UINT32 src2 = *sptr2++;
			UINT32 alpha = ALPHA(src1) + 1;

			if (alpha == 256)
			{
				/* If alpha is 255+1, just copy src1. */
				*dptr++ = src1;
			}
			else if (alpha <= 1)
			{
				/* If alpha is 0+1, just copy src2. */
				*dptr++ = src2;
			}
			else
			{
				/* A perfectly accurate blend would do (a*src + (255-a)*dst)/255
				 * rather than adding one to alpha and dividing by 256, but this
				 * is much faster and only differs by one 16% of the time.
				 * I'm not sure who first designed the double-ops trick
				 * (Red Blue and Alpha Green).
				 */
				UINT32 rb, ag;
				UINT32 s2rb = src2 & 0x00FF00FFU;
				UINT32 s2ag = (src2 >> 8) & 0x00FF00FFU;
				UINT32 s1rb = src1 & 0x00FF00FFU;
				UINT32 s1ag = (src1 >> 8) & 0x00FF00FFU;
				UINT32 drb = s1rb - s2rb;
				UINT32 dag = s1ag - s2ag;
				drb *= alpha;
				dag *= alpha;
				rb = ((drb >> 8) + s2rb) & 0x00FF00FFU;
				ag = (((dag >> 8) + s2ag) << 8) & 0xFF00FF00U;
				*dptr++ = rb | ag;
			}
		}
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_alphaComp(primitives_t* prims)
{
	prims->alphaComp_argb = general_alphaComp_argb;
}
