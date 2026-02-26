/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NSCodec Codec
 *
 * Copyright 2011 Samsung, Author Jiten Pathy
 * Copyright 2012 Vic Lee
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CODEC_NSCODEC_H
#define FREERDP_CODEC_NSCODEC_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/constants.h>

#include <winpr/stream.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/// parameter types available to change in a \ref NSC_CONTEXT See [MS-RDPNSC] 2.2.1 NSCodec
	/// Capability Set (TS_NSCODEC_CAPABILITYSET) for details
	typedef enum
	{
		NSC_COLOR_LOSS_LEVEL,       /**< \b colorLossLevel */
		NSC_ALLOW_SUBSAMPLING,      /**< \b fAllowSubsampling */
		NSC_DYNAMIC_COLOR_FIDELITY, /**< \b fAllowDynamicFidelity */
		NSC_COLOR_FORMAT /**< \ref PIXEL_FORMAT color format used for internal bitmap buffer */
	} NSC_PARAMETER;

	typedef struct S_NSC_CONTEXT NSC_CONTEXT;

#if defined(WITH_FREERDP_DEPRECATED)
	WINPR_DEPRECATED_VAR("Use nsc_context_set_parameters(NSC_COLOR_FORMAT)",
	                     WINPR_ATTR_NODISCARD FREERDP_API BOOL nsc_context_set_pixel_format(
	                         NSC_CONTEXT* context, UINT32 pixel_format));
#endif

	/** @brief Set a \ref NSC_PARAMETER for a \ref NSC_CONTEXT
	 *
	 *  @param context The \ref NSC_CONTEXT context to work on. Must not be \b nullptr
	 *  @param what A \ref NSC_PARAMETER to identify what to change
	 *  @param value The value to set
	 *
	 *  @return \b TRUE if successful, \b FALSE otherwise
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL nsc_context_set_parameters(NSC_CONTEXT* WINPR_RESTRICT context,
	                                            NSC_PARAMETER what, UINT32 value);

	/** @brief decode a NSC message
	 *
	 *  @param context The \ref NSC_CONTEXT context to work on. Must not be \b nullptr
	 *  @param bpp The bit depth of the data
	 *  @param width The width in pixels of the NSC surface
	 *  @param height The height in pixels of the NSC surface
	 *  @param data The data to decode
	 *  @param length The length of \ref data in bytes
	 *  @param pDstData The destination buffer. Must be at least \n nDstStride * nHeight in size.
	 *  @param DstFormat The color format of the destination
	 *  @param nDstStride The number of bytes per destination buffer line. Must be larger than
	 * bytes(DstFormat) * nWidth or \0 (in which case it is set to former minimum bound)
	 *  @param nXDst The X offset in the destination buffer in pixels
	 *  @param nYDst The Y offset in the destination buffer in pixels
	 *  @param nWidth The width of the destination buffer in pixels
	 *  @param nHeight The height of the destination buffer in pixels
	 *  @param flip Image flipping flags FREERDP_FLIP_NONE et al passed on to \ref
	 * freerdp_image_copy
	 *
	 *  @return \b TRUE in case of success, \b FALSE for any error. Check WLog for details.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL nsc_process_message(NSC_CONTEXT* WINPR_RESTRICT context, UINT16 bpp,
	                                     UINT32 width, UINT32 height, const BYTE* data,
	                                     UINT32 length, BYTE* WINPR_RESTRICT pDstData,
	                                     UINT32 DstFormat, UINT32 nDstStride, UINT32 nXDst,
	                                     UINT32 nYDst, UINT32 nWidth, UINT32 nHeight, UINT32 flip);

	/** @brief Encode a bitmap with \b NSC
	 *
	 *  @param context The \ref NSC_CONTEXT context to work on. Must not be \b nullptr
	 *  @param s a \ref wStream used to write \b NSC encoded data to. Must not be \b nullptr
	 *  @param bmpdata A pointer to the bitmap to encode. Format must match \b NSC_COLOR_FORMAT
	 *  @param width The width of the bitmap in pixels
	 *  @param height The height of the bitmap in pixels
	 *  @param scanline The stride of a bitmap line in bytes. If \b 0 the value of \b width * bytes
	 * \b NSC_COLOR_FORMAT is used.
	 *
	 *  @bug Versions < 3.23.0 do not support \b 0 for scanlines and abort.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL nsc_compose_message(NSC_CONTEXT* WINPR_RESTRICT context,
	                                     wStream* WINPR_RESTRICT s,
	                                     const BYTE* WINPR_RESTRICT bmpdata, UINT32 width,
	                                     UINT32 height, UINT32 scanline);

#if !defined(WITHOUT_FREERDP_3x_DEPRECATED)

	WINPR_DEPRECATED_VAR(
	    "[since 3.23.0] deprecated, insecure! missing length checks. use nsc_process_message",
	    WINPR_ATTR_NODISCARD FREERDP_API BOOL
	        nsc_decompose_message(NSC_CONTEXT* WINPR_RESTRICT context, wStream* WINPR_RESTRICT s,
	                              BYTE* WINPR_RESTRICT bmpdata, UINT32 x, UINT32 y, UINT32 width,
	                              UINT32 height, UINT32 rowstride, UINT32 format, UINT32 flip));
#endif

	/** @brief This function resets a \ref NSC_CONTEXT to a new resolution
	 *
	 *  @param context The \ref NSC_CONTEXT context to work on. Must not be \b nullptr
	 *  @param width The width of the context in pixels
	 *  @param height The height of the context in pixels
	 *
	 *  @return \b TRUE if successful, \b FALSE otherwise
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL nsc_context_reset(NSC_CONTEXT* WINPR_RESTRICT context, UINT32 width,
	                                   UINT32 height);

	FREERDP_API void nsc_context_free(NSC_CONTEXT* context);

	WINPR_ATTR_MALLOC(nsc_context_free, 1)
	WINPR_ATTR_NODISCARD
	FREERDP_API NSC_CONTEXT* nsc_context_new(void);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_NSCODEC_H */
