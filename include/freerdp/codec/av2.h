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

	typedef enum
	{
		FREERDP_AV2_VBR, /**< Variable Bit Rate (VBR) mode */
		FREERDP_AV2_CBR, /**< Constant Bit Rate (CBR) mode */
		FREERDP_AV2_CQ,  /**< Constrained Quality (CQ)  mode */
		FREERDP_AV2_Q    /**< Constant Quality (Q) mode */
	} FREERDP_AV2_RATECONTROL;

	typedef enum
	{
		FREERDP_AV2_CONTEXT_OPTION_PROFILE,     /**< 0: 4:2:0, 1: 4:4:4 */
		FREERDP_AV2_CONTEXT_OPTION_RATECONTROL, /**< a FREERDP_AV2_RATECONTROL value */
		FREERDP_AV2_CONTEXT_OPTION_BITRATE,     /**< target bitrate in kbit/s */
		FREERDP_AV2_CONTEXT_OPTION_USAGETYPE    /**< the encoder's usage (good quality, realtime) */
	} FREERDP_AV2_CONTEXT_OPTION;

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_av2_context_set_option(FREERDP_AV2_CONTEXT* av2,
	                                                FREERDP_AV2_CONTEXT_OPTION option,
	                                                UINT32 value);

	WINPR_ATTR_NODISCARD
	FREERDP_API UINT32 freerdp_av2_context_get_option(FREERDP_AV2_CONTEXT* av2,
	                                                  FREERDP_AV2_CONTEXT_OPTION option);

	WINPR_ATTR_NODISCARD
	FREERDP_API INT32 freerdp_av2_compress(FREERDP_AV2_CONTEXT* av2, const BYTE* pSrcData,
	                                       DWORD SrcFormat, UINT32 nSrcStep, UINT32 nSrcWidth,
	                                       UINT32 nSrcHeight, const RECTANGLE_16* regionRect,
	                                       BYTE** ppDstData, UINT32* pDstSize,
	                                       RDPGFX_H264_METABLOCK* meta);

	WINPR_ATTR_NODISCARD
	FREERDP_API INT32 freerdp_av2_decompress(FREERDP_AV2_CONTEXT* av2, const BYTE* pSrcData,
	                                         UINT32 SrcSize, BYTE* pDstData, DWORD DstFormat,
	                                         UINT32 nDstStep, UINT32 nDstWidth, UINT32 nDstHeight,
	                                         const RECTANGLE_16* regionRects, UINT32 numRegionRect);

	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_av2_context_reset(FREERDP_AV2_CONTEXT* av2, UINT32 width,
	                                           UINT32 height);

	FREERDP_API void freerdp_av2_context_free(FREERDP_AV2_CONTEXT* av2);

	/** @brief create a new encoder/decoder instance for AV2 streams
	 *
	 *  @param Compressor \b TRUE if an encoder should be created, \b FALSE for a decoder
	 *
	 *  @return An allocated instance or \b nullptr if failed.
	 *  @since version 3.33.0
	 */
	WINPR_ATTR_MALLOC(freerdp_av2_context_free, 1)
	FREERDP_API FREERDP_AV2_CONTEXT* freerdp_av2_context_new(BOOL Compressor);

#ifdef __cplusplus
}
#endif
