/* primitives.h
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
 */

#ifdef __GNUC__
# pragma once
#endif

#ifndef __PRIMITIVES_H_INCLUDED__
#define __PRIMITIVES_H_INCLUDED__

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <winpr/platform.h>

typedef INT32 pstatus_t;				/* match IppStatus. */
#define PRIMITIVES_SUCCESS		(0)		/* match ippStsNoErr */

/* Simple macro for address of an x,y location in 2d 4-byte memory block */
#define PIXMAP4_ADDR(_dst_, _x_, _y_, _span_) \
	((void *) (((BYTE *) (_dst_)) + (((_x_) + (_y_)*(_span_)) << 2)))

#define PRIM_X86_MMX_AVAILABLE					(1U<<0)
#define PRIM_X86_3DNOW_AVAILABLE				(1U<<1)
#define PRIM_X86_3DNOW_PREFETCH_AVAILABLE		(1U<<2)
#define PRIM_X86_SSE_AVAILABLE					(1U<<3)
#define PRIM_X86_SSE2_AVAILABLE					(1U<<4)
#define PRIM_X86_SSE3_AVAILABLE					(1U<<5)
#define PRIM_X86_SSSE3_AVAILABLE				(1U<<6)
#define PRIM_X86_SSE41_AVAILABLE				(1U<<7)
#define PRIM_X86_SSE42_AVAILABLE				(1U<<8)
#define PRIM_X86_AVX_AVAILABLE					(1U<<9)
#define PRIM_X86_FMA_AVAILABLE					(1U<<10)
#define PRIM_X86_AVX_AES_AVAILABLE				(1U<<11)
#define PRIM_X86_AVX2_AVAILABLE					(1U<<12)

#define PRIM_ARM_VFP1_AVAILABLE					(1U<<0)
#define PRIM_ARM_VFP2_AVAILABLE					(1U<<1)
#define PRIM_ARM_VFP3_AVAILABLE					(1U<<2)
#define PRIM_ARM_VFP4_AVAILABLE					(1U<<3)
#define PRIM_ARM_FPA_AVAILABLE					(1U<<4)
#define PRIM_ARM_FPE_AVAILABLE					(1U<<5)
#define PRIM_ARM_IWMMXT_AVAILABLE				(1U<<6)
#define PRIM_ARM_NEON_AVAILABLE					(1U<<7)

/* Structures compatible with IPP */
typedef struct
{
	UINT32 width;
	UINT32 height;
} prim_size_t;		/* like IppiSize */

/* Function prototypes for all of the supported primitives. */
typedef pstatus_t (*__copy_t)(
	const void *pSrc,
	void *pDst,
	INT32 bytes);
typedef pstatus_t (*__copy_8u_t)(
	const BYTE *pSrc,
	BYTE *pDst,
	INT32 len);
typedef pstatus_t (*__copy_8u_AC4r_t)(
	const BYTE *pSrc,
	INT32 srcStep,	/* bytes */
	BYTE *pDst,
	INT32 dstStep,	/* bytes */
	INT32 width,  INT32 height);	/* pixels */
typedef pstatus_t (*__set_8u_t)(
	BYTE val,
	BYTE *pDst,
	INT32 len);
typedef pstatus_t (*__set_32s_t)(
	INT32 val,
	INT32 *pDst,
	INT32 len);
typedef pstatus_t (*__set_32u_t)(
	UINT32 val,
	UINT32 *pDst,
	INT32 len);
typedef pstatus_t (*__zero_t)(
	void *pDst,
	size_t bytes);
typedef pstatus_t (*__alphaComp_argb_t)(
	const BYTE *pSrc1,  INT32 src1Step,
	const BYTE *pSrc2,  INT32 src2Step,
	BYTE *pDst,  INT32 dstStep,
	INT32 width,  INT32 height);
typedef pstatus_t (*__add_16s_t)(
	const INT16 *pSrc1,
	const INT16 *pSrc2,
	INT16 *pDst,
	INT32 len);
