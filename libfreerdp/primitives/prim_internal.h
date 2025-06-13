/* prim_internal.h
 * vi:ts=4 sw=4
 *
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.  Algorithms used by
 * this code may be covered by patents by HP, Microsoft, or other parties.
 *
 */

#ifndef FREERDP_LIB_PRIM_INTERNAL_H
#define FREERDP_LIB_PRIM_INTERNAL_H

#include <winpr/platform.h>
#include <freerdp/config.h>

#include <freerdp/primitives.h>
#include <freerdp/api.h>

#include "../core/simd.h"

#ifdef __GNUC__
#define PRIM_ALIGN_128 __attribute__((aligned(16)))
#else
#ifdef _WIN32
#define PRIM_ALIGN_128 __declspec(align(16))
#endif
#endif

#if defined(SSE_AVX_INTRINSICS_ENABLED) || defined(NEON_INTRINSICS_ENABLED) || defined(WITH_OPENCL)
#define HAVE_OPTIMIZED_PRIMITIVES 1
#endif

#if defined(SSE_AVX_INTRINSICS_ENABLED) || defined(NEON_INTRINSICS_ENABLED)
#define HAVE_CPU_OPTIMIZED_PRIMITIVES 1
#endif

static INLINE BYTE* writePixelBGRA(BYTE* dst, DWORD formatSize, UINT32 format, BYTE R, BYTE G,
                                   BYTE B, BYTE A)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(format);

	*dst++ = B;
	*dst++ = G;
	*dst++ = R;
	*dst++ = A;
	return dst;
}

static INLINE BYTE* writePixelBGRX(BYTE* dst, DWORD formatSize, UINT32 format, BYTE R, BYTE G,
                                   BYTE B, BYTE A)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(format);
	WINPR_UNUSED(A);

	*dst++ = B;
	*dst++ = G;
	*dst++ = R;
	dst++; /* Do not touch alpha */

	return dst;
}

static INLINE BYTE* writePixelRGBA(BYTE* dst, DWORD formatSize, UINT32 format, BYTE R, BYTE G,
                                   BYTE B, BYTE A)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(format);

	*dst++ = R;
	*dst++ = G;
	*dst++ = B;
	*dst++ = A;
	return dst;
}

static INLINE BYTE* writePixelRGBX(BYTE* dst, DWORD formatSize, UINT32 format, BYTE R, BYTE G,
                                   BYTE B, BYTE A)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(format);
	WINPR_UNUSED(A);

	*dst++ = R;
	*dst++ = G;
	*dst++ = B;
	dst++; /* Do not touch alpha */

	return dst;
}

static INLINE BYTE* writePixelABGR(BYTE* dst, DWORD formatSize, UINT32 format, BYTE R, BYTE G,
                                   BYTE B, BYTE A)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(format);

	*dst++ = A;
	*dst++ = B;
	*dst++ = G;
	*dst++ = R;
	return dst;
}

static INLINE BYTE* writePixelXBGR(BYTE* dst, DWORD formatSize, UINT32 format, BYTE R, BYTE G,
                                   BYTE B, BYTE A)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(format);
	WINPR_UNUSED(A);

	dst++; /* Do not touch alpha */
	*dst++ = B;
	*dst++ = G;
	*dst++ = R;
	return dst;
}

static INLINE BYTE* writePixelARGB(BYTE* dst, DWORD formatSize, UINT32 format, BYTE R, BYTE G,
                                   BYTE B, BYTE A)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(format);

	*dst++ = A;
	*dst++ = R;
	*dst++ = G;
	*dst++ = B;
	return dst;
}

static INLINE BYTE* writePixelXRGB(BYTE* dst, DWORD formatSize, UINT32 format, BYTE R, BYTE G,
                                   BYTE B, BYTE A)
{
	WINPR_UNUSED(formatSize);
	WINPR_UNUSED(format);
	WINPR_UNUSED(A);

	dst++; /* Do not touch alpha */
	*dst++ = R;
	*dst++ = G;
	*dst++ = B;
	return dst;
}

