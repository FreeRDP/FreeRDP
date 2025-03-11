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
#pragma once
#endif

#ifndef FREERDP_PRIMITIVES_H
#define FREERDP_PRIMITIVES_H

#include <winpr/wtypes.h>

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/codec/color.h>

#include <winpr/platform.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef INT32 pstatus_t;   /* match IppStatus. */
#define PRIMITIVES_SUCCESS (0) /* match ippStsNoErr */

/* Simple macro for address of an x,y location in 2d 4-byte memory block */
#define PIXMAP4_ADDR(_dst_, _x_, _y_, _span_) \
	((void*)(((BYTE*)(_dst_)) + (((_x_) + (_y_) * (_span_)) << 2)))

#define PRIM_X86_MMX_AVAILABLE (1U << 0)
#define PRIM_X86_3DNOW_AVAILABLE (1U << 1)
#define PRIM_X86_3DNOW_PREFETCH_AVAILABLE (1U << 2)
#define PRIM_X86_SSE_AVAILABLE (1U << 3)
#define PRIM_X86_SSE2_AVAILABLE (1U << 4)
#define PRIM_X86_SSE3_AVAILABLE (1U << 5)
#define PRIM_X86_SSSE3_AVAILABLE (1U << 6)
#define PRIM_X86_SSE41_AVAILABLE (1U << 7)
#define PRIM_X86_SSE42_AVAILABLE (1U << 8)
#define PRIM_X86_AVX_AVAILABLE (1U << 9)
#define PRIM_X86_FMA_AVAILABLE (1U << 10)
#define PRIM_X86_AVX_AES_AVAILABLE (1U << 11)
#define PRIM_X86_AVX2_AVAILABLE (1U << 12)

#define PRIM_ARM_VFP1_AVAILABLE (1U << 0)
#define PRIM_ARM_VFP2_AVAILABLE (1U << 1)
#define PRIM_ARM_VFP3_AVAILABLE (1U << 2)
#define PRIM_ARM_VFP4_AVAILABLE (1U << 3)
#define PRIM_ARM_FPA_AVAILABLE (1U << 4)
#define PRIM_ARM_FPE_AVAILABLE (1U << 5)
#define PRIM_ARM_IWMMXT_AVAILABLE (1U << 6)
#define PRIM_ARM_NEON_AVAILABLE (1U << 7)

/** @brief flags of primitives */
enum
{
	PRIM_FLAGS_HAVE_EXTCPU = (1U << 0), /* primitives are using CPU extensions */
	PRIM_FLAGS_HAVE_EXTGPU = (1U << 1), /* primitives are using the GPU */
};

typedef struct
{
	UINT32 width;
	UINT32 height;
} prim_size_t;

typedef enum
{
	AVC444_LUMA,
	AVC444_CHROMAv1,
	AVC444_CHROMAv2
} avc444_frame_type;

/* Function prototypes for all of the supported primitives. */
typedef pstatus_t (*fn_copy_t)(const void* WINPR_RESTRICT pSrc, void* WINPR_RESTRICT pDst,
	                           INT32 bytes);
typedef pstatus_t (*fn_copy_8u_t)(const BYTE* WINPR_RESTRICT pSrc, BYTE* WINPR_RESTRICT pDst,
	                              INT32 len);
typedef pstatus_t (*fn_copy_8u_AC4r_t)(const BYTE* WINPR_RESTRICT pSrc, INT32 srcStep, /* bytes */
	                                   BYTE* WINPR_RESTRICT pDst, INT32 dstStep,       /* bytes */
	                                   INT32 width, INT32 height);                     /* pixels */
typedef pstatus_t (*fn_set_8u_t)(BYTE val, BYTE* WINPR_RESTRICT pDst, UINT32 len);
typedef pstatus_t (*fn_set_32s_t)(INT32 val, INT32* WINPR_RESTRICT pDst, UINT32 len);
typedef pstatus_t (*fn_set_32u_t)(UINT32 val, UINT32* WINPR_RESTRICT pDst, UINT32 len);
typedef pstatus_t (*fn_zero_t)(void* WINPR_RESTRICT pDst, size_t bytes);
typedef pstatus_t (*fn_alphaComp_argb_t)(const BYTE* WINPR_RESTRICT pSrc1, UINT32 src1Step,
	                                     const BYTE* WINPR_RESTRICT pSrc2, UINT32 src2Step,
	                                     BYTE* WINPR_RESTRICT pDst, UINT32 dstStep, UINT32 width,
	                                     UINT32 height);
