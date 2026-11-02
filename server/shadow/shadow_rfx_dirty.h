/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2026 Caomengxuan Cao <114896873+caomengxuan666@users.noreply.github.com>
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

/**
 * @brief Server side dirty tile skipping for the shadow GFX/RFX path.
 *
 * The shadow GFX path re-encodes every 64x64 RFX tile below the dirty region
 * extents of each frame unconditionally. RFX is a tile based codec: tiles
 * whose content did not change encode to byte-identical output, and the RFX
 * surface semantics allow a TileSet to carry only the changed tiles while the
 * client keeps the previous pixels for tiles it does not receive.
 *
 * This module maintains, per client encoder, a snapshot of the last encoded
 * content (a BGRX copy sharing the surface geometry) plus per-tile encoded
 * flags:
 *   - plan:    memcmp each tile against the snapshot and return the list of
 *              changed tiles as RFX_RECT entries (surface absolute
 *              coordinates, aligned to the 64 pixel grid, edge tiles
 *              truncated);
 *   - commit:  after a successful encode, write the encoded tile pixels into
 *              the snapshot and mark the tiles as encoded;
 *   - invalidate: drop the snapshot. Every moment where the client surface
 *              has no dependable state (new client, ResetGraphics, lobby
 *              switch, resize) reaches this via first_frame handling or
 *              shadow_encoder_reset/shadow_encoder_free, so the first frame
 *              afterwards is always encoded in full.
 */

#ifndef FREERDP_SERVER_SHADOW_RFX_DIRTY_H
#define FREERDP_SERVER_SHADOW_RFX_DIRTY_H

#include <winpr/wtypes.h>

#include <freerdp/api.h>
#include <freerdp/server/shadow.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/* Return the list of tiles (RFX_RECT, surface absolute coordinates) below
	 * cmd whose content differs from the last encoded snapshot. rects is
	 * allocated here and freed by the caller. Returns 0 on success and <0 on
	 * allocation failure (the caller should fall back to encoding the whole
	 * frame or fail the frame). */
	WINPR_ATTR_NODISCARD
	int shadow_rfx_plan_tiles(rdpShadowEncoder* encoder, const BYTE* pSrcData, UINT32 nSrcStep,
	                          const RDPGFX_SURFACE_COMMAND* cmd, UINT16 srcWidth, UINT16 srcHeight,
	                          RFX_RECT** rects, UINT16* numRects);

	/* Call after a successful encode: copy the encoded tile pixels into the
	 * snapshot, mark the tiles encoded and accumulate telemetry (one log line
	 * every 120 frames). wireBytes is the size of the encoded RFX message. */
	void shadow_rfx_commit_tiles(rdpShadowEncoder* encoder, const BYTE* pSrcData, UINT32 nSrcStep,
	                             const RFX_RECT* rects, UINT16 numRects, UINT32 wireBytes);

	/* Invalidate the snapshot (release buffers). Called from
	 * shadow_encoder_uninit (reached by reset/free) and when the GFX surface
	 * is reset (ResetGraphics / first frame). */
	void shadow_rfx_invalidate_snapshot(rdpShadowEncoder* encoder);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_RFX_DIRTY_H */