static INLINE BYTE* writePixelGenericAlpha(BYTE* dst, DWORD formatSize, UINT32 format, BYTE R,
                                           BYTE G, BYTE B, BYTE A)
{
	UINT32 color = FreeRDPGetColor(format, R, G, B, A);
	FreeRDPWriteColor(dst, format, color);
	return dst + formatSize;
}

static INLINE BYTE* writePixelGeneric(BYTE* dst, DWORD formatSize, UINT32 format, BYTE R, BYTE G,
                                      BYTE B, BYTE A)
{
	UINT32 color = FreeRDPGetColor(format, R, G, B, A);
	FreeRDPWriteColorIgnoreAlpha(dst, format, color);
	return dst + formatSize;
}

typedef BYTE* (*fkt_writePixel)(BYTE*, DWORD, UINT32, BYTE, BYTE, BYTE, BYTE);

static INLINE fkt_writePixel getPixelWriteFunction(DWORD format, BOOL useAlpha)
{
	switch (format)
	{
		case PIXEL_FORMAT_ARGB32:
		case PIXEL_FORMAT_XRGB32:
			return useAlpha ? writePixelARGB : writePixelXRGB;

		case PIXEL_FORMAT_ABGR32:
		case PIXEL_FORMAT_XBGR32:
			return useAlpha ? writePixelABGR : writePixelXBGR;

		case PIXEL_FORMAT_RGBA32:
		case PIXEL_FORMAT_RGBX32:
			return useAlpha ? writePixelRGBA : writePixelRGBX;

		case PIXEL_FORMAT_BGRA32:
		case PIXEL_FORMAT_BGRX32:
			return useAlpha ? writePixelBGRA : writePixelBGRX;

		default:
			return useAlpha ? writePixelGenericAlpha : writePixelGeneric;
	}
}

static INLINE BYTE CLIP(INT64 X)
{
	if (X > 255L)
		return 255L;

	if (X < 0L)
		return 0L;

	return (BYTE)X;
}

static INLINE BYTE CONDITIONAL_CLIP(INT32 in, BYTE original)
{
	BYTE out = CLIP(in);
	BYTE diff = 0;
	if (out > original)
		diff = out - original;
	else
		diff = original - out;
	if (diff < 30)
		return original;
	return out;
}

/**
 * | R |   ( | 256     0    403 | |    Y    | )
 * | G | = ( | 256   -48   -120 | | U - 128 | ) >> 8
 * | B |   ( | 256   475      0 | | V - 128 | )
 */
static INLINE INT32 C(INT32 Y)
{
	return (Y)-0;
}

static INLINE INT32 D(INT32 U)
{
	return (U)-128;
}

static INLINE INT32 E(INT32 V)
{
	return (V)-128;
}

static INLINE BYTE YUV2R(INT32 Y, INT32 U, INT32 V)
{
	const INT32 r = (256 * C(Y) + 0 * D(U) + 403 * E(V));
	const INT32 r8 = r >> 8;
	return CLIP(r8);
}

static INLINE BYTE YUV2G(INT32 Y, INT32 U, INT32 V)
{
	const INT32 g = (256 * C(Y) - 48 * D(U) - 120 * E(V));
	const INT32 g8 = g >> 8;
	return CLIP(g8);
}

static INLINE BYTE YUV2B(INT32 Y, INT32 U, INT32 V)
{
	const INT32 b = (256 * C(Y) + 475 * D(U) + 0 * E(V));
	const INT32 b8 = b >> 8;
	return CLIP(b8);
}

/**
 * | Y |    ( |  54   183     18 | | R | )        |  0  |
 * | U | =  ( | -29   -99    128 | | G | ) >> 8 + | 128 |
 * | V |    ( | 128  -116    -12 | | B | )        | 128 |
 */
