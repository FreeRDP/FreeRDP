/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Graphics Pipeline
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

#include "xf_gfx.h"

int xf_ResetGraphics(RdpgfxClientContext* context, RDPGFX_RESET_GRAPHICS_PDU* resetGraphics)
{
	xfContext* xfc = (xfContext*) context->custom;

	printf("xf_ResetGraphics: width: %d height: %d\n",
			resetGraphics->width, resetGraphics->height);

	if (xfc->rfx)
	{
		rfx_context_free(xfc->rfx);
		xfc->rfx = NULL;
	}

	xfc->rfx = rfx_context_new(FALSE);

	xfc->rfx->width = resetGraphics->width;
	xfc->rfx->height = resetGraphics->height;
	rfx_context_set_pixel_format(xfc->rfx, RDP_PIXEL_FORMAT_B8G8R8A8);

	if (xfc->nsc)
	{
		nsc_context_free(xfc->nsc);
		xfc->nsc = NULL;
	}

	xfc->nsc = nsc_context_new();

	xfc->nsc->width = resetGraphics->width;
	xfc->nsc->height = resetGraphics->height;
	nsc_context_set_pixel_format(xfc->nsc, RDP_PIXEL_FORMAT_B8G8R8A8);

	region16_init(&(xfc->invalidRegion));

	return 1;
}

int xf_SurfaceUpdate(xfContext* xfc)
{
	UINT16 width, height;
	RECTANGLE_16 surfaceRect;
	const RECTANGLE_16* extents;

	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = xfc->width - 1;
	surfaceRect.bottom = xfc->height - 1;

	region16_intersect_rect(&(xfc->invalidRegion), &(xfc->invalidRegion), &surfaceRect);

	if (!region16_is_empty(&(xfc->invalidRegion)))
	{
		extents = region16_extents(&(xfc->invalidRegion));

		width = extents->right - extents->left;
		height = extents->bottom - extents->top;

		if (width > xfc->width)
			width = xfc->width;

		if (height > xfc->height)
			height = xfc->height;

		printf("xf_SurfaceUpdate: x: %d y: %d width: %d height: %d\n",
				extents->left, extents->top, width, height);
	}
	else
	{
		printf("xf_SurfaceUpdate: null region\n");
	}

	region16_clear(&(xfc->invalidRegion));

	XSync(xfc->display, True);

	return 1;
}

int xf_StartFrame(RdpgfxClientContext* context, RDPGFX_START_FRAME_PDU* startFrame)
{
	xfContext* xfc = (xfContext*) context->custom;

	printf("xf_StartFrame: %d\n", startFrame->frameId);

	xfc->inGfxFrame = TRUE;

	return 1;
}

int xf_EndFrame(RdpgfxClientContext* context, RDPGFX_END_FRAME_PDU* endFrame)
{
	xfContext* xfc = (xfContext*) context->custom;

	printf("xf_EndFrame: %d\n", endFrame->frameId);

	xf_SurfaceUpdate(xfc);

	xfc->inGfxFrame = FALSE;

	return 1;
}

int xf_SurfaceCommand_Uncompressed(xfContext* xfc, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	BYTE* data;
	XImage* image;
	RECTANGLE_16 invalidRect;

	printf("xf_SurfaceCommand_Uncompressed\n");

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);

	data = (BYTE*) malloc(cmd->width * cmd->height * 4);

	freerdp_image_flip(cmd->data, data, cmd->width, cmd->height, 32);

	image = XCreateImage(xfc->display, xfc->visual, 24, ZPixmap, 0, (char*) data, cmd->width, cmd->height, 32, 0);

	XPutImage(xfc->display, xfc->drawing, xfc->gc, image, 0, 0, cmd->left, cmd->top, cmd->width, cmd->height);

	XFree(image);

	free(data);

	invalidRect.left = cmd->left;
	invalidRect.top = cmd->top;
	invalidRect.right = cmd->right;
	invalidRect.bottom = cmd->bottom;

	region16_union_rect(&(xfc->invalidRegion), &(xfc->invalidRegion), &invalidRect);

	XSetClipMask(xfc->display, xfc->gc, None);

	if (!xfc->inGfxFrame)
		xf_SurfaceUpdate(xfc);

	return 1;
}

