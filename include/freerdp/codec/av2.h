/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * AV2 Bitmap Compression
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

#pragma once

#include <winpr/wlog.h>

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/channels/rdpgfx.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct S_FREERDP_AV2_CONTEXT FREERDP_AV2_CONTEXT;

	/** @since version 3.34,0 */
	typedef enum
	{
		FREERDP_AV2_VBR, /**< Variable Bit Rate (VBR) mode */
		FREERDP_AV2_CBR, /**< Constant Bit Rate (CBR) mode */
		FREERDP_AV2_CQ,  /**< Constrained Quality (CQ)  mode */
		FREERDP_AV2_Q    /**< Constant Quality (Q) mode */
	} FREERDP_AV2_RATECONTROL;

	/** @since version 3.34,0 */
	typedef enum
	{
		FREERDP_AV2_CONTEXT_OPTION_PROFILE,     /**< 0: 4:2:0, 1: 4:4:4 */
		FREERDP_AV2_CONTEXT_OPTION_RATECONTROL, /**< a FREERDP_AV2_RATECONTROL value */
		FREERDP_AV2_CONTEXT_OPTION_BITRATE,     /**< target bitrate in kbit/s */
		FREERDP_AV2_CONTEXT_OPTION_USAGETYPE,   /**< the encoder's usage (good quality, realtime) */
		FREERDP_AV2_CONTEXT_OPTION_QP,          /**< the quantizer of FREERDP_AV2_Q, 1 to 255
		                                             (lower is better); 0 leaves libavm's */
		FREERDP_AV2_CONTEXT_OPTION_THREADS,     /**< encoder threads, 0 for one */
		FREERDP_AV2_CONTEXT_OPTION_SPEED,       /**< libavm's cpu-used, 0 (slowest) to 10;
		                                             UINT32_MAX for 9 (realtime) or 6 */
		FREERDP_AV2_CONTEXT_OPTION_TILE_COLUMNS, /**< log2 of the tile columns, 0 to 6 */
		FREERDP_AV2_CONTEXT_OPTION_DPB_SIZE      /**< reference frames kept, 1 to 16; 0 leaves
		                                              libavm's 8 */
	} FREERDP_AV2_CONTEXT_OPTION;

	/** @brief Set a codec option
	 *
	 * @param av2 A pointer to a AV2 context to use, must not be NULL
	 * @param option The option to set
	 * @param value The value to set
	 *
	 * @return TRUE for success, FALSE otherwise
	 * @since version 3.34.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_av2_context_set_option(FREERDP_AV2_CONTEXT* av2,
	                                                FREERDP_AV2_CONTEXT_OPTION option,
	                                                UINT32 value);

	/** @brief Get a codec option
	 *
	 * @param av2 A pointer to a AV2 context to use, must not be NULL
	 * @param option The option to query
	 * @return the option value
	 * @since version 3.34.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API UINT32 freerdp_av2_context_get_option(FREERDP_AV2_CONTEXT* av2,
	                                                  FREERDP_AV2_CONTEXT_OPTION option);

	/** @brief Compress a framebuffer image with AV2
	 *
	 * @param av2 A pointer to a AV2 context to use, must not be NULL
	 * @param pSrcData A pointer to the source data to compress
	 * @param SrcFormat The pixel format of \ref pSrcData
	 * @param nSrcStep The stride in bytes of \ref pSrcData
	 * @param nSrcWidth The width in pixels of \ref pSrcData
	 * @param nSrcHeight The height in pixels of \ref pSrcData
	 * @param regionRect An optional rectangle limiting the area to encode
	 * @param ppDstData A pointer that will be set to the encoded data, must not be NULL
	 * @param pDstSize A pointer that will be set to the encoded data length, must not be NULL
	 * @param meta A pointer to the H264 metablock that will be sent, must not be NULL
	 *
	 * @return 0 for success, a negative value on error
	 * @since version 3.34.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API INT32 freerdp_av2_compress(FREERDP_AV2_CONTEXT* av2, const BYTE* pSrcData,
	                                       DWORD SrcFormat, UINT32 nSrcStep, UINT32 nSrcWidth,
	                                       UINT32 nSrcHeight, const RECTANGLE_16* regionRect,
	                                       BYTE** ppDstData, UINT32* pDstSize,
	                                       RDPGFX_H264_METABLOCK* meta);

	/** @brief Decompress AV2 data to a framebuffer image
	 *
	 * @param av2 A pointer to a AV2 context to use, must not be NULL
	 * @param pSrcData A pointer to the compressed input data to decode
	 * @param SrcSize The size in bytes of \ref pSrcData
	 * @param pDstData A pointer to the destination data after decompress
	 * @param DstFormat The pixel format of \ref pDstData
	 * @param nDstStep The stride in bytes of \ref pDstData
	 * @param nDstWidth The width in pixels of \ref pDstData
	 * @param nDstHeight The height in pixels of \ref pDstData
	 * @param regionRects An optional rectangle array limiting the area to decode
	 * @param numRegionRect The number of rectangles found in \ref regionRects
	 *
	 * @return 0 for success, a negative value on error
	 * @since version 3.34.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API INT32 freerdp_av2_decompress(FREERDP_AV2_CONTEXT* av2, const BYTE* pSrcData,
	                                         UINT32 SrcSize, BYTE* pDstData, DWORD DstFormat,
	                                         UINT32 nDstStep, UINT32 nDstWidth, UINT32 nDstHeight,
	                                         const RECTANGLE_16* regionRects, UINT32 numRegionRect);

	/** @brief Reset a AV2 codec context
	 *
	 * @param av2 A pointer to a AV2 context to use, must not be NULL
	 * @since version 3.34.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_av2_context_reset(FREERDP_AV2_CONTEXT* av2, UINT32 width,
	                                           UINT32 height);

	/** @brief free a AV2 codec context
	 *
	 * @param av2 A pointer to a AV2 context to use, may be NULL
	 * @since version 3.34.0
	 */
	FREERDP_API void freerdp_av2_context_free(FREERDP_AV2_CONTEXT* av2);

	/** @brief create a new encoder/decoder instance for AV2 streams
	 *
	 *  @param Compressor \b TRUE if an encoder should be created, \b FALSE for a decoder
	 *
	 *  @return An allocated instance or \b nullptr if failed.
	 *  @since version 3.34.0
	 */
	WINPR_ATTR_MALLOC(freerdp_av2_context_free, 1)
	FREERDP_API FREERDP_AV2_CONTEXT* freerdp_av2_context_new(BOOL Compressor);

#ifdef __cplusplus
}
#endif
