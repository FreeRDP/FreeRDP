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
	 * These values do not map directly to FFmpeg constants - conversion happens internally.
	 */
	typedef enum
	{
		FREERDP_VIDEO_FORMAT_NONE = 0,

		/* Compressed formats */
		FREERDP_VIDEO_FORMAT_MJPEG,
		FREERDP_VIDEO_FORMAT_H264,

		/* Planar YUV formats */
		FREERDP_VIDEO_FORMAT_YUV420P, /* I420 */
		FREERDP_VIDEO_FORMAT_YUV422P,
		FREERDP_VIDEO_FORMAT_YUV444P,
		FREERDP_VIDEO_FORMAT_YUV411P,
		FREERDP_VIDEO_FORMAT_YUV440P,
		FREERDP_VIDEO_FORMAT_NV12,
		FREERDP_VIDEO_FORMAT_NV21,

		/* Packed YUV formats */
		FREERDP_VIDEO_FORMAT_YUYV422, /* YUY2 */
		FREERDP_VIDEO_FORMAT_UYVY422,

		/* RGB formats */
		FREERDP_VIDEO_FORMAT_RGB24,
		FREERDP_VIDEO_FORMAT_BGR24,
		FREERDP_VIDEO_FORMAT_RGBA,
		FREERDP_VIDEO_FORMAT_BGRA,
		FREERDP_VIDEO_FORMAT_ARGB,
		FREERDP_VIDEO_FORMAT_ABGR,
		FREERDP_VIDEO_FORMAT_RGB32, /* Platform-specific BGRA/ARGB */

		/* JPEG full-range YUV formats */
		FREERDP_VIDEO_FORMAT_YUVJ420P,
		FREERDP_VIDEO_FORMAT_YUVJ422P,
		FREERDP_VIDEO_FORMAT_YUVJ444P,
		FREERDP_VIDEO_FORMAT_YUVJ440P,
		FREERDP_VIDEO_FORMAT_YUVJ411P,
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
	 * @brief Video frame data container
	 *
	 * Flexible structure that can hold both compressed and raw video data.
	 * The format field determines which union member is valid:
	 * - Compressed formats (MJPEG, H264) use the compressed member
	 * - Raw/planar formats use the raw member
	 */
	typedef struct
	{
		FREERDP_VIDEO_FORMAT format; /** Format of the data */
		UINT32 width;                 /** Frame width in pixels */
		UINT32 height;                /** Frame height in pixels */

		union
		{
			/** For compressed formats (MJPEG, H264) */
			struct
			{
				BYTE* data;  /** Compressed bitstream */
				size_t size; /** Size in bytes */
			} compressed;

			/** For raw/planar formats */
			struct
			{
				BYTE* data[4];     /** Plane pointers (Y, U, V, A) */
				int linesize[4];   /** Stride for each plane */
			} raw;
		};
	} FREERDP_VIDEO_FRAME;

	/**
	 * @brief Initialize a frame structure for compressed data
	 *
	 * @param frame Frame structure to initialize
	 * @param format Video format (must be MJPEG or H264)
	 * @param data Compressed bitstream data
	 * @param size Size of compressed data in bytes
	 * @param width Frame width in pixels
	 * @param height Frame height in pixels
	 */
	FREERDP_API void freerdp_video_frame_init_compressed(FREERDP_VIDEO_FRAME* frame,
	                                                     FREERDP_VIDEO_FORMAT format, BYTE* data,
	                                                     size_t size, UINT32 width, UINT32 height);

	/**
	 * @brief Initialize a frame structure for raw/planar data
	 *
	 * @param frame Frame structure to initialize
	 * @param format Video format (raw/planar format)
	 * @param data Array of plane pointers
	 * @param linesize Array of stride values for each plane
	 * @param width Frame width in pixels
	 * @param height Frame height in pixels
	 */
	FREERDP_API void freerdp_video_frame_init_raw(FREERDP_VIDEO_FRAME* frame,
	                                              FREERDP_VIDEO_FORMAT format, BYTE* data[4],
	                                              int linesize[4], UINT32 width, UINT32 height);

	/**
	 * @brief Initialize a frame structure for packed data (convenience)
	 *
	 * For formats like YUY2, RGB24 where data is in a single contiguous buffer.
	 * This is a convenience wrapper around freerdp_video_frame_init_raw().
	 *
	 * @param frame Frame structure to initialize
	 * @param format Video format (packed format like YUYV422, RGB24, etc.)
	 * @param buffer Pointer to packed pixel data
	 * @param width Frame width in pixels
	 * @param height Frame height in pixels
	 */
	FREERDP_API void freerdp_video_frame_init_packed(FREERDP_VIDEO_FRAME* frame,
	                                                 FREERDP_VIDEO_FORMAT format, BYTE* buffer,
	                                                 UINT32 width, UINT32 height);

	/**
	 * @brief Convert video data between formats
	 *
	 * Unified function that handles:
	 * - Decoding compressed formats (MJPEG, H264)
	 * - Pixel format conversion (YUY2 → YUV420P, etc.)
	 * - Encoding to compressed formats
	 *
	 * The function automatically detects required operations based on src/dst formats.
	 *
	 * @param context Video context (reused for efficiency, can be NULL for one-shot)
	 * @param src Source frame
	 * @param dst Destination frame (data buffers must be pre-allocated by caller)
	 * @return TRUE on success, FALSE on failure
	 *
	 * @note For compressed output, caller must provide dst->compressed.data buffer
	 *       with sufficient size. Use freerdp_video_estimate_size() to determine size.
	 *
	 * @example
	 * // MJPEG → YUV420P conversion
	 * FREERDP_VIDEO_FRAME src, dst;
	 * BYTE* dstPlanes[4] = {yBuffer, uBuffer, vBuffer, NULL};
	 * int dstLinesize[4] = {width, width/2, width/2, 0};
	 *
	 * freerdp_video_frame_init_compressed(&src, FREERDP_VIDEO_FORMAT_MJPEG,
	 *                                     mjpegData, mjpegSize, width, height);
	 * freerdp_video_frame_init_raw(&dst, FREERDP_VIDEO_FORMAT_YUV420P,
	 *                              dstPlanes, dstLinesize, width, height);
	 *
	 * if (freerdp_video_convert(ctx, &src, &dst))
	 *     // Conversion successful
	 */
	FREERDP_API BOOL freerdp_video_convert(FREERDP_VIDEO_CONTEXT* context,
	                                       const FREERDP_VIDEO_FRAME* src,
	                                       FREERDP_VIDEO_FRAME* dst);

	/**
	 * @brief Check if a specific format conversion is supported
	 *
	 * Queries whether the video subsystem can convert from srcFormat to dstFormat.
	 * This allows dynamic capability checking based on compiled backends.
	 *
	 * @param srcFormat Source video format
	 * @param dstFormat Destination video format
	 * @return TRUE if conversion is supported, FALSE otherwise
	 *
	 * @example
	 * if (freerdp_video_conversion_supported(FREERDP_VIDEO_FORMAT_MJPEG,
	 *                                        FREERDP_VIDEO_FORMAT_YUV420P))
	 * {
	 *     // Can decode MJPEG to YUV420P
	 * }
	 */
	FREERDP_API BOOL freerdp_video_conversion_supported(FREERDP_VIDEO_FORMAT srcFormat,
	                                                    FREERDP_VIDEO_FORMAT dstFormat);

	/**
	 * @brief Configure H.264 encoder settings
	 *
	 * Must be called before first H.264 encoding operation. Can be called again
	 * to reconfigure (will reset encoder).
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
	 * // Configure for camera streaming at 30fps with auto bitrate
	 * freerdp_video_context_configure_h264(ctx, 1920, 1080, 30, 0,
	 *                                      H264_CAMERA_VIDEO_REAL_TIME);
	 */
	FREERDP_API BOOL freerdp_video_context_configure_h264(FREERDP_VIDEO_CONTEXT* context,
	                                                      UINT32 width, UINT32 height,
	                                                      UINT32 framerate, UINT32 bitrate,
	                                                      UINT32 usageType);

	/**
	 * @brief Convert video sample from one format to another (high-level API)
	 *
	 * High-level function that handles all video encoding/decoding/conversion in one call:
	 * - Format pass-through (when srcFormat == dstFormat)
	 * - Pixel format conversion (e.g., YUY2 → YUV420P)
	 * - MJPEG decoding
	 * - H.264 encoding (including intermediate YUV conversion)
	 *
	 * For H.264 output, encoder must be configured first using
	 * freerdp_video_context_configure_h264().
	 *
	 * @param context Video context (manages swscale and H.264 encoders)
	 * @param srcFormat Source video format
	 * @param srcSampleData Pointer to source sample data (serialized buffer)
	 * @param srcSampleLength Length of source sample in bytes
	 * @param width Frame width in pixels
	 * @param height Frame height in pixels
	 * @param dstFormat Destination video format
	 * @param output Output stream to write result to (will be resized automatically)
	 * @return TRUE on success, FALSE on failure
	 *
	 * @example
	 * // Setup (once per stream)
	 * FREERDP_VIDEO_CONTEXT* ctx = freerdp_video_context_new(1920, 1080);
	 * freerdp_video_context_configure_h264(ctx, 1920, 1080, 30, 0,
	 *                                      H264_CAMERA_VIDEO_REAL_TIME);
	 *
	 * // Convert MJPEG sample to H.264 (per frame)
	 * wStream* output = Stream_New(NULL, 1024 * 1024);
	 * if (freerdp_video_sample_convert(ctx, FREERDP_VIDEO_FORMAT_MJPEG,
	 *                                  mjpegData, mjpegSize, 1920, 1080,
	 *                                  FREERDP_VIDEO_FORMAT_H264, output))
	 * {
	 *     // Send Stream_Buffer(output), Stream_GetPosition(output) bytes
	 * }
	 * Stream_Free(output, TRUE);
	 *
	 * // Cleanup
	 * freerdp_video_context_free(ctx);
	 */
	FREERDP_API BOOL freerdp_video_sample_convert(FREERDP_VIDEO_CONTEXT* context,
	                                              FREERDP_VIDEO_FORMAT srcFormat,
	                                              const void* srcSampleData,
	                                              size_t srcSampleLength, UINT32 width,
	                                              UINT32 height, FREERDP_VIDEO_FORMAT dstFormat,
	                                              wStream* output);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_VIDEO_H */