int xf_SurfaceCommand_RemoteFX(xfContext* xfc, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT16 index;
	XImage* image;
	RFX_RECT* rect;
	RFX_TILE* tile;
	XRectangle* xrects;
	RFX_MESSAGE* message;
	RECTANGLE_16 invalidRect;
	RECTANGLE_16 surfaceRect;

	printf("xf_SurfaceCommand_RemoteFX\n");

	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = xfc->width - 1;
	surfaceRect.bottom = xfc->height - 1;

	message = rfx_process_message(xfc->rfx, cmd->data, cmd->length);

	if (!message)
		return -1;

	xrects = (XRectangle*) malloc(message->numRects * sizeof(XRectangle));

	if (!xrects)
		return -1;

	for (index = 0; index < message->numRects; index++)
	{
		rect = &(message->rects[index]);

		xrects[index].x = cmd->left + rect->x;
		xrects[index].y = cmd->top + rect->y;
		xrects[index].width = rect->width;
		xrects[index].height = rect->height;

		invalidRect.left = cmd->left + rect->x;
		invalidRect.top = cmd->top + rect->y;
		invalidRect.right = invalidRect.left + rect->width - 1;
		invalidRect.bottom = invalidRect.top + rect->height - 1;

		region16_union_rect(&(xfc->invalidRegion), &(xfc->invalidRegion), &invalidRect);
	}

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);

	XSetClipRectangles(xfc->display, xfc->gc, cmd->left, cmd->top, xrects, message->numRects, YXBanded);

	for (index = 0; index < message->numTiles; index++)
	{
		tile = message->tiles[index];

		image = XCreateImage(xfc->display, xfc->visual, 24, ZPixmap, 0,
			(char*) tile->data, 64, 64, 32, 0);

		XPutImage(xfc->display, xfc->drawable, xfc->gc, image, 0, 0,
				cmd->left + tile->x, cmd->top + tile->y, 64, 64);

		XFree(image);
	}

	XSetClipMask(xfc->display, xfc->gc, None);

	rfx_message_free(xfc->rfx, message);
	free(xrects);

	if (!xfc->inGfxFrame)
		xf_SurfaceUpdate(xfc);

	return 1;
}

int xf_SurfaceCommand_ClearCodec(xfContext* xfc, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	printf("xf_SurfaceCommand_ClearCodec\n");

	if (!xfc->inGfxFrame)
		xf_SurfaceUpdate(xfc);

	return 1;
}

int xf_SurfaceCommand_Planar(xfContext* xfc, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	printf("xf_SurfaceCommand_Planar\n");

	if (!xfc->inGfxFrame)
		xf_SurfaceUpdate(xfc);

	return 1;
}

int xf_SurfaceCommand(RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	int status = 1;
	xfContext* xfc = (xfContext*) context->custom;

	switch (cmd->codecId)
	{
		case RDPGFX_CODECID_UNCOMPRESSED:
			status = xf_SurfaceCommand_Uncompressed(xfc, context, cmd);
			break;

		case RDPGFX_CODECID_CAVIDEO:
			status = xf_SurfaceCommand_RemoteFX(xfc, context, cmd);
			break;

		case RDPGFX_CODECID_CLEARCODEC:
			status = xf_SurfaceCommand_ClearCodec(xfc, context, cmd);
			break;

		case RDPGFX_CODECID_PLANAR:
			status = xf_SurfaceCommand_Planar(xfc, context, cmd);
			break;

		case RDPGFX_CODECID_H264:
			printf("xf_SurfaceCommand_H264\n");
			break;

		case RDPGFX_CODECID_ALPHA:
			printf("xf_SurfaceCommand_Alpha\n");
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE:
			printf("xf_SurfaceCommand_Progressive\n");
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE_V2:
			printf("xf_SurfaceCommand_ProgressiveV2\n");
			break;
	}

	return 1;
}

int xf_DeleteEncodingContext(RdpgfxClientContext* context, RDPGFX_DELETE_ENCODING_CONTEXT_PDU* deleteEncodingContext)
{
	return 1;
}

int xf_CreateSurface(RdpgfxClientContext* context, RDPGFX_CREATE_SURFACE_PDU* createSurface)
{
	xfGfxSurface* surface;
	xfContext* xfc = (xfContext*) context->custom;

	printf("xf_CreateSurface: surfaceId: %d width: %d height: %d format: 0x%02X\n",
			createSurface->surfaceId, createSurface->width, createSurface->height, createSurface->pixelFormat);

	surface = (xfGfxSurface*) calloc(1, sizeof(xfGfxSurface));

	if (!surface)
		return -1;

	surface->surfaceId = createSurface->surfaceId;
	surface->width = (UINT32) createSurface->width;
	surface->height = (UINT32) createSurface->height;
	surface->alpha = (createSurface->pixelFormat == PIXEL_FORMAT_ARGB_8888) ? TRUE : FALSE;

	surface->data = (BYTE*) calloc(1, surface->width * surface->height * 4);

	if (!surface->data)
		return -1;

	surface->image = XCreateImage(xfc->display, xfc->visual, 24, ZPixmap, 0,
			(char*) surface->data, surface->width, surface->height, 32, 0);

	context->SetSurfaceData(context, surface->surfaceId, (void*) surface);

	return 1;
}

