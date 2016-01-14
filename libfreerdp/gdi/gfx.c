/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * GDI Graphics Pipeline
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/log.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/gdi/region.h>

#define TAG FREERDP_TAG("gdi")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_ResetGraphics(RdpgfxClientContext* context, RDPGFX_RESET_GRAPHICS_PDU* resetGraphics)
{
	int index;
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

		freerdp_client_codecs_reset(surface->codecs, FREERDP_CODEC_ALL);

		region16_clear(&surface->invalidRegion);
	}

	free(pSurfaceIds);

	freerdp_client_codecs_reset(gdi->codecs, FREERDP_CODEC_ALL);

	gdi->graphicsReset = TRUE;

	return CHANNEL_RC_OK;
}

int gdi_OutputUpdate(rdpGdi* gdi, gdiGfxSurface* surface)
{
	int nDstStep;
	BYTE* pDstData;
	int nXDst, nYDst;
	int nXSrc, nYSrc;
	UINT16 width, height;
	UINT32 surfaceX, surfaceY;
	RECTANGLE_16 surfaceRect;
	const RECTANGLE_16* extents;
	rdpUpdate* update = gdi->context->update;

	pDstData = gdi->primary_buffer;
	nDstStep = gdi->bytesPerPixel * gdi->width;

	surfaceX = surface->outputOriginX;
	surfaceY = surface->outputOriginY;

	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = surface->width;
	surfaceRect.bottom = surface->height;

	region16_intersect_rect(&(surface->invalidRegion), &(surface->invalidRegion), &surfaceRect);

	if (!region16_is_empty(&(surface->invalidRegion)))
	{
		extents = region16_extents(&(surface->invalidRegion));

		nXSrc = extents->left;
		nYSrc = extents->top;

		nXDst = surfaceX + extents->left;
		nYDst = surfaceY + extents->top;

		width = extents->right - extents->left;
		height = extents->bottom - extents->top;

		update->BeginPaint(gdi->context);

		freerdp_image_copy(pDstData, gdi->format, nDstStep, nXDst, nYDst,
				width, height, surface->data, surface->format,
				surface->scanline, nXSrc, nYSrc, NULL);

		gdi_InvalidateRegion(gdi->primary->hdc, nXDst, nYDst, width, height);

		update->EndPaint(gdi->context);
	}

	region16_clear(&(surface->invalidRegion));

	return 1;
}

int gdi_UpdateSurfaces(rdpGdi* gdi)
{
	UINT16 count;
	int index;
	int status = 1;
	gdiGfxSurface* surface;
	UINT16* pSurfaceIds = NULL;
	RdpgfxClientContext* context = gdi->gfx;

	if (!gdi->graphicsReset)
		return 1;

	context->GetSurfaceIds(context, &pSurfaceIds, &count);

	for (index = 0; index < count; index++)
	{
		surface = (gdiGfxSurface*) context->GetSurfaceData(context, pSurfaceIds[index]);

		if (!surface || !surface->outputMapped)
			continue;

		status = gdi_OutputUpdate(gdi, surface);

		if (status < 0)
			break;
	}

	free(pSurfaceIds);

	return status;
}

