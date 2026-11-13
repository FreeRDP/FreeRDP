/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_SERVER_SHADOW_ENCODER_H
#define FREERDP_SERVER_SHADOW_ENCODER_H

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/freerdp.h>
#include <freerdp/codecs.h>

#include <freerdp/server/shadow.h>

/* GFX frames a client may have unacknowledged before it gets no new one, like xrdp's
 * XRDP_GFX_FRAMES_IN_FLIGHT=8; FREERDP_SHADOW_GFX_FRAMES_IN_FLIGHT overrides it */
#define SHADOW_GFX_FRAMES_IN_FLIGHT 8

struct rdp_shadow_encoder
{
	rdpShadowClient* client;
	rdpShadowServer* server;

	UINT32 width;
	UINT32 height;
	UINT32 codecs;

	BYTE** grid;
	UINT32 gridWidth;
	UINT32 gridHeight;
	BYTE* gridBuffer;
	UINT32 maxTileWidth;
	UINT32 maxTileHeight;

	wStream* bs;

	RFX_CONTEXT* rfx;
	NSC_CONTEXT* nsc;
	BITMAP_PLANAR_CONTEXT* planar;
	BITMAP_INTERLEAVED_CONTEXT* interleaved;
	H264_CONTEXT* h264;
	PROGRESSIVE_CONTEXT* progressive;
#if defined(WITH_GFX_AV1)
	FREERDP_AV1_CONTEXT* av1;
#endif
#if defined(WITH_GFX_AV2)
	FREERDP_AV2_CONTEXT* av2;
#endif
#if defined(WITH_GFX_AZURE)
	FREERDP_HEVC_CONTEXT* hevc;
#endif
	UINT32 fps;
	UINT32 maxFps;
	BOOL frameAck;
	UINT32 frameId;
	UINT32 lastAckframeId;
	UINT32 queueDepth;

	/* GFX flow control (shadow_client.c): no new frame while maxInFlight frames are not
	 * acknowledged (0: no limit). gfxHeld: a round's damage waits in the client's region. */
	UINT32 maxInFlight;
	UINT64 lastAckTick;
	BOOL gfxHeld;
	BOOL everAcked; /* the client acknowledged a frame at least once */
	BOOL warnedNoAck;
	UINT32 statsMaxInFlight;
	UINT64 statsHeld;
	UINT64 statsLogged;

	/* GFX RFX dirty tile skipping (shadow_rfx_dirty.c): snapshot of the last
	 * encoded content (BGRX copy sharing the surface geometry) plus per tile
	 * encoded flags. Only used by the GFX RFX path; a missing or mismatching
	 * snapshot encodes the frame in full. */
	BYTE* gfxSnapshot;
	BOOL* gfxTileEncoded;
	UINT32 gfxSnapshotWidth;
	UINT32 gfxSnapshotHeight;
	UINT32 gfxSnapshotStep;
	BOOL gfxSnapshotValid;
};

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_ATTR_NODISCARD int shadow_encoder_reset(rdpShadowEncoder* encoder);
	WINPR_ATTR_NODISCARD int shadow_encoder_prepare(rdpShadowEncoder* encoder, UINT32 codecs);
	WINPR_ATTR_NODISCARD UINT32 shadow_encoder_create_frame_id(rdpShadowEncoder* encoder);

	void shadow_encoder_free(rdpShadowEncoder* encoder);

	WINPR_ATTR_MALLOC(shadow_encoder_free, 1)
	rdpShadowEncoder* shadow_encoder_new(rdpShadowClient* client);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_ENCODER_H */
