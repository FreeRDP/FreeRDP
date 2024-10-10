/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Graphics Pipeline
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@thincast.com>
 * Copyright 2016 Thincast Technologies GmbH
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

#include "../core/update.h"

#include <freerdp/api.h>
#include <freerdp/log.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/gdi/region.h>
#include <freerdp/utils/gfx.h>
#include <math.h>

#define TAG FREERDP_TAG("gdi")

static BOOL is_rect_valid(const RECTANGLE_16* rect, size_t width, size_t height)
{
	if (!rect)
		return FALSE;
	if ((rect->left > rect->right) || (rect->right > width))
		return FALSE;
	if ((rect->top > rect->bottom) || (rect->bottom > height))
		return FALSE;
	return TRUE;
}

static BOOL is_within_surface(const gdiGfxSurface* surface, const RDPGFX_SURFACE_COMMAND* cmd)
{
	RECTANGLE_16 rect;
	if (!surface || !cmd)
		return FALSE;
	rect.left = (UINT16)MIN(UINT16_MAX, cmd->left);
	rect.top = (UINT16)MIN(UINT16_MAX, cmd->top);
	rect.right = (UINT16)MIN(UINT16_MAX, cmd->right);
	rect.bottom = (UINT16)MIN(UINT16_MAX, cmd->bottom);
	if (!is_rect_valid(&rect, surface->width, surface->height))
	{
		WLog_ERR(TAG,
		         "Command rect %" PRIu32 "x%" PRIu32 "-%" PRIu32 "x%" PRIu32
		         " not within bounds of %" PRIu32 "x%" PRIu32,
		         rect.left, rect.top, cmd->width, cmd->height, surface->width, surface->height);
		return FALSE;
	}

	return TRUE;
}

static DWORD gfx_align_scanline(DWORD widthInBytes, DWORD alignment)
{
	const UINT32 align = alignment;
	const UINT32 pad = align - (widthInBytes % alignment);
	UINT32 scanline = widthInBytes;

	if (align != pad)
		scanline += pad;

	return scanline;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_ResetGraphics(RdpgfxClientContext* context,
                              const RDPGFX_RESET_GRAPHICS_PDU* resetGraphics)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	UINT16 count = 0;
	UINT32 DesktopWidth = 0;
	UINT32 DesktopHeight = 0;
	UINT16* pSurfaceIds = NULL;
	rdpGdi* gdi = NULL;
	rdpUpdate* update = NULL;
	rdpSettings* settings = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(resetGraphics);

	gdi = (rdpGdi*)context->custom;
	WINPR_ASSERT(gdi);

	update = gdi->context->update;
	WINPR_ASSERT(update);

	settings = gdi->context->settings;
	WINPR_ASSERT(settings);
	EnterCriticalSection(&context->mux);
	DesktopWidth = resetGraphics->width;
	DesktopHeight = resetGraphics->height;

	if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, DesktopWidth))
		goto fail;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, DesktopHeight))
		goto fail;

	if (update)
	{
		WINPR_ASSERT(update->DesktopResize);
		update->DesktopResize(gdi->context);
	}

	WINPR_ASSERT(context->GetSurfaceIds);
	context->GetSurfaceIds(context, &pSurfaceIds, &count);

	for (UINT32 index = 0; index < count; index++)
	{
		WINPR_ASSERT(context->GetSurfaceData);
		gdiGfxSurface* surface =
		    (gdiGfxSurface*)context->GetSurfaceData(context, pSurfaceIds[index]);

		if (!surface)
			continue;

		memset(surface->data, 0xFF, (size_t)surface->scanline * surface->height);
		region16_clear(&surface->invalidRegion);
	}

	free(pSurfaceIds);

	if (!freerdp_settings_get_bool(gdi->context->settings, FreeRDP_DeactivateClientDecoding))
	{
		const UINT32 width = (UINT32)MAX(0, gdi->width);
		const UINT32 height = (UINT32)MAX(0, gdi->height);

		if (!freerdp_client_codecs_reset(
		        context->codecs, freerdp_settings_get_codecs_flags(settings), width, height))
		{
			goto fail;
		}
		if (!freerdp_client_codecs_reset(
		        gdi->context->codecs, freerdp_settings_get_codecs_flags(settings), width, height))
		{
			goto fail;
		}
	}

	rc = CHANNEL_RC_OK;
fail:
	LeaveCriticalSection(&context->mux);
	return rc;
}

static UINT gdi_OutputUpdate(rdpGdi* gdi, gdiGfxSurface* surface)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	UINT32 surfaceX = 0;
	UINT32 surfaceY = 0;
	RECTANGLE_16 surfaceRect;
	const RECTANGLE_16* rects = NULL;
	UINT32 nbRects = 0;
	rdpUpdate* update = NULL;

	WINPR_ASSERT(gdi);
	WINPR_ASSERT(gdi->context);
	WINPR_ASSERT(surface);

	update = gdi->context->update;
	WINPR_ASSERT(update);

	if (gdi->suppressOutput)
		return CHANNEL_RC_OK;

	surfaceX = surface->outputOriginX;
	surfaceY = surface->outputOriginY;
	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = (UINT16)MIN(UINT16_MAX, surface->mappedWidth);
	surfaceRect.bottom = (UINT16)MIN(UINT16_MAX, surface->mappedHeight);
	region16_intersect_rect(&(surface->invalidRegion), &(surface->invalidRegion), &surfaceRect);
	const double sx = surface->outputTargetWidth / (double)surface->mappedWidth;
	const double sy = surface->outputTargetHeight / (double)surface->mappedHeight;

	if (!(rects = region16_rects(&surface->invalidRegion, &nbRects)) || !nbRects)
		return CHANNEL_RC_OK;

	if (!update_begin_paint(update))
		goto fail;

	for (UINT32 i = 0; i < nbRects; i++)
	{
		const UINT32 nXSrc = rects[i].left;
		const UINT32 nYSrc = rects[i].top;
		const UINT32 nXDst = (UINT32)MIN(surfaceX + nXSrc * sx, gdi->width - 1);
		const UINT32 nYDst = (UINT32)MIN(surfaceY + nYSrc * sy, gdi->height - 1);
		const UINT32 swidth = rects[i].right - rects[i].left;
		const UINT32 sheight = rects[i].bottom - rects[i].top;
		const UINT32 dwidth = MIN((UINT32)(swidth * sx), (UINT32)gdi->width - nXDst);
		const UINT32 dheight = MIN((UINT32)(sheight * sy), (UINT32)gdi->height - nYDst);

		if (!freerdp_image_scale(gdi->primary_buffer, gdi->dstFormat, gdi->stride, nXDst, nYDst,
		                         dwidth, dheight, surface->data, surface->format, surface->scanline,
		                         nXSrc, nYSrc, swidth, sheight))
		{
			rc = CHANNEL_RC_NULL_DATA;
			goto fail;
		}

		gdi_InvalidateRegion(gdi->primary->hdc, (INT32)nXDst, (INT32)nYDst, (INT32)dwidth,
		                     (INT32)dheight);
	}

	rc = CHANNEL_RC_OK;
fail:

	if (!update_end_paint(update))
		rc = ERROR_INTERNAL_ERROR;

	region16_clear(&(surface->invalidRegion));
	return rc;
}

static UINT gdi_WindowUpdate(RdpgfxClientContext* context, gdiGfxSurface* surface)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(surface);
	return IFCALLRESULT(CHANNEL_RC_OK, context->UpdateWindowFromSurface, context, surface);
}

