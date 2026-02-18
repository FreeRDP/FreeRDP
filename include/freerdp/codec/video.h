/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Codec Wrappers
 *
 * Copyright 2025 Devolutions Inc.
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

#ifndef FREERDP_CODEC_VIDEO_H
#define FREERDP_CODEC_VIDEO_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct s_FREERDP_VIDEO_CONTEXT FREERDP_VIDEO_CONTEXT;

	/**
	 * @brief Video pixel format enumeration
	 * Maps to AVPixelFormat but provides independence from FFmpeg headers
	 */
	typedef enum
	{
		FREERDP_VIDEO_FORMAT_NONE = -1,
		FREERDP_VIDEO_FORMAT_YUV420P = 0,
		FREERDP_VIDEO_FORMAT_YUYV422 = 1,
		FREERDP_VIDEO_FORMAT_RGB24 = 2,
		FREERDP_VIDEO_FORMAT_BGR24 = 3,
		FREERDP_VIDEO_FORMAT_YUV422P = 4,
		FREERDP_VIDEO_FORMAT_YUV444P = 5,
		FREERDP_VIDEO_FORMAT_NV12 = 23,
		FREERDP_VIDEO_FORMAT_NV21 = 24,
		FREERDP_VIDEO_FORMAT_ARGB = 25,
		FREERDP_VIDEO_FORMAT_RGBA = 26,
		FREERDP_VIDEO_FORMAT_ABGR = 27,
		FREERDP_VIDEO_FORMAT_BGRA = 28,
		FREERDP_VIDEO_FORMAT_YUV411P = 7,
		FREERDP_VIDEO_FORMAT_YUVJ420P = 12,
		FREERDP_VIDEO_FORMAT_YUVJ422P = 13,
		FREERDP_VIDEO_FORMAT_YUVJ444P = 14,
		FREERDP_VIDEO_FORMAT_YUVJ440P = 32,
		FREERDP_VIDEO_FORMAT_YUVJ411P = 33,
		FREERDP_VIDEO_FORMAT_YUV440P = 31,
		FREERDP_VIDEO_FORMAT_RGB32 = 100 /* Will map to platform-specific BGRA/ARGB */
	} FREERDP_VIDEO_FORMAT;

	/**
	 * @brief Video context options
	 */
	typedef enum
	{
		FREERDP_VIDEO_CONTEXT_OPTION_DECODER_TYPE, /** MJPEG decoder type */
		FREERDP_VIDEO_CONTEXT_OPTION_SCALING_QUALITY /** Scaling algorithm quality */
	} FREERDP_VIDEO_CONTEXT_OPTION;

	/**
	 * @brief Video feature flags for capability checking
	 */
	typedef enum
	{
		FREERDP_VIDEO_FEATURE_NONE = 0,
		FREERDP_VIDEO_FEATURE_MJPEG_DECODE = (1 << 0), /** MJPEG decoding support */
		FREERDP_VIDEO_FEATURE_H264_ENCODE = (1 << 1), /** H264 encoding support */
		FREERDP_VIDEO_FEATURE_H264_DECODE = (1 << 2) /** H264 decoding support */
	} FREERDP_VIDEO_FEATURE;

	/**
	 * @brief Free a video processing context
	 *
	 * @param context Context to free
	 */
	FREERDP_API void freerdp_video_context_free(FREERDP_VIDEO_CONTEXT* context);

	/**
	 * @brief Create a new video processing context
	 *
	 * @param width Video frame width
	 * @param height Video frame height
	 * @return New context or NULL on failure
	 */
	WINPR_ATTR_MALLOC(freerdp_video_context_free, 1)
	FREERDP_API FREERDP_VIDEO_CONTEXT* freerdp_video_context_new(UINT32 width, UINT32 height);

	/**
	 * @brief Reset context for new dimensions
	 *
	 * @param context Video context
	 * @param width New width
	 * @param height New height
	 * @return TRUE on success
	 */
	FREERDP_API BOOL freerdp_video_context_reset(FREERDP_VIDEO_CONTEXT* context, UINT32 width,
	                                             UINT32 height);

	/**
	 * @brief Decode MJPEG frame to raw pixels
	 *
	 * @param context Video context
	 * @param srcData MJPEG compressed data
	 * @param srcSize Size of compressed data
	 * @param dstData Output planes (array of 4 pointers)
	 * @param dstLineSize Output line sizes (array of 4 ints)
	 * @param dstFormat Output pixel format
	 * @return TRUE on success
	 */
	FREERDP_API BOOL freerdp_video_decode_mjpeg(FREERDP_VIDEO_CONTEXT* context,
	                                            const BYTE* srcData, size_t srcSize,
	                                            BYTE* dstData[4], int dstLineSize[4],
	                                            FREERDP_VIDEO_FORMAT* dstFormat);

	/**
	 * @brief Convert pixel format to YUV for encoding
	 *
	 * This function handles color space conversion from various input formats
	 * to YUV420P or NV12 for video encoding. It wraps libswscale functionality.
	 *
	 * @param context Video context (can be NULL for stateless operation)
	 * @param srcData Source image planes (array of 4 pointers)
	 * @param srcLineSize Source line sizes (array of 4 ints)
	 * @param srcFormat Source pixel format
	 * @param dstData Destination YUV planes (array of 3 pointers)
	 * @param dstLineSize Destination line sizes (array of 3 ints)
	 * @param dstFormat Destination YUV format (YUV420P or NV12)
	 * @param width Frame width
	 * @param height Frame height
	 * @return TRUE on success
	 */
	FREERDP_API BOOL freerdp_video_convert_to_yuv(FREERDP_VIDEO_CONTEXT* context,
	                                              const BYTE* srcData[4], const int srcLineSize[4],
	                                              FREERDP_VIDEO_FORMAT srcFormat, BYTE* dstData[3],
	                                              const int dstLineSize[3],
	                                              FREERDP_VIDEO_FORMAT dstFormat, UINT32 width,
	                                              UINT32 height);

	/**
	 * @brief Fill image plane pointers and line sizes
	 *
	 * Helper function to initialize plane pointers and line sizes for a given format
	 *
	 * @param data Output plane pointers (array of 4)
	 * @param lineSize Output line sizes (array of 4)
	 * @param format Pixel format
	 * @param width Image width
	 * @param height Image height
	 * @param buffer Source buffer
	 * @return TRUE on success
	 */
	FREERDP_API BOOL freerdp_video_fill_plane_info(BYTE* data[4], int lineSize[4],
	                                               FREERDP_VIDEO_FORMAT format, UINT32 width,
	                                               UINT32 height, const BYTE* buffer);

	/**
	 * @brief Check if video processing is available
	 *
	 * @return TRUE if video codecs are available (FFmpeg loaded)
	 */
	FREERDP_API BOOL freerdp_video_available(void);

	/**
	 * @brief Check if specific video features are available
	 *
	 * @param features Feature flags to check (can combine with bitwise OR)
	 * @return TRUE if all requested features are available
	 */
	FREERDP_API BOOL freerdp_video_feature_available(UINT32 features);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_VIDEO_H */
