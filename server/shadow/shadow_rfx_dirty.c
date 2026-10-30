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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>

#include <freerdp/log.h>

#include "shadow.h"
#include "shadow_encoder.h"
#include "shadow_rfx_dirty.h"

#define TAG SERVER_TAG("shadow.rfx_dirty")

#define RFX_TILE_SIZE 64ul

/* The RFX context is fixed to BGRX32 (validated by shadow_client_send_rfx) */
#define RFX_BYTES_PER_PIXEL 4ul

/* Telemetry counters shared by all client send threads (observation only,
 * racy reads are tolerated) */
static UINT64 sFrames = 0;
static UINT64 sTilesSeen = 0;
static UINT64 sTilesSent = 0;
static UINT64 sEmptyFrames = 0;
static UINT64 sWireBytes = 0;

void shadow_rfx_invalidate_snapshot(rdpShadowEncoder* encoder)
{
	WINPR_ASSERT(encoder);

	free(encoder->gfxSnapshot);
	encoder->gfxSnapshot = nullptr;
	free(encoder->gfxTileEncoded);
	encoder->gfxTileEncoded = nullptr;
	encoder->gfxSnapshotWidth = 0;
	encoder->gfxSnapshotHeight = 0;
	encoder->gfxSnapshotStep = 0;
	encoder->gfxSnapshotValid = FALSE;
}

static BOOL shadow_rfx_snapshot_ensure(rdpShadowEncoder* encoder, UINT16 width, UINT16 height,
                                       UINT32 step)
{
	WINPR_ASSERT(encoder);

	if (encoder->gfxSnapshotValid && (encoder->gfxSnapshotWidth == width) &&
	    (encoder->gfxSnapshotHeight == height) && (encoder->gfxSnapshotStep == step))
		return TRUE;

	shadow_rfx_invalidate_snapshot(encoder);

	if (width == 0 || height == 0 || step == 0)
	{
		WLog_WARN(TAG, "invalid snapshot geometry (%" PRIu16 "x%" PRIu16 " step=%" PRIu32 ")",
		          width, height, step);
		return FALSE;
	}
	if (step > SIZE_MAX / height)
	{
		WLog_WARN(TAG, "snapshot size overflows SIZE_MAX (%" PRIu32 "x%" PRIu16 ")",
		          step, height);
		return FALSE;
	}

	const UINT32 gridW = (width + RFX_TILE_SIZE - 1) / RFX_TILE_SIZE;
	const UINT32 gridH = (height + RFX_TILE_SIZE - 1) / RFX_TILE_SIZE;

	encoder->gfxSnapshot = (BYTE*)malloc((size_t)step * height);
	encoder->gfxTileEncoded =
	    (BOOL*)calloc(WINPR_ASSERTING_INT_CAST(size_t, gridW * gridH), sizeof(BOOL));

	if (!encoder->gfxSnapshot || !encoder->gfxTileEncoded)
	{
		WLog_WARN(TAG, "snapshot alloc failed (%ux%u step=%" PRIu32 ")", width, height, step);
		shadow_rfx_invalidate_snapshot(encoder);
		return FALSE;
	}

	encoder->gfxSnapshotWidth = width;
	encoder->gfxSnapshotHeight = height;
	encoder->gfxSnapshotStep = step;
	encoder->gfxSnapshotValid = TRUE;
	return TRUE;
}