int xf_DeleteSurface(RdpgfxClientContext* context, RDPGFX_DELETE_SURFACE_PDU* deleteSurface)
{
	xfGfxSurface* surface = NULL;

	surface = (xfGfxSurface*) context->GetSurfaceData(context, deleteSurface->surfaceId);

	printf("xf_DeleteSurface: surfaceId: %d\n", deleteSurface->surfaceId);

	if (surface)
	{
		XFree(surface->image);
		free(surface->data);
		free(surface);
	}

	context->SetSurfaceData(context, deleteSurface->surfaceId, NULL);

	return 1;
}

int xf_SolidFill(RdpgfxClientContext* context, RDPGFX_SOLID_FILL_PDU* solidFill)
{
	UINT16 index;
	UINT32 color;
	BYTE a, r, g, b;
	XRectangle* xrects;
	RDPGFX_RECT16* rect;
	RECTANGLE_16 invalidRect;
	xfContext* xfc = (xfContext*) context->custom;

	printf("xf_SolidFill\n");

	b = solidFill->fillPixel.B;
	g = solidFill->fillPixel.G;
	r = solidFill->fillPixel.R;
	a = solidFill->fillPixel.XA;

	color = ARGB32(a, r, g, b);

	xrects = (XRectangle*) malloc(solidFill->fillRectCount * sizeof(XRectangle));

	if (!xrects)
		return -1;

	for (index = 0; index < solidFill->fillRectCount; index++)
	{
		rect = &(solidFill->fillRects[index]);

		xrects[index].x = rect->left;
		xrects[index].y = rect->top;
		xrects[index].width = rect->right - rect->left + 1;
		xrects[index].height = rect->bottom - rect->top + 1;

		invalidRect.left = rect->left;
		invalidRect.top = rect->top;
		invalidRect.right = rect->right;
		invalidRect.bottom = rect->bottom;

		region16_union_rect(&(xfc->invalidRegion), &(xfc->invalidRegion), &invalidRect);
	}

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);
	XSetForeground(xfc->display, xfc->gc, color);

	XFillRectangles(xfc->display, xfc->drawing, xfc->gc, xrects, solidFill->fillRectCount);

	free(xrects);

	if (!xfc->inGfxFrame)
		xf_SurfaceUpdate(xfc);

	return 1;
}

int xf_SurfaceToSurface(RdpgfxClientContext* context, RDPGFX_SURFACE_TO_SURFACE_PDU* surfaceToSurface)
{
	return 1;
}

int xf_SurfaceToCache(RdpgfxClientContext* context, RDPGFX_SURFACE_TO_CACHE_PDU* surfaceToCache)
{
	return 1;
}

int xf_CacheToSurface(RdpgfxClientContext* context, RDPGFX_CACHE_TO_SURFACE_PDU* cacheToSurface)
{
	return 1;
}

int xf_CacheImportReply(RdpgfxClientContext* context, RDPGFX_CACHE_IMPORT_REPLY_PDU* cacheImportReply)
{
	return 1;
}

int xf_EvictCacheEntry(RdpgfxClientContext* context, RDPGFX_EVICT_CACHE_ENTRY_PDU* evictCacheEntry)
{
	return 1;
}

int xf_MapSurfaceToOutput(RdpgfxClientContext* context, RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU* surfaceToOutput)
{
	return 1;
}

int xf_MapSurfaceToWindow(RdpgfxClientContext* context, RDPGFX_MAP_SURFACE_TO_WINDOW_PDU* surfaceToWindow)
{
	return 1;
}

void xf_register_graphics_pipeline(xfContext* xfc, RdpgfxClientContext* gfx)
{
	xfc->gfx = gfx;
	gfx->custom = (void*) xfc;

	gfx->ResetGraphics = xf_ResetGraphics;
	gfx->StartFrame = xf_StartFrame;
	gfx->EndFrame = xf_EndFrame;
	gfx->SurfaceCommand = xf_SurfaceCommand;
	gfx->DeleteEncodingContext = xf_DeleteEncodingContext;
	gfx->CreateSurface = xf_CreateSurface;
	gfx->DeleteSurface = xf_DeleteSurface;
	gfx->SolidFill = xf_SolidFill;
	gfx->SurfaceToSurface = xf_SurfaceToSurface;
	gfx->SurfaceToCache = xf_SurfaceToCache;
	gfx->CacheToSurface = xf_CacheToSurface;
	gfx->CacheImportReply = xf_CacheImportReply;
	gfx->EvictCacheEntry = xf_EvictCacheEntry;
	gfx->MapSurfaceToOutput = xf_MapSurfaceToOutput;
	gfx->MapSurfaceToWindow = xf_MapSurfaceToWindow;
}
