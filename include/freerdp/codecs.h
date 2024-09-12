/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Codecs
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

#ifndef FREERDP_CODECS_H
#define FREERDP_CODECS_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <freerdp/codec/color.h>

#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/codec/h264.h>
#include <freerdp/codec/clear.h>
#include <freerdp/codec/planar.h>
#include <freerdp/codec/interleaved.h>
#include <freerdp/codec/progressive.h>

typedef enum
{
	FREERDP_CODEC_INTERLEAVED = 0x00000001,
	FREERDP_CODEC_PLANAR = 0x00000002,
	FREERDP_CODEC_NSCODEC = 0x00000004,
	FREERDP_CODEC_REMOTEFX = 0x00000008,
	FREERDP_CODEC_CLEARCODEC = 0x00000010,
	FREERDP_CODEC_ALPHACODEC = 0x00000020,
	FREERDP_CODEC_PROGRESSIVE = 0x00000040,
	FREERDP_CODEC_AVC420 = 0x00000080,
	FREERDP_CODEC_AVC444 = 0x00000100,
	FREERDP_CODEC_ALL = 0x7FFFFFFF /* C enum types are restricted to int */
} FreeRDP_CodecFlags;

#ifdef __cplusplus
extern "C"
{
#endif

	struct rdp_codecs
	{
		UINT32 ThreadingFlags; /** @since version 3.6.0 */

		RFX_CONTEXT* rfx;
		NSC_CONTEXT* nsc;
		H264_CONTEXT* h264;
		CLEAR_CONTEXT* clear;
		PROGRESSIVE_CONTEXT* progressive;
		BITMAP_PLANAR_CONTEXT* planar;
		BITMAP_INTERLEAVED_CONTEXT* interleaved;
	};
	typedef struct rdp_codecs rdpCodecs;

	FREERDP_API BOOL freerdp_client_codecs_prepare(rdpCodecs* codecs, UINT32 flags, UINT32 width,
	                                               UINT32 height);
	FREERDP_API BOOL freerdp_client_codecs_reset(rdpCodecs* codecs, UINT32 flags, UINT32 width,
	                                             UINT32 height);

	/**
	 * @brief Free a rdpCodecs instance
	 * @param codecs A pointer to a rdpCodecs instance or NULL
	 *  @since version 3.6.0
	 */
	FREERDP_API void freerdp_client_codecs_free(rdpCodecs* codecs);

	/**
	 * @brief Allocate a rdpCodecs instance.
	 * @return A newly allocated instance or \b NULL in case of failure.
	 *  @since version 3.6.0
	 */
	WINPR_ATTR_MALLOC(freerdp_client_codecs_free, 1)
	FREERDP_API rdpCodecs* freerdp_client_codecs_new(UINT32 TheadingFlags);

	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.6.0] Use freerdp_client_codecs_free",
	                                 void codecs_free(rdpCodecs* codecs));

	FREERDP_API WINPR_DEPRECATED_VAR("[since 3.6.0] Use freerdp_client_codecs_new",
	                                 rdpCodecs* codecs_new(rdpContext* context));

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODECS_H */
