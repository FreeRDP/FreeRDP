/* FreeRDP: A Remote Desktop Protocol Client
 * Copy operations.
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
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>
#ifdef WITH_IPP
# include <ipps.h>
# include <ippi.h>
#endif /* WITH_IPP */
#include "prim_internal.h"

/* ------------------------------------------------------------------------- */
/*static inline BOOL memory_regions_overlap_1d(*/
static BOOL memory_regions_overlap_1d(
	const BYTE *p1,
	const BYTE *p2,
	size_t bytes)
{
	const ULONG_PTR p1m = (const ULONG_PTR) p1;
	const ULONG_PTR p2m = (const ULONG_PTR) p2;
	if (p1m <= p2m)
	{
		if (p1m + bytes > p2m) return TRUE;
	}
	else
	{
		if (p2m + bytes > p1m) return TRUE;
	}
	/* else */
	return FALSE;
}

/* ------------------------------------------------------------------------- */
/*static inline BOOL memory_regions_overlap_2d( */
static BOOL memory_regions_overlap_2d(
	const BYTE *p1,  int p1Step,  int p1Size,
	const BYTE *p2,  int p2Step,  int p2Size,
	int width,  int height)
{
	ULONG_PTR p1m = (ULONG_PTR) p1;
	ULONG_PTR p2m = (ULONG_PTR) p2;

	if (p1m <= p2m)
	{
		ULONG_PTR p1mEnd = p1m + (height-1)*p1Step + width*p1Size;
		if (p1mEnd > p2m) return TRUE;
	}
	else
	{
		ULONG_PTR p2mEnd = p2m + (height-1)*p2Step + width*p2Size;
		if (p2mEnd > p1m) return TRUE;
	}
	/* else */
	return FALSE;
}

/* ------------------------------------------------------------------------- */
pstatus_t general_copy_8u(
	const BYTE *pSrc,
	BYTE *pDst,
	INT32 len)
{
	if (memory_regions_overlap_1d(pSrc, pDst, (size_t) len))
	{
		memmove((void *) pDst, (const void *) pSrc, (size_t) len);
	}
	else
	{
		memcpy((void *) pDst, (const void *) pSrc, (size_t) len);
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
/* Copy a block of pixels from one buffer to another.
 * The addresses are assumed to have been already offset to the upper-left
 * corners of the source and destination region of interest.
 */
pstatus_t general_copy_8u_AC4r(
	const BYTE *pSrc,  INT32 srcStep,
	BYTE *pDst,  INT32 dstStep,
	INT32 width,  INT32 height)
{
	primitives_t *prims = primitives_get();
	const BYTE *src = (const BYTE *) pSrc;
	BYTE *dst = (BYTE *) pDst;
	int rowbytes = width * sizeof(UINT32);

	if ((width == 0) || (height == 0)) return PRIMITIVES_SUCCESS;

	if (memory_regions_overlap_2d(pSrc, srcStep, sizeof(UINT32),
			pDst, dstStep, sizeof(UINT32), width, height))
	{
		do {
			prims->copy(src, dst, rowbytes);
			src += srcStep;
			dst += dstStep;
		} while (--height);
	}
	else
	{
		/* TODO: do it in one operation when the rowdata is adjacent. */
		do {
			/* If we find a replacement for memcpy that is consistently
			 * faster, this could be replaced with that.
			 */
			memcpy(dst, src, rowbytes);
			src += srcStep;
			dst += dstStep;
		} while (--height);
	}

	return PRIMITIVES_SUCCESS;
}

#ifdef WITH_IPP
/* ------------------------------------------------------------------------- */
/* This is just ippiCopy_8u_AC4R without the IppiSize structure parameter.   */
static pstatus_t ippiCopy_8u_AC4r(
	const BYTE *pSrc,  INT32 srcStep,
	BYTE *pDst,  INT32 dstStep,
	INT32 width,  INT32 height)
{
	IppiSize roi;
	roi.width  = width;
	roi.height = height;
	return (pstatus_t) ippiCopy_8u_AC4R(pSrc, srcStep, pDst, dstStep, roi);
}
#endif /* WITH_IPP */

/* ------------------------------------------------------------------------- */
void primitives_init_copy(
	primitives_t *prims)
{
	/* Start with the default. */
	prims->copy_8u = general_copy_8u;
	prims->copy_8u_AC4r = general_copy_8u_AC4r;

	/* Pick tuned versions if possible. */
#ifdef WITH_IPP
	prims->copy_8u = (__copy_8u_t) ippsCopy_8u;
	prims->copy_8u_AC4r = (__copy_8u_AC4r_t) ippiCopy_8u_AC4r;
#endif
	/* Performance with an SSE2 version with no prefetch seemed to be
	 * all over the map vs. memcpy.  
	 * Sometimes it was significantly faster, sometimes dreadfully slower,
	 * and it seemed to vary a lot depending on block size and processor.
	 * Hence, no SSE version is used here unless once can be written that
	 * is consistently faster than memcpy.
	 */

	/* This is just an alias with void* parameters */
	prims->copy    = (__copy_t) (prims->copy_8u);
}

/* ------------------------------------------------------------------------- */
void primitives_deinit_copy(
	primitives_t *prims)
{
	/* Nothing to do. */
}
