/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Graphics Pipeline
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
#include "xf_gfx.h"

#define TAG CLIENT_TAG("x11")

static UINT xf_OutputUpdate(xfContext* xfc, xfGfxSurface* surface)
{
	UINT16 width, height;
	UINT32 surfaceX, surfaceY;
	RECTANGLE_16 surfaceRect;
	const RECTANGLE_16* extents;
	rdpGdi* gdi;
	gdi = xfc->context.gdi;
	surfaceX = surface->gdi.outputOriginX;
	surfaceY = surface->gdi.outputOriginY;
	surfaceRect.left = surfaceX;
	surfaceRect.top = surfaceY;
	surfaceRect.right = surfaceX + surface->gdi.width;
	surfaceRect.bottom = surfaceY + surface->gdi.height;
	XSetClipMask(xfc->display, xfc->gc, None);
	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);

	if (!region16_is_empty(&surface->gdi.invalidRegion))
	{
		extents = region16_extents(&surface->gdi.invalidRegion);
		width = extents->right - extents->left;
		height = extents->bottom - extents->top;

		if (width > surface->gdi.width)
			width = surface->gdi.width;

		if (height > surface->gdi.height)
			height = surface->gdi.height;

		if (surface->stage)
		{
			freerdp_image_copy(surface->stage, gdi->dstFormat,
			                   surface->stageScanline, 0, 0,
			                   surface->gdi.width, surface->gdi.height,
			                   surface->gdi.data, surface->gdi.format,
			                   surface->gdi.scanline, 0, 0, NULL);
		}

#ifdef WITH_XRENDER

		if (xfc->context.settings->SmartSizing
		    || xfc->context.settings->MultiTouchGestures)
		{
			XPutImage(xfc->display, xfc->primary, xfc->gc, surface->image,
			          extents->left, extents->top, extents->left + surfaceX, extents->top + surfaceY,
			          width, height);
			xf_draw_screen(xfc, extents->left, extents->top, width, height);
		}
		else
#endif
		{
			XPutImage(xfc->display, xfc->drawable, xfc->gc,
			          surface->image, extents->left, extents->top,
			          extents->left + surfaceX, extents->top + surfaceY,
			          width, height);
		}
	}

	region16_clear(&surface->gdi.invalidRegion);
	XSetClipMask(xfc->display, xfc->gc, None);
	XSync(xfc->display, False);
	return 0;
}

static UINT xf_UpdateSurfaces(RdpgfxClientContext* context)
{
	UINT16 count;
	UINT32 index;
	UINT status = CHANNEL_RC_OK;
	xfGfxSurface* surface;
	UINT16* pSurfaceIds = NULL;
	rdpGdi* gdi = (rdpGdi*)context->custom;
	xfContext* xfc = (xfContext*) gdi->context;

	if (!gdi)
		return status;

	if (!gdi->graphicsReset)
		return status;

	context->GetSurfaceIds(context, &pSurfaceIds, &count);

	for (index = 0; index < count; index++)
	{
		surface = (xfGfxSurface*) context->GetSurfaceData(context, pSurfaceIds[index]);

		if (!surface || !surface->gdi.outputMapped)
			continue;

		status = xf_OutputUpdate(xfc, surface);

		if (status != 0)
			break;
	}

	free(pSurfaceIds);
	return status;
}