typedef pstatus_t (*fn_add_16s_t)(const INT16* WINPR_RESTRICT pSrc1,
	                              const INT16* WINPR_RESTRICT pSrc2, INT16* WINPR_RESTRICT pDst,
	                              UINT32 len);
/**
 *  @brief Add INT16 from pSrcDst2 to pSrcDst1 and store in both arrays
 *  @param pSrcDst1 A pointer to the array of INT16 to add to
 *  @param pSrcDst2 A pointer to the array of INT16 to add to
 *  @param len The number of INT16 in the arrays
 *  @return \b <=0 for failure, success otherwise
 *  @since version 3.6.0
 */
typedef pstatus_t (*fn_add_16s_inplace_t)(INT16* WINPR_RESTRICT pSrcDst1,
	                                      INT16* WINPR_RESTRICT pSrcDst2, UINT32 len);

/**
 * @brief Copy (sub)image data without overlapping
 *
 * @param pDstData The destination image buffer
 * @param DstFormat The destination image format @ref PIXEL_FORMAT
 * @param nDstStep The destination image line width in bytes (including padding)
 * @param nXDst The X coordinate to start copying to
 * @param nYDst The Y coordinate to start copying to
 * @param nWidth The width in pixels to copy
 * @param nHeight The height in pixels to copy
 * @param pSrcData The source image buffer
 * @param SrcFormat The source image format @ref PIXEL_FORMAT
 * @param nSrcStep The source image line with in bytes (including padding)
 * @param nXSrc The X coordinate to start copying from
 * @param nYSrc The Y coordinate to start copying from
 * @param palette A color palette for 8 bit colors
 * @param flags Copy flags @ref FREERDP_IMAGE_FLAGS
 * @return \b <=0 for failure, success otherwise
 *  @since version 3.6.0
 */
typedef pstatus_t (*fn_copy_no_overlap_t)(BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat,
	                                      UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
	                                      UINT32 nWidth, UINT32 nHeight,
	                                      const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
	                                      UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc,
	                                      const gdiPalette* WINPR_RESTRICT palette, UINT32 flags);
typedef pstatus_t (*fn_lShiftC_16s_inplace_t)(INT16* WINPR_RESTRICT pSrcDst, UINT32 val,
	                                          UINT32 len);
typedef pstatus_t (*fn_lShiftC_16s_t)(const INT16* WINPR_RESTRICT pSrc, UINT32 val,
	                                  INT16* WINPR_RESTRICT pSrcDst, UINT32 len);
typedef pstatus_t (*fn_lShiftC_16u_t)(const UINT16* WINPR_RESTRICT pSrc, UINT32 val,
	                                  UINT16* WINPR_RESTRICT pSrcDst, UINT32 len);
typedef pstatus_t (*fn_rShiftC_16s_t)(const INT16* WINPR_RESTRICT pSrc, UINT32 val,
	                                  INT16* WINPR_RESTRICT pSrcDst, UINT32 len);
typedef pstatus_t (*fn_rShiftC_16u_t)(const UINT16* WINPR_RESTRICT pSrc, UINT32 val,
	                                  UINT16* WINPR_RESTRICT pSrcDst, UINT32 len);
typedef pstatus_t (*fn_shiftC_16s_t)(const INT16* WINPR_RESTRICT pSrc, INT32 val,
	                                 INT16* WINPR_RESTRICT pSrcDst, UINT32 len);
typedef pstatus_t (*fn_shiftC_16u_t)(const UINT16* WINPR_RESTRICT pSrc, INT32 val,
	                                 UINT16* WINPR_RESTRICT pSrcDst, UINT32 len);
typedef pstatus_t (*fn_sign_16s_t)(const INT16* WINPR_RESTRICT pSrc, INT16* WINPR_RESTRICT pSrcDst,
	                               UINT32 len);
typedef pstatus_t (*fn_yCbCrToRGB_16s8u_P3AC4R_t)(const INT16* WINPR_RESTRICT pSrc[3],
	                                              UINT32 srcStep, BYTE* WINPR_RESTRICT pDst,
	                                              UINT32 dstStep, UINT32 DstFormat,
	                                              const prim_size_t* WINPR_RESTRICT roi);
