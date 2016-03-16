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

#include <freerdp/codec/color.h>

#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/codec/h264.h>
#include <freerdp/codec/clear.h>
#include <freerdp/codec/planar.h>
#include <freerdp/codec/interleaved.h>
#include <freerdp/codec/progressive.h>

#define FREERDP_CODEC_INTERLEAVED		0x00000001
#define FREERDP_CODEC_PLANAR			0x00000002
#define FREERDP_CODEC_NSCODEC			0x00000004
#define FREERDP_CODEC_REMOTEFX			0x00000008
#define FREERDP_CODEC_CLEARCODEC		0x00000010
#define FREERDP_CODEC_ALPHACODEC		0x00000020
#define FREERDP_CODEC_PROGRESSIVE		0x00000040
#define FREERDP_CODEC_AVC420			0x00000080
#define FREERDP_CODEC_AVC444			0x00000100
#define FREERDP_CODEC_ALL			0xFFFFFFFF

struct rdp_codecs
{
	rdpContext* context;

	RFX_CONTEXT* rfx;
	NSC_CONTEXT* nsc;
	H264_CONTEXT* h264;
	CLEAR_CONTEXT* clear;
	PROGRESSIVE_CONTEXT* progressive;
	BITMAP_PLANAR_CONTEXT* planar;
	BITMAP_INTERLEAVED_CONTEXT* interleaved;
};

#ifdef __cplusplus
 extern "C" {
#endif

FREERDP_API BOOL freerdp_client_codecs_prepare(rdpCodecs* codecs, UINT32 flags);
FREERDP_API BOOL freerdp_client_codecs_reset(rdpCodecs* codecs, UINT32 flags,
					     UINT32 width, UINT32 height);

FREERDP_API rdpCodecs* codecs_new(rdpContext* context);
FREERDP_API void codecs_free(rdpCodecs* codecs);

#ifdef __cplusplus
 }
#endif

#endif /* FREERDP_CODECS_H */