static UINT gdi_UpdateSurfaces(RdpgfxClientContext* context)
{
	UINT16 count = 0;
	UINT status = ERROR_INTERNAL_ERROR;
	UINT16* pSurfaceIds = NULL;
	rdpGdi* gdi = NULL;

	WINPR_ASSERT(context);

	gdi = (rdpGdi*)context->custom;
	WINPR_ASSERT(gdi);

	EnterCriticalSection(&context->mux);

	WINPR_ASSERT(context->GetSurfaceIds);
	context->GetSurfaceIds(context, &pSurfaceIds, &count);
	status = CHANNEL_RC_OK;

	for (UINT32 index = 0; index < count; index++)
	{
		WINPR_ASSERT(context->GetSurfaceData);
		gdiGfxSurface* surface =
		    (gdiGfxSurface*)context->GetSurfaceData(context, pSurfaceIds[index]);

		if (!surface)
			continue;

		/* Already handled in UpdateSurfaceArea callbacks */
		if (context->UpdateSurfaceArea)
		{
			if (surface->handleInUpdateSurfaceArea)
				continue;
		}

		if (surface->outputMapped)
			status = gdi_OutputUpdate(gdi, surface);
		else if (surface->windowMapped)
			status = gdi_WindowUpdate(context, surface);

		if (status != CHANNEL_RC_OK)
			break;
	}

	free(pSurfaceIds);
	LeaveCriticalSection(&context->mux);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_StartFrame(RdpgfxClientContext* context, const RDPGFX_START_FRAME_PDU* startFrame)
{
	rdpGdi* gdi = NULL;

	WINPR_ASSERT(context);
	WINPR_ASSERT(startFrame);

	gdi = (rdpGdi*)context->custom;
	WINPR_ASSERT(gdi);
	gdi->inGfxFrame = TRUE;
	gdi->frameId = startFrame->frameId;
	return CHANNEL_RC_OK;
}

static UINT gdi_call_update_surfaces(RdpgfxClientContext* context)
{
	WINPR_ASSERT(context);
	return IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaces, context);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_EndFrame(RdpgfxClientContext* context, const RDPGFX_END_FRAME_PDU* endFrame)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(endFrame);

	rdpGdi* gdi = (rdpGdi*)context->custom;
	WINPR_ASSERT(gdi);
	const UINT status = gdi_call_update_surfaces(context);
	gdi->inGfxFrame = FALSE;
	return status;
}