typedef pstatus_t (*__lShiftC_16s_t)(
	const INT16 *pSrc,
	INT32 val,
	INT16 *pSrcDst,
	INT32 len);
typedef pstatus_t (*__lShiftC_16u_t)(
	const UINT16 *pSrc,
	INT32 val,
	UINT16 *pSrcDst,
	INT32 len);
typedef pstatus_t (*__rShiftC_16s_t)(
	const INT16 *pSrc,
	INT32 val,
	INT16 *pSrcDst,
	INT32 len);
typedef pstatus_t (*__rShiftC_16u_t)(
	const UINT16 *pSrc,
	INT32 val,
	UINT16 *pSrcDst,
	INT32 len);
typedef pstatus_t (*__shiftC_16s_t)(
	const INT16 *pSrc,
	INT32 val,
	INT16 *pSrcDst,
	INT32 len);
typedef pstatus_t (*__shiftC_16u_t)(
	const UINT16 *pSrc,
	INT32 val,
	UINT16 *pSrcDst,
	INT32 len);
typedef pstatus_t (*__sign_16s_t)(
	const INT16 *pSrc,
	INT16 *pDst,
	INT32 len);
typedef pstatus_t (*__yCbCrToRGB_16s8u_P3AC4R_t)(
	const INT16* pSrc[3], INT32 srcStep,
	BYTE* pDst, INT32 dstStep,
	const prim_size_t* roi);
typedef pstatus_t (*__yCbCrToBGR_16s8u_P3AC4R_t)(
	const INT16* pSrc[3], INT32 srcStep,
	BYTE* pDst, INT32 dstStep,
	const prim_size_t* roi);
typedef pstatus_t (*__yCbCrToRGB_16s16s_P3P3_t)(
	const INT16 *pSrc[3],  INT32 srcStep,
	INT16 *pDst[3],  INT32 dstStep,
	const prim_size_t *roi);
typedef pstatus_t (*__RGBToYCbCr_16s16s_P3P3_t)(
	const INT16 *pSrc[3],  INT32 srcStep,
	INT16 *pDst[3],  INT32 dstStep,
	const prim_size_t *roi);
typedef pstatus_t (*__RGBToRGB_16s8u_P3AC4R_t)(
	const INT16 *pSrc[3],  INT32 srcStep,
	BYTE *pDst,  INT32 dstStep,
	const prim_size_t *roi);
typedef pstatus_t (*__YCoCgToRGB_8u_AC4R_t)(
	const BYTE *pSrc, INT32 srcStep,
	BYTE *pDst, INT32 dstStep,
	UINT32 width, UINT32 height,
	UINT8 shift,
	BOOL withAlpha,
	BOOL invert);
typedef pstatus_t (*__RGB565ToARGB_16u32u_C3C4_t)(
	const UINT16* pSrc, INT32 srcStep,
	UINT32* pDst, INT32 dstStep,
	UINT32 width, UINT32 height,
	BOOL alpha, BOOL invert);
typedef pstatus_t (*__YUV420ToRGB_8u_P3AC4R_t)(
	const BYTE* pSrc[3], const UINT32 srcStep[3],
	BYTE* pDst, UINT32 dstStep,
	const prim_size_t* roi);
typedef pstatus_t (*__YUV444ToRGB_8u_P3AC4R_t)(
	const BYTE* pSrc[3], const UINT32 srcStep[3],
	BYTE* pDst, UINT32 dstStep,
	const prim_size_t* roi);
typedef pstatus_t (*__RGBToYUV420_8u_P3AC4R_t)(
	const BYTE* pSrc, UINT32 srcStep,
	BYTE* pDst[3], UINT32 dstStep[3],
	const prim_size_t* roi);
typedef pstatus_t (*__RGBToYUV444_8u_P3AC4R_t)(
	const BYTE* pSrc, UINT32 srcStep,
	BYTE* pDst[3], UINT32 dstStep[3],
	const prim_size_t* roi);
