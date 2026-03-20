/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Video Codec API
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

#ifndef FREERDP_CODEC_VIDEO_H
#define FREERDP_CODEC_VIDEO_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <winpr/stream.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct s_FREERDP_VIDEO_CONTEXT FREERDP_VIDEO_CONTEXT;

	/**
	 * @brief Video format enumeration
	 *
	 * Abstract video format identifiers independent of underlying codec implementation.
	 *
	 * @since version 3.24.0
	 */
	typedef enum
	{
		FREERDP_VIDEO_FORMAT_NONE = 0,

		/* Compressed formats */
		FREERDP_VIDEO_FORMAT_MJPEG,
		FREERDP_VIDEO_FORMAT_H264,

		/* Planar YUV formats */
		FREERDP_VIDEO_FORMAT_YUV420P, /* I420 */
		FREERDP_VIDEO_FORMAT_NV12,

		/* Packed YUV formats */
		FREERDP_VIDEO_FORMAT_YUYV422, /* YUY2 */

		/* RGB formats */
		FREERDP_VIDEO_FORMAT_RGB24,
		FREERDP_VIDEO_FORMAT_RGB32, /* Platform-specific BGRA/ARGB */
	} FREERDP_VIDEO_FORMAT;

	/** @brief Convert a \ref FREERDP_VIDEO_FORMAT to a string representation.
	 *
	 *  @return A string representation of the format or \b FREERDP_VIDEO_FORMAT_UNKNOWN
	 *  @since version 3.25.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API const char* freerdp_video_format_string(UINT32 format);

	/**
	 * @brief Free a video processing context
	 *
	 * @param context Context to free
	 * @since version 3.24.0
	 */
	FREERDP_API void freerdp_video_context_free(FREERDP_VIDEO_CONTEXT* context);

	/**
	 * @brief Create a new video processing context
	 *
	 * @param width Video frame width
	 * @param height Video frame height
	 * @return New context or nullptr on failure
	 * @since version 3.24.0
	 */
	WINPR_ATTR_MALLOC(freerdp_video_context_free, 1)
	FREERDP_API FREERDP_VIDEO_CONTEXT* freerdp_video_context_new(UINT32 width, UINT32 height);

	/**
	 * @brief Check if video processing is available
	 *
	 * @return TRUE if video codecs are available (FFmpeg loaded)
	 * @since version 3.24.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_video_available(void);

	/**
	 * @brief Check if conversion between two formats is supported
	 *
	 * @param srcFormat Source video format
	 * @param dstFormat Destination video format
	 * @return TRUE if conversion is supported, FALSE otherwise
	 * @since version 3.24.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_video_conversion_supported(FREERDP_VIDEO_FORMAT srcFormat,
	                                                    FREERDP_VIDEO_FORMAT dstFormat);

	/**
	 * @brief Configure H.264 encoder settings
	 *
	 * Must be called before the first encoding operation. Should be called again
	 * to reconfigure encoding settings (will reset encoder only if settings differ).
	 *
	 * @param context Video context
	 * @param width Frame width in pixels
	 * @param height Frame height in pixels
	 * @param framerate Target framerate (fps)
	 * @param bitrate Target bitrate (bps), 0 for auto-calculate based on height
	 * @param usageType H264 usage type (H264_CAMERA_VIDEO_REAL_TIME, etc.)
	 * @return TRUE on success, FALSE on failure
	 *
	 * @example
	 * FREERDP_VIDEO_CONTEXT* ctx = freerdp_video_context_new(1920, 1080);
	 * freerdp_video_context_reconfigure(ctx, 1920, 1080, 30, 0,
	 *                                      H264_CAMERA_VIDEO_REAL_TIME);
	 * @since version 3.24.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_video_context_reconfigure(FREERDP_VIDEO_CONTEXT* context, UINT32 width,
	                                                   UINT32 height, UINT32 framerate,
	                                                   UINT32 bitrate, UINT32 usageType);

	/**
	 * @brief Convert video sample from one format to another
	 *
	 * High-level function that handles all video encoding/decoding/conversion in one call:
	 * - Format pass-through (when srcFormat == dstFormat)
	 * - Pixel format conversion (e.g., YUY2 → YUV420P)
	 * - MJPEG decoding
	 * - H.264 encoding (including intermediate YUV conversion)
	 *
	 * For H.264 output, encoder must be configured first using
	 * freerdp_video_context_reconfigure().
	 *
	 * @param context Video context (manages swscale and H.264 encoders)
	 * @param srcFormat Source video format
	 * @param srcSampleData Pointer to source sample data (serialized buffer)
	 * @param srcSampleLength Length of source sample in bytes
	 * @param dstFormat Destination video format
	 * @param output Output stream to write result to (will be resized automatically)
	 * @return TRUE on success, FALSE on failure
	 *
	 * @example
	 * // Setup (once per stream)
	 * FREERDP_VIDEO_CONTEXT* ctx = freerdp_video_context_new(1920, 1080);
	 * freerdp_video_context_reconfigure(ctx, 1920, 1080, 30, 0,
	 *                                      H264_CAMERA_VIDEO_REAL_TIME);
	 *
	 * // Convert MJPEG sample to H.264 (per frame)
	 * wStream* output = Stream_New(nullptr, 1024 * 1024);
	 * if (freerdp_video_sample_convert(ctx, FREERDP_VIDEO_FORMAT_MJPEG,
	 *                                  mjpegData, mjpegSize,
	 *                                  FREERDP_VIDEO_FORMAT_H264, output))
	 * {
	 *     // Send Stream_Buffer(output), Stream_GetPosition(output) bytes
	 * }
	 * Stream_Free(output, TRUE);
	 *
	 * // Cleanup
	 * freerdp_video_context_free(ctx);
	 * @since version 3.24.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_video_sample_convert(FREERDP_VIDEO_CONTEXT* context,
	                                              FREERDP_VIDEO_FORMAT srcFormat,
	                                              const void* srcSampleData, size_t srcSampleLength,
	                                              FREERDP_VIDEO_FORMAT dstFormat, wStream* output);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_VIDEO_H */