typedef pstatus_t (*fn_yCbCrToRGB_16s16s_P3P3_t)(const INT16* WINPR_RESTRICT pSrc[3], INT32 srcStep,
	                                             INT16* WINPR_RESTRICT pDst[3], INT32 dstStep,
	                                             const prim_size_t* WINPR_RESTRICT roi);
typedef pstatus_t (*fn_RGBToYCbCr_16s16s_P3P3_t)(const INT16* WINPR_RESTRICT pSrc[3], INT32 srcStep,
	                                             INT16* WINPR_RESTRICT pDst[3], INT32 dstStep,
	                                             const prim_size_t* WINPR_RESTRICT roi);
typedef pstatus_t (*fn_RGBToRGB_16s8u_P3AC4R_t)(const INT16* WINPR_RESTRICT pSrc[3], UINT32 srcStep,
	                                            BYTE* WINPR_RESTRICT pDst, UINT32 dstStep,
	                                            UINT32 DstFormat,
	                                            const prim_size_t* WINPR_RESTRICT roi);
typedef pstatus_t (*fn_YCoCgToRGB_8u_AC4R_t)(const BYTE* WINPR_RESTRICT pSrc, INT32 srcStep,
	                                         BYTE* WINPR_RESTRICT pDst, UINT32 DstFormat,
	                                         INT32 dstStep, UINT32 width, UINT32 height,
	                                         UINT8 shift, BOOL withAlpha);
typedef pstatus_t (*fn_RGB565ToARGB_16u32u_C3C4_t)(const UINT16* WINPR_RESTRICT pSrc, INT32 srcStep,
	                                               UINT32* WINPR_RESTRICT pDst, INT32 dstStep,
	                                               UINT32 width, UINT32 height, UINT32 format);
typedef pstatus_t (*fn_YUV420ToRGB_8u_P3AC4R_t)(const BYTE* WINPR_RESTRICT pSrc[3],
	                                            const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pDst,
	                                            UINT32 dstStep, UINT32 DstFormat,
	                                            const prim_size_t* WINPR_RESTRICT roi);
typedef pstatus_t (*fn_YUV444ToRGB_8u_P3AC4R_t)(const BYTE* WINPR_RESTRICT pSrc[3],
	                                            const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pDst,
	                                            UINT32 dstStep, UINT32 DstFormat,
	                                            const prim_size_t* WINPR_RESTRICT roi);
typedef pstatus_t (*fn_RGBToYUV420_8u_P3AC4R_t)(const BYTE* WINPR_RESTRICT pSrc, UINT32 SrcFormat,
	                                            UINT32 srcStep, BYTE* WINPR_RESTRICT pDst[3],
	                                            const UINT32 dstStep[3],
	                                            const prim_size_t* WINPR_RESTRICT roi);
typedef pstatus_t (*fn_RGBToYUV444_8u_P3AC4R_t)(const BYTE* WINPR_RESTRICT pSrc, UINT32 SrcFormat,
	                                            UINT32 srcStep, BYTE* WINPR_RESTRICT pDst[3],
	                                            const UINT32 dstStep[3],
	                                            const prim_size_t* WINPR_RESTRICT roi);
typedef pstatus_t (*fn_YUV420CombineToYUV444_t)(avc444_frame_type type,
	                                            const BYTE* WINPR_RESTRICT pSrc[3],
	                                            const UINT32 srcStep[3], UINT32 nWidth,
	                                            UINT32 nHeight, BYTE* WINPR_RESTRICT pDst[3],
	                                            const UINT32 dstStep[3],
	                                            const RECTANGLE_16* WINPR_RESTRICT roi);
typedef pstatus_t (*fn_YUV444SplitToYUV420_t)(
	const BYTE* WINPR_RESTRICT pSrc[3], const UINT32 srcStep[3], BYTE* WINPR_RESTRICT pMainDst[3],
	const UINT32 dstMainStep[3], BYTE* WINPR_RESTRICT pAuxDst[3], const UINT32 srcAuxStep[3],
	const prim_size_t* WINPR_RESTRICT roi);
typedef pstatus_t (*fn_RGBToAVC444YUV_t)(const BYTE* WINPR_RESTRICT pSrc, UINT32 srcFormat,
	                                     UINT32 srcStep, BYTE* WINPR_RESTRICT pMainDst[3],
	                                     const UINT32 dstMainStep[3],
	                                     BYTE* WINPR_RESTRICT pAuxDst[3],
	                                     const UINT32 dstAuxStep[3],
	                                     const prim_size_t* WINPR_RESTRICT roi);