static INLINE BYTE RGB2Y(INT32 R, INT32 G, INT32 B)
{
	const INT32 val = ((54 * R + 183 * G + 18 * B) >> 8);
	return WINPR_ASSERTING_INT_CAST(BYTE, val);
}

static INLINE BYTE RGB2U(INT32 R, INT32 G, INT32 B)
{
	const INT32 val = (((-29 * R - 99 * G + 128 * B) >> 8) + 128);
	return WINPR_ASSERTING_INT_CAST(BYTE, val);
}

static INLINE BYTE RGB2V(INT32 R, INT32 G, INT32 B)
{
	const INT32 val = (((128 * R - 116 * G - 12 * B) >> 8) + 128);
	return WINPR_ASSERTING_INT_CAST(BYTE, val);
}

static inline BYTE* writeYUVPixel(BYTE* dst, UINT32 DstFormat, INT32 y, INT32 u, INT32 v,
                                  fkt_writePixel fkt)
{
	WINPR_ASSERT(fkt);
	const BYTE r = YUV2R(y, u, v);
	const BYTE g = YUV2G(y, u, v);
	const BYTE b = YUV2B(y, u, v);
	const DWORD formatSize = FreeRDPGetBytesPerPixel(DstFormat);
	return fkt(dst, formatSize, DstFormat, r, g, b, 0);
}

FREERDP_LOCAL void general_RGBToAVC444YUV_BGRX_DOUBLE_ROW(
    size_t offset, const BYTE* WINPR_RESTRICT srcEven, const BYTE* WINPR_RESTRICT srcOdd,
    BYTE* WINPR_RESTRICT b1Even, BYTE* WINPR_RESTRICT b1Odd, BYTE* WINPR_RESTRICT b2,
    BYTE* WINPR_RESTRICT b3, BYTE* WINPR_RESTRICT b4, BYTE* WINPR_RESTRICT b5,
    BYTE* WINPR_RESTRICT b6, BYTE* WINPR_RESTRICT b7, UINT32 width);

FREERDP_LOCAL void general_RGBToAVC444YUVv2_BGRX_DOUBLE_ROW(
    size_t offset, const BYTE* WINPR_RESTRICT pSrcEven, const BYTE* WINPR_RESTRICT pSrcOdd,
    BYTE* WINPR_RESTRICT yLumaDstEven, BYTE* WINPR_RESTRICT yLumaDstOdd,
    BYTE* WINPR_RESTRICT uLumaDst, BYTE* WINPR_RESTRICT vLumaDst,
    BYTE* WINPR_RESTRICT yEvenChromaDst1, BYTE* WINPR_RESTRICT yEvenChromaDst2,
    BYTE* WINPR_RESTRICT yOddChromaDst1, BYTE* WINPR_RESTRICT yOddChromaDst2,
    BYTE* WINPR_RESTRICT uChromaDst1, BYTE* WINPR_RESTRICT uChromaDst2,
    BYTE* WINPR_RESTRICT vChromaDst1, BYTE* WINPR_RESTRICT vChromaDst2, UINT32 width);

/* Function prototypes for all the init/deinit routines. */
FREERDP_LOCAL void primitives_init_copy(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_set(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_add(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_andor(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_shift(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_sign(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_alphaComp(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_colors(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_YCoCg(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_YUV(primitives_t* WINPR_RESTRICT prims);

FREERDP_LOCAL void primitives_init_copy_opt(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_set_opt(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_add_opt(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_andor_opt(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_shift_opt(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_sign_opt(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_alphaComp_opt(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_colors_opt(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_YCoCg_opt(primitives_t* WINPR_RESTRICT prims);
FREERDP_LOCAL void primitives_init_YUV_opt(primitives_t* WINPR_RESTRICT prims);

#if defined(WITH_OPENCL)
FREERDP_LOCAL BOOL primitives_init_opencl(primitives_t* WINPR_RESTRICT prims);
#endif

#endif /* FREERDP_LIB_PRIM_INTERNAL_H */
