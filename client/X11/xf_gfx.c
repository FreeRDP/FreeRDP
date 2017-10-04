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
	UINT rc = ERROR_INTERNAL_ERROR;
	UINT32 surfaceX, surfaceY;
	RECTANGLE_16 surfaceRect;
	rdpGdi* gdi;
	UINT32 nbRects, x;
	const RECTANGLE_16* rects;
	gdi = xfc->context.gdi;
	surfaceX = surface->gdi.outputOriginX;
	surfaceY = surface->gdi.outputOriginY;
	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = surface->gdi.width;
	surfaceRect.bottom = surface->gdi.height;
	XSetClipMask(xfc->display, xfc->gc, None);
	XSetFunction(xfc->display, xfc->gc, GXcopy);
	XSetFillStyle(xfc->display, xfc->gc, FillSolid);

	region16_intersect_rect(&(surface->gdi.invalidRegion),
	                        &(surface->gdi.invalidRegion), &surfaceRect);

	if (!(rects = region16_rects(&surface->gdi.invalidRegion, &nbRects)))
		return CHANNEL_RC_OK;

	for (x = 0; x < nbRects; x++)
	{
		const UINT32 nXSrc = rects[x].left;
		const UINT32 nYSrc = rects[x].top;
		const UINT32 width = rects[x].right - nXSrc;
		const UINT32 height = rects[x].bottom - nYSrc;
		const UINT32 nXDst = surfaceX + nXSrc;
		const UINT32 nYDst = surfaceY + nYSrc;

		if (surface->stage)
		{
			if (!freerdp_image_copy(surface->stage, gdi->dstFormat,
			                        surface->stageScanline, nXSrc, nYSrc,
			                        width, height,
			                        surface->gdi.data, surface->gdi.format,
			                        surface->gdi.scanline, nXSrc, nYSrc,
			                        NULL, FREERDP_FLIP_NONE))
				goto fail;
		}

#ifdef WITH_XRENDER

		if (xfc->context.settings->SmartSizing
		    || xfc->context.settings->MultiTouchGestures)
		{
			XPutImage(xfc->display, xfc->primary, xfc->gc, surface->image,
			          nXSrc, nYSrc, nXDst, nYDst, width, height);
			xf_draw_screen(xfc, nXDst, nYDst, width, height);
		}
		else
#endif
		{
			XPutImage(xfc->display, xfc->drawable, xfc->gc,
			          surface->image, nXSrc, nYSrc,
			          nXDst, nYDst, width, height);
		}
	}

	rc = CHANNEL_RC_OK;
fail:
	region16_clear(&surface->gdi.invalidRegion);
	XSetClipMask(xfc->display, xfc->gc, None);
	XSync(xfc->display, False);
	return rc;
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

UINT32 x11_pad_scanline(UINT32 scanline, UINT32 inPad)
{
	/* Ensure X11 alignment is met */
	if (inPad > 0)
	{
		const UINT32 align = inPad / 8;
		const UINT32 pad = align - scanline % align;

		if (align != pad)
			scanline += pad;
	}

	/* 16 byte alingment is required for ASM optimized code */
	if (scanline % 16)
		scanline += 16 - scanline % 16;

	return scanline;
}


/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT xf_CreateSurface(RdpgfxClientContext* context,
                             const RDPGFX_CREATE_SURFACE_PDU* createSurface)
{
	UINT ret = CHANNEL_RC_NO_MEMORY;
	size_t size;
	xfGfxSurface* surface;
	rdpGdi* gdi = (rdpGdi*)context->custom;
	xfContext* xfc = (xfContext*) gdi->context;

	surface = (xfGfxSurface *) calloc(1, sizeof(xfGfxSurface));
	if (!surface)
		return CHANNEL_RC_NO_MEMORY;

	surface->gdi.codecs = gdi->context->codecs;
	if (!surface->gdi.codecs)
	{
		WLog_ERR(TAG, "%s: global GDI codecs aren't set", __FUNCTION__);
		goto out_free;
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
			WLog_ERR(TAG, "%s: unknown pixelFormat 0x%"PRIx32"", __FUNCTION__, createSurface->pixelFormat);
			ret = ERROR_INTERNAL_ERROR;
			goto out_free;
	}

	surface->gdi.scanline = surface->gdi.width * GetBytesPerPixel(surface->gdi.format);
	surface->gdi.scanline = x11_pad_scanline(surface->gdi.scanline, xfc->scanline_pad);
	size = surface->gdi.scanline * surface->gdi.height;

	surface->gdi.data = (BYTE*)_aligned_malloc(size, 16);
	if (!surface->gdi.data)
	{
		WLog_ERR(TAG, "%s: unable to allocate GDI data", __FUNCTION__);
		goto out_free;
	}
	ZeroMemory(surface->gdi.data, size);

	if (AreColorFormatsEqualNoAlpha(gdi->dstFormat, surface->gdi.format))
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
		surface->stageScanline = x11_pad_scanline(surface->stageScanline, xfc->scanline_pad);
		size = surface->stageScanline * surface->gdi.height;

		surface->stage = (BYTE*) _aligned_malloc(size, 16);
		if (!surface->stage)
		{
			WLog_ERR(TAG, "%s: unable to allocate stage buffer", __FUNCTION__);
			goto out_free_gdidata;
		}
		ZeroMemory(surface->stage, size);

		surface->image = XCreateImage(xfc->display, xfc->visual, xfc->depth,
		                              ZPixmap, 0, (char*) surface->stage,
		                              surface->gdi.width, surface->gdi.height,
		                              xfc->scanline_pad, surface->stageScanline);
	}

	if (!surface->image)
	{
		WLog_ERR(TAG, "%s: an error occurred when creating the XImage", __FUNCTION__);
		goto error_surface_image;
	}

	surface->image->byte_order = LSBFirst;
	surface->image->bitmap_bit_order = LSBFirst;

	surface->gdi.outputMapped = FALSE;
	region16_init(&surface->gdi.invalidRegion);
	if (context->SetSurfaceData(context, surface->gdi.surfaceId, (void*) surface) != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "%s: an error occurred during SetSurfaceData", __FUNCTION__);
		goto error_set_surface_data;
	}
	return CHANNEL_RC_OK;

error_set_surface_data:
	XFree(surface->image);
error_surface_image:
	_aligned_free(surface->stage);
out_free_gdidata:
	_aligned_free(surface->gdi.data);
out_free:
	free(surface);
	return ret;
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
#ifdef WITH_GFX_H264
		h264_context_free(surface->gdi.h264);
#endif
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
