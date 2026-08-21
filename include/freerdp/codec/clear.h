/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ClearCodec Bitmap Compression
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CODEC_CLEAR_H
#define FREERDP_CODEC_CLEAR_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/config.h>

#include <freerdp/codec/nsc.h>
#include <freerdp/codec/color.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef struct S_CLEAR_CONTEXT CLEAR_CONTEXT;

	/** @brief compress an image rectangle to CLEAR codec data (CLEARCODEC_BITMAP_STREAM)
	 *
	 *  @param clear The context to use for compression, must not be \b nullptr, must have been
	 *  created with \ref Compressor = TRUE
	 *  @param pSrcData A pointer to the source image data, must not be \b nullptr
	 *  @param SrcFormat The pixel format of the source image data
	 *  @param nSrcStep The size in bytes of a source image line, or \b 0 to derive it from
	 *  nWidth and SrcFormat
	 *  @param nWidth The width in pixels of the rectangle to compress
	 *  @param nHeight The height in lines of the rectangle to compress
	 *  @param ppDstData A pointer that receives the compressed data. The returned buffer is
	 *  owned by \b clear and is only valid until the next call to \ref clear_compress or to
	 *  \ref clear_context_free; the caller must not free it.
	 *  @param pDstSize A pointer that receives the size in bytes of the compressed data
	 *
	 *  @return \b TRUE for success, \b FALSE otherwise
	 *  @since version 3.34.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL clear_compress(CLEAR_CONTEXT* WINPR_RESTRICT clear,
	                                const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcFormat,
	                                UINT32 nSrcStep, UINT32 nWidth, UINT32 nHeight,
	                                BYTE** WINPR_RESTRICT ppDstData,
	                                UINT32* WINPR_RESTRICT pDstSize);

	/** @brief decompress clear codec data
	 *
	 *  @param clear The context to use for decompression, must not be \b nullptr, must have been
	 * created with \ref Compressor = FALSE
	 *  @param pSrcData A pointer to the data to feed to the decoder
	 *  @param SrcSize The size in bytes of the source data
	 *  @param nWidth The width in pixels of the output rectangle
	 *  @param nHeight The height in lines of the output rectangle
	 *  @param pDstData A pointer to the output image data, must not be \b nullptr
	 *  @param DstFormat The bitmap format of the output buffer. Must not change during \ref clear
	 * lifecycle.
	 *  @param nDstStep The size in bytes of a destination image line
	 *  @param nXDst The x offset in pixels in the destination buffer
	 *  @param nYDst The y offset in lines in the destination buffer
	 *  @param nDstWidth The width in pixels of the output buffer
	 *  @param nDstHeight The height in lines of the output buffer
	 *  @param palette An optional color palette to use if the output format is 8 bit
	 *
	 *  @return \b 0 in case of success, a negative error code otherwise.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API INT32 clear_decompress(CLEAR_CONTEXT* WINPR_RESTRICT clear,
	                                   const BYTE* WINPR_RESTRICT pSrcData, UINT32 SrcSize,
	                                   UINT32 nWidth, UINT32 nHeight, BYTE* WINPR_RESTRICT pDstData,
	                                   UINT32 DstFormat, UINT32 nDstStep, UINT32 nXDst,
	                                   UINT32 nYDst, UINT32 nDstWidth, UINT32 nDstHeight,
	                                   const gdiPalette* WINPR_RESTRICT palette);

	/** @brief reset the clear codec state.
	 *
	 *  @warning This does not reset internal buffers, these are not bound to the context lifecycle
	 *
	 *  @param clear the context to reset, must not be \b nullptr
	 *
	 *  @return \b TRUE for success, \b FALSE otherwise
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL clear_context_reset(CLEAR_CONTEXT* WINPR_RESTRICT clear);

	/** @brief free a clear codec context and all attached buffers.
	 *
	 *  @param clear The context to clear, may be \b nullptr
	 */
	FREERDP_API void clear_context_free(CLEAR_CONTEXT* WINPR_RESTRICT clear);

	/** @brief allocate a clear codec context
	 *
	 *  @param Compressor Allocate for compression (set to \b TRUE ) or decompression
	 *  @return An allocated context or \b nullptr in case of an error
	 */
	WINPR_ATTR_MALLOC(clear_context_free, 1)
	FREERDP_API CLEAR_CONTEXT* clear_context_new(BOOL Compressor);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_CLEAR_H */