typedef pstatus_t (*fn_andC_32u_t)(const UINT32* WINPR_RESTRICT pSrc, UINT32 val,
	                               UINT32* WINPR_RESTRICT pDst, INT32 len);
typedef pstatus_t (*fn_orC_32u_t)(const UINT32* WINPR_RESTRICT pSrc, UINT32 val,
	                              UINT32* WINPR_RESTRICT pDst, INT32 len);
typedef pstatus_t (*primitives_uninit_t)(void);

#if defined(WITH_FREERDP_3x_DEPRECATED)
typedef fn_copy_t __copy_t;
typedef fn_copy_8u_t __copy_8u_t;
typedef fn_copy_8u_AC4r_t __copy_8u_AC4r_t;
typedef fn_set_8u_t __set_8u_t;
typedef fn_set_32s_t __set_32s_t;
typedef fn_set_32u_t __set_32u_t;
typedef fn_zero_t __zero_t;
typedef fn_alphaComp_argb_t __alphaComp_argb_t;
typedef fn_add_16s_t __add_16s_t;
typedef fn_add_16s_inplace_t __add_16s_inplace_t;
typedef fn_copy_no_overlap_t __copy_no_overlap_t;
typedef fn_lShiftC_16s_inplace_t __lShiftC_16s_inplace_t;
typedef fn_lShiftC_16s_t __lShiftC_16s_t;
typedef fn_lShiftC_16u_t __lShiftC_16u_t;
typedef fn_rShiftC_16s_t __rShiftC_16s_t;
typedef fn_rShiftC_16u_t __rShiftC_16u_t;
typedef fn_shiftC_16s_t __shiftC_16s_t;
typedef fn_shiftC_16u_t __shiftC_16u_t;
typedef fn_sign_16s_t __sign_16s_t;
typedef fn_yCbCrToRGB_16s8u_P3AC4R_t __yCbCrToRGB_16s8u_P3AC4R_t;
typedef fn_yCbCrToRGB_16s16s_P3P3_t __yCbCrToRGB_16s16s_P3P3_t;
typedef fn_RGBToYCbCr_16s16s_P3P3_t __RGBToYCbCr_16s16s_P3P3_t;
typedef fn_RGBToRGB_16s8u_P3AC4R_t __RGBToRGB_16s8u_P3AC4R_t;
typedef fn_YCoCgToRGB_8u_AC4R_t __YCoCgToRGB_8u_AC4R_t;
typedef fn_RGB565ToARGB_16u32u_C3C4_t __RGB565ToARGB_16u32u_C3C4_t;
typedef fn_YUV420ToRGB_8u_P3AC4R_t __YUV420ToRGB_8u_P3AC4R_t;
typedef fn_YUV444ToRGB_8u_P3AC4R_t __YUV444ToRGB_8u_P3AC4R_t;
typedef fn_RGBToYUV420_8u_P3AC4R_t __RGBToYUV420_8u_P3AC4R_t;
typedef fn_RGBToYUV444_8u_P3AC4R_t __RGBToYUV444_8u_P3AC4R_t;
typedef fn_YUV420CombineToYUV444_t __YUV420CombineToYUV444_t;
typedef fn_YUV444SplitToYUV420_t __YUV444SplitToYUV420_t;
typedef fn_RGBToAVC444YUV_t __RGBToAVC444YUV_t;
typedef fn_andC_32u_t __andC_32u_t;
typedef fn_orC_32u_t __orC_32u_t;
#endif

