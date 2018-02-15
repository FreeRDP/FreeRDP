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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/log.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/gdi/region.h>

#define TAG FREERDP_TAG("gdi")

static DWORD gfx_align_scanline(DWORD widthInBytes, DWORD alignment)
{
	const UINT32 align = 16;
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
	UINT32 index;
	UINT16 count;
	UINT32 DesktopWidth;
	UINT32 DesktopHeight;
	gdiGfxSurface* surface;
	UINT16* pSurfaceIds = NULL;
	rdpGdi* gdi = (rdpGdi*) context->custom;
	rdpUpdate* update = gdi->context->update;
	rdpSettings* settings = gdi->context->settings;
	DesktopWidth = resetGraphics->width;
	DesktopHeight = resetGraphics->height;

	if ((DesktopWidth != settings->DesktopWidth) || (DesktopHeight != settings->DesktopHeight))
	{
		settings->DesktopWidth = DesktopWidth;
		settings->DesktopHeight = DesktopHeight;

		if (update)
			update->DesktopResize(gdi->context);
	}

	context->GetSurfaceIds(context, &pSurfaceIds, &count);

	for (index = 0; index < count; index++)
	{
		surface = (gdiGfxSurface*) context->GetSurfaceData(context, pSurfaceIds[index]);

		if (!surface || !surface->outputMapped)
			continue;

		region16_clear(&surface->invalidRegion);
	}

	free(pSurfaceIds);

	if (!freerdp_client_codecs_reset(gdi->context->codecs, FREERDP_CODEC_ALL,
	                                 gdi->width, gdi->height))
		return ERROR_INTERNAL_ERROR;

	gdi->graphicsReset = TRUE;
	return CHANNEL_RC_OK;
}

static UINT gdi_OutputUpdate(rdpGdi* gdi, gdiGfxSurface* surface)
{
	UINT32 nXDst, nYDst;
	UINT32 nXSrc, nYSrc;
	UINT16 width, height;
	UINT32 surfaceX, surfaceY;
	RECTANGLE_16 surfaceRect;
	const RECTANGLE_16* rects;
	UINT32 i, nbRects;
	rdpUpdate* update = gdi->context->update;

	if (gdi->suppressOutput)
		return CHANNEL_RC_OK;

	surfaceX = surface->outputOriginX;
	surfaceY = surface->outputOriginY;
	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = surface->width;
	surfaceRect.bottom = surface->height;
	region16_intersect_rect(&(surface->invalidRegion),
	                        &(surface->invalidRegion), &surfaceRect);

	if (!(rects = region16_rects(&surface->invalidRegion, &nbRects)) || !nbRects)
		return CHANNEL_RC_OK;

	update->BeginPaint(gdi->context);

	for (i = 0; i < nbRects; i++)
	{
		nXSrc = rects[i].left;
		nYSrc = rects[i].top;
		nXDst = surfaceX + nXSrc;
		nYDst = surfaceY + nYSrc;
		width = rects[i].right - rects[i].left;
		height = rects[i].bottom - rects[i].top;

		if (!freerdp_image_copy(gdi->primary_buffer, gdi->primary->hdc->format,
		                        gdi->stride,
		                        nXDst, nYDst, width, height,
		                        surface->data, surface->format,
		                        surface->scanline, nXSrc, nYSrc, NULL, FREERDP_FLIP_NONE))
			return CHANNEL_RC_NULL_DATA;

		gdi_InvalidateRegion(gdi->primary->hdc, nXDst, nYDst, width, height);
	}

	update->EndPaint(gdi->context);
	region16_clear(&(surface->invalidRegion));
	return CHANNEL_RC_OK;
}