static UINT gdi_interFrameUpdate(rdpGdi* gdi, RdpgfxClientContext* context)
{
	WINPR_ASSERT(gdi);
	UINT status = CHANNEL_RC_OK;
	if (!gdi->inGfxFrame)
		status = gdi_call_update_surfaces(context);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_Uncompressed(rdpGdi* gdi, RdpgfxClientContext* context,
                                            const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = CHANNEL_RC_OK;
	gdiGfxSurface* surface = NULL;
	RECTANGLE_16 invalidRect;
	DWORD bpp = 0;
	size_t size = 0;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(context);
	WINPR_ASSERT(cmd);

	WINPR_ASSERT(context->GetSurfaceData);
	surface =
	    (gdiGfxSurface*)context->GetSurfaceData(context, (UINT16)MIN(UINT16_MAX, cmd->surfaceId));

	if (!surface)
	{
		WLog_ERR(TAG, "unable to retrieve surfaceData for surfaceId=%" PRIu32 "", cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	if (!is_within_surface(surface, cmd))
		return ERROR_INVALID_DATA;

	bpp = FreeRDPGetBytesPerPixel(cmd->format);
	size = 1ull * bpp * cmd->width * cmd->height;
	if (cmd->length < size)
	{
		WLog_ERR(TAG, "Not enough data, got %" PRIu32 ", expected %" PRIuz, cmd->length, size);
		return ERROR_INVALID_DATA;
	}

	if (!freerdp_image_copy_no_overlap(surface->data, surface->format, surface->scanline, cmd->left,
	                                   cmd->top, cmd->width, cmd->height, cmd->data, cmd->format, 0,
	                                   0, 0, NULL, FREERDP_FLIP_NONE))
		return ERROR_INTERNAL_ERROR;

	invalidRect.left = (UINT16)MIN(UINT16_MAX, cmd->left);
	invalidRect.top = (UINT16)MIN(UINT16_MAX, cmd->top);
	invalidRect.right = (UINT16)MIN(UINT16_MAX, cmd->right);
	invalidRect.bottom = (UINT16)MIN(UINT16_MAX, cmd->bottom);
	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);
	status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context, surface->surfaceId, 1,
	                      &invalidRect);

	if (status != CHANNEL_RC_OK)
		goto fail;

	status = gdi_interFrameUpdate(gdi, context);

fail:
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_RemoteFX(rdpGdi* gdi, RdpgfxClientContext* context,
                                        const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = ERROR_INTERNAL_ERROR;
	gdiGfxSurface* surface = NULL;
	REGION16 invalidRegion;
	const RECTANGLE_16* rects = NULL;
	UINT32 nrRects = 0;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(context);
	WINPR_ASSERT(cmd);

	WINPR_ASSERT(context->GetSurfaceData);
	surface =
	    (gdiGfxSurface*)context->GetSurfaceData(context, (UINT16)MIN(UINT16_MAX, cmd->surfaceId));

	if (!surface)
	{
		WLog_ERR(TAG, "unable to retrieve surfaceData for surfaceId=%" PRIu32 "", cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	WINPR_ASSERT(surface->codecs);
	rfx_context_set_pixel_format(surface->codecs->rfx, cmd->format);
	region16_init(&invalidRegion);

	if (!rfx_process_message(surface->codecs->rfx, cmd->data, cmd->length, cmd->left, cmd->top,
	                         surface->data, surface->format, surface->scanline, surface->height,
	                         &invalidRegion))
	{
		WLog_ERR(TAG, "Failed to process RemoteFX message");
		goto fail;
	}

	rects = region16_rects(&invalidRegion, &nrRects);
	status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context, surface->surfaceId,
	                      nrRects, rects);

	if (status != CHANNEL_RC_OK)
		goto fail;

	for (UINT32 x = 0; x < nrRects; x++)
		region16_union_rect(&surface->invalidRegion, &surface->invalidRegion, &rects[x]);

	status = gdi_interFrameUpdate(gdi, context);

fail:
	region16_uninit(&invalidRegion);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_ClearCodec(rdpGdi* gdi, RdpgfxClientContext* context,
                                          const RDPGFX_SURFACE_COMMAND* cmd)
{
	INT32 rc = 0;
	UINT status = CHANNEL_RC_OK;
	gdiGfxSurface* surface = NULL;
	RECTANGLE_16 invalidRect;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(context);
	WINPR_ASSERT(cmd);

	WINPR_ASSERT(context->GetSurfaceData);
	surface =
	    (gdiGfxSurface*)context->GetSurfaceData(context, (UINT16)MIN(UINT16_MAX, cmd->surfaceId));

	if (!surface)
	{
		WLog_ERR(TAG, "unable to retrieve surfaceData for surfaceId=%" PRIu32 "", cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	WINPR_ASSERT(surface->codecs);
	rc = clear_decompress(surface->codecs->clear, cmd->data, cmd->length, cmd->width, cmd->height,
	                      surface->data, surface->format, surface->scanline, cmd->left, cmd->top,
	                      surface->width, surface->height, &gdi->palette);

	if (rc < 0)
	{
		WLog_ERR(TAG, "clear_decompress failure: %" PRId32 "", rc);
		return ERROR_INTERNAL_ERROR;
	}

	invalidRect.left = (UINT16)MIN(UINT16_MAX, cmd->left);
	invalidRect.top = (UINT16)MIN(UINT16_MAX, cmd->top);
	invalidRect.right = (UINT16)MIN(UINT16_MAX, cmd->right);
	invalidRect.bottom = (UINT16)MIN(UINT16_MAX, cmd->bottom);
	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);
	status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context, surface->surfaceId, 1,
	                      &invalidRect);

	if (status != CHANNEL_RC_OK)
		goto fail;

	status = gdi_interFrameUpdate(gdi, context);

fail:
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_Planar(rdpGdi* gdi, RdpgfxClientContext* context,
                                      const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = CHANNEL_RC_OK;
	BYTE* DstData = NULL;
	gdiGfxSurface* surface = NULL;
	RECTANGLE_16 invalidRect;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(context);
	WINPR_ASSERT(cmd);

	WINPR_ASSERT(context->GetSurfaceData);
	surface =
	    (gdiGfxSurface*)context->GetSurfaceData(context, (UINT16)MIN(UINT16_MAX, cmd->surfaceId));

	if (!surface)
	{
		WLog_ERR(TAG, "unable to retrieve surfaceData for surfaceId=%" PRIu32 "", cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	DstData = surface->data;

	if (!is_within_surface(surface, cmd))
		return ERROR_INVALID_DATA;

	if (!planar_decompress(surface->codecs->planar, cmd->data, cmd->length, cmd->width, cmd->height,
	                       DstData, surface->format, surface->scanline, cmd->left, cmd->top,
	                       cmd->width, cmd->height, FALSE))
		return ERROR_INTERNAL_ERROR;

	invalidRect.left = (UINT16)MIN(UINT16_MAX, cmd->left);
	invalidRect.top = (UINT16)MIN(UINT16_MAX, cmd->top);
	invalidRect.right = (UINT16)MIN(UINT16_MAX, cmd->right);
	invalidRect.bottom = (UINT16)MIN(UINT16_MAX, cmd->bottom);
	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);
	status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context, surface->surfaceId, 1,
	                      &invalidRect);

	if (status != CHANNEL_RC_OK)
		goto fail;

	status = gdi_interFrameUpdate(gdi, context);

fail:
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_AVC420(rdpGdi* gdi, RdpgfxClientContext* context,
                                      const RDPGFX_SURFACE_COMMAND* cmd)
{
#ifdef WITH_GFX_H264
	INT32 rc = 0;
	UINT status = CHANNEL_RC_OK;
	gdiGfxSurface* surface = NULL;
	RDPGFX_H264_METABLOCK* meta = NULL;
	RDPGFX_AVC420_BITMAP_STREAM* bs = NULL;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(context);
	WINPR_ASSERT(cmd);

	WINPR_ASSERT(context->GetSurfaceData);
	surface =
	    (gdiGfxSurface*)context->GetSurfaceData(context, (UINT16)MIN(UINT16_MAX, cmd->surfaceId));

	if (!surface)
	{
		WLog_ERR(TAG, "unable to retrieve surfaceData for surfaceId=%" PRIu32 "", cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	if (!surface->h264)
	{
		surface->h264 = h264_context_new(FALSE);

		if (!surface->h264)
		{
			WLog_ERR(TAG, "unable to create h264 context");
			return ERROR_NOT_ENOUGH_MEMORY;
		}

		if (!h264_context_reset(surface->h264, surface->width, surface->height))
			return ERROR_INTERNAL_ERROR;
	}

	if (!surface->h264)
		return ERROR_NOT_SUPPORTED;

	bs = (RDPGFX_AVC420_BITMAP_STREAM*)cmd->extra;

	if (!bs)
		return ERROR_INTERNAL_ERROR;

	meta = &(bs->meta);
	rc = avc420_decompress(surface->h264, bs->data, bs->length, surface->data, surface->format,
	                       surface->scanline, surface->width, surface->height, meta->regionRects,
	                       meta->numRegionRects);

	if (rc < 0)
	{
		WLog_WARN(TAG, "avc420_decompress failure: %" PRId32 ", ignoring update.", rc);
		return CHANNEL_RC_OK;
	}

	for (UINT32 i = 0; i < meta->numRegionRects; i++)
	{
		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion),
		                    &(meta->regionRects[i]));
	}

	status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context, surface->surfaceId,
	                      meta->numRegionRects, meta->regionRects);

	if (status != CHANNEL_RC_OK)
		goto fail;

	status = gdi_interFrameUpdate(gdi, context);

fail:
	return status;
#else
	return ERROR_NOT_SUPPORTED;
#endif
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_AVC444(rdpGdi* gdi, RdpgfxClientContext* context,
                                      const RDPGFX_SURFACE_COMMAND* cmd)
{
#ifdef WITH_GFX_H264
	INT32 rc = 0;
	UINT status = CHANNEL_RC_OK;
	gdiGfxSurface* surface = NULL;
	RDPGFX_AVC444_BITMAP_STREAM* bs = NULL;
	RDPGFX_AVC420_BITMAP_STREAM* avc1 = NULL;
	RDPGFX_H264_METABLOCK* meta1 = NULL;
	RDPGFX_AVC420_BITMAP_STREAM* avc2 = NULL;
	RDPGFX_H264_METABLOCK* meta2 = NULL;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(context);
	WINPR_ASSERT(cmd);

	WINPR_ASSERT(context->GetSurfaceData);
	surface =
	    (gdiGfxSurface*)context->GetSurfaceData(context, (UINT16)MIN(UINT16_MAX, cmd->surfaceId));

	if (!surface)
	{
		WLog_ERR(TAG, "unable to retrieve surfaceData for surfaceId=%" PRIu32 "", cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	if (!surface->h264)
	{
		surface->h264 = h264_context_new(FALSE);

		if (!surface->h264)
		{
			WLog_ERR(TAG, "unable to create h264 context");
			return ERROR_NOT_ENOUGH_MEMORY;
		}

		if (!h264_context_reset(surface->h264, surface->width, surface->height))
			return ERROR_INTERNAL_ERROR;
	}

	if (!surface->h264)
		return ERROR_NOT_SUPPORTED;

	bs = (RDPGFX_AVC444_BITMAP_STREAM*)cmd->extra;

	if (!bs)
		return ERROR_INTERNAL_ERROR;

	avc1 = &bs->bitstream[0];
	avc2 = &bs->bitstream[1];
	meta1 = &avc1->meta;
	meta2 = &avc2->meta;
	rc = avc444_decompress(surface->h264, bs->LC, meta1->regionRects, meta1->numRegionRects,
	                       avc1->data, avc1->length, meta2->regionRects, meta2->numRegionRects,
	                       avc2->data, avc2->length, surface->data, surface->format,
	                       surface->scanline, surface->width, surface->height, cmd->codecId);

	if (rc < 0)
	{
		WLog_WARN(TAG, "avc444_decompress failure: %" PRIu32 ", ignoring update.", status);
		return CHANNEL_RC_OK;
	}

	for (UINT32 i = 0; i < meta1->numRegionRects; i++)
	{
		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion),
		                    &(meta1->regionRects[i]));
	}

	status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context, surface->surfaceId,
	                      meta1->numRegionRects, meta1->regionRects);

	if (status != CHANNEL_RC_OK)
		goto fail;

	for (UINT32 i = 0; i < meta2->numRegionRects; i++)
	{
		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion),
		                    &(meta2->regionRects[i]));
	}

	status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context, surface->surfaceId,
	                      meta2->numRegionRects, meta2->regionRects);

	if (status != CHANNEL_RC_OK)
		goto fail;

	status = gdi_interFrameUpdate(gdi, context);

fail:
	return status;
#else
	return ERROR_NOT_SUPPORTED;
#endif
}

static BOOL gdi_apply_alpha(BYTE* data, UINT32 format, UINT32 stride, RECTANGLE_16* rect,
                            UINT32 startOffsetX, UINT32 count, BYTE a)
{
	UINT32 written = 0;
	BOOL first = TRUE;
	const UINT32 bpp = FreeRDPGetBytesPerPixel(format);
	WINPR_ASSERT(rect);

	for (size_t y = rect->top; y < rect->bottom; y++)
	{
		BYTE* line = &data[y * stride];

		for (size_t x = first ? rect->left + startOffsetX : rect->left; x < rect->right; x++)
		{
			BYTE r = 0;
			BYTE g = 0;
			BYTE b = 0;

			if (written == count)
				return TRUE;

			BYTE* src = &line[x * bpp];
			UINT32 color = FreeRDPReadColor(src, format);
			FreeRDPSplitColor(color, format, &r, &g, &b, NULL, NULL);
			color = FreeRDPGetColor(format, r, g, b, a);
			FreeRDPWriteColor(src, format, color);
			written++;
		}

		first = FALSE;
	}

	return TRUE;
}
/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_Alpha(rdpGdi* gdi, RdpgfxClientContext* context,
                                     const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = CHANNEL_RC_OK;
	UINT16 alphaSig = 0;
	UINT16 compressed = 0;
	gdiGfxSurface* surface = NULL;
	RECTANGLE_16 invalidRect;
	wStream buffer;
	wStream* s = NULL;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(context);
	WINPR_ASSERT(cmd);

	s = Stream_StaticConstInit(&buffer, cmd->data, cmd->length);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	WINPR_ASSERT(context->GetSurfaceData);
	surface =
	    (gdiGfxSurface*)context->GetSurfaceData(context, (UINT16)MIN(UINT16_MAX, cmd->surfaceId));

	if (!surface)
	{
		WLog_ERR(TAG, "unable to retrieve surfaceData for surfaceId=%" PRIu32 "", cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	if (!is_within_surface(surface, cmd))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT16(s, alphaSig);
	Stream_Read_UINT16(s, compressed);

	if (alphaSig != 0x414C)
		return ERROR_INVALID_DATA;

	if (compressed == 0)
	{
		if (!Stream_CheckAndLogRequiredLengthOfSize(TAG, s, cmd->height, cmd->width))
			return ERROR_INVALID_DATA;

		for (size_t y = cmd->top; y < cmd->top + cmd->height; y++)
		{
			BYTE* line = &surface->data[y * surface->scanline];

			for (size_t x = cmd->left; x < cmd->left + cmd->width; x++)
			{
				UINT32 color = 0;
				BYTE r = 0;
				BYTE g = 0;
				BYTE b = 0;
				BYTE a = 0;
				BYTE* src = &line[x * FreeRDPGetBytesPerPixel(surface->format)];
				Stream_Read_UINT8(s, a);
				color = FreeRDPReadColor(src, surface->format);
				FreeRDPSplitColor(color, surface->format, &r, &g, &b, NULL, NULL);
				color = FreeRDPGetColor(surface->format, r, g, b, a);
				FreeRDPWriteColor(src, surface->format, color);
			}
		}
	}
	else
	{
		UINT32 startOffsetX = 0;
		RECTANGLE_16 rect = { 0 };
		rect.left = (UINT16)MIN(UINT16_MAX, cmd->left);
		rect.top = (UINT16)MIN(UINT16_MAX, cmd->top);
		rect.right = (UINT16)MIN(UINT16_MAX, cmd->left + cmd->width);
		rect.bottom = (UINT16)MIN(UINT16_MAX, cmd->top + cmd->height);

		while (rect.top < rect.bottom)
		{
			UINT32 count = 0;
			BYTE a = 0;

			if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
				return ERROR_INVALID_DATA;

			Stream_Read_UINT8(s, a);
			Stream_Read_UINT8(s, count);

			if (count >= 0xFF)
			{
				if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
					return ERROR_INVALID_DATA;

				Stream_Read_UINT16(s, count);

				if (count >= 0xFFFF)
				{
					if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
						return ERROR_INVALID_DATA;

					Stream_Read_UINT32(s, count);
				}
			}

			if (!gdi_apply_alpha(surface->data, surface->format, surface->scanline, &rect,
			                     startOffsetX, count, a))
				return ERROR_INTERNAL_ERROR;

			startOffsetX += count;

			while (startOffsetX >= cmd->width)
			{
				startOffsetX -= cmd->width;
				rect.top++;
			}
		}
	}

	invalidRect.left = (UINT16)MIN(UINT16_MAX, cmd->left);
	invalidRect.top = (UINT16)MIN(UINT16_MAX, cmd->top);
	invalidRect.right = (UINT16)MIN(UINT16_MAX, cmd->right);
	invalidRect.bottom = (UINT16)MIN(UINT16_MAX, cmd->bottom);
	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);
	status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context, surface->surfaceId, 1,
	                      &invalidRect);

	if (status != CHANNEL_RC_OK)
		goto fail;

	status = gdi_interFrameUpdate(gdi, context);

fail:
	return status;
}

#if defined(WITH_GFX_FRAME_DUMP)
static void dump_cmd(const RDPGFX_SURFACE_COMMAND* cmd, UINT32 frameId)
{
	static UINT64 xxx = 0;
	const char* path = "/tmp/dump/";
	WINPR_ASSERT(cmd);
	char fname[1024] = { 0 };

	snprintf(fname, sizeof(fname), "%s/%08" PRIx64 ".raw", path, xxx++);
	FILE* fp = fopen(fname, "w");
	if (!fp)
		return;
	(void)fprintf(fp, "frameid: %" PRIu32 "\n", frameId);
	(void)fprintf(fp, "surfaceId: %" PRIu32 "\n", cmd->surfaceId);
	(void)fprintf(fp, "codecId: %" PRIu32 "\n", cmd->codecId);
	(void)fprintf(fp, "contextId: %" PRIu32 "\n", cmd->contextId);
	(void)fprintf(fp, "format: %" PRIu32 "\n", cmd->format);
	(void)fprintf(fp, "left: %" PRIu32 "\n", cmd->left);
	(void)fprintf(fp, "top: %" PRIu32 "\n", cmd->top);
	(void)fprintf(fp, "right: %" PRIu32 "\n", cmd->right);
	(void)fprintf(fp, "bottom: %" PRIu32 "\n", cmd->bottom);
	(void)fprintf(fp, "width: %" PRIu32 "\n", cmd->width);
	(void)fprintf(fp, "height: %" PRIu32 "\n", cmd->height);
	(void)fprintf(fp, "length: %" PRIu32 "\n", cmd->length);

	char* bdata = crypto_base64_encode_ex(cmd->data, cmd->length, FALSE);
	(void)fprintf(fp, "data: %s\n", bdata);
	free(bdata);
	fclose(fp);
}
#endif

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_Progressive(rdpGdi* gdi, RdpgfxClientContext* context,
                                           const RDPGFX_SURFACE_COMMAND* cmd)
{
	INT32 rc = 0;
	UINT status = CHANNEL_RC_OK;
	gdiGfxSurface* surface = NULL;
	REGION16 invalidRegion;
	const RECTANGLE_16* rects = NULL;
	UINT32 nrRects = 0;
	/**
	 * Note: Since this comes via a Wire-To-Surface-2 PDU the
	 * cmd's top/left/right/bottom/width/height members are always zero!
	 * The update region is determined during decompression.
	 */
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(context);
	WINPR_ASSERT(cmd);
	const UINT16 surfaceId = (UINT16)MIN(UINT16_MAX, cmd->surfaceId);

	WINPR_ASSERT(context->GetSurfaceData);
	surface = (gdiGfxSurface*)context->GetSurfaceData(context, surfaceId);

	if (!surface)
	{
		WLog_ERR(TAG, "unable to retrieve surfaceData for surfaceId=%" PRIu32 "", cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	if (!is_within_surface(surface, cmd))
		return ERROR_INVALID_DATA;

	WINPR_ASSERT(surface->codecs);
	rc = progressive_create_surface_context(surface->codecs->progressive, surfaceId, surface->width,
	                                        surface->height);

	if (rc < 0)
	{
		WLog_ERR(TAG, "progressive_create_surface_context failure: %" PRId32 "", rc);
		return ERROR_INTERNAL_ERROR;
	}

	region16_init(&invalidRegion);

	rc = progressive_decompress(surface->codecs->progressive, cmd->data, cmd->length, surface->data,
	                            surface->format, surface->scanline, cmd->left, cmd->top,
	                            &invalidRegion, surfaceId, gdi->frameId);

	if (rc < 0)
	{
		WLog_ERR(TAG, "progressive_decompress failure: %" PRId32 "", rc);
		region16_uninit(&invalidRegion);
		return ERROR_INTERNAL_ERROR;
	}

	rects = region16_rects(&invalidRegion, &nrRects);
	status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context, surface->surfaceId,
	                      nrRects, rects);

	if (status != CHANNEL_RC_OK)
		goto fail;

	for (UINT32 x = 0; x < nrRects; x++)
		region16_union_rect(&surface->invalidRegion, &surface->invalidRegion, &rects[x]);

	region16_uninit(&invalidRegion);

	status = gdi_interFrameUpdate(gdi, context);

fail:
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand(RdpgfxClientContext* context, const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = CHANNEL_RC_OK;
	rdpGdi* gdi = NULL;

	if (!context || !cmd)
		return ERROR_INVALID_PARAMETER;

	gdi = (rdpGdi*)context->custom;

	EnterCriticalSection(&context->mux);
	WLog_Print(gdi->log, WLOG_TRACE,
	           "surfaceId=%" PRIu32 ", codec=%s [%" PRIu32 "], contextId=%" PRIu32 ", format=%s, "
	           "left=%" PRIu32 ", top=%" PRIu32 ", right=%" PRIu32 ", bottom=%" PRIu32
	           ", width=%" PRIu32 ", height=%" PRIu32 " "
	           "length=%" PRIu32 ", data=%p, extra=%p",
	           cmd->surfaceId, rdpgfx_get_codec_id_string(cmd->codecId), cmd->codecId,
	           cmd->contextId, FreeRDPGetColorFormatName(cmd->format), cmd->left, cmd->top,
	           cmd->right, cmd->bottom, cmd->width, cmd->height, cmd->length, (void*)cmd->data,
	           (void*)cmd->extra);
#if defined(WITH_GFX_FRAME_DUMP)
	dump_cmd(cmd, gdi->frameId);
#endif

	switch (cmd->codecId)
	{
		case RDPGFX_CODECID_UNCOMPRESSED:
			status = gdi_SurfaceCommand_Uncompressed(gdi, context, cmd);
			break;

		case RDPGFX_CODECID_CAVIDEO:
			status = gdi_SurfaceCommand_RemoteFX(gdi, context, cmd);
			break;

		case RDPGFX_CODECID_CLEARCODEC:
			status = gdi_SurfaceCommand_ClearCodec(gdi, context, cmd);
			break;

		case RDPGFX_CODECID_PLANAR:
			status = gdi_SurfaceCommand_Planar(gdi, context, cmd);
			break;

		case RDPGFX_CODECID_AVC420:
			status = gdi_SurfaceCommand_AVC420(gdi, context, cmd);
			break;

		case RDPGFX_CODECID_AVC444v2:
		case RDPGFX_CODECID_AVC444:
			status = gdi_SurfaceCommand_AVC444(gdi, context, cmd);
			break;

		case RDPGFX_CODECID_ALPHA:
			status = gdi_SurfaceCommand_Alpha(gdi, context, cmd);
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE:
			status = gdi_SurfaceCommand_Progressive(gdi, context, cmd);
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE_V2:
			WLog_WARN(TAG, "SurfaceCommand %s [0x%08" PRIX32 "] not implemented",
			          rdpgfx_get_codec_id_string(cmd->codecId), cmd->codecId);
			break;

		default:
			WLog_WARN(TAG, "Invalid SurfaceCommand %s [0x%08" PRIX32 "]",
			          rdpgfx_get_codec_id_string(cmd->codecId), cmd->codecId);
			break;
	}

	LeaveCriticalSection(&context->mux);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT
gdi_DeleteEncodingContext(RdpgfxClientContext* context,
                          const RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(deleteEncodingContext);
	WINPR_UNUSED(context);
	WINPR_UNUSED(deleteEncodingContext);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_CreateSurface(RdpgfxClientContext* context,
                              const RDPGFX_CREATE_SURFACE_PDU* createSurface)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	gdiGfxSurface* surface = NULL;
	rdpGdi* gdi = NULL;
	WINPR_ASSERT(context);
	WINPR_ASSERT(createSurface);
	gdi = (rdpGdi*)context->custom;
	WINPR_ASSERT(gdi);
	WINPR_ASSERT(gdi->context);
	EnterCriticalSection(&context->mux);
	surface = (gdiGfxSurface*)calloc(1, sizeof(gdiGfxSurface));

	if (!surface)
		goto fail;

	if (!freerdp_settings_get_bool(gdi->context->settings, FreeRDP_DeactivateClientDecoding))
	{
		WINPR_ASSERT(context->codecs);
		surface->codecs = context->codecs;

		if (!surface->codecs)
		{
			free(surface);
			goto fail;
		}
	}

	surface->surfaceId = createSurface->surfaceId;
	surface->width = gfx_align_scanline(createSurface->width, 16);
	surface->height = gfx_align_scanline(createSurface->height, 16);
	surface->mappedWidth = createSurface->width;
	surface->mappedHeight = createSurface->height;
	surface->outputTargetWidth = createSurface->width;
	surface->outputTargetHeight = createSurface->height;

	switch (createSurface->pixelFormat)
	{
		case GFX_PIXEL_FORMAT_ARGB_8888:
			surface->format = PIXEL_FORMAT_BGRA32;
			break;

		case GFX_PIXEL_FORMAT_XRGB_8888:
			surface->format = PIXEL_FORMAT_BGRX32;
			break;

		default:
			free(surface);
			goto fail;
	}

	surface->scanline = gfx_align_scanline(surface->width * 4UL, 16);
	surface->data = (BYTE*)winpr_aligned_malloc(1ull * surface->scanline * surface->height, 16);

	if (!surface->data)
	{
		free(surface);
		goto fail;
	}

	memset(surface->data, 0xFF, (size_t)surface->scanline * surface->height);
	region16_init(&surface->invalidRegion);

	WINPR_ASSERT(context->SetSurfaceData);
	rc = context->SetSurfaceData(context, surface->surfaceId, (void*)surface);
fail:
	LeaveCriticalSection(&context->mux);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_DeleteSurface(RdpgfxClientContext* context,
                              const RDPGFX_DELETE_SURFACE_PDU* deleteSurface)
{
	UINT rc = CHANNEL_RC_OK;
	UINT res = ERROR_INTERNAL_ERROR;
	rdpCodecs* codecs = NULL;
	gdiGfxSurface* surface = NULL;
	EnterCriticalSection(&context->mux);

	WINPR_ASSERT(context->GetSurfaceData);
	surface = (gdiGfxSurface*)context->GetSurfaceData(context, deleteSurface->surfaceId);

	if (surface)
	{
		if (surface->windowMapped)
			rc = IFCALLRESULT(CHANNEL_RC_OK, context->UnmapWindowForSurface, context,
			                  surface->windowId);

#ifdef WITH_GFX_H264
		h264_context_free(surface->h264);
#endif
		region16_uninit(&surface->invalidRegion);
		codecs = surface->codecs;
		winpr_aligned_free(surface->data);
		free(surface);
	}

	WINPR_ASSERT(context->SetSurfaceData);
	res = context->SetSurfaceData(context, deleteSurface->surfaceId, NULL);
	if (res)
		rc = res;

	if (codecs && codecs->progressive)
		progressive_delete_surface_context(codecs->progressive, deleteSurface->surfaceId);

	LeaveCriticalSection(&context->mux);
	return rc;
}

static BOOL intersect_rect(const RECTANGLE_16* rect, const gdiGfxSurface* surface,
                           RECTANGLE_16* prect)
{
	WINPR_ASSERT(rect);
	WINPR_ASSERT(surface);
	WINPR_ASSERT(prect);

	if (rect->left > rect->right)
		return FALSE;
	if (rect->left > surface->width)
		return FALSE;
	if (rect->top > rect->bottom)
		return FALSE;
	if (rect->top > surface->height)
		return FALSE;
	prect->left = rect->left;
	prect->top = rect->top;
	prect->right = MIN(rect->right, surface->width);
	prect->bottom = MIN(rect->bottom, surface->height);
	return TRUE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SolidFill(RdpgfxClientContext* context, const RDPGFX_SOLID_FILL_PDU* solidFill)
{
	UINT status = ERROR_INTERNAL_ERROR;
	BYTE a = 0xff;
	RECTANGLE_16 invalidRect = { 0 };
	rdpGdi* gdi = (rdpGdi*)context->custom;

	EnterCriticalSection(&context->mux);

	WINPR_ASSERT(context->GetSurfaceData);
	gdiGfxSurface* surface = (gdiGfxSurface*)context->GetSurfaceData(context, solidFill->surfaceId);

	if (!surface)
		goto fail;

	const BYTE b = solidFill->fillPixel.B;
	const BYTE g = solidFill->fillPixel.G;
	const BYTE r = solidFill->fillPixel.R;

#if 0
	/* [MS-RDPEGFX] 3.3.5.4 Processing an RDPGFX_SOLIDFILL_PDU message
	 * https://learn.microsoft.com/en-us/windows/win32/gdi/binary-raster-operations
	 *
	 * this sounds like the alpha value is always ignored.
	 */
	if (FreeRDPColorHasAlpha(surface->format))
		a = solidFill->fillPixel.XA;
#endif

	const UINT32 color = FreeRDPGetColor(surface->format, r, g, b, a);

	for (UINT16 index = 0; index < solidFill->fillRectCount; index++)
	{
		const RECTANGLE_16* rect = &(solidFill->fillRects[index]);

		if (!intersect_rect(rect, surface, &invalidRect))
			goto fail;

		const UINT32 nWidth = invalidRect.right - invalidRect.left;
		const UINT32 nHeight = invalidRect.bottom - invalidRect.top;

		if (!freerdp_image_fill(surface->data, surface->format, surface->scanline, invalidRect.left,
		                        invalidRect.top, nWidth, nHeight, color))
			goto fail;

		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);
	}

	status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context, surface->surfaceId,
	                      solidFill->fillRectCount, solidFill->fillRects);

	if (status != CHANNEL_RC_OK)
		goto fail;

	LeaveCriticalSection(&context->mux);

	return gdi_interFrameUpdate(gdi, context);
fail:
	LeaveCriticalSection(&context->mux);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceToSurface(RdpgfxClientContext* context,
                                 const RDPGFX_SURFACE_TO_SURFACE_PDU* surfaceToSurface)
{
	UINT status = ERROR_INTERNAL_ERROR;
	BOOL sameSurface = 0;
	UINT32 nWidth = 0;
	UINT32 nHeight = 0;
	const RECTANGLE_16* rectSrc = NULL;
	RECTANGLE_16 invalidRect;
	gdiGfxSurface* surfaceSrc = NULL;
	gdiGfxSurface* surfaceDst = NULL;
	rdpGdi* gdi = (rdpGdi*)context->custom;
	EnterCriticalSection(&context->mux);
	rectSrc = &(surfaceToSurface->rectSrc);

	WINPR_ASSERT(context->GetSurfaceData);
	surfaceSrc = (gdiGfxSurface*)context->GetSurfaceData(context, surfaceToSurface->surfaceIdSrc);
	sameSurface =
	    (surfaceToSurface->surfaceIdSrc == surfaceToSurface->surfaceIdDest) ? TRUE : FALSE;

	if (!sameSurface)
		surfaceDst =
		    (gdiGfxSurface*)context->GetSurfaceData(context, surfaceToSurface->surfaceIdDest);
	else
		surfaceDst = surfaceSrc;

	if (!surfaceSrc || !surfaceDst)
		goto fail;

	if (!is_rect_valid(rectSrc, surfaceSrc->width, surfaceSrc->height))
		goto fail;

	nWidth = rectSrc->right - rectSrc->left;
	nHeight = rectSrc->bottom - rectSrc->top;

	for (UINT16 index = 0; index < surfaceToSurface->destPtsCount; index++)
	{
		const RDPGFX_POINT16* destPt = &surfaceToSurface->destPts[index];
		const RECTANGLE_16 rect = { destPt->x, destPt->y,
			                        (UINT16)MIN(UINT16_MAX, destPt->x + nWidth),
			                        (UINT16)MIN(UINT16_MAX, destPt->y + nHeight) };
		if (!is_rect_valid(&rect, surfaceDst->width, surfaceDst->height))
			goto fail;

		if (!freerdp_image_copy(surfaceDst->data, surfaceDst->format, surfaceDst->scanline,
		                        destPt->x, destPt->y, nWidth, nHeight, surfaceSrc->data,
		                        surfaceSrc->format, surfaceSrc->scanline, rectSrc->left,
		                        rectSrc->top, NULL, FREERDP_FLIP_NONE))
			goto fail;

		invalidRect = rect;
		region16_union_rect(&surfaceDst->invalidRegion, &surfaceDst->invalidRegion, &invalidRect);
		status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context,
		                      surfaceDst->surfaceId, 1, &invalidRect);

		if (status != CHANNEL_RC_OK)
			goto fail;
	}

	LeaveCriticalSection(&context->mux);

	return gdi_interFrameUpdate(gdi, context);
fail:
	LeaveCriticalSection(&context->mux);
	return status;
}

static void gdi_GfxCacheEntryFree(gdiGfxCacheEntry* entry)
{
	if (!entry)
		return;
	free(entry->data);
	free(entry);
}

static gdiGfxCacheEntry* gdi_GfxCacheEntryNew(UINT64 cacheKey, UINT32 width, UINT32 height,
                                              UINT32 format)
{
	gdiGfxCacheEntry* cacheEntry = (gdiGfxCacheEntry*)calloc(1, sizeof(gdiGfxCacheEntry));
	if (!cacheEntry)
		goto fail;

	cacheEntry->cacheKey = cacheKey;
	cacheEntry->width = width;
	cacheEntry->height = height;
	cacheEntry->format = format;
	cacheEntry->scanline = gfx_align_scanline(cacheEntry->width * 4, 16);

	if ((cacheEntry->width > 0) && (cacheEntry->height > 0))
	{
		cacheEntry->data = (BYTE*)calloc(cacheEntry->height, cacheEntry->scanline);

		if (!cacheEntry->data)
			goto fail;
	}
	return cacheEntry;
fail:
	gdi_GfxCacheEntryFree(cacheEntry);
	return NULL;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceToCache(RdpgfxClientContext* context,
                               const RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache)
{
	const RECTANGLE_16* rect = NULL;
	gdiGfxSurface* surface = NULL;
	gdiGfxCacheEntry* cacheEntry = NULL;
	UINT rc = ERROR_INTERNAL_ERROR;
	EnterCriticalSection(&context->mux);
	rect = &(surfaceToCache->rectSrc);

	WINPR_ASSERT(context->GetSurfaceData);
	surface = (gdiGfxSurface*)context->GetSurfaceData(context, surfaceToCache->surfaceId);

	if (!surface)
		goto fail;

	if (!is_rect_valid(rect, surface->width, surface->height))
		goto fail;

	cacheEntry = gdi_GfxCacheEntryNew(surfaceToCache->cacheKey, (UINT32)(rect->right - rect->left),
	                                  (UINT32)(rect->bottom - rect->top), surface->format);

	if (!cacheEntry)
		goto fail;

	if (!cacheEntry->data)
		goto fail;

	if (!freerdp_image_copy_no_overlap(cacheEntry->data, cacheEntry->format, cacheEntry->scanline,
	                                   0, 0, cacheEntry->width, cacheEntry->height, surface->data,
	                                   surface->format, surface->scanline, rect->left, rect->top,
	                                   NULL, FREERDP_FLIP_NONE))
		goto fail;

	RDPGFX_EVICT_CACHE_ENTRY_PDU evict = { surfaceToCache->cacheSlot };
	WINPR_ASSERT(context->EvictCacheEntry);
	context->EvictCacheEntry(context, &evict);

	WINPR_ASSERT(context->SetCacheSlotData);
	rc = context->SetCacheSlotData(context, surfaceToCache->cacheSlot, (void*)cacheEntry);
fail:
	if (rc != CHANNEL_RC_OK)
		gdi_GfxCacheEntryFree(cacheEntry);
	LeaveCriticalSection(&context->mux);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_CacheToSurface(RdpgfxClientContext* context,
                               const RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface)
{
	UINT status = ERROR_INTERNAL_ERROR;
	gdiGfxSurface* surface = NULL;
	gdiGfxCacheEntry* cacheEntry = NULL;
	RECTANGLE_16 invalidRect;
	rdpGdi* gdi = (rdpGdi*)context->custom;

	EnterCriticalSection(&context->mux);

	WINPR_ASSERT(context->GetSurfaceData);
	surface = (gdiGfxSurface*)context->GetSurfaceData(context, cacheToSurface->surfaceId);

	WINPR_ASSERT(context->GetCacheSlotData);
	cacheEntry = (gdiGfxCacheEntry*)context->GetCacheSlotData(context, cacheToSurface->cacheSlot);

	if (!surface || !cacheEntry)
		goto fail;

	for (UINT16 index = 0; index < cacheToSurface->destPtsCount; index++)
	{
		const RDPGFX_POINT16* destPt = &cacheToSurface->destPts[index];
		const RECTANGLE_16 rect = { destPt->x, destPt->y,
			                        (UINT16)MIN(UINT16_MAX, destPt->x + cacheEntry->width),
			                        (UINT16)MIN(UINT16_MAX, destPt->y + cacheEntry->height) };

		if (rectangle_is_empty(&rect))
			continue;

		if (!is_rect_valid(&rect, surface->width, surface->height))
			goto fail;

		if (!freerdp_image_copy_no_overlap(surface->data, surface->format, surface->scanline,
		                                   destPt->x, destPt->y, cacheEntry->width,
		                                   cacheEntry->height, cacheEntry->data, cacheEntry->format,
		                                   cacheEntry->scanline, 0, 0, NULL, FREERDP_FLIP_NONE))
			goto fail;

		invalidRect = rect;
		region16_union_rect(&surface->invalidRegion, &surface->invalidRegion, &invalidRect);
		status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaceArea, context,
		                      surface->surfaceId, 1, &invalidRect);

		if (status != CHANNEL_RC_OK)
			goto fail;
	}

	LeaveCriticalSection(&context->mux);

	return gdi_interFrameUpdate(gdi, context);

fail:
	LeaveCriticalSection(&context->mux);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_CacheImportReply(RdpgfxClientContext* context,
                                 const RDPGFX_CACHE_IMPORT_REPLY_PDU* cacheImportReply)
{
	UINT16 count = 0;
	const UINT16* slots = NULL;
	UINT error = CHANNEL_RC_OK;

	slots = cacheImportReply->cacheSlots;
	count = cacheImportReply->importedEntriesCount;

	for (UINT16 index = 0; index < count; index++)
	{
		UINT16 cacheSlot = slots[index];

		if (cacheSlot == 0)
			continue;

		WINPR_ASSERT(context->GetCacheSlotData);
		gdiGfxCacheEntry* cacheEntry =
		    (gdiGfxCacheEntry*)context->GetCacheSlotData(context, cacheSlot);

		if (cacheEntry)
			continue;

		cacheEntry = gdi_GfxCacheEntryNew(cacheSlot, 0, 0, PIXEL_FORMAT_BGRX32);

		if (!cacheEntry)
			return ERROR_INTERNAL_ERROR;

		WINPR_ASSERT(context->SetCacheSlotData);
		error = context->SetCacheSlotData(context, cacheSlot, (void*)cacheEntry);

		if (error)
		{
			WLog_ERR(TAG, "CacheImportReply: SetCacheSlotData failed with error %" PRIu32 "",
			         error);
			gdi_GfxCacheEntryFree(cacheEntry);
			break;
		}
	}

	return error;
}

static UINT gdi_ImportCacheEntry(RdpgfxClientContext* context, UINT16 cacheSlot,
                                 const PERSISTENT_CACHE_ENTRY* importCacheEntry)
{
	UINT error = ERROR_INTERNAL_ERROR;
	gdiGfxCacheEntry* cacheEntry = NULL;

	if (cacheSlot == 0)
		return CHANNEL_RC_OK;

	cacheEntry = gdi_GfxCacheEntryNew(importCacheEntry->key64, importCacheEntry->width,
	                                  importCacheEntry->height, PIXEL_FORMAT_BGRX32);

	if (!cacheEntry)
		goto fail;

	if (!freerdp_image_copy_no_overlap(cacheEntry->data, cacheEntry->format, cacheEntry->scanline,
	                                   0, 0, cacheEntry->width, cacheEntry->height,
	                                   importCacheEntry->data, PIXEL_FORMAT_BGRX32, 0, 0, 0, NULL,
	                                   FREERDP_FLIP_NONE))
		goto fail;

	RDPGFX_EVICT_CACHE_ENTRY_PDU evict = { cacheSlot };
	WINPR_ASSERT(context->EvictCacheEntry);
	error = context->EvictCacheEntry(context, &evict);
	if (error != CHANNEL_RC_OK)
		goto fail;

	WINPR_ASSERT(context->SetCacheSlotData);
	error = context->SetCacheSlotData(context, cacheSlot, (void*)cacheEntry);

fail:
	if (error)
	{
		gdi_GfxCacheEntryFree(cacheEntry);
		WLog_ERR(TAG, "ImportCacheEntry: SetCacheSlotData failed with error %" PRIu32 "", error);
	}

	return error;
}

static UINT gdi_ExportCacheEntry(RdpgfxClientContext* context, UINT16 cacheSlot,
                                 PERSISTENT_CACHE_ENTRY* exportCacheEntry)
{
	gdiGfxCacheEntry* cacheEntry = NULL;

	WINPR_ASSERT(context->GetCacheSlotData);
	cacheEntry = (gdiGfxCacheEntry*)context->GetCacheSlotData(context, cacheSlot);

	if (cacheEntry)
	{
		exportCacheEntry->key64 = cacheEntry->cacheKey;
		exportCacheEntry->width = (UINT16)MIN(UINT16_MAX, cacheEntry->width);
		exportCacheEntry->height = (UINT16)MIN(UINT16_MAX, cacheEntry->height);
		exportCacheEntry->size = cacheEntry->width * cacheEntry->height * 4;
		exportCacheEntry->flags = 0;
		exportCacheEntry->data = cacheEntry->data;
		return CHANNEL_RC_OK;
	}

	return ERROR_NOT_FOUND;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_EvictCacheEntry(RdpgfxClientContext* context,
                                const RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry)
{
	gdiGfxCacheEntry* cacheEntry = NULL;
	UINT rc = ERROR_NOT_FOUND;

	WINPR_ASSERT(context);
	WINPR_ASSERT(evictCacheEntry);

	EnterCriticalSection(&context->mux);

	WINPR_ASSERT(context->GetCacheSlotData);
	cacheEntry = (gdiGfxCacheEntry*)context->GetCacheSlotData(context, evictCacheEntry->cacheSlot);

	gdi_GfxCacheEntryFree(cacheEntry);

	WINPR_ASSERT(context->SetCacheSlotData);
	rc = context->SetCacheSlotData(context, evictCacheEntry->cacheSlot, NULL);
	LeaveCriticalSection(&context->mux);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_MapSurfaceToOutput(RdpgfxClientContext* context,
                                   const RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	gdiGfxSurface* surface = NULL;
	EnterCriticalSection(&context->mux);

	WINPR_ASSERT(context->GetSurfaceData);
	surface = (gdiGfxSurface*)context->GetSurfaceData(context, surfaceToOutput->surfaceId);

	if (!surface)
		goto fail;

	if (surface->windowMapped)
	{
		WLog_WARN(TAG, "surface already windowMapped when trying to set outputMapped");
		goto fail;
	}

	surface->outputMapped = TRUE;
	surface->outputOriginX = surfaceToOutput->outputOriginX;
	surface->outputOriginY = surfaceToOutput->outputOriginY;
	surface->outputTargetWidth = surface->mappedWidth;
	surface->outputTargetHeight = surface->mappedHeight;
	region16_clear(&surface->invalidRegion);
	rc = CHANNEL_RC_OK;
fail:
	LeaveCriticalSection(&context->mux);
	return rc;
}

static UINT
gdi_MapSurfaceToScaledOutput(RdpgfxClientContext* context,
                             const RDPGFX_MAP_SURFACE_TO_SCALED_OUTPUT_PDU* surfaceToOutput)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	gdiGfxSurface* surface = NULL;
	EnterCriticalSection(&context->mux);

	WINPR_ASSERT(context->GetSurfaceData);
	surface = (gdiGfxSurface*)context->GetSurfaceData(context, surfaceToOutput->surfaceId);

	if (!surface)
		goto fail;

	if (surface->windowMapped)
	{
		WLog_WARN(TAG, "surface already windowMapped when trying to set outputMapped");
		goto fail;
	}

	surface->outputMapped = TRUE;
	surface->outputOriginX = surfaceToOutput->outputOriginX;
	surface->outputOriginY = surfaceToOutput->outputOriginY;
	surface->outputTargetWidth = surfaceToOutput->targetWidth;
	surface->outputTargetHeight = surfaceToOutput->targetHeight;
	region16_clear(&surface->invalidRegion);
	rc = CHANNEL_RC_OK;
fail:
	LeaveCriticalSection(&context->mux);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_MapSurfaceToWindow(RdpgfxClientContext* context,
                                   const RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	gdiGfxSurface* surface = NULL;
	EnterCriticalSection(&context->mux);

	WINPR_ASSERT(context->GetSurfaceData);
	surface = (gdiGfxSurface*)context->GetSurfaceData(context, surfaceToWindow->surfaceId);

	if (!surface)
		goto fail;

	if (surface->outputMapped)
	{
		WLog_WARN(TAG, "surface already outputMapped when trying to set windowMapped");
		goto fail;
	}

	if (surface->windowMapped)
	{
		if (surface->windowId != surfaceToWindow->windowId)
		{
			WLog_WARN(TAG, "surface windowId mismatch, has %" PRIu64 ", expected %" PRIu64,
			          surface->windowId, surfaceToWindow->windowId);
			goto fail;
		}
	}
	surface->windowMapped = TRUE;

	surface->windowId = surfaceToWindow->windowId;
	surface->mappedWidth = surfaceToWindow->mappedWidth;
	surface->mappedHeight = surfaceToWindow->mappedHeight;
	surface->outputTargetWidth = surfaceToWindow->mappedWidth;
	surface->outputTargetHeight = surfaceToWindow->mappedHeight;
	rc = IFCALLRESULT(CHANNEL_RC_OK, context->MapWindowForSurface, context,
	                  surfaceToWindow->surfaceId, surfaceToWindow->windowId);
fail:
	LeaveCriticalSection(&context->mux);
	return rc;
}

static UINT
gdi_MapSurfaceToScaledWindow(RdpgfxClientContext* context,
                             const RDPGFX_MAP_SURFACE_TO_SCALED_WINDOW_PDU* surfaceToWindow)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	gdiGfxSurface* surface = NULL;
	EnterCriticalSection(&context->mux);

	WINPR_ASSERT(context->GetSurfaceData);
	surface = (gdiGfxSurface*)context->GetSurfaceData(context, surfaceToWindow->surfaceId);

	if (!surface)
		goto fail;

	if (surface->outputMapped)
	{
		WLog_WARN(TAG, "surface already outputMapped when trying to set windowMapped");
		goto fail;
	}

	if (surface->windowMapped)
	{
		if (surface->windowId != surfaceToWindow->windowId)
		{
			WLog_WARN(TAG, "surface windowId mismatch, has %" PRIu64 ", expected %" PRIu64,
			          surface->windowId, surfaceToWindow->windowId);
			goto fail;
		}
	}
	surface->windowMapped = TRUE;

	surface->windowId = surfaceToWindow->windowId;
	surface->mappedWidth = surfaceToWindow->mappedWidth;
	surface->mappedHeight = surfaceToWindow->mappedHeight;
	surface->outputTargetWidth = surfaceToWindow->targetWidth;
	surface->outputTargetHeight = surfaceToWindow->targetHeight;
	rc = IFCALLRESULT(CHANNEL_RC_OK, context->MapWindowForSurface, context,
	                  surfaceToWindow->surfaceId, surfaceToWindow->windowId);
fail:
	LeaveCriticalSection(&context->mux);
	return rc;
}

BOOL gdi_graphics_pipeline_init(rdpGdi* gdi, RdpgfxClientContext* gfx)
{
	return gdi_graphics_pipeline_init_ex(gdi, gfx, NULL, NULL, NULL);
}

BOOL gdi_graphics_pipeline_init_ex(rdpGdi* gdi, RdpgfxClientContext* gfx,
                                   pcRdpgfxMapWindowForSurface map,
                                   pcRdpgfxUnmapWindowForSurface unmap,
                                   pcRdpgfxUpdateSurfaceArea update)
{
	if (!gdi || !gfx || !gdi->context || !gdi->context->settings)
		return FALSE;

	rdpContext* context = gdi->context;
	rdpSettings* settings = context->settings;

	gdi->gfx = gfx;
	gfx->custom = (void*)gdi;
	gfx->ResetGraphics = gdi_ResetGraphics;
	gfx->StartFrame = gdi_StartFrame;
	gfx->EndFrame = gdi_EndFrame;
	gfx->SurfaceCommand = gdi_SurfaceCommand;
	gfx->DeleteEncodingContext = gdi_DeleteEncodingContext;
	gfx->CreateSurface = gdi_CreateSurface;
	gfx->DeleteSurface = gdi_DeleteSurface;
	gfx->SolidFill = gdi_SolidFill;
	gfx->SurfaceToSurface = gdi_SurfaceToSurface;
	gfx->SurfaceToCache = gdi_SurfaceToCache;
	gfx->CacheToSurface = gdi_CacheToSurface;
	gfx->CacheImportReply = gdi_CacheImportReply;
	gfx->ImportCacheEntry = gdi_ImportCacheEntry;
	gfx->ExportCacheEntry = gdi_ExportCacheEntry;
	gfx->EvictCacheEntry = gdi_EvictCacheEntry;
	gfx->MapSurfaceToOutput = gdi_MapSurfaceToOutput;
	gfx->MapSurfaceToWindow = gdi_MapSurfaceToWindow;
	gfx->MapSurfaceToScaledOutput = gdi_MapSurfaceToScaledOutput;
	gfx->MapSurfaceToScaledWindow = gdi_MapSurfaceToScaledWindow;
	gfx->UpdateSurfaces = gdi_UpdateSurfaces;
	gfx->MapWindowForSurface = map;
	gfx->UnmapWindowForSurface = unmap;
	gfx->UpdateSurfaceArea = update;

	if (!freerdp_settings_get_bool(settings, FreeRDP_DeactivateClientDecoding))
	{
		const UINT32 w = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
		const UINT32 h = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
		const UINT32 flags = freerdp_settings_get_uint32(settings, FreeRDP_ThreadingFlags);

		gfx->codecs = freerdp_client_codecs_new(flags);
		if (!gfx->codecs)
			return FALSE;
		if (!freerdp_client_codecs_prepare(gfx->codecs, FREERDP_CODEC_ALL, w, h))
			return FALSE;
	}
	InitializeCriticalSection(&gfx->mux);
	PROFILER_CREATE(gfx->SurfaceProfiler, "GFX-PROFILER")

	/**
	 * gdi->graphicsReset will be removed in FreeRDP v3 from public headers,
	 * since the EGFX Reset Graphics PDU seems to be optional.
	 * There are still some clients that expect and check it and therefore
	 * we simply initialize it with TRUE here for now.
	 */
	gdi->graphicsReset = TRUE;
	if (freerdp_settings_get_bool(settings, FreeRDP_DeactivateClientDecoding))
	{
		gfx->UpdateSurfaceArea = NULL;
		gfx->UpdateSurfaces = NULL;
		gfx->SurfaceCommand = NULL;
	}

	return TRUE;
}

void gdi_graphics_pipeline_uninit(rdpGdi* gdi, RdpgfxClientContext* gfx)
{
	if (gdi)
		gdi->gfx = NULL;

	if (!gfx)
		return;

	gfx->custom = NULL;
	freerdp_client_codecs_free(gfx->codecs);
	gfx->codecs = NULL;
	DeleteCriticalSection(&gfx->mux);
	PROFILER_PRINT_HEADER
	PROFILER_PRINT(gfx->SurfaceProfiler)
	PROFILER_PRINT_FOOTER
	PROFILER_FREE(gfx->SurfaceProfiler)
}

const char* rdpgfx_caps_version_str(UINT32 capsVersion)
{
	switch (capsVersion)
	{
		case RDPGFX_CAPVERSION_8:
			return "RDPGFX_CAPVERSION_8";
		case RDPGFX_CAPVERSION_81:
			return "RDPGFX_CAPVERSION_81";
		case RDPGFX_CAPVERSION_10:
			return "RDPGFX_CAPVERSION_10";
		case RDPGFX_CAPVERSION_101:
			return "RDPGFX_CAPVERSION_101";
		case RDPGFX_CAPVERSION_102:
			return "RDPGFX_CAPVERSION_102";
		case RDPGFX_CAPVERSION_103:
			return "RDPGFX_CAPVERSION_103";
		case RDPGFX_CAPVERSION_104:
			return "RDPGFX_CAPVERSION_104";
		case RDPGFX_CAPVERSION_105:
			return "RDPGFX_CAPVERSION_105";
		case RDPGFX_CAPVERSION_106:
			return "RDPGFX_CAPVERSION_106";
		case RDPGFX_CAPVERSION_106_ERR:
			return "RDPGFX_CAPVERSION_106_ERR";
		case RDPGFX_CAPVERSION_107:
			return "RDPGFX_CAPVERSION_107";
		default:
			return "RDPGFX_CAPVERSION_UNKNOWN";
	}
}