int shadow_rfx_plan_tiles(rdpShadowEncoder* encoder, const BYTE* pSrcData, UINT32 nSrcStep,
                          const RDPGFX_SURFACE_COMMAND* cmd, UINT16 srcWidth, UINT16 srcHeight,
                          RFX_RECT** outRects, UINT16* outNumRects)
{
	WINPR_ASSERT(encoder);
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(cmd);
	WINPR_ASSERT(outRects);
	WINPR_ASSERT(outNumRects);
	WINPR_ASSERT(nSrcStep > 0);

	*outRects = nullptr;
	*outNumRects = 0;

	UINT32 x1 = cmd->right;
	UINT32 y1 = cmd->bottom;
	/* the rect must not exceed the source image (surface) bounds */
	if (x1 > srcWidth)
		x1 = srcWidth;
	if (y1 > srcHeight)
		y1 = srcHeight;

	if ((cmd->left >= x1) || (cmd->top >= y1))
		return 0; /* empty rect: no tiles */

	const UINT32 gridW = (srcWidth + RFX_TILE_SIZE - 1) / RFX_TILE_SIZE;
	const UINT32 gridH = (srcHeight + RFX_TILE_SIZE - 1) / RFX_TILE_SIZE;
	const UINT32 startTx = cmd->left / RFX_TILE_SIZE;
	const UINT32 endTx = (x1 - 1) / RFX_TILE_SIZE;
	const UINT32 startTy = cmd->top / RFX_TILE_SIZE;
	const UINT32 endTy = (y1 - 1) / RFX_TILE_SIZE;

	RFX_RECT* rects =
	    (RFX_RECT*)calloc(WINPR_ASSERTING_INT_CAST(size_t, gridW * gridH), sizeof(RFX_RECT));
	if (!rects)
		return -1;

	const BOOL snapOk = shadow_rfx_snapshot_ensure(encoder, srcWidth, srcHeight, nSrcStep);
	UINT16 numRects = 0;

	/* per-tile dirty flags within one tile row (reused per band) */
	BOOL* bandChanged = (BOOL*)calloc(gridW, sizeof(BOOL));
	if (!bandChanged)
	{
		free(rects);
		return -1;
	}

	for (UINT32 ty = startTy; ty <= endTy; ty++)
	{
		const UINT32 tileY = ty * RFX_TILE_SIZE;
		UINT32 tileH = RFX_TILE_SIZE;
		if ((tileY + RFX_TILE_SIZE) > srcHeight)
			tileH = srcHeight - tileY;

		memset(bandChanged, 0, gridW * sizeof(BOOL));

		/* band level fast path: when all rows covered by this tile row are
		 * identical to the snapshot, every tile in the row is unchanged
		 * (idle/small dirty frames then only need tileH row memcmps instead
		 * of one memcmp per tile row); fall back to per tile compares for
		 * rows that differ */
		BOOL bandDirty = FALSE;

		if (snapOk)
		{
			for (UINT32 r = 0; r < tileH; r++)
			{
				const size_t ofs =
				    ((size_t)(tileY + r)) * nSrcStep + ((size_t)cmd->left * RFX_BYTES_PER_PIXEL);
				const UINT32 spanBytes = (x1 - cmd->left) * RFX_BYTES_PER_PIXEL;
				if (memcmp(&pSrcData[ofs], &encoder->gfxSnapshot[ofs], spanBytes) != 0)
				{
					bandDirty = TRUE;
					break;
				}
			}
		}

		for (UINT32 tx = startTx; tx <= endTx; tx++)
		{
			const UINT32 tileX = tx * RFX_TILE_SIZE;
			UINT32 tileW = RFX_TILE_SIZE;
			if ((tileX + RFX_TILE_SIZE) > srcWidth)
				tileW = srcWidth - tileX;

			BOOL changed = TRUE;

			if (snapOk && encoder->gfxTileEncoded[ty * gridW + tx])
			{
				if (bandDirty)
				{
					changed = FALSE;
					for (UINT32 r = 0; r < tileH; r++)
					{
						const size_t ofs = ((size_t)(tileY + r)) * nSrcStep +
						                   ((size_t)tileX * RFX_BYTES_PER_PIXEL);
						if (memcmp(&pSrcData[ofs], &encoder->gfxSnapshot[ofs],
						           (size_t)tileW * RFX_BYTES_PER_PIXEL) != 0)
						{
							changed = TRUE;
							break;
						}
					}
				}
				else
				{
					changed = FALSE; /* whole band identical to snapshot */
				}
			}

			if (changed)
			{
				rects[numRects].x = (UINT16)tileX;
				rects[numRects].y = (UINT16)tileY;
				rects[numRects].width = (UINT16)tileW;
				rects[numRects].height = (UINT16)tileH;
				numRects++;
			}
		}
	}

	free(bandChanged);

	sTilesSeen += (UINT64)(endTx - startTx + 1) * (endTy - startTy + 1);
	if (numRects == 0)
		sEmptyFrames += 1;

	*outRects = rects;
	*outNumRects = numRects;
	return 0;
}

void shadow_rfx_commit_tiles(rdpShadowEncoder* encoder, const BYTE* pSrcData, UINT32 nSrcStep,
                             const RFX_RECT* rects, UINT16 numRects, UINT32 wireBytes)
{
	WINPR_ASSERT(encoder);
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(rects || (numRects == 0));

	if (!encoder->gfxSnapshotValid)
		return;

	const UINT32 gridW = (encoder->gfxSnapshotWidth + RFX_TILE_SIZE - 1) / RFX_TILE_SIZE;
	const UINT32 gridH = (encoder->gfxSnapshotHeight + RFX_TILE_SIZE - 1) / RFX_TILE_SIZE;

	for (UINT16 i = 0; i < numRects; i++)
	{
		const UINT32 tileX = rects[i].x;
		const UINT32 tileY = rects[i].y;
		const UINT32 tx = tileX / RFX_TILE_SIZE;
		const UINT32 ty = tileY / RFX_TILE_SIZE;

		if ((tx < gridW) && (ty < gridH))
		{
			for (UINT32 r = 0; r < rects[i].height; r++)
			{
				const size_t ofs =
				    ((size_t)(tileY + r)) * nSrcStep + ((size_t)tileX * RFX_BYTES_PER_PIXEL);
				CopyMemory(&encoder->gfxSnapshot[ofs], &pSrcData[ofs],
				           rects[i].width * RFX_BYTES_PER_PIXEL);
			}

			encoder->gfxTileEncoded[ty * gridW + tx] = TRUE;
		}
	}

	sFrames += 1;
	sTilesSent += numRects;
	sWireBytes += wireBytes;

	if ((sFrames % 120) == 0)
	{
		WLog_DBG(TAG,
		         "stats: frames=%" PRIu64 " tiles_seen=%" PRIu64 " tiles_sent=%" PRIu64
		         " tiles_skipped=%" PRIu64 " empty_frames=%" PRIu64 " wire_bytes=%" PRIu64,
		         sFrames, sTilesSeen, sTilesSent, sTilesSeen - sTilesSent, sEmptyFrames,
		         sWireBytes);
	}
}