static UINT gdi_UpdateSurfaces(RdpgfxClientContext* context)
{
	UINT16 count;
	UINT16 index;
	UINT status = CHANNEL_RC_OK;
	gdiGfxSurface* surface;
	UINT16* pSurfaceIds = NULL;
	rdpGdi* gdi = (rdpGdi*)context->custom;

	if (!gdi->graphicsReset)
		return status;

	context->GetSurfaceIds(context, &pSurfaceIds, &count);

	for (index = 0; index < count; index++)
	{
		surface = (gdiGfxSurface*) context->GetSurfaceData(context, pSurfaceIds[index]);

		if (!surface || !surface->outputMapped)
			continue;

		status = gdi_OutputUpdate(gdi, surface);

		if (status != CHANNEL_RC_OK)
			break;
	}

	free(pSurfaceIds);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_StartFrame(RdpgfxClientContext* context,
                           const RDPGFX_START_FRAME_PDU* startFrame)
{
	rdpGdi* gdi = (rdpGdi*) context->custom;
	gdi->inGfxFrame = TRUE;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_EndFrame(RdpgfxClientContext* context,
                         const RDPGFX_END_FRAME_PDU* endFrame)
{
	UINT status = CHANNEL_RC_NOT_INITIALIZED;
	rdpGdi* gdi = (rdpGdi*) context->custom;
	IFCALLRET(context->UpdateSurfaces, status, context);
	gdi->inGfxFrame = FALSE;
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_Uncompressed(rdpGdi* gdi,
        RdpgfxClientContext* context,
        const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = CHANNEL_RC_OK;
	gdiGfxSurface* surface;
	RECTANGLE_16 invalidRect;
	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
	{
		WLog_ERR(TAG, "%s: unable to retrieve surfaceData for surfaceId=%"PRIu32"", __FUNCTION__,
		         cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	if (!freerdp_image_copy(surface->data, surface->format, surface->scanline,
	                        cmd->left, cmd->top, cmd->width, cmd->height,
	                        cmd->data, cmd->format, 0, 0, 0, NULL, FREERDP_FLIP_NONE))
		return ERROR_INTERNAL_ERROR;

	invalidRect.left = cmd->left;
	invalidRect.top = cmd->top;
	invalidRect.right = cmd->right;
	invalidRect.bottom = cmd->bottom;
	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion),
	                    &invalidRect);
	IFCALL(context->UpdateSurfaceArea, context, surface->surfaceId, 1, &invalidRect);

	if (!gdi->inGfxFrame)
	{
		status = CHANNEL_RC_NOT_INITIALIZED;
		IFCALLRET(context->UpdateSurfaces, status, context);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_RemoteFX(rdpGdi* gdi,
                                        RdpgfxClientContext* context,
                                        const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = CHANNEL_RC_OK;
	gdiGfxSurface* surface;
	REGION16 invalidRegion;
	const RECTANGLE_16* rects;
	UINT32 nrRects, x;
	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
	{
		WLog_ERR(TAG, "%s: unable to retrieve surfaceData for surfaceId=%"PRIu32"", __FUNCTION__,
		         cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	rfx_context_set_pixel_format(surface->codecs->rfx, cmd->format);
	region16_init(&invalidRegion);

	if (!rfx_process_message(surface->codecs->rfx, cmd->data, cmd->length,
	                         cmd->left, cmd->top,
	                         surface->data, surface->format, surface->scanline,
	                         surface->height, &invalidRegion))
	{
		WLog_ERR(TAG, "Failed to process RemoteFX message");
		region16_uninit(&invalidRegion);
		return ERROR_INTERNAL_ERROR;
	}

	rects = region16_rects(&invalidRegion, &nrRects);
	IFCALL(context->UpdateSurfaceArea, context, surface->surfaceId, nrRects, rects);

	for (x = 0; x < nrRects; x++)
		region16_union_rect(&surface->invalidRegion, &surface->invalidRegion, &rects[x]);

	region16_uninit(&invalidRegion);

	if (!gdi->inGfxFrame)
	{
		status = CHANNEL_RC_NOT_INITIALIZED;
		IFCALLRET(context->UpdateSurfaces, status, context);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_ClearCodec(rdpGdi* gdi,
        RdpgfxClientContext* context,
        const RDPGFX_SURFACE_COMMAND* cmd)
{
	INT32 rc;
	UINT status = CHANNEL_RC_OK;
	gdiGfxSurface* surface;
	RECTANGLE_16 invalidRect;
	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
	{
		WLog_ERR(TAG, "%s: unable to retrieve surfaceData for surfaceId=%"PRIu32"", __FUNCTION__,
		         cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	rc = clear_decompress(surface->codecs->clear, cmd->data, cmd->length,
	                      cmd->width, cmd->height,
	                      surface->data, surface->format,
	                      surface->scanline, cmd->left, cmd->top,
	                      surface->width, surface->height, &gdi->palette);

	if (rc < 0)
	{
		WLog_ERR(TAG, "clear_decompress failure: %"PRId32"", rc);
		return ERROR_INTERNAL_ERROR;
	}

	invalidRect.left = cmd->left;
	invalidRect.top = cmd->top;
	invalidRect.right = cmd->right;
	invalidRect.bottom = cmd->bottom;
	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion),
	                    &invalidRect);
	IFCALL(context->UpdateSurfaceArea, context, surface->surfaceId, 1, &invalidRect);

	if (!gdi->inGfxFrame)
	{
		status = CHANNEL_RC_NOT_INITIALIZED;
		IFCALLRET(context->UpdateSurfaces, status, context);
	}

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
	gdiGfxSurface* surface;
	RECTANGLE_16 invalidRect;
	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
	{
		WLog_ERR(TAG, "%s: unable to retrieve surfaceData for surfaceId=%"PRIu32"", __FUNCTION__,
		         cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	DstData = surface->data;

	if (!planar_decompress(surface->codecs->planar, cmd->data, cmd->length,
	                       cmd->width, cmd->height,
	                       DstData, surface->format,
	                       surface->scanline, cmd->left, cmd->top,
	                       cmd->width, cmd->height, FALSE))
		return ERROR_INTERNAL_ERROR;

	invalidRect.left = cmd->left;
	invalidRect.top = cmd->top;
	invalidRect.right = cmd->right;
	invalidRect.bottom = cmd->bottom;
	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion),
	                    &invalidRect);
	IFCALL(context->UpdateSurfaceArea, context, surface->surfaceId, 1, &invalidRect);

	if (!gdi->inGfxFrame)
	{
		status = CHANNEL_RC_NOT_INITIALIZED;
		IFCALLRET(context->UpdateSurfaces, status, context);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_AVC420(rdpGdi* gdi,
                                      RdpgfxClientContext* context,
                                      const RDPGFX_SURFACE_COMMAND* cmd)
{
#ifdef WITH_GFX_H264
	INT32 rc;
	UINT status = CHANNEL_RC_OK;
	UINT32 i;
	gdiGfxSurface* surface;
	RDPGFX_H264_METABLOCK* meta;
	RDPGFX_AVC420_BITMAP_STREAM* bs;
	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
	{
		WLog_ERR(TAG, "%s: unable to retrieve surfaceData for surfaceId=%"PRIu32"", __FUNCTION__,
		         cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	if (!surface->h264)
	{
		surface->h264 = h264_context_new(FALSE);

		if (!surface->h264)
		{
			WLog_ERR(TAG, "%s: unable to create h264 context", __FUNCTION__);
			return ERROR_NOT_ENOUGH_MEMORY;
		}

		if (!h264_context_reset(surface->h264, surface->width, surface->height))
			return ERROR_INTERNAL_ERROR;
	}

	bs = (RDPGFX_AVC420_BITMAP_STREAM*) cmd->extra;

	if (!bs)
		return ERROR_INTERNAL_ERROR;

	meta = &(bs->meta);
	rc = avc420_decompress(surface->h264, bs->data, bs->length, surface->data, surface->format,
	                       surface->scanline, surface->width,
	                       surface->height, meta->regionRects,
	                       meta->numRegionRects);

	if (rc < 0)
	{
		WLog_WARN(TAG, "avc420_decompress failure: %"PRIu32", ignoring update.", status);
		return CHANNEL_RC_OK;
	}

	for (i = 0; i < meta->numRegionRects; i++)
	{
		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion),
		                    (RECTANGLE_16*) & (meta->regionRects[i]));
	}

	IFCALL(context->UpdateSurfaceArea, context, surface->surfaceId,
	       meta->numRegionRects, meta->regionRects);

	if (!gdi->inGfxFrame)
	{
		status = CHANNEL_RC_NOT_INITIALIZED;
		IFCALLRET(context->UpdateSurfaces, status, context);
	}

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
	INT32 rc;
	UINT status = CHANNEL_RC_OK;
	UINT32 i;
	gdiGfxSurface* surface;
	RDPGFX_AVC444_BITMAP_STREAM* bs;
	RDPGFX_AVC420_BITMAP_STREAM* avc1;
	RDPGFX_H264_METABLOCK* meta1;
	RDPGFX_AVC420_BITMAP_STREAM* avc2;
	RDPGFX_H264_METABLOCK* meta2;
	RECTANGLE_16* regionRects = NULL;
	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
	{
		WLog_ERR(TAG, "%s: unable to retrieve surfaceData for surfaceId=%"PRIu32"", __FUNCTION__,
		         cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	if (!surface->h264)
	{
		surface->h264 = h264_context_new(FALSE);

		if (!surface->h264)
		{
			WLog_ERR(TAG, "%s: unable to create h264 context", __FUNCTION__);
			return ERROR_NOT_ENOUGH_MEMORY;
		}

		if (!h264_context_reset(surface->h264, surface->width, surface->height))
			return ERROR_INTERNAL_ERROR;
	}

	bs = (RDPGFX_AVC444_BITMAP_STREAM*) cmd->extra;

	if (!bs)
		return ERROR_INTERNAL_ERROR;

	avc1 = &bs->bitstream[0];
	avc2 = &bs->bitstream[1];
	meta1 = &avc1->meta;
	meta2 = &avc2->meta;
	rc = avc444_decompress(surface->h264, bs->LC,
	                       meta1->regionRects, meta1->numRegionRects,
	                       avc1->data, avc1->length,
	                       meta2->regionRects, meta2->numRegionRects,
	                       avc2->data, avc2->length,
	                       surface->data, surface->format,
	                       surface->scanline, surface->width,
	                       surface->height, cmd->codecId);

	if (rc < 0)
	{
		WLog_WARN(TAG, "avc444_decompress failure: %"PRIu32", ignoring update.", status);
		return CHANNEL_RC_OK;
	}

	for (i = 0; i < meta1->numRegionRects; i++)
	{
		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &(meta1->regionRects[i]));
	}

	IFCALL(context->UpdateSurfaceArea, context, surface->surfaceId,
	       meta1->numRegionRects, meta1->regionRects);

	for (i = 0; i < meta2->numRegionRects; i++)
	{
		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &(meta2->regionRects[i]));
	}

	IFCALL(context->UpdateSurfaceArea, context, surface->surfaceId,
	       meta2->numRegionRects, meta2->regionRects);

	if (!gdi->inGfxFrame)
	{
		status = CHANNEL_RC_NOT_INITIALIZED;
		IFCALLRET(context->UpdateSurfaces, status, context);
	}

	free(regionRects);
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
static UINT gdi_SurfaceCommand_Alpha(rdpGdi* gdi, RdpgfxClientContext* context,
                                     const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = CHANNEL_RC_OK;
	UINT32 color;
	gdiGfxSurface* surface;
	RECTANGLE_16 invalidRect;
	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
	{
		WLog_ERR(TAG, "%s: unable to retrieve surfaceData for surfaceId=%"PRIu32"", __FUNCTION__,
		         cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	WLog_WARN(TAG, "TODO gdi_SurfaceCommand_Alpha: status: %"PRIu32"", status);
	/* fill with green for now to distinguish from the rest */
	color = FreeRDPGetColor(surface->format, 0x00, 0xFF, 0x00, 0xFF);

	if (!freerdp_image_fill(surface->data, surface->format, surface->scanline,
	                        cmd->left, cmd->top, cmd->width, cmd->height, color))
		return ERROR_INTERNAL_ERROR;

	invalidRect.left = cmd->left;
	invalidRect.top = cmd->top;
	invalidRect.right = cmd->right;
	invalidRect.bottom = cmd->bottom;
	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion),
	                    &invalidRect);
	IFCALL(context->UpdateSurfaceArea, context, surface->surfaceId, 1, &invalidRect);

	if (!gdi->inGfxFrame)
	{
		status = CHANNEL_RC_NOT_INITIALIZED;
		IFCALLRET(context->UpdateSurfaces, status, context);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand_Progressive(rdpGdi* gdi,
        RdpgfxClientContext* context,
        const RDPGFX_SURFACE_COMMAND* cmd)
{
	INT32 rc;
	UINT status = CHANNEL_RC_OK;
	gdiGfxSurface* surface;
	REGION16 invalidRegion;
	const RECTANGLE_16* rects;
	UINT32 nrRects, x;
	/**
	 * Note: Since this comes via a Wire-To-Surface-2 PDU the
	 * cmd's top/left/right/bottom/width/height members are always zero!
	 * The update region is determined during decompression.
	 */
	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
	{
		WLog_ERR(TAG, "%s: unable to retrieve surfaceData for surfaceId=%"PRIu32"", __FUNCTION__,
		         cmd->surfaceId);
		return ERROR_NOT_FOUND;
	}

	rc = progressive_create_surface_context(surface->codecs->progressive,
	                                        cmd->surfaceId,
	                                        surface->width, surface->height);

	if (rc < 0)
	{
		WLog_ERR(TAG, "progressive_create_surface_context failure: %"PRId32"", rc);
		return ERROR_INTERNAL_ERROR;
	}

	region16_init(&invalidRegion);
	rc = progressive_decompress(surface->codecs->progressive, cmd->data,
	                            cmd->length, surface->data, surface->format,
	                            surface->scanline, cmd->left, cmd->top,
	                            &invalidRegion, cmd->surfaceId);

	if (rc < 0)
	{
		WLog_ERR(TAG, "progressive_decompress failure: %"PRId32"", rc);
		region16_uninit(&invalidRegion);
		return ERROR_INTERNAL_ERROR;
	}

	rects = region16_rects(&invalidRegion, &nrRects);
	IFCALL(context->UpdateSurfaceArea, context, surface->surfaceId, nrRects, rects);

	for (x = 0; x < nrRects; x++)
		region16_union_rect(&surface->invalidRegion, &surface->invalidRegion, &rects[x]);

	region16_uninit(&invalidRegion);

	if (!gdi->inGfxFrame)
	{
		status = CHANNEL_RC_NOT_INITIALIZED;
		IFCALLRET(context->UpdateSurfaces, status, context);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceCommand(RdpgfxClientContext* context,
                               const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = CHANNEL_RC_OK;
	rdpGdi* gdi = (rdpGdi*) context->custom;

	if (!context || !cmd)
		return ERROR_INVALID_PARAMETER;

	WLog_Print(gdi->log, WLOG_TRACE,
	           "surfaceId=%"PRIu32", codec=%"PRIu32", contextId=%"PRIu32", format=%s, "
	           "left=%"PRIu32", top=%"PRIu32", right=%"PRIu32", bottom=%"PRIu32", width=%"PRIu32", height=%"PRIu32" "
	           "length=%"PRIu32", data=%p, extra=%p",
	           cmd->surfaceId, cmd->codecId, cmd->contextId,
	           FreeRDPGetColorFormatName(cmd->format), cmd->left, cmd->top, cmd->right,
	           cmd->bottom, cmd->width, cmd->height, cmd->length, (void*) cmd->data, (void*) cmd->extra);

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
			WLog_WARN(TAG, "SurfaceCommand 0x%08"PRIX32" not implemented", cmd->codecId);
			break;

		default:
			WLog_WARN(TAG, "Invalid SurfaceCommand 0x%08"PRIX32"", cmd->codecId);
			break;
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_DeleteEncodingContext(RdpgfxClientContext* context,
                                      const RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext)
{
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
	gdiGfxSurface* surface;
	rdpGdi* gdi = (rdpGdi*) context->custom;
	surface = (gdiGfxSurface*) calloc(1, sizeof(gdiGfxSurface));

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	surface->codecs = gdi->context->codecs;

	if (!surface->codecs)
	{
		free(surface);
		return CHANNEL_RC_NO_MEMORY;
	}

	surface->surfaceId = createSurface->surfaceId;
	surface->width = (UINT32) createSurface->width;
	surface->height = (UINT32) createSurface->height;

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
			return ERROR_INTERNAL_ERROR;
	}

	surface->scanline = gfx_align_scanline(surface->width * 4, 16);
	surface->data = (BYTE*) _aligned_malloc(surface->scanline * surface->height, 16);

	if (!surface->data)
	{
		free(surface);
		return ERROR_INTERNAL_ERROR;
	}

	surface->outputMapped = FALSE;
	region16_init(&surface->invalidRegion);
	context->SetSurfaceData(context, surface->surfaceId, (void*) surface);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_DeleteSurface(RdpgfxClientContext* context,
                              const RDPGFX_DELETE_SURFACE_PDU* deleteSurface)
{
	rdpCodecs* codecs = NULL;
	gdiGfxSurface* surface = NULL;
	surface = (gdiGfxSurface*) context->GetSurfaceData(context, deleteSurface->surfaceId);

	if (surface)
	{
#ifdef WITH_GFX_H264
		h264_context_free(surface->h264);
#endif
		region16_uninit(&surface->invalidRegion);
		codecs = surface->codecs;
		_aligned_free(surface->data);
		free(surface);
	}

	context->SetSurfaceData(context, deleteSurface->surfaceId, NULL);

	if (codecs && codecs->progressive)
		progressive_delete_surface_context(codecs->progressive, deleteSurface->surfaceId);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SolidFill(RdpgfxClientContext* context,
                          const RDPGFX_SOLID_FILL_PDU* solidFill)
{
	UINT status = CHANNEL_RC_OK;
	UINT16 index;
	UINT32 color;
	BYTE a, r, g, b;
	UINT32 nWidth, nHeight;
	RECTANGLE_16* rect;
	gdiGfxSurface* surface;
	RECTANGLE_16 invalidRect;
	rdpGdi* gdi = (rdpGdi*) context->custom;
	surface = (gdiGfxSurface*) context->GetSurfaceData(context,
	          solidFill->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	b = solidFill->fillPixel.B;
	g = solidFill->fillPixel.G;
	r = solidFill->fillPixel.R;
	/* a = solidFill->fillPixel.XA;
	 * Ignore alpha channel, this is a solid fill. */
	a = 0xFF;
	color = FreeRDPGetColor(surface->format, r, g, b, a);

	for (index = 0; index < solidFill->fillRectCount; index++)
	{
		rect = &(solidFill->fillRects[index]);
		nWidth = rect->right - rect->left;
		nHeight = rect->bottom - rect->top;
		invalidRect.left = rect->left;
		invalidRect.top = rect->top;
		invalidRect.right = rect->right;
		invalidRect.bottom = rect->bottom;

		if (!freerdp_image_fill(surface->data, surface->format, surface->scanline,
		                        rect->left, rect->top, nWidth, nHeight, color))
			return ERROR_INTERNAL_ERROR;

		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion),
		                    &invalidRect);
	}

	IFCALL(context->UpdateSurfaceArea, context, surface->surfaceId,
	       solidFill->fillRectCount, solidFill->fillRects);

	if (!gdi->inGfxFrame)
	{
		status = CHANNEL_RC_NOT_INITIALIZED;
		IFCALLRET(context->UpdateSurfaces, status, context);
	}

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
	UINT status = CHANNEL_RC_OK;
	UINT16 index;
	BOOL sameSurface;
	UINT32 nWidth, nHeight;
	const RECTANGLE_16* rectSrc;
	RDPGFX_POINT16* destPt;
	RECTANGLE_16 invalidRect;
	gdiGfxSurface* surfaceSrc;
	gdiGfxSurface* surfaceDst;
	rdpGdi* gdi = (rdpGdi*) context->custom;
	rectSrc = &(surfaceToSurface->rectSrc);
	surfaceSrc = (gdiGfxSurface*) context->GetSurfaceData(context,
	             surfaceToSurface->surfaceIdSrc);
	sameSurface = (surfaceToSurface->surfaceIdSrc ==
	               surfaceToSurface->surfaceIdDest) ? TRUE : FALSE;

	if (!sameSurface)
		surfaceDst = (gdiGfxSurface*) context->GetSurfaceData(context,
		             surfaceToSurface->surfaceIdDest);
	else
		surfaceDst = surfaceSrc;

	if (!surfaceSrc || !surfaceDst)
		return ERROR_INTERNAL_ERROR;

	nWidth = rectSrc->right - rectSrc->left;
	nHeight = rectSrc->bottom - rectSrc->top;

	for (index = 0; index < surfaceToSurface->destPtsCount; index++)
	{
		destPt = &surfaceToSurface->destPts[index];

		if (!freerdp_image_copy(surfaceDst->data, surfaceDst->format,
		                        surfaceDst->scanline,
		                        destPt->x, destPt->y, nWidth, nHeight,
		                        surfaceSrc->data, surfaceSrc->format,
		                        surfaceSrc->scanline,
		                        rectSrc->left, rectSrc->top, NULL, FREERDP_FLIP_NONE))
			return ERROR_INTERNAL_ERROR;

		invalidRect.left = destPt->x;
		invalidRect.top = destPt->y;
		invalidRect.right = destPt->x + rectSrc->right;
		invalidRect.bottom = destPt->y + rectSrc->bottom;
		region16_union_rect(&surfaceDst->invalidRegion, &surfaceDst->invalidRegion,
		                    &invalidRect);
		IFCALL(context->UpdateSurfaceArea, context, surfaceDst->surfaceId, 1, &invalidRect);
	}

	if (!gdi->inGfxFrame)
	{
		status = CHANNEL_RC_NOT_INITIALIZED;
		IFCALLRET(context->UpdateSurfaces, status, context);
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_SurfaceToCache(RdpgfxClientContext* context,
                               const RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache)
{
	const RECTANGLE_16* rect;
	gdiGfxSurface* surface;
	gdiGfxCacheEntry* cacheEntry;
	rect = &(surfaceToCache->rectSrc);
	surface = (gdiGfxSurface*) context->GetSurfaceData(context, surfaceToCache->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	cacheEntry = (gdiGfxCacheEntry*) calloc(1, sizeof(gdiGfxCacheEntry));

	if (!cacheEntry)
		return ERROR_NOT_ENOUGH_MEMORY;

	cacheEntry->width = (UINT32)(rect->right - rect->left);
	cacheEntry->height = (UINT32)(rect->bottom - rect->top);
	cacheEntry->format = surface->format;
	cacheEntry->scanline = gfx_align_scanline(cacheEntry->width * 4, 16);
	cacheEntry->data = (BYTE*) calloc(cacheEntry->height, cacheEntry->scanline);

	if (!cacheEntry->data)
	{
		free(cacheEntry);
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	if (!freerdp_image_copy(cacheEntry->data, cacheEntry->format, cacheEntry->scanline,
	                        0, 0, cacheEntry->width, cacheEntry->height, surface->data,
	                        surface->format, surface->scanline, rect->left, rect->top, NULL, FREERDP_FLIP_NONE))
	{
		free(cacheEntry);
		return ERROR_INTERNAL_ERROR;
	}

	return context->SetCacheSlotData(context, surfaceToCache->cacheSlot, (void*) cacheEntry);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_CacheToSurface(RdpgfxClientContext* context,
                               const RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface)
{
	UINT status = CHANNEL_RC_OK;
	UINT16 index;
	RDPGFX_POINT16* destPt;
	gdiGfxSurface* surface;
	gdiGfxCacheEntry* cacheEntry;
	RECTANGLE_16 invalidRect;
	rdpGdi* gdi = (rdpGdi*) context->custom;
	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cacheToSurface->surfaceId);
	cacheEntry = (gdiGfxCacheEntry*) context->GetCacheSlotData(context, cacheToSurface->cacheSlot);

	if (!surface || !cacheEntry)
		return ERROR_INTERNAL_ERROR;

	for (index = 0; index < cacheToSurface->destPtsCount; index++)
	{
		destPt = &cacheToSurface->destPts[index];

		if (!freerdp_image_copy(surface->data, surface->format, surface->scanline,
		                        destPt->x, destPt->y, cacheEntry->width, cacheEntry->height,
		                        cacheEntry->data, cacheEntry->format, cacheEntry->scanline,
		                        0, 0, NULL, FREERDP_FLIP_NONE))
			return ERROR_INTERNAL_ERROR;

		invalidRect.left = destPt->x;
		invalidRect.top = destPt->y;
		invalidRect.right = destPt->x + cacheEntry->width;
		invalidRect.bottom = destPt->y + cacheEntry->height;
		region16_union_rect(&surface->invalidRegion, &surface->invalidRegion,
		                    &invalidRect);
		IFCALL(context->UpdateSurfaceArea, context, surface->surfaceId, 1, &invalidRect);
	}

	if (!gdi->inGfxFrame)
	{
		status = CHANNEL_RC_NOT_INITIALIZED;
		IFCALLRET(context->UpdateSurfaces, status, context);
	}

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
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_EvictCacheEntry(RdpgfxClientContext* context,
                                const RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry)
{
	gdiGfxCacheEntry* cacheEntry;
	cacheEntry = (gdiGfxCacheEntry*) context->GetCacheSlotData(context,
	             evictCacheEntry->cacheSlot);

	if (cacheEntry)
	{
		free(cacheEntry->data);
		free(cacheEntry);
	}

	context->SetCacheSlotData(context, evictCacheEntry->cacheSlot, NULL);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_MapSurfaceToOutput(RdpgfxClientContext* context,
                                   const RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput)
{
	gdiGfxSurface* surface;
	surface = (gdiGfxSurface*) context->GetSurfaceData(context,
	          surfaceToOutput->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	surface->outputMapped = TRUE;
	surface->outputOriginX = surfaceToOutput->outputOriginX;
	surface->outputOriginY = surfaceToOutput->outputOriginY;
	region16_clear(&surface->invalidRegion);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT gdi_MapSurfaceToWindow(RdpgfxClientContext* context,
                                   const RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow)
{
	return CHANNEL_RC_OK;
}

void gdi_graphics_pipeline_init(rdpGdi* gdi, RdpgfxClientContext* gfx)
{
	gdi->gfx = gfx;
	gfx->custom = (void*) gdi;
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
	gfx->EvictCacheEntry = gdi_EvictCacheEntry;
	gfx->MapSurfaceToOutput = gdi_MapSurfaceToOutput;
	gfx->MapSurfaceToWindow = gdi_MapSurfaceToWindow;
	gfx->UpdateSurfaces = gdi_UpdateSurfaces;
	PROFILER_CREATE(gfx->SurfaceProfiler, "GFX-PROFILER")
}

void gdi_graphics_pipeline_uninit(rdpGdi* gdi, RdpgfxClientContext* gfx)
{
	gdi->gfx = NULL;
	gfx->custom = NULL;
	PROFILER_PRINT_HEADER
	PROFILER_PRINT(gfx->SurfaceProfiler)
	PROFILER_PRINT_FOOTER
	PROFILER_FREE(gfx->SurfaceProfiler)
}

