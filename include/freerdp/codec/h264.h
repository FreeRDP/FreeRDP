/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * H.264 Bitmap Compression
 *
 * Copyright 2014 Mike McDonald <Mike.McDonald@software.dell.com>
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

#ifndef FREERDP_CODEC_H264_H
#define FREERDP_CODEC_H264_H

#include <winpr/wlog.h>

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/rdpgfx.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct S_H264_CONTEXT_SUBSYSTEM H264_CONTEXT_SUBSYSTEM;
	typedef struct S_H264_CONTEXT H264_CONTEXT;
	typedef struct S_YUV_CONTEXT YUV_CONTEXT;

	typedef enum
	{
		H264_RATECONTROL_VBR = 0,
		H264_RATECONTROL_CQP
	} H264_RATECONTROL_MODE;

	/**
	 * @brief The usage types for H264 encoding
	 * @since version 3.6.0
	 */
	typedef enum
	{
		H264_SCREEN_CONTENT_REAL_TIME = 0,
		H264_SCREEN_CONTENT_NON_REAL_TIME,
		H264_CAMERA_VIDEO_REAL_TIME,
		H264_CAMERA_VIDEO_NON_REAL_TIME,

	} H264_USAGETYPE;

	typedef enum
	{
		H264_CONTEXT_OPTION_RATECONTROL,
		H264_CONTEXT_OPTION_BITRATE,
		H264_CONTEXT_OPTION_FRAMERATE,
		H264_CONTEXT_OPTION_QP,
		H264_CONTEXT_OPTION_USAGETYPE, /** @since version 3.6.0 */
	} H264_CONTEXT_OPTION;

	FREERDP_API void free_h264_metablock(RDPGFX_H264_METABLOCK* meta);

	FREERDP_API BOOL h264_context_set_option(H264_CONTEXT* h264, H264_CONTEXT_OPTION option,
	                                         UINT32 value);
	FREERDP_API UINT32 h264_context_get_option(H264_CONTEXT* h264, H264_CONTEXT_OPTION option);

	FREERDP_API INT32 avc420_compress(H264_CONTEXT* h264, const BYTE* pSrcData, DWORD SrcFormat,
	                                  UINT32 nSrcStep, UINT32 nSrcWidth, UINT32 nSrcHeight,
	                                  const RECTANGLE_16* regionRect, BYTE** ppDstData,
	                                  UINT32* pDstSize, RDPGFX_H264_METABLOCK* meta);

	/** @brief API for user to fill YUV I420 buffer before encoding
	 *
	 *  @param h264 The h264 context to query
	 *  @param nSrcStride The size of a line in bytes of the source image
	 *  @param nSrcWidth The width of the source image in pixels
	 *  @param nSrcHeight The height of the source image
	 *  @param YUVData A pointer to hold the current YUV buffers
	 *  @param stride A pointer to hold the byte length of a line in the YUV buffers
	 *  @return \b >= 0 for success, \b <0 for an error
	 *  @since version 3.6.0
	 */
	FREERDP_API INT32 h264_get_yuv_buffer(H264_CONTEXT* h264, UINT32 nSrcStride, UINT32 nSrcWidth,
	                                      UINT32 nSrcHeight, BYTE* YUVData[3], UINT32 stride[3]);

	/**
	 * @brief Compress currently filled image data to H264 stream
	 *
	 * @param h264 The H264 context to use for compression
	 * @param ppDstData A pointer that will hold the allocated result buffer
	 * @param pDstSize A pointer for the destination buffer size in bytes
	 * @return \b >= 0 for success, \b <0 for an error
	 * @since version 3.6.0
	 */
	FREERDP_API INT32 h264_compress(H264_CONTEXT* h264, BYTE** ppDstData, UINT32* pDstSize);

	FREERDP_API INT32 avc420_decompress(H264_CONTEXT* h264, const BYTE* pSrcData, UINT32 SrcSize,
	                                    BYTE* pDstData, DWORD DstFormat, UINT32 nDstStep,
	                                    UINT32 nDstWidth, UINT32 nDstHeight,
	                                    const RECTANGLE_16* regionRects, UINT32 numRegionRect);

	FREERDP_API INT32 avc444_compress(H264_CONTEXT* h264, const BYTE* pSrcData, DWORD SrcFormat,
	                                  UINT32 nSrcStep, UINT32 nSrcWidth, UINT32 nSrcHeight,
	                                  BYTE version, const RECTANGLE_16* regionRect, BYTE* op,
	                                  BYTE** pDstData, UINT32* pDstSize, BYTE** pAuxDstData,
	                                  UINT32* pAuxDstSize, RDPGFX_H264_METABLOCK* meta,
	                                  RDPGFX_H264_METABLOCK* auxMeta);

	FREERDP_API INT32 avc444_decompress(H264_CONTEXT* h264, BYTE op,
	                                    const RECTANGLE_16* regionRects, UINT32 numRegionRect,
	                                    const BYTE* pSrcData, UINT32 SrcSize,
	                                    const RECTANGLE_16* auxRegionRects, UINT32 numAuxRegionRect,
	                                    const BYTE* pAuxSrcData, UINT32 AuxSrcSize, BYTE* pDstData,
	                                    DWORD DstFormat, UINT32 nDstStep, UINT32 nDstWidth,
	                                    UINT32 nDstHeight, UINT32 codecId);

	FREERDP_API BOOL h264_context_reset(H264_CONTEXT* h264, UINT32 width, UINT32 height);

	FREERDP_API void h264_context_free(H264_CONTEXT* h264);

	WINPR_ATTR_MALLOC(h264_context_free, 1)
	FREERDP_API H264_CONTEXT* h264_context_new(BOOL Compressor);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_H264_H */
