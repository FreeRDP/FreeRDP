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

	return 1;
}

int xf_StartFrame(RdpgfxClientContext* context, RDPGFX_START_FRAME_PDU* startFrame)
{
	return 1;
}

int xf_EndFrame(RdpgfxClientContext* context, RDPGFX_END_FRAME_PDU* endFrame)
{
	return 1;
}

int xf_SurfaceCommand_Uncompressed(xfContext* xfc, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	BYTE* data;
	XImage* image;

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);

	data = (BYTE*) malloc(cmd->width * cmd->height * 4);

	freerdp_image_flip(cmd->data, data, cmd->width, cmd->height, 32);

	image = XCreateImage(xfc->display, xfc->visual, 24, ZPixmap, 0, (char*) data, cmd->width, cmd->height, 32, 0);

	XPutImage(xfc->display, xfc->primary, xfc->gc, image, 0, 0,
			cmd->left, cmd->top, cmd->width, cmd->height);

	XFree(image);

	free(data);

	if (!xfc->remote_app)
	{
		XCopyArea(xfc->display, xfc->primary, xfc->window->handle, xfc->gc,
				cmd->left, cmd->top, cmd->width, cmd->height, cmd->left, cmd->top);
	}

	XSetClipMask(xfc->display, xfc->gc, None);

	return 1;
}

int xf_SurfaceCommand_RemoteFX(xfContext* xfc, RdpgfxClientContext* context, RDPGFX_SURFACE_COMMAND* cmd)
{
	int i, tx, ty;
	XImage* image;
	RFX_MESSAGE* message;

	message = rfx_process_message(xfc->rfx, cmd->data, cmd->length);

	if (!message)
		return -1;

	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);

	XSetClipRectangles(xfc->display, xfc->gc, cmd->left, cmd->top,
			(XRectangle*) message->rects, message->numRects, YXBanded);

	for (i = 0; i < message->numTiles; i++)
	{
		image = XCreateImage(xfc->display, xfc->visual, 24, ZPixmap, 0,
			(char*) message->tiles[i]->data, 64, 64, 32, 0);

		tx = message->tiles[i]->x + cmd->left;
		ty = message->tiles[i]->y + cmd->top;

		XPutImage(xfc->display, xfc->primary, xfc->gc, image, 0, 0, tx, ty, 64, 64);
		XFree(image);
	}

	/* Copy the updated region from backstore to the window. */
	for (i = 0; i < message->numRects; i++)
	{
		tx = message->rects[i].x + cmd->left;
		ty = message->rects[i].y + cmd->top;

		if (!xfc->remote_app)
		{
			XCopyArea(xfc->display, xfc->primary, xfc->drawable, xfc->gc, tx, ty,
					message->rects[i].width, message->rects[i].height, tx, ty);
		}
	}

	XSetClipMask(xfc->display, xfc->gc, None);
	rfx_message_free(xfc->rfx, message);

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
			break;

		case RDPGFX_CODECID_PLANAR:
			break;

		case RDPGFX_CODECID_H264:
			break;

		case RDPGFX_CODECID_ALPHA:
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE:
			break;

		case RDPGFX_CODECID_CAPROGRESSIVE_V2:
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
	return 1;
}

int xf_DeleteSurface(RdpgfxClientContext* context, RDPGFX_DELETE_SURFACE_PDU* deleteSurface)
{
	return 1;
}

int xf_SolidFill(RdpgfxClientContext* context, RDPGFX_SOLID_FILL_PDU* solidFill)
{
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