UINT xf_OutputExpose(xfContext* xfc, UINT32 x, UINT32 y,
                     UINT32 width, UINT32 height)
{
	UINT16 count;
	UINT32 index;
	UINT status = CHANNEL_RC_OK;
	xfGfxSurface* surface;
	RECTANGLE_16 invalidRect;
	RECTANGLE_16 surfaceRect;
	RECTANGLE_16 intersection;
	UINT16* pSurfaceIds = NULL;
	RdpgfxClientContext* context = xfc->gfx;
	invalidRect.left = x;
	invalidRect.top = y;
	invalidRect.right = x + width;
	invalidRect.bottom = y + height;
	context->GetSurfaceIds(context, &pSurfaceIds, &count);

	for (index = 0; index < count; index++)
	{
		surface = (xfGfxSurface*) context->GetSurfaceData(context, pSurfaceIds[index]);

		if (!surface || !surface->gdi.outputMapped)
			continue;

		surfaceRect.left = surface->gdi.outputOriginX;
		surfaceRect.top = surface->gdi.outputOriginY;
		surfaceRect.right = surface->gdi.outputOriginX + surface->gdi.width;
		surfaceRect.bottom = surface->gdi.outputOriginY + surface->gdi.height;

		if (rectangles_intersection(&invalidRect, &surfaceRect, &intersection))
		{
			/* Invalid rects are specified relative to surface origin */
			intersection.left -= surfaceRect.left;
			intersection.top -= surfaceRect.top;
			intersection.right -= surfaceRect.left;
			intersection.bottom -= surfaceRect.top;
			region16_union_rect(&surface->gdi.invalidRegion,
			                    &surface->gdi.invalidRegion,
			                    &intersection);
		}
	}

	free(pSurfaceIds);
	IFCALLRET(context->UpdateSurfaces, status, context);
	return status;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_CreateSurface(RdpgfxClientContext* context,
                             const RDPGFX_CREATE_SURFACE_PDU* createSurface)
{
	size_t size;
	xfGfxSurface* surface;
	rdpGdi* gdi = (rdpGdi*)context->custom;
	xfContext* xfc = (xfContext*) gdi->context;
	surface = (xfGfxSurface*) calloc(1, sizeof(xfGfxSurface));

	if (!surface)
		return CHANNEL_RC_NO_MEMORY;

	surface->gdi.codecs = codecs_new(gdi->context);

	if (!surface->gdi.codecs)
	{
		free(surface);
		return CHANNEL_RC_NO_MEMORY;
	}

	if (!freerdp_client_codecs_prepare(surface->gdi.codecs, FREERDP_CODEC_ALL,
	                                   createSurface->width, createSurface->height))
	{
		free(surface);
		return ERROR_INTERNAL_ERROR;
	}

	surface->gdi.surfaceId = createSurface->surfaceId;
	surface->gdi.width = (UINT32) createSurface->width;
	surface->gdi.height = (UINT32) createSurface->height;

	switch (createSurface->pixelFormat)
	{
		case GFX_PIXEL_FORMAT_ARGB_8888:
			surface->gdi.format = PIXEL_FORMAT_BGRA32;
			break;

		case GFX_PIXEL_FORMAT_XRGB_8888:
			surface->gdi.format = PIXEL_FORMAT_BGRX32;
			break;

		default:
			free(surface);
			return ERROR_INTERNAL_ERROR;
	}

	surface->gdi.scanline = surface->gdi.width * GetBytesPerPixel(
	                            surface->gdi.format);

	if (xfc->scanline_pad > 0)
	{
		surface->gdi.scanline += (xfc->scanline_pad / 8);
		surface->gdi.scanline -= (surface->gdi.scanline % (xfc->scanline_pad / 8));
	}

	size = surface->gdi.scanline * surface->gdi.height;
	surface->gdi.data = (BYTE*) _aligned_malloc(size, 16);

	if (!surface->gdi.data)
	{
		free(surface);
		return CHANNEL_RC_NO_MEMORY;
	}

	ZeroMemory(surface->gdi.data, size);

	if (gdi->dstFormat == surface->gdi.format)
	{
		surface->image = XCreateImage(xfc->display, xfc->visual, xfc->depth, ZPixmap, 0,
		                              (char*) surface->gdi.data, surface->gdi.width, surface->gdi.height,
		                              xfc->scanline_pad, surface->gdi.scanline);
	}
	else
	{
		UINT32 width = surface->gdi.width;
		UINT32 bytes = GetBytesPerPixel(gdi->dstFormat);
		surface->stageScanline = width * bytes;

		if (xfc->scanline_pad > 0)
		{
			surface->stageScanline += (xfc->scanline_pad / 8);
			surface->stageScanline -= (surface->stageScanline % (xfc->scanline_pad / 8));
		}

		size = surface->stageScanline * surface->gdi.height;
		surface->stage = (BYTE*) _aligned_malloc(size, 16);

		if (!surface->stage)
		{
			_aligned_free(surface->gdi.data);
			free(surface);
			return CHANNEL_RC_NO_MEMORY;
		}

		ZeroMemory(surface->stage, size);
		surface->image = XCreateImage(xfc->display, xfc->visual, xfc->depth,
		                              ZPixmap, 0, (char*) surface->stage,
		                              surface->gdi.width, surface->gdi.height,
		                              xfc->scanline_pad, surface->gdi.scanline);
	}

	surface->gdi.outputMapped = FALSE;
	region16_init(&surface->gdi.invalidRegion);
	context->SetSurfaceData(context, surface->gdi.surfaceId, (void*) surface);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_DeleteSurface(RdpgfxClientContext* context,
                             const RDPGFX_DELETE_SURFACE_PDU* deleteSurface)
{
	rdpCodecs* codecs = NULL;
	xfGfxSurface* surface = NULL;
	surface = (xfGfxSurface*) context->GetSurfaceData(context,
	          deleteSurface->surfaceId);

	if (surface)
	{
		XFree(surface->image);
		_aligned_free(surface->gdi.data);
		_aligned_free(surface->stage);
		region16_uninit(&surface->gdi.invalidRegion);
		codecs = surface->gdi.codecs;
		free(surface);
	}

	context->SetSurfaceData(context, deleteSurface->surfaceId, NULL);

	if (codecs && codecs->progressive)
		progressive_delete_surface_context(codecs->progressive,
		                                   deleteSurface->surfaceId);

	codecs_free(codecs);
	return CHANNEL_RC_OK;
}

void xf_graphics_pipeline_init(xfContext* xfc, RdpgfxClientContext* gfx)
{
	rdpGdi* gdi = xfc->context.gdi;
	gdi_graphics_pipeline_init(gdi, gfx);
	gfx->UpdateSurfaces = xf_UpdateSurfaces;
	gfx->CreateSurface = xf_CreateSurface;
	gfx->DeleteSurface = xf_DeleteSurface;
}

void xf_graphics_pipeline_uninit(xfContext* xfc, RdpgfxClientContext* gfx)
{
	rdpGdi* gdi = xfc->context.gdi;
	gdi_graphics_pipeline_uninit(gdi, gfx);
}
