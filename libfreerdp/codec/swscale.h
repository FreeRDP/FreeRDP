/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * swscale Runtime Loading
 *
 * Copyright 2026 Devolutions Inc.
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

#ifndef FREERDP_LIB_CODEC_SWSCALE_H
#define FREERDP_LIB_CODEC_SWSCALE_H

#include <freerdp/config.h>

#if defined(WITH_SWSCALE)

#if defined(WITH_SWSCALE_LOADING)

#include <winpr/wtypes.h>
#include <stdint.h>

// Forward declarations to avoid requiring swscale headers at compile time
struct SwsContext;

// AVPixelFormat enum values we need (from libavutil/pixfmt.h)
// Define only what we use to avoid requiring FFmpeg headers at build time
enum AVPixelFormat
{
	AV_PIX_FMT_NONE = -1,
	AV_PIX_FMT_YUV420P = 0,
	AV_PIX_FMT_YUYV422 = 1,
	AV_PIX_FMT_RGB24 = 2,
	AV_PIX_FMT_BGR24 = 3,
	AV_PIX_FMT_YUV422P = 4,
	AV_PIX_FMT_YUV444P = 5,
	AV_PIX_FMT_YUV410P = 6,
	AV_PIX_FMT_YUV411P = 7,
	AV_PIX_FMT_GRAY8 = 8,
	AV_PIX_FMT_MONOWHITE = 9,
	AV_PIX_FMT_MONOBLACK = 10,
	AV_PIX_FMT_PAL8 = 11,
	AV_PIX_FMT_YUVJ420P = 12,
	AV_PIX_FMT_YUVJ422P = 13,
	AV_PIX_FMT_YUVJ444P = 14,
	AV_PIX_FMT_UYVY422 = 15,
	AV_PIX_FMT_UYYVYY411 = 16,
	AV_PIX_FMT_BGR8 = 17,
	AV_PIX_FMT_BGR4 = 18,
	AV_PIX_FMT_BGR4_BYTE = 19,
	AV_PIX_FMT_RGB8 = 20,
	AV_PIX_FMT_RGB4 = 21,
	AV_PIX_FMT_RGB4_BYTE = 22,
	AV_PIX_FMT_NV12 = 23,
	AV_PIX_FMT_NV21 = 24,
	AV_PIX_FMT_ARGB = 25,
	AV_PIX_FMT_RGBA = 26,
	AV_PIX_FMT_ABGR = 27,
	AV_PIX_FMT_BGRA = 28,
	AV_PIX_FMT_GRAY16BE = 29,
	AV_PIX_FMT_GRAY16LE = 30,
	AV_PIX_FMT_YUV440P = 31,
	AV_PIX_FMT_YUVJ440P = 32,
	AV_PIX_FMT_YUVA420P = 33,
	AV_PIX_FMT_RGB48BE = 34,
	AV_PIX_FMT_RGB48LE = 35,
	AV_PIX_FMT_RGB565BE = 36,
	AV_PIX_FMT_RGB565LE = 37,
	AV_PIX_FMT_RGB555BE = 38,
	AV_PIX_FMT_RGB555LE = 39,
	AV_PIX_FMT_BGR565BE = 40,
	AV_PIX_FMT_BGR565LE = 41,
	AV_PIX_FMT_BGR555BE = 42,
	AV_PIX_FMT_BGR555LE = 43,
	// Specific ones used in color.c
	AV_PIX_FMT_RGB0 = 123,
	AV_PIX_FMT_BGR0 = 124
};

// FFmpeg compatibility macros for deprecated RGB32/BGR32 formats
// These map to different formats based on endianness
#if defined(__BIG_ENDIAN__) || (defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#define AV_PIX_FMT_RGB32 AV_PIX_FMT_ARGB
#define AV_PIX_FMT_BGR32 AV_PIX_FMT_ABGR
#else
#define AV_PIX_FMT_RGB32 AV_PIX_FMT_BGRA
#define AV_PIX_FMT_BGR32 AV_PIX_FMT_RGBA
#endif

// swscale algorithm flags (from libswscale/swscale.h)
#define SWS_FAST_BILINEAR 1
#define SWS_BILINEAR 2
#define SWS_BICUBIC 4
#define SWS_X 8
#define SWS_POINT 0x10
#define SWS_AREA 0x20
#define SWS_BICUBLIN 0x40
#define SWS_GAUSS 0x80
#define SWS_SINC 0x100
#define SWS_LANCZOS 0x200
#define SWS_SPLINE 0x400

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * @brief Initialize and load the swscale library at runtime
	 * @return TRUE if swscale is available and loaded successfully, FALSE otherwise
	 */
	BOOL freerdp_swscale_init(void);

	/**
	 * @brief Check if swscale is available
	 * @return TRUE if swscale library is loaded and ready to use
	 */
	BOOL freerdp_swscale_available(void);

	/**
	 * @brief Get a swscale context (wrapper for sws_getContext)
	 */
	struct SwsContext* freerdp_sws_getContext(int srcW, int srcH, int srcFormat, int dstW, int dstH,
	                                          int dstFormat, int flags, void* srcFilter,
	                                          void* dstFilter, const double* param);

	/**
	 * @brief Scale image data (wrapper for sws_scale)
	 */
	int freerdp_sws_scale(struct SwsContext* ctx, const uint8_t* const srcSlice[],
	                      const int srcStride[], int srcSliceY, int srcSliceH, uint8_t* const dst[],
	                      const int dstStride[]);

	/**
	 * @brief Free swscale context (wrapper for sws_freeContext)
	 */
	void freerdp_sws_freeContext(struct SwsContext* ctx);

	/**
	 * @brief Initialize and load the avutil library at runtime
	 * @return TRUE if avutil is available and loaded successfully, FALSE otherwise
	 */
	BOOL freerdp_avutil_init(void);

	/**
	 * @brief Check if avutil is available
	 * @return TRUE if avutil library is loaded and ready to use
	 */
	BOOL freerdp_avutil_available(void);

	/**
	 * @brief Fill line sizes for image buffer (wrapper for av_image_fill_linesizes)
	 */
	int freerdp_av_image_fill_linesizes(int linesizes[4], int pix_fmt, int width);

	/**
	 * @brief Fill image data pointers (wrapper for av_image_fill_pointers)
	 */
	int freerdp_av_image_fill_pointers(uint8_t* data[4], int pix_fmt, int height, uint8_t* ptr,
	                                   const int linesizes[4]);

#ifdef __cplusplus
}
#endif

#endif /* WITH_SWSCALE_LOADING */

#endif /* WITH_SWSCALE */

#endif /* FREERDP_LIB_CODEC_SWSCALE_H */