typedef struct
{
	/* Memory-to-memory copy routines */
	fn_copy_t copy;                 /* memcpy/memmove, basically */
	fn_copy_8u_t copy_8u;           /* more strongly typed */
	fn_copy_8u_AC4r_t copy_8u_AC4r; /* pixel copy function */
	/* Memory setting routines */
	fn_set_8u_t set_8u; /* memset, basically */
	fn_set_32s_t set_32s;
	fn_set_32u_t set_32u;
	fn_zero_t zero; /* bzero or faster */
	/* Arithmetic functions */
	fn_add_16s_t add_16s;
	/* And/or */
	fn_andC_32u_t andC_32u;
	fn_orC_32u_t orC_32u;
	/* Shifts */
	fn_lShiftC_16s_t lShiftC_16s;
	fn_lShiftC_16u_t lShiftC_16u;
	fn_rShiftC_16s_t rShiftC_16s;
	fn_rShiftC_16u_t rShiftC_16u;
	fn_shiftC_16s_t shiftC_16s;
	fn_shiftC_16u_t shiftC_16u;
	/* Alpha Composition */
	fn_alphaComp_argb_t alphaComp_argb;
	/* Sign */
	fn_sign_16s_t sign_16s;
	/* Color conversions */
	fn_yCbCrToRGB_16s8u_P3AC4R_t yCbCrToRGB_16s8u_P3AC4R;
	fn_yCbCrToRGB_16s16s_P3P3_t yCbCrToRGB_16s16s_P3P3;
	fn_RGBToYCbCr_16s16s_P3P3_t RGBToYCbCr_16s16s_P3P3;
	fn_RGBToRGB_16s8u_P3AC4R_t RGBToRGB_16s8u_P3AC4R;
	fn_YCoCgToRGB_8u_AC4R_t YCoCgToRGB_8u_AC4R;
	fn_YUV420ToRGB_8u_P3AC4R_t YUV420ToRGB_8u_P3AC4R;
	fn_RGBToYUV420_8u_P3AC4R_t RGBToYUV420_8u_P3AC4R;
	fn_RGBToYUV444_8u_P3AC4R_t RGBToYUV444_8u_P3AC4R;
	fn_YUV420CombineToYUV444_t YUV420CombineToYUV444;
	fn_YUV444SplitToYUV420_t YUV444SplitToYUV420;
	fn_YUV444ToRGB_8u_P3AC4R_t YUV444ToRGB_8u_P3AC4R;
	fn_RGBToAVC444YUV_t RGBToAVC444YUV;
	fn_RGBToAVC444YUV_t RGBToAVC444YUVv2;
	/* flags */
	DWORD flags;
	primitives_uninit_t uninit;

	/** \brief Do vecotor addition, store result in both input buffers
	 *  pSrcDst1 = pSrcDst2 = pSrcDst1  + pSrcDst2
	 */
	fn_add_16s_inplace_t add_16s_inplace;         /** @since version 3.6.0 */
	fn_lShiftC_16s_inplace_t lShiftC_16s_inplace; /** @since version 3.6.0 */
	fn_copy_no_overlap_t copy_no_overlap;         /** @since version 3.6.0 */
} primitives_t;

typedef enum
{
	PRIMITIVES_PURE_SOFT, /** use generic software implementation */
	PRIMITIVES_ONLY_CPU,  /** use generic software or cpu optimized routines */
	PRIMITIVES_ONLY_GPU,  /** use opencl optimized routines */
	PRIMITIVES_AUTODETECT /** detect the best routines */
} primitive_hints;

	FREERDP_API primitives_t* primitives_get(void);
	FREERDP_API void primitives_set_hints(primitive_hints hints);
	FREERDP_API primitive_hints primitives_get_hints(void);
	FREERDP_API primitives_t* primitives_get_generic(void);
	FREERDP_API DWORD primitives_flags(primitives_t* p);
	FREERDP_API BOOL primitives_init(primitives_t* p, primitive_hints hints);
	FREERDP_API void primitives_uninit(void);

	/** @brief get a specific primitives implementation
	 *
	 *  This will try to return the primitives implementation suggested by \b type
	 *  If that does not exist or does not work on the platform any other (e.g. usually pure
	 * software) is returned
	 *
	 *  @param type the type of primitives desired.
	 *  @return A primitive implementation matching the hint closest or \b NULL in case of failure.
	 *  @since version 3.11.0
	 */
	FREERDP_API primitives_t* primitives_get_by_type(primitive_hints type);

	/** @brief stringify a \b avc444_frame_type
	 *
	 *  @param type the type to stringify
	 *  @return A string representation of \b type
	 *  @since version 3.11.0
	 */
	FREERDP_API const char* primitives_avc444_frame_type_str(avc444_frame_type type);

	/** @brief convert a hint to a string
	 *
	 *  @param hint the hint to stringify
	 *  @return the string representation of the hint
	 *  @since version 3.11.0
	 */
	FREERDP_API const char* primtives_hint_str(primitive_hints hint);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_PRIMITIVES_H */
