/* FreeRDP: A Remote Desktop Protocol Client
 * Routines to set a chunk of memory to a constant.
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
 */

#include <config.h>
#include <string.h>
#include <freerdp/types.h>
#include <freerdp/primitives.h>
#ifdef WITH_SSE2
# include <emmintrin.h>
#endif /* WITH_SSE2 */
#ifdef WITH_IPP
# include <ipps.h>
#endif /* WITH_IPP */
#include "prim_internal.h"

/* ========================================================================= */
PRIM_STATIC pstatus_t general_set_8u(
	BYTE val,
	BYTE *pDst,
	INT32 len)
{
	memset((void *) pDst, (int) val, (size_t) len);
	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
PRIM_STATIC pstatus_t general_zero(
	void *pDst,
	size_t len)
{
	memset(pDst, 0, len);
	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
#ifdef WITH_SSE2
# if !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS)
PRIM_STATIC pstatus_t sse2_set_8u(
	BYTE val,
	BYTE *pDst,
	INT32 len)
{
	BYTE byte, *dptr;
	__m128i xmm0;
	size_t count;

	if (len < 16) return general_set_8u(val, pDst, len);

	byte  = val;
	dptr = (BYTE *) pDst;

	/* Seek 16-byte alignment. */
	while ((ULONG_PTR) dptr & 0x0f)
	{
		*dptr++ = byte;
		if (--len == 0) return PRIMITIVES_SUCCESS;
	}

	xmm0 = _mm_set1_epi8(byte);

	/* Cover 256-byte chunks via SSE register stores. */
	count = len >> 8;
	len -= count << 8;
	/* Do 256-byte chunks using one XMM register. */
	while (count--)
	{
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
	}

	/* Cover 16-byte chunks via SSE register stores. */
	count = len >> 4;
	len -= count << 4;
	/* Do 16-byte chunks using one XMM register. */
	while (count--)
	{
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 16;
	}

	/* Do leftover bytes. */
	while (len--) *dptr++ = byte;

	return PRIMITIVES_SUCCESS;
}
# endif /* !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS) */
#endif /* WITH_SSE2 */

/* ========================================================================= */
PRIM_STATIC pstatus_t general_set_32s(
	INT32 val,
	INT32 *pDst,
	INT32 len)
{
	INT32 *dptr = (INT32 *) pDst;
	size_t span, remaining;
	primitives_t *prims;

	if (len < 256)
	{
		while (len--) *dptr++ = val;
		return PRIMITIVES_SUCCESS;
	}

	/* else quadratic growth memcpy algorithm */
	span = 1;
	*dptr = val;
	remaining = len - 1;
	prims = primitives_get();
	while (remaining)
	{
		size_t thiswidth = span;
		if (thiswidth > remaining) thiswidth = remaining;
		prims->copy_8u((BYTE *) dptr, (BYTE *) (dptr + span), thiswidth<<2);
		remaining -= thiswidth;
		span <<= 1;
	}
	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
PRIM_STATIC pstatus_t general_set_32u(
	UINT32 val,
	UINT32 *pDst,
	INT32 len)
{
	UINT32 *dptr = (UINT32 *) pDst;
	size_t span, remaining;
	primitives_t *prims;

	if (len < 256)
	{
		while (len--) *dptr++ = val;
		return PRIMITIVES_SUCCESS;
	}

	/* else quadratic growth memcpy algorithm */
	span = 1;
	*dptr = val;
	remaining = len - 1;
	prims = primitives_get();
	while (remaining)
	{
		size_t thiswidth = span;
		if (thiswidth > remaining) thiswidth = remaining;
		prims->copy_8u((BYTE *) dptr, (BYTE *) (dptr + span), thiswidth<<2);
		remaining -= thiswidth;
		span <<= 1;
	}
	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
#ifdef WITH_SSE2
# if !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS)
PRIM_STATIC pstatus_t sse2_set_32u(
	UINT32 val,
	UINT32 *pDst,
	INT32 len)
{
	UINT32 *dptr = (UINT32 *) pDst;
	__m128i xmm0;
	size_t count;

	/* If really short, just do it here. */
	if (len < 32)
	{
		while (len--) *dptr++ = val;
		return PRIMITIVES_SUCCESS;
	}

	/* Assure we can reach 16-byte alignment. */
	if (((ULONG_PTR) dptr & 0x03) != 0)
	{
		return general_set_32u(val, pDst, len);
	}

	/* Seek 16-byte alignment. */
	while ((ULONG_PTR) dptr & 0x0f)
	{
		*dptr++ = val;
		if (--len == 0) return PRIMITIVES_SUCCESS;
	}

	xmm0 = _mm_set1_epi32(val);

	/* Cover 256-byte chunks via SSE register stores. */
	count = len >> 6;
	len -= count << 6;
	/* Do 256-byte chunks using one XMM register. */
	while (count--)
	{
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
	}

	/* Cover 16-byte chunks via SSE register stores. */
	count = len >> 2;
	len -= count << 2;
	/* Do 16-byte chunks using one XMM register. */
	while (count--)
	{
		_mm_store_si128((__m128i *) dptr, xmm0);  dptr += 4;
	}

	/* Do leftover bytes. */
	while (len--) *dptr++ = val;

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
PRIM_STATIC pstatus_t sse2_set_32s(
	INT32 val,
	INT32 *pDst,
	INT32 len)
{
	UINT32 uval = *((UINT32 *) &val);
	return sse2_set_32u(uval, (UINT32 *) pDst, len);
}
# endif /* !defined(WITH_IPP) || defined(ALL_PRIMITIVES_VERSIONS) */
#endif /* WITH_SSE2 */

#ifdef WITH_IPP
/* ------------------------------------------------------------------------- */
PRIM_STATIC pstatus_t ipp_wrapper_set_32u(
	UINT32 val,
	UINT32 *pDst,
	INT32 len)
{
	/* A little type conversion, then use the signed version. */
	INT32 sval = *((INT32 *) &val);
	return ippsSet_32s(sval, (INT32 *) pDst, len);
}
#endif

/* ------------------------------------------------------------------------- */
void primitives_init_set(
	const primitives_hints_t *hints,
	primitives_t *prims)
{
	/* Start with the default. */
	prims->set_8u  = general_set_8u;
	prims->set_32s = general_set_32s;
	prims->set_32u = general_set_32u;
	prims->zero = general_zero;

	/* Pick tuned versions if possible. */
#ifdef WITH_IPP
	prims->set_8u  = (__set_8u_t)  ippsSet_8u;
	prims->set_32s = (__set_32s_t) ippsSet_32s;
	prims->set_32u = (__set_32u_t) ipp_wrapper_set_32u;
	prims->zero = (__zero_t) ippsZero_8u;
#elif defined(WITH_SSE2)
	if (hints->x86_flags & PRIM_X86_SSE2_AVAILABLE)
	{
		prims->set_8u  = sse2_set_8u;
		prims->set_32s = sse2_set_32s;
		prims->set_32u = sse2_set_32u;
	}
#endif
}

/* ------------------------------------------------------------------------- */
void primitives_deinit_set(
	primitives_t *prims)
{
	/* Nothing to do. */
}