typedef pstatus_t (*__YUV420CombineToYUV444_t)(
		const BYTE* pMainSrc[3], const UINT32 srcMainStep[3],
		const BYTE* pAuxSrc[3], const UINT32 srcAuxStep[3],
		BYTE* pDst[3], const UINT32 dstStep[3],
		const prim_size_t* roi);
typedef pstatus_t (*__YUV444SplitToYUV420_t)(
		const BYTE* pSrc[3], const UINT32 srcStep[3],
		BYTE* pMainDst[3], const UINT32 dstMainStep[3],
		BYTE* pAuxDst[3], const UINT32 srcAuxStep[3],
		const prim_size_t* roi);
typedef pstatus_t (*__andC_32u_t)(
	const UINT32 *pSrc,
	UINT32 val,
	UINT32 *pDst,
	INT32 len);
typedef pstatus_t (*__orC_32u_t)(
	const UINT32 *pSrc,
	UINT32 val,
	UINT32 *pDst,
	INT32 len);

typedef struct
{
	/* Memory-to-memory copy routines */
	__copy_t copy;						/* memcpy/memmove, basically */
	__copy_8u_t copy_8u;				/* more strongly typed */
	__copy_8u_AC4r_t copy_8u_AC4r;		/* pixel copy function */
	/* Memory setting routines */
	__set_8u_t set_8u;					/* memset, basically */
	__set_32s_t set_32s;
	__set_32u_t set_32u;
	__zero_t zero;						/* bzero or faster */
	/* Arithmetic functions */
	__add_16s_t add_16s;
	/* And/or */
	__andC_32u_t andC_32u;
	__orC_32u_t orC_32u;
	/* Shifts */
	__lShiftC_16s_t lShiftC_16s;
	__lShiftC_16u_t lShiftC_16u;
	__rShiftC_16s_t rShiftC_16s;
	__rShiftC_16u_t rShiftC_16u;
	__shiftC_16s_t   shiftC_16s;
	__shiftC_16u_t   shiftC_16u;
	/* Alpha Composition */
	__alphaComp_argb_t alphaComp_argb;
	/* Sign */
	__sign_16s_t sign_16s;
	/* Color conversions */
	__yCbCrToRGB_16s8u_P3AC4R_t yCbCrToRGB_16s8u_P3AC4R;
	__yCbCrToBGR_16s8u_P3AC4R_t yCbCrToBGR_16s8u_P3AC4R;
	__yCbCrToRGB_16s16s_P3P3_t yCbCrToRGB_16s16s_P3P3;
	__RGBToYCbCr_16s16s_P3P3_t RGBToYCbCr_16s16s_P3P3;
	__RGBToRGB_16s8u_P3AC4R_t RGBToRGB_16s8u_P3AC4R;
	__YCoCgToRGB_8u_AC4R_t YCoCgToRGB_8u_AC4R;
	__RGB565ToARGB_16u32u_C3C4_t RGB565ToARGB_16u32u_C3C4;
	__YUV420ToRGB_8u_P3AC4R_t YUV420ToRGB_8u_P3AC4R;
	__RGBToYUV420_8u_P3AC4R_t RGBToYUV420_8u_P3AC4R;
	__RGBToYUV444_8u_P3AC4R_t RGBToYUV444_8u_P3AC4R;
	__YUV420CombineToYUV444_t YUV420CombineToYUV444;
	__YUV444SplitToYUV420_t YUV444SplitToYUV420;
	__YUV420ToRGB_8u_P3AC4R_t YUV444ToRGB_8u_P3AC4R;
} primitives_t;

#ifdef __cplusplus
extern "C" {
#endif

/* Prototypes for the externally-visible entrypoints. */
FREERDP_API void primitives_init(void);
FREERDP_API primitives_t *primitives_get(void);
FREERDP_API void primitives_deinit(void);

#ifdef __cplusplus
}
#endif

#endif /* !__PRIMITIVES_H_INCLUDED__ */