int gdi_OutputExpose(rdpGdi* gdi, int x, int y, int width, int height)
{
	int index;
	int status = 1;
	UINT16 count;
	gdiGfxSurface* surface;
	RECTANGLE_16 invalidRect;
	RECTANGLE_16 surfaceRect;
	RECTANGLE_16 intersection;
	UINT16* pSurfaceIds = NULL;
	RdpgfxClientContext* context = gdi->gfx;

	invalidRect.left = x;
	invalidRect.top = y;
	invalidRect.right = x + width;
	invalidRect.bottom = y + height;

	context->GetSurfaceIds(context, &pSurfaceIds, &count);

	for (index = 0; index < count; index++)
	{
		surface = (gdiGfxSurface*) context->GetSurfaceData(context, pSurfaceIds[index]);

		if (!surface || !surface->outputMapped)
			continue;

		surfaceRect.left = surface->outputOriginX;
		surfaceRect.top = surface->outputOriginY;
		surfaceRect.right = surface->outputOriginX + surface->width;
		surfaceRect.bottom = surface->outputOriginY + surface->height;

		if (rectangles_intersection(&invalidRect, &surfaceRect, &intersection))
		{
			/* Invalid rects are specified relative to surface origin */
			intersection.left -= surfaceRect.left;
			intersection.top -= surfaceRect.top;
			intersection.right -= surfaceRect.left;
			intersection.bottom -= surfaceRect.top;

			region16_union_rect(&surface->invalidRegion, &surface->invalidRegion, &intersection);
		}
	}

	free(pSurfaceIds);

	if (gdi_UpdateSurfaces(gdi) < 0)
		status = -1;

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_StartFrame(RdpgfxClientContext* context, RDPGFX_START_FRAME_PDU* startFrame)
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
UINT gdi_EndFrame(RdpgfxClientContext* context, RDPGFX_END_FRAME_PDU* endFrame)
{
	rdpGdi* gdi = (rdpGdi*) context->custom;

	gdi_UpdateSurfaces(gdi);

	gdi->inGfxFrame = FALSE;

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_SurfaceCommand_Uncompressed(rdpGdi* gdi, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	gdiGfxSurface* surface;
	RECTANGLE_16 invalidRect;

	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	freerdp_image_copy(surface->data, surface->format, surface->scanline, cmd->left, cmd->top,
			cmd->width, cmd->height, cmd->data, PIXEL_FORMAT_XRGB32, cmd->width * 4, 0, 0, NULL);

	invalidRect.left = cmd->left;
	invalidRect.top = cmd->top;
	invalidRect.right = cmd->right;
	invalidRect.bottom = cmd->bottom;

	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);

	if (!gdi->inGfxFrame)
		gdi_UpdateSurfaces(gdi);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_SurfaceCommand_RemoteFX(rdpGdi* gdi, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	int j;
	UINT16 i;
	RFX_RECT* rect;
	RFX_TILE* tile;
	int nXDst, nYDst;
	int nWidth, nHeight;
	int nbUpdateRects;
	RFX_MESSAGE* message;
	gdiGfxSurface* surface;
	REGION16 updateRegion;
	RECTANGLE_16 updateRect;
	RECTANGLE_16* updateRects;
	REGION16 clippingRects;
	RECTANGLE_16 clippingRect;

	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	if (!freerdp_client_codecs_prepare(surface->codecs, FREERDP_CODEC_REMOTEFX))
		return ERROR_INTERNAL_ERROR;

	if (!(message = rfx_process_message(surface->codecs->rfx, cmd->data, cmd->length)))
	{
		WLog_ERR(TAG, "Failed to process RemoteFX message");
		return ERROR_INTERNAL_ERROR;
	}

	region16_init(&clippingRects);

	for (i = 0; i < message->numRects; i++)
	{
		rect = &(message->rects[i]);

		clippingRect.left = cmd->left + rect->x;
		clippingRect.top = cmd->top + rect->y;
		clippingRect.right = clippingRect.left + rect->width;
		clippingRect.bottom = clippingRect.top + rect->height;

		region16_union_rect(&clippingRects, &clippingRects, &clippingRect);
	}

	for (i = 0; i < message->numTiles; i++)
	{
		tile = message->tiles[i];

		updateRect.left = cmd->left + tile->x;
		updateRect.top = cmd->top + tile->y;
		updateRect.right = updateRect.left + 64;
		updateRect.bottom = updateRect.top + 64;

		region16_init(&updateRegion);
		region16_intersect_rect(&updateRegion, &clippingRects, &updateRect);
		updateRects = (RECTANGLE_16*) region16_rects(&updateRegion, &nbUpdateRects);

		for (j = 0; j < nbUpdateRects; j++)
		{
			nXDst = updateRects[j].left;
			nYDst = updateRects[j].top;
			nWidth = updateRects[j].right - updateRects[j].left;
			nHeight = updateRects[j].bottom - updateRects[j].top;

			freerdp_image_copy(surface->data, surface->format, surface->scanline,
					nXDst, nYDst, nWidth, nHeight,
					tile->data, PIXEL_FORMAT_XRGB32, 64 * 4, 0, 0, NULL);

			region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &updateRects[j]);
		}

		region16_uninit(&updateRegion);
	}

	region16_uninit(&clippingRects);

	rfx_message_free(surface->codecs->rfx, message);

	if (!gdi->inGfxFrame)
		gdi_UpdateSurfaces(gdi);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_SurfaceCommand_ClearCodec(rdpGdi* gdi, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	int status;
	BYTE* DstData = NULL;
	gdiGfxSurface* surface;
	RECTANGLE_16 invalidRect;

	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	if (!freerdp_client_codecs_prepare(gdi->codecs, FREERDP_CODEC_CLEARCODEC))
		return ERROR_INTERNAL_ERROR;

	DstData = surface->data;

	status = clear_decompress(gdi->codecs->clear, cmd->data, cmd->length, &DstData,
			surface->format, surface->scanline, cmd->left, cmd->top, cmd->width, cmd->height);

	if (status < 0)
	{
		WLog_ERR(TAG, "clear_decompress failure: %d", status);
		return ERROR_INTERNAL_ERROR;
	}

	invalidRect.left = cmd->left;
	invalidRect.top = cmd->top;
	invalidRect.right = cmd->right;
	invalidRect.bottom = cmd->bottom;

	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);

	if (!gdi->inGfxFrame)
		gdi_UpdateSurfaces(gdi);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_SurfaceCommand_Planar(rdpGdi* gdi, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	int status;
	BYTE* DstData = NULL;
	gdiGfxSurface* surface;
	RECTANGLE_16 invalidRect;

	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	if (!freerdp_client_codecs_prepare(surface->codecs, FREERDP_CODEC_PLANAR))
		return ERROR_INTERNAL_ERROR;

	DstData = surface->data;

	status = planar_decompress(surface->codecs->planar, cmd->data, cmd->length, &DstData,
			PIXEL_FORMAT_XRGB32, surface->scanline, cmd->left, cmd->top, cmd->width, cmd->height, FALSE);

	invalidRect.left = cmd->left;
	invalidRect.top = cmd->top;
	invalidRect.right = cmd->right;
	invalidRect.bottom = cmd->bottom;

	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);

	if (!gdi->inGfxFrame)
		gdi_UpdateSurfaces(gdi);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_SurfaceCommand_H264(rdpGdi* gdi, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	int status;
	UINT32 i;
	BYTE* DstData = NULL;
	gdiGfxSurface* surface;
	RDPGFX_H264_METABLOCK* meta;
	RDPGFX_H264_BITMAP_STREAM* bs;

	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	if (!freerdp_client_codecs_prepare(surface->codecs, FREERDP_CODEC_H264))
		return ERROR_INTERNAL_ERROR;

	bs = (RDPGFX_H264_BITMAP_STREAM*) cmd->extra;

	if (!bs)
		return ERROR_INTERNAL_ERROR;

	meta = &(bs->meta);

	DstData = surface->data;

	status = h264_decompress(surface->codecs->h264, bs->data, bs->length, &DstData,
			PIXEL_FORMAT_XRGB32, surface->scanline, surface->width, surface->height,
			meta->regionRects, meta->numRegionRects);

	if (status < 0)
	{
		WLog_WARN(TAG, "h264_decompress failure: %d, ignoring update.", status);
		return CHANNEL_RC_OK;
	}

	for (i = 0; i < meta->numRegionRects; i++)
	{
		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), (RECTANGLE_16*) &(meta->regionRects[i]));
	}

	if (!gdi->inGfxFrame)
		gdi_UpdateSurfaces(gdi);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_SurfaceCommand_Alpha(rdpGdi* gdi, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	int status = 0;
	gdiGfxSurface* surface;
	RECTANGLE_16 invalidRect;

	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	if (!freerdp_client_codecs_prepare(surface->codecs, FREERDP_CODEC_ALPHACODEC))
		return ERROR_INTERNAL_ERROR;

	WLog_DBG(TAG, "gdi_SurfaceCommand_Alpha: status: %d", status);

	/* fill with green for now to distinguish from the rest */

	freerdp_image_fill(surface->data, PIXEL_FORMAT_XRGB32, surface->scanline,
			cmd->left, cmd->top, cmd->width, cmd->height, 0x00FF00);

	invalidRect.left = cmd->left;
	invalidRect.top = cmd->top;
	invalidRect.right = cmd->right;
	invalidRect.bottom = cmd->bottom;

	region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);

	if (!gdi->inGfxFrame)
		gdi_UpdateSurfaces(gdi);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_SurfaceCommand_Progressive(rdpGdi* gdi, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	int i, j;
	int status;
	BYTE* DstData;
	RFX_RECT* rect;
	int nXDst, nYDst;
	int nXSrc, nYSrc;
	int nWidth, nHeight;
	int nbUpdateRects;
	gdiGfxSurface* surface;
	REGION16 updateRegion;
	RECTANGLE_16 updateRect;
	RECTANGLE_16* updateRects;
	REGION16 clippingRects;
	RECTANGLE_16 clippingRect;
	RFX_PROGRESSIVE_TILE* tile;
	PROGRESSIVE_BLOCK_REGION* region;

	surface = (gdiGfxSurface*) context->GetSurfaceData(context, cmd->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	if (!freerdp_client_codecs_prepare(surface->codecs, FREERDP_CODEC_PROGRESSIVE))
		return ERROR_INTERNAL_ERROR;

	progressive_create_surface_context(surface->codecs->progressive, cmd->surfaceId, surface->width, surface->height);

	DstData = surface->data;

	status = progressive_decompress(surface->codecs->progressive, cmd->data, cmd->length, &DstData,
			PIXEL_FORMAT_XRGB32, surface->scanline, cmd->left, cmd->top, cmd->width, cmd->height, cmd->surfaceId);

	if (status < 0)
	{
		WLog_ERR(TAG, "progressive_decompress failure: %d", status);
		return ERROR_INTERNAL_ERROR;
	}

	region = &(surface->codecs->progressive->region);

	region16_init(&clippingRects);

	for (i = 0; i < region->numRects; i++)
	{
		rect = &(region->rects[i]);

		clippingRect.left = cmd->left + rect->x;
		clippingRect.top = cmd->top + rect->y;
		clippingRect.right = clippingRect.left + rect->width;
		clippingRect.bottom = clippingRect.top + rect->height;

		region16_union_rect(&clippingRects, &clippingRects, &clippingRect);
	}

	for (i = 0; i < region->numTiles; i++)
	{
		tile = region->tiles[i];

		updateRect.left = cmd->left + tile->x;
		updateRect.top = cmd->top + tile->y;
		updateRect.right = updateRect.left + 64;
		updateRect.bottom = updateRect.top + 64;

		region16_init(&updateRegion);
		region16_intersect_rect(&updateRegion, &clippingRects, &updateRect);
		updateRects = (RECTANGLE_16*) region16_rects(&updateRegion, &nbUpdateRects);

		for (j = 0; j < nbUpdateRects; j++)
		{
			nXDst = updateRects[j].left;
			nYDst = updateRects[j].top;
			nWidth = updateRects[j].right - updateRects[j].left;
			nHeight = updateRects[j].bottom - updateRects[j].top;

			nXSrc = nXDst - (cmd->left + tile->x);
			nYSrc = nYDst - (cmd->top + tile->y);

			freerdp_image_copy(surface->data, surface->format,
					surface->scanline, nXDst, nYDst, nWidth, nHeight,
					tile->data, PIXEL_FORMAT_XRGB32, 64 * 4, nXSrc, nYSrc, NULL);

			region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &updateRects[j]);
		}

		region16_uninit(&updateRegion);
	}

	region16_uninit(&clippingRects);

	if (!gdi->inGfxFrame)
		gdi_UpdateSurfaces(gdi);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_SurfaceCommand(RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = CHANNEL_RC_OK;
	rdpGdi* gdi = (rdpGdi*) context->custom;

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

		case RDPGFX_CODECID_H264:
			status = gdi_SurfaceCommand_H264(gdi, context, cmd);
			break;

		case RDPGFX_CODECID_ALPHA:
			status = gdi_SurfaceCommand_Alpha(gdi, context, cmd);
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE:
			status = gdi_SurfaceCommand_Progressive(gdi, context, cmd);
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE_V2:
			break;
	}

	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_DeleteEncodingContext(RdpgfxClientContext* context, RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_CreateSurface(RdpgfxClientContext* context, RDPGFX_CREATE_SURFACE_PDU* createSurface)
{
	gdiGfxSurface* surface;
	rdpGdi* gdi = (rdpGdi*) context->custom;

	surface = (gdiGfxSurface*) calloc(1, sizeof(gdiGfxSurface));

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	surface->codecs = codecs_new(gdi->context);

	if (!surface->codecs)
	{
		free (surface);
		return CHANNEL_RC_NO_MEMORY;
	}

	surface->surfaceId = createSurface->surfaceId;
	surface->width = (UINT32) createSurface->width;
	surface->height = (UINT32) createSurface->height;
	surface->alpha = (createSurface->pixelFormat == PIXEL_FORMAT_ARGB_8888) ? TRUE : FALSE;

	surface->format = (!gdi->invert) ? PIXEL_FORMAT_XRGB32 : PIXEL_FORMAT_XBGR32;

	surface->scanline = (surface->width + (surface->width % 4)) * 4;
	surface->data = (BYTE*) calloc(1, surface->scanline * surface->height);

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
UINT gdi_DeleteSurface(RdpgfxClientContext* context, RDPGFX_DELETE_SURFACE_PDU* deleteSurface)
{
	rdpCodecs* codecs = NULL;
	gdiGfxSurface* surface = NULL;

	surface = (gdiGfxSurface*) context->GetSurfaceData(context, deleteSurface->surfaceId);

	if (surface)
	{
		region16_uninit(&surface->invalidRegion);
		codecs = surface->codecs;
		free(surface->data);
		free(surface);
	}

	context->SetSurfaceData(context, deleteSurface->surfaceId, NULL);

	if (codecs && codecs->progressive)
		progressive_delete_surface_context(codecs->progressive, deleteSurface->surfaceId);

	codecs_free(codecs);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_SolidFill(RdpgfxClientContext* context, RDPGFX_SOLID_FILL_PDU* solidFill)
{
	UINT16 index;
	UINT32 color;
	BYTE a, r, g, b;
	int nWidth, nHeight;
	RDPGFX_RECT16* rect;
	gdiGfxSurface* surface;
	RECTANGLE_16 invalidRect;
	rdpGdi* gdi = (rdpGdi*) context->custom;

	surface = (gdiGfxSurface*) context->GetSurfaceData(context, solidFill->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	b = solidFill->fillPixel.B;
	g = solidFill->fillPixel.G;
	r = solidFill->fillPixel.R;
	a = solidFill->fillPixel.XA;

	if (!gdi->invert)
		color = ARGB32(a, r, g, b);
	else
		color = ABGR32(a, r, g, b);

	for (index = 0; index < solidFill->fillRectCount; index++)
	{
		rect = &(solidFill->fillRects[index]);

		nWidth = rect->right - rect->left;
		nHeight = rect->bottom - rect->top;

		invalidRect.left = rect->left;
		invalidRect.top = rect->top;
		invalidRect.right = rect->right;
		invalidRect.bottom = rect->bottom;

		freerdp_image_fill(surface->data, surface->format, surface->scanline,
				rect->left, rect->top, nWidth, nHeight, color);

		region16_union_rect(&(surface->invalidRegion), &(surface->invalidRegion), &invalidRect);
	}

	if (!gdi->inGfxFrame)
		gdi_UpdateSurfaces(gdi);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_SurfaceToSurface(RdpgfxClientContext* context, RDPGFX_SURFACE_TO_SURFACE_PDU* surfaceToSurface)
{
	UINT16 index;
	BOOL sameSurface;
	int nWidth, nHeight;
	RDPGFX_RECT16* rectSrc;
	RDPGFX_POINT16* destPt;
	RECTANGLE_16 invalidRect;
	gdiGfxSurface* surfaceSrc;
	gdiGfxSurface* surfaceDst;
	rdpGdi* gdi = (rdpGdi*) context->custom;

	rectSrc = &(surfaceToSurface->rectSrc);

	surfaceSrc = (gdiGfxSurface*) context->GetSurfaceData(context, surfaceToSurface->surfaceIdSrc);

	sameSurface = (surfaceToSurface->surfaceIdSrc == surfaceToSurface->surfaceIdDest) ? TRUE : FALSE;

	if (!sameSurface)
		surfaceDst = (gdiGfxSurface*) context->GetSurfaceData(context, surfaceToSurface->surfaceIdDest);
	else
		surfaceDst = surfaceSrc;

	if (!surfaceSrc || !surfaceDst)
		return ERROR_INTERNAL_ERROR;

	nWidth = rectSrc->right - rectSrc->left;
	nHeight = rectSrc->bottom - rectSrc->top;

	for (index = 0; index < surfaceToSurface->destPtsCount; index++)
	{
		destPt = &surfaceToSurface->destPts[index];

		if (sameSurface)
		{
			freerdp_image_move(surfaceDst->data, surfaceDst->format, surfaceDst->scanline,
					destPt->x, destPt->y, nWidth, nHeight, rectSrc->left, rectSrc->top);
		}
		else
		{
			freerdp_image_copy(surfaceDst->data, surfaceDst->format, surfaceDst->scanline,
					destPt->x, destPt->y, nWidth, nHeight, surfaceSrc->data, surfaceSrc->format,
					surfaceSrc->scanline, rectSrc->left, rectSrc->top, NULL);
		}

		invalidRect.left = destPt->x;
		invalidRect.top = destPt->y;
		invalidRect.right = destPt->x + rectSrc->right;
		invalidRect.bottom = destPt->y + rectSrc->bottom;

		region16_union_rect(&surfaceDst->invalidRegion, &surfaceDst->invalidRegion, &invalidRect);
	}

	if (!gdi->inGfxFrame)
		gdi_UpdateSurfaces(gdi);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_SurfaceToCache(RdpgfxClientContext* context, RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache)
{
	RDPGFX_RECT16* rect;
	gdiGfxSurface* surface;
	gdiGfxCacheEntry* cacheEntry;
	rdpGdi* gdi = (rdpGdi*) context->custom;

	rect = &(surfaceToCache->rectSrc);

	surface = (gdiGfxSurface*) context->GetSurfaceData(context, surfaceToCache->surfaceId);

	if (!surface)
		return ERROR_INTERNAL_ERROR;

	cacheEntry = (gdiGfxCacheEntry*) calloc(1, sizeof(gdiGfxCacheEntry));

	if (!cacheEntry)
		return ERROR_INTERNAL_ERROR;

	cacheEntry->width = (UINT32) (rect->right - rect->left);
	cacheEntry->height = (UINT32) (rect->bottom - rect->top);
	cacheEntry->alpha = surface->alpha;

	cacheEntry->format = (!gdi->invert) ? PIXEL_FORMAT_XRGB32 : PIXEL_FORMAT_XBGR32;

	cacheEntry->scanline = (cacheEntry->width + (cacheEntry->width % 4)) * 4;
	cacheEntry->data = (BYTE*) calloc(1, cacheEntry->scanline * cacheEntry->height);

	if (!cacheEntry->data)
	{
		free(cacheEntry);
		return ERROR_INTERNAL_ERROR;
	}

	freerdp_image_copy(cacheEntry->data, cacheEntry->format, cacheEntry->scanline,
			0, 0, cacheEntry->width, cacheEntry->height, surface->data,
			surface->format, surface->scanline, rect->left, rect->top, NULL);

	context->SetCacheSlotData(context, surfaceToCache->cacheSlot, (void*) cacheEntry);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_CacheToSurface(RdpgfxClientContext* context, RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface)
{
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

		freerdp_image_copy(surface->data, surface->format, surface->scanline,
				destPt->x, destPt->y, cacheEntry->width, cacheEntry->height,
				cacheEntry->data, cacheEntry->format, cacheEntry->scanline, 0, 0, NULL);

		invalidRect.left = destPt->x;
		invalidRect.top = destPt->y;
		invalidRect.right = destPt->x + cacheEntry->width - 1;
		invalidRect.bottom = destPt->y + cacheEntry->height - 1;

		region16_union_rect(&surface->invalidRegion, &surface->invalidRegion, &invalidRect);
	}

	if (!gdi->inGfxFrame)
		gdi_UpdateSurfaces(gdi);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_CacheImportReply(RdpgfxClientContext* context, RDPGFX_CACHE_IMPORT_REPLY_PDU* cacheImportReply)
{
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT gdi_EvictCacheEntry(RdpgfxClientContext* context, RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry)
{
	gdiGfxCacheEntry* cacheEntry;

	cacheEntry = (gdiGfxCacheEntry*) context->GetCacheSlotData(context, evictCacheEntry->cacheSlot);

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
UINT gdi_MapSurfaceToOutput(RdpgfxClientContext* context, RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput)
{
	gdiGfxSurface* surface;

	surface = (gdiGfxSurface*) context->GetSurfaceData(context, surfaceToOutput->surfaceId);

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
UINT gdi_MapSurfaceToWindow(RdpgfxClientContext* context, RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow)
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
}

void gdi_graphics_pipeline_uninit(rdpGdi* gdi, RdpgfxClientContext* gfx)
{
	region16_uninit(&(gdi->invalidRegion));

	gdi->gfx = NULL;
	gfx->custom = NULL;
}

