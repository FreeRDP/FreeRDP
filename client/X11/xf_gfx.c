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

#include <freerdp/config.h>

#include <math.h>
#include <string.h>

#include <winpr/assert.h>
#include <winpr/cast.h>

#include <freerdp/log.h>
#include "xf_gfx.h"
#include "xf_rail.h"
#include "xf_utils.h"
#include "xf_window.h"

#include <X11/Xutil.h>

#ifdef WITH_XSHM
#include <X11/extensions/XShm.h>
#include <X11/extensions/shm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
#include <va/va.h>
#include <va/va_x11.h>
#endif

#define TAG CLIENT_TAG("x11")

#ifdef WITH_XSHM
typedef struct xf_shm_surface_entry
{
	const xfGfxSurface* surface;
	XShmSegmentInfo* shminfo;
	struct xf_shm_surface_entry* next;
} xfShmSurfaceEntry;

static xfShmSurfaceEntry* g_xf_shm_surfaces = NULL;

static xfShmSurfaceEntry* xf_shm_find_surface(const xfGfxSurface* surface)
{
	for (xfShmSurfaceEntry* cur = g_xf_shm_surfaces; cur; cur = cur->next)
	{
		if (cur->surface == surface)
			return cur;
	}

	return NULL;
}

static BOOL xf_shm_register_surface(const xfGfxSurface* surface, XShmSegmentInfo* shminfo)
{
	xfShmSurfaceEntry* entry = (xfShmSurfaceEntry*)calloc(1, sizeof(xfShmSurfaceEntry));
	if (!entry)
		return FALSE;

	entry->surface = surface;
	entry->shminfo = shminfo;
	entry->next = g_xf_shm_surfaces;
	g_xf_shm_surfaces = entry;
	return TRUE;
}

static XShmSegmentInfo* xf_shm_unregister_surface(const xfGfxSurface* surface)
{
	xfShmSurfaceEntry** cur = &g_xf_shm_surfaces;

	while (*cur)
	{
		xfShmSurfaceEntry* entry = *cur;
		if (entry->surface == surface)
		{
			XShmSegmentInfo* shminfo = entry->shminfo;
			*cur = entry->next;
			free(entry);
			return shminfo;
		}
		cur = &entry->next;
	}

	return NULL;
}


static int g_xf_shm_error_base = -1;
static int g_xf_shm_last_error = 0;
static BOOL g_xf_shm_runtime_disabled = FALSE;
static int (*g_xf_shm_old_error_handler)(Display*, XErrorEvent*) = NULL;

static int xf_shm_temp_error_handler(Display* display, XErrorEvent* event)
{
	WINPR_UNUSED(display);

	if ((g_xf_shm_error_base >= 0) && (event->error_code == (g_xf_shm_error_base + BadShmSeg)))
	{
		g_xf_shm_last_error = event->error_code;
		return 0;
	}

	if (g_xf_shm_old_error_handler)
		return g_xf_shm_old_error_handler(display, event);

	return 0;
}

static BOOL xf_shm_attach_checked(Display* display, XShmSegmentInfo* shminfo)
{
	int majorOpcode = 0;
	int eventBase = 0;
	int errorBase = 0;

	WINPR_ASSERT(display);
	WINPR_ASSERT(shminfo);

	if (!XQueryExtension(display, "MIT-SHM", &majorOpcode, &eventBase, &errorBase))
		return FALSE;

	g_xf_shm_error_base = errorBase;
	g_xf_shm_last_error = 0;
	g_xf_shm_old_error_handler = XSetErrorHandler(xf_shm_temp_error_handler);

	const Bool attached = XShmAttach(display, shminfo);
	XSync(display, False);

	XSetErrorHandler(g_xf_shm_old_error_handler);
	g_xf_shm_old_error_handler = NULL;
	g_xf_shm_error_base = -1;

	return attached && (g_xf_shm_last_error == 0);
}

static BOOL xf_shm_call_checked(Display* display, void (*fn)(Display*, void*), void* arg)
{
	int majorOpcode = 0;
	int eventBase = 0;
	int errorBase = 0;

	WINPR_ASSERT(display);
	WINPR_ASSERT(fn);

	if (!XQueryExtension(display, "MIT-SHM", &majorOpcode, &eventBase, &errorBase))
		return FALSE;

	g_xf_shm_error_base = errorBase;
	g_xf_shm_last_error = 0;
	g_xf_shm_old_error_handler = XSetErrorHandler(xf_shm_temp_error_handler);

	fn(display, arg);
	XSync(display, False);

	XSetErrorHandler(g_xf_shm_old_error_handler);
	g_xf_shm_old_error_handler = NULL;
	g_xf_shm_error_base = -1;

	return g_xf_shm_last_error == 0;
}

typedef struct
{
	XShmSegmentInfo* shminfo;
} xfShmDetachArg;

static void xf_shm_detach_call(Display* display, void* arg)
{
	xfShmDetachArg* detach = (xfShmDetachArg*)arg;
	WINPR_ASSERT(detach);
	WINPR_ASSERT(detach->shminfo);
	XShmDetach(display, detach->shminfo);
}

typedef struct
{
	Drawable drawable;
	GC gc;
	XImage* image;
	int srcX;
	int srcY;
	int dstX;
	int dstY;
	unsigned int width;
	unsigned int height;
} xfShmPutArg;

static void xf_shm_put_call(Display* display, void* arg)
{
	xfShmPutArg* put = (xfShmPutArg*)arg;
	WINPR_ASSERT(put);
	WINPR_ASSERT(put->image);
	XShmPutImage(display, put->drawable, put->gc, put->image, put->srcX, put->srcY,
	             put->dstX, put->dstY, put->width, put->height, False);
}

static BOOL xf_shm_create_surface_image(xfContext* xfc, xfGfxSurface* surface)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);

	if (g_xf_shm_runtime_disabled)
		return FALSE;

	if (!XShmQueryExtension(xfc->display))
		return FALSE;

	/* Xlib stores the XShmSegmentInfo pointer in XImage->obdata.
	 * Therefore this structure must outlive XShmCreateImage() and must
	 * be the exact pointer later used by XShmPutImage()/XShmDetach().
	 */
	XShmSegmentInfo* shminfo = (XShmSegmentInfo*)calloc(1, sizeof(XShmSegmentInfo));
	if (!shminfo)
		return FALSE;

	XImage* image = XShmCreateImage(xfc->display, xfc->visual,
	                                WINPR_ASSERTING_INT_CAST(uint32_t, xfc->depth), ZPixmap,
	                                NULL, shminfo, surface->gdi.mappedWidth,
	                                surface->gdi.mappedHeight);
	if (!image)
	{
		free(shminfo);
		return FALSE;
	}

	const size_t size = (size_t)image->bytes_per_line * (size_t)image->height;
	shminfo->shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0600);
	if (shminfo->shmid < 0)
	{
		image->data = NULL;
		XDestroyImage(image);
		free(shminfo);
		return FALSE;
	}

	shminfo->shmaddr = (char*)shmat(shminfo->shmid, NULL, 0);
	if (shminfo->shmaddr == (char*)-1)
	{
		shmctl(shminfo->shmid, IPC_RMID, NULL);
		image->data = NULL;
		XDestroyImage(image);
		free(shminfo);
		return FALSE;
	}

	shminfo->readOnly = False;
	image->data = shminfo->shmaddr;

	if (!xf_shm_attach_checked(xfc->display, shminfo))
	{
		shmdt(shminfo->shmaddr);
		shmctl(shminfo->shmid, IPC_RMID, NULL);
		image->data = NULL;
		XDestroyImage(image);
		free(shminfo);
		return FALSE;
	}

	if (!xf_shm_register_surface(surface, shminfo))
	{
		xfShmDetachArg detach = { .shminfo = shminfo };
		(void)xf_shm_call_checked(xfc->display, xf_shm_detach_call, &detach);
		shmdt(shminfo->shmaddr);
		shmctl(shminfo->shmid, IPC_RMID, NULL);
		image->data = NULL;
		XDestroyImage(image);
		free(shminfo);
		return FALSE;
	}

	surface->image = image;
	surface->gdi.data = (BYTE*)image->data;
	surface->gdi.scanline = WINPR_ASSERTING_INT_CAST(UINT32, image->bytes_per_line);
	ZeroMemory(surface->gdi.data, size);
	return TRUE;
}

static BOOL xf_shm_destroy_surface_image(xfContext* xfc, xfGfxSurface* surface)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);

	XShmSegmentInfo* shminfo = xf_shm_unregister_surface(surface);
	if (!shminfo)
		return FALSE;

	xfShmDetachArg detach = { .shminfo = shminfo };
	(void)xf_shm_call_checked(xfc->display, xf_shm_detach_call, &detach);

	if (surface->image)
	{
		/* XDestroyImage frees the XImage object but not the SysV SHM segment. */
		XDestroyImage(surface->image);
		surface->image = NULL;
	}

	if (shminfo->shmaddr && (shminfo->shmaddr != (char*)-1))
		shmdt(shminfo->shmaddr);

	if (shminfo->shmid >= 0)
		shmctl(shminfo->shmid, IPC_RMID, NULL);

	free(shminfo);
	surface->gdi.data = NULL;
	return TRUE;
}

static void xf_surface_put_image(xfContext* xfc, xfGfxSurface* surface, Drawable drawable,
                                 int srcX, int srcY, int dstX, int dstY, unsigned int width,
                                 unsigned int height)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);

	if (xf_shm_find_surface(surface) && !g_xf_shm_runtime_disabled)
	{
		xfShmPutArg put = { .drawable = drawable,
		                    .gc = xfc->gc,
		                    .image = surface->image,
		                    .srcX = srcX,
		                    .srcY = srcY,
		                    .dstX = dstX,
		                    .dstY = dstY,
		                    .width = width,
		                    .height = height };

		if (xf_shm_call_checked(xfc->display, xf_shm_put_call, &put))
			return;

		/* XShmPutImage can fail asynchronously with BadShmSeg on some X setups.
		 * Disable XShm for the remaining lifetime of this process and fall back
		 * to plain XPutImage using the same local image buffer. */
		g_xf_shm_runtime_disabled = TRUE;
	}

	LogDynAndXPutImage(xfc->log, xfc->display, drawable, xfc->gc, surface->image, srcX, srcY,
	                  dstX, dstY, width, height);
}
#else
static void xf_surface_put_image(xfContext* xfc, xfGfxSurface* surface, Drawable drawable,
                                 int srcX, int srcY, int dstX, int dstY, unsigned int width,
                                 unsigned int height)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);
	LogDynAndXPutImage(xfc->log, xfc->display, drawable, xfc->gc, surface->image, srcX, srcY,
	                  dstX, dstY, width, height);
}
#endif

#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
#define XF_VAAPI_FALLBACK UINT32_MAX


#define XF_VA_FOURCC(a, b, c, d) \
	((UINT32)(a) | ((UINT32)(b) << 8) | ((UINT32)(c) << 16) | ((UINT32)(d) << 24))

static BOOL xf_vaapi_find_image_format(VADisplay dpy, UINT32 fourcc, VAImageFormat* out)
{
	WINPR_ASSERT(out);

	const int maxFormats = vaMaxNumImageFormats(dpy);
	if (maxFormats <= 0)
		return FALSE;

	VAImageFormat* formats = calloc((size_t)maxFormats, sizeof(VAImageFormat));
	if (!formats)
		return FALSE;

	int numFormats = 0;
	const VAStatus status = vaQueryImageFormats(dpy, formats, &numFormats);
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI direct: vaQueryImageFormats failed status=%d (%s)", status,
		          vaErrorStr(status));
		free(formats);
		return FALSE;
	}

	for (int i = 0; i < numFormats; i++)
	{
		if (formats[i].fourcc == fourcc)
		{
			*out = formats[i];
			free(formats);
			return TRUE;
		}
	}

	free(formats);
	return FALSE;
}



static BOOL xf_vaapi_getimage_bgrx_to_gdi(xfGfxSurface* surface,
                                          const H264_VAAPI_SURFACE* vaSurface)
{
	WINPR_ASSERT(surface);
	WINPR_ASSERT(vaSurface);

	VADisplay dpy = (VADisplay)vaSurface->display;
	VASurfaceID vaId = (VASurfaceID)vaSurface->surface;
	if (!dpy || !surface->gdi.data || (vaSurface->width == 0) || (vaSurface->height == 0))
		return FALSE;

	static VADisplay cachedFmtDpy = NULL;
	static BOOL cachedFmtValid = FALSE;
	static VAImageFormat cachedFmt = WINPR_C_ARRAY_INIT;

	static VADisplay cachedImageDpy = NULL;
	static BOOL cachedImageValid = FALSE;
	static VAImage cachedImage = WINPR_C_ARRAY_INIT;
	static UINT32 cachedImageWidth = 0;
	static UINT32 cachedImageHeight = 0;

	const UINT32 selectedFourcc = XF_VA_FOURCC('B', 'G', 'R', 'X');
	if (!cachedFmtValid || (cachedFmtDpy != dpy))
	{
		if (!xf_vaapi_find_image_format(dpy, selectedFourcc, &cachedFmt))
		{
			WLog_WARN(TAG, "VAAPI getimage: BGRX VAImage format unavailable");
			return FALSE;
		}

		cachedFmtDpy = dpy;
		cachedFmtValid = TRUE;
	}

	const UINT32 width = MIN(vaSurface->width, surface->gdi.mappedWidth);
	const UINT32 height = MIN(vaSurface->height, surface->gdi.mappedHeight);
	if ((width == 0) || (height == 0))
		return FALSE;

	if (cachedImageValid && ((cachedImageDpy != dpy) || (cachedImageWidth != width) ||
	                       (cachedImageHeight != height)))
	{
		vaDestroyImage(cachedImageDpy, cachedImage.image_id);
		cachedImage = (VAImage)WINPR_C_ARRAY_INIT;
		cachedImageValid = FALSE;
		cachedImageDpy = NULL;
		cachedImageWidth = 0;
		cachedImageHeight = 0;
	}

	if (!cachedImageValid)
	{
		VAStatus status = vaCreateImage(dpy, &cachedFmt, width, height, &cachedImage);
		if (status != VA_STATUS_SUCCESS)
		{
			WLog_WARN(TAG, "VAAPI getimage: vaCreateImage(BGRX) failed status=%d (%s)", status,
			          vaErrorStr(status));
			return FALSE;
		}

		cachedImageDpy = dpy;
		cachedImageWidth = width;
		cachedImageHeight = height;
		cachedImageValid = TRUE;
	}

	VAStatus status = vaSyncSurface(dpy, vaId);
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI getimage: vaSyncSurface failed status=%d (%s)", status,
		          vaErrorStr(status));
		return FALSE;
	}

	status = vaGetImage(dpy, vaId, 0, 0, width, height, cachedImage.image_id);
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI getimage: vaGetImage(BGRX) failed status=%d (%s)", status,
		          vaErrorStr(status));
		return FALSE;
	}

	void* mapped = NULL;
	status = vaMapBuffer(dpy, cachedImage.buf, &mapped);
	if (status != VA_STATUS_SUCCESS || !mapped)
	{
		WLog_WARN(TAG, "VAAPI getimage: vaMapBuffer failed status=%d (%s) mapped=%p", status,
		          vaErrorStr(status), mapped);
		return FALSE;
	}

	const UINT32 bytesPerPixel = 4;
	const UINT32 copyBytes = width * bytesPerPixel;
	if ((cachedImage.pitches[0] < copyBytes) || (surface->gdi.scanline < copyBytes))
	{
		WLog_WARN(TAG, "VAAPI getimage: invalid pitch imagePitch=%u dstScanline=%u copyBytes=%u",
		          cachedImage.pitches[0], surface->gdi.scanline, copyBytes);
		vaUnmapBuffer(dpy, cachedImage.buf);
		return FALSE;
	}

	const BYTE* src = (const BYTE*)mapped + cachedImage.offsets[0];
	BYTE* dst = surface->gdi.data;

	for (UINT32 y = 0; y < height; y++)
	{
		const BYTE* srcRow = src + y * cachedImage.pitches[0];
		BYTE* dstRow = dst + y * surface->gdi.scanline;
		memcpy(dstRow, srcRow, copyBytes);
	}

	vaUnmapBuffer(dpy, cachedImage.buf);
	return TRUE;
}

static BOOL xf_vaapi_avc420_full_surface(const xfGfxSurface* surface,
                                         const RDPGFX_H264_METABLOCK* meta)
{
	WINPR_ASSERT(surface);
	WINPR_ASSERT(meta);

	if (meta->numRegionRects != 1)
		return FALSE;

	const RECTANGLE_16* rect = &meta->regionRects[0];
	return (rect->left == 0) && (rect->top == 0) &&
	       (rect->right >= surface->gdi.mappedWidth) &&
	       (rect->bottom >= surface->gdi.mappedHeight);
}


static BOOL xf_vaapi_render_allowed(const xfContext* xfc, const xfGfxSurface* surface)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);

	if (xfc->remote_app || !xfc->drawable)
		return FALSE;

	if (!surface->gdi.outputMapped || surface->gdi.windowMapped)
		return FALSE;

	if (surface->vaapiDirectDisabled)
		return FALSE;

	const rdpSettings* settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	if (freerdp_settings_get_bool(settings, FreeRDP_SmartSizing) ||
	    freerdp_settings_get_bool(settings, FreeRDP_MultiTouchGestures))
		return FALSE;

	return TRUE;
}

static void xf_vaapi_disable_direct(xfGfxSurface* surface)
{
	WINPR_ASSERT(surface);

	surface->vaapiDirectDisabled = TRUE;
	surface->vaapiDirectReady = FALSE;
}

static BOOL xf_vaapi_bgrx_output_available(xfGfxSurface* surface)
{
	WINPR_ASSERT(surface);

	const UINT32 width = surface->gdi.mappedWidth;
	const UINT32 height = surface->gdi.mappedHeight;
	if (!surface->vaDisplay || (width == 0) || (height == 0))
		return FALSE;

	VAImageFormat format = WINPR_C_ARRAY_INIT;
	if (!xf_vaapi_find_image_format((VADisplay)surface->vaDisplay, XF_VA_FOURCC('B', 'G', 'R', 'X'),
	                                &format))
	{
		WLog_WARN(TAG, "VAAPI direct: BGRX VAImage format unavailable, disabling direct path");
		return FALSE;
	}

	VAImage image = WINPR_C_ARRAY_INIT;
	const VAStatus status = vaCreateImage((VADisplay)surface->vaDisplay, &format, width, height,
	                                      &image);
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG,
		          "VAAPI direct: BGRX VAImage preflight failed status=%d (%s), disabling direct path",
		          status, vaErrorStr(status));
		return FALSE;
	}

	vaDestroyImage((VADisplay)surface->vaDisplay, image.image_id);
	return TRUE;
}

static BOOL xf_vaapi_ensure_h264(xfContext* xfc, xfGfxSurface* surface)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);

	if (surface->vaapiDirectDisabled)
		return FALSE;

	if (surface->vaapiH264)
		return surface->vaapiDirectReady;

	if (!surface->vaDisplay)
		surface->vaDisplay = vaGetDisplay(xfc->display);

	if (!surface->vaDisplay)
	{
		WLog_WARN(TAG, "VAAPI direct: ensure_h264 failed, vaGetDisplay returned NULL");
		xf_vaapi_disable_direct(surface);
		return FALSE;
	}

	int major = 0;
	int minor = 0;
	const VAStatus vaStatus = vaInitialize((VADisplay)surface->vaDisplay, &major, &minor);
	if (vaStatus != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI direct: vaInitialize failed status=%d", vaStatus);
		xf_vaapi_disable_direct(surface);
		return FALSE;
	}

	if (!xf_vaapi_bgrx_output_available(surface))
	{
		xf_vaapi_disable_direct(surface);
		return FALSE;
	}

	surface->vaapiH264 = h264_context_new(FALSE);
	if (!surface->vaapiH264)
	{
		WLog_WARN(TAG, "VAAPI direct: ensure_h264 failed at h264_context_new");
		xf_vaapi_disable_direct(surface);
		return FALSE;
	}

	if (!h264_context_set_vaapi_display(surface->vaapiH264, surface->vaDisplay))
	{
		WLog_WARN(TAG, "VAAPI direct: ensure_h264 failed at set_vaapi_display");
		goto fail;
	}

	if (!h264_context_reset(surface->vaapiH264, surface->gdi.width, surface->gdi.height))
	{
		WLog_WARN(TAG, "VAAPI direct: ensure_h264 failed at h264_context_reset size=%ux%u",
		          surface->gdi.width, surface->gdi.height);
		goto fail;
	}

	if (!h264_context_get_option(surface->vaapiH264, H264_CONTEXT_OPTION_HW_ACCEL))
	{
		WLog_WARN(TAG, "VAAPI direct: ensure_h264 failed at HW_ACCEL option");
		goto fail;
	}

	surface->vaapiDirectReady = TRUE;
	return TRUE;

fail:
	h264_context_free(surface->vaapiH264);
	surface->vaapiH264 = NULL;
	xf_vaapi_disable_direct(surface);
	WLog_WARN(TAG, "VAAPI direct: ensure_h264 failed, disabling direct path");
	return FALSE;
}

static UINT xf_OutputUpdateVAAPI(xfContext* xfc, xfGfxSurface* surface, BOOL* handled)
{
	UINT rc = CHANNEL_RC_OK;
	UINT32 nbRects = 0;
	const RECTANGLE_16* rects = nullptr;
	RECTANGLE_16 surfaceRect = WINPR_C_ARRAY_INIT;
	H264_VAAPI_SURFACE vaSurface = WINPR_C_ARRAY_INIT;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);
	WINPR_ASSERT(handled);

	*handled = FALSE;

	if (!surface->vaapiFramePending)
		return CHANNEL_RC_OK;

	/* Consume the pending VAAPI frame marker. This function does not present with
	 * vaPutSurface anymore. It converts the VAAPI surface to BGRX/BGRA with
	 * vaGetImage(), copies it into the normal GDI buffer, marks the GDI region
	 * dirty and then lets the existing XPutImage path draw it. */
	surface->vaapiFramePending = FALSE;

	if (!xf_vaapi_render_allowed(xfc, surface))
		goto clear;

	if (!h264_context_get_vaapi_surface(surface->vaapiH264, &vaSurface))
	{
		WLog_WARN(TAG, "VAAPI direct: no VAAPI surface available for vaGetImage output");
		goto clear;
	}

	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.mappedWidth);
	surfaceRect.bottom = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.mappedHeight);

	if (!region16_intersect_rect(&surface->vaapiInvalidRegion, &surface->vaapiInvalidRegion,
	                             &surfaceRect))
	{
		WLog_WARN(TAG, "VAAPI direct: invalid region intersect failed");
		goto clear;
	}

	rects = region16_rects(&surface->vaapiInvalidRegion, &nbRects);
	if (!rects || (nbRects == 0))
	{
		goto clear;
	}

	if (!xf_vaapi_getimage_bgrx_to_gdi(surface, &vaSurface))
	{
		WLog_WARN(TAG, "VAAPI direct: vaGetImage BGRX/BGRA copy failed, leaving normal path unhandled");
		goto clear;
	}

	/* The normal xf_OutputUpdate() path consumes surface->gdi.invalidRegion.
	 * We copied the full current decoded VAAPI frame into surface->gdi.data, so
	 * transfer the pending VAAPI invalid rects to the GDI invalid region. */
	region16_clear(&surface->gdi.invalidRegion);
	for (UINT32 x = 0; x < nbRects; x++)
	{
		if (!region16_union_rect(&surface->gdi.invalidRegion, &surface->gdi.invalidRegion,
		                         &rects[x]))
		{
			WLog_WARN(TAG, "VAAPI direct: failed to mark GDI invalid rect after vaGetImage");
			rc = ERROR_INTERNAL_ERROR;
			goto clear;
		}
	}


	/* Do not set *handled=TRUE here. The existing X11/GDI output path must still
	 * run so that XPutImage draws the GDI buffer we just filled. */
	*handled = FALSE;

clear:
	region16_clear(&surface->vaapiInvalidRegion);
	return rc;
}


static UINT xf_SurfaceCommand_AVC420_VAAPI(RdpgfxClientContext* context,
                                           const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = CHANNEL_RC_OK;
	rdpGdi* gdi = nullptr;
	xfContext* xfc = nullptr;
	xfGfxSurface* surface = nullptr;
	RDPGFX_AVC420_BITMAP_STREAM* bs = nullptr;
	RDPGFX_H264_METABLOCK* meta = nullptr;
	RECTANGLE_16 invalidRect = WINPR_C_ARRAY_INIT;
	BOOL updateNow = FALSE;


	WINPR_ASSERT(context);
	WINPR_ASSERT(cmd);

	gdi = (rdpGdi*)context->custom;
	WINPR_ASSERT(gdi);
	xfc = (xfContext*)gdi->context;
	WINPR_ASSERT(xfc);

	EnterCriticalSection(&context->mux);

	surface =
	    (xfGfxSurface*)context->GetSurfaceData(context, (UINT16)MIN(UINT16_MAX, cmd->surfaceId));
	if (!surface)
	{
		status = ERROR_NOT_FOUND;
		goto fail;
	}

	bs = (RDPGFX_AVC420_BITMAP_STREAM*)cmd->extra;
	if (!bs)
	{
		status = ERROR_INTERNAL_ERROR;
		goto fail;
	}

	meta = &bs->meta;


	const BOOL allowed = xf_vaapi_render_allowed(xfc, surface);
	if (!allowed)
	{
		status = XF_VAAPI_FALLBACK;
		goto fail;
	}

	if (!surface->vaapiDirectReady)
	{
		if (!xf_vaapi_avc420_full_surface(surface, meta))
		{
			status = XF_VAAPI_FALLBACK;
			goto fail;
		}

		if (!xf_vaapi_ensure_h264(xfc, surface))
		{
			status = XF_VAAPI_FALLBACK;
			goto fail;
		}
	}
/* #DATISCUM
	if (!xf_vaapi_render_allowed(xfc, surface) || !xf_vaapi_avc420_full_surface(surface, meta) ||
	    !xf_vaapi_ensure_h264(xfc, surface))
	{
		status = XF_VAAPI_FALLBACK;
		goto fail;
	}
*/
	const INT32 rc = avc420_decompress_to_vaapi(
//            surface->gdi.h264, bs->data, bs->length, surface->gdi.width, surface->gdi.height,
	    surface->vaapiH264, bs->data, bs->length, surface->gdi.width, surface->gdi.height,
	    meta->regionRects, meta->numRegionRects);
	if (rc < 0)
	{
		/*
		 * The normal FreeRDP AVC420 path also sees transient decode failures
		 * during stream start/resync. Do not fall back to the CPU GDI path here,
		 * otherwise one bad initial update permanently ruins the direct-output
		 * performance test.
		 */
		WLog_WARN(TAG,
		          "VAAPI direct: AVC420 decode failed rc=%" PRId32
		          ", dropping update without fallback",
		          rc);
		status = CHANNEL_RC_OK;
		goto fail;
	}

	if (rc == 0)
	{
		status = CHANNEL_RC_OK;
		goto fail;
	}

	invalidRect.left = 0;
	invalidRect.top = 0;
	invalidRect.right = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.mappedWidth);
	invalidRect.bottom = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.mappedHeight);

	region16_clear(&surface->gdi.invalidRegion);
	region16_clear(&surface->vaapiInvalidRegion);
	if (!region16_union_rect(&surface->vaapiInvalidRegion, &surface->vaapiInvalidRegion,
	                         &invalidRect))
	{
		status = ERROR_INTERNAL_ERROR;
		goto fail;
	}

	surface->vaapiFramePending = TRUE;
	updateNow = !gdi->inGfxFrame;

fail:
	LeaveCriticalSection(&context->mux);

	if ((status == CHANNEL_RC_OK) && updateNow)
		status = IFCALLRESULT(CHANNEL_RC_OK, context->UpdateSurfaces, context);

	return status;
}

static UINT xf_SurfaceCommand(RdpgfxClientContext* context, const RDPGFX_SURFACE_COMMAND* cmd)
{
	UINT status = XF_VAAPI_FALLBACK;
	rdpGdi* gdi = nullptr;
	xfContext* xfc = nullptr;

	if (!context || !cmd)
		return ERROR_INVALID_PARAMETER;
	gdi = (rdpGdi*)context->custom;
	if (!gdi)
		return ERROR_INVALID_PARAMETER;

	xfc = (xfContext*)gdi->context;
	if (!xfc)
		return ERROR_INTERNAL_ERROR;

/*	if (cmd->codecId == RDPGFX_CODECID_AVC420)
		status = xf_SurfaceCommand_AVC420_VAAPI(context, cmd);

	if (status == XF_VAAPI_FALLBACK)
		status = xfc->gfxSurfaceCommand(context, cmd);
#DATISCUM  */
	if (cmd->codecId == RDPGFX_CODECID_AVC420)
	{
		status = xf_SurfaceCommand_AVC420_VAAPI(context, cmd);
	}

	if (status == XF_VAAPI_FALLBACK)
	{
		if (xfc->gfxSurfaceCommand)
		{
			status = xfc->gfxSurfaceCommand(context, cmd);
		}
		else
		{
			WLog_WARN(TAG, "VAAPI direct: fallback requested but original SurfaceCommand is NULL");
			status = CHANNEL_RC_OK;
		}
	}


	return status;
}
#endif

static UINT xf_OutputUpdate(xfContext* xfc, xfGfxSurface* surface)
{
	UINT rc = ERROR_INTERNAL_ERROR;
	UINT32 surfaceX = 0;
	UINT32 surfaceY = 0;
	RECTANGLE_16 surfaceRect = WINPR_C_ARRAY_INIT;
	UINT32 nbRects = 0;
	const RECTANGLE_16* rects = nullptr;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);

#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
	BOOL vaapiHandled = FALSE;

	rc = xf_OutputUpdateVAAPI(xfc, surface, &vaapiHandled);
	if (rc != CHANNEL_RC_OK)
		return rc;

	if (vaapiHandled)
	{
		return CHANNEL_RC_OK;
	}
#endif

	rdpGdi* gdi = xfc->common.context.gdi;
	WINPR_ASSERT(gdi);

	rdpSettings* settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	surfaceX = surface->gdi.outputOriginX;
	surfaceY = surface->gdi.outputOriginY;
	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.mappedWidth);
	surfaceRect.bottom = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.mappedHeight);
	LogDynAndXSetClipMask(xfc->log, xfc->display, xfc->gc, None);
	LogDynAndXSetFunction(xfc->log, xfc->display, xfc->gc, GXcopy);
	LogDynAndXSetFillStyle(xfc->log, xfc->display, xfc->gc, FillSolid);
	if (!region16_intersect_rect(&(surface->gdi.invalidRegion), &(surface->gdi.invalidRegion),
	                             &surfaceRect))
		return ERROR_INTERNAL_ERROR;

	WINPR_ASSERT(surface->gdi.mappedWidth);
	WINPR_ASSERT(surface->gdi.mappedHeight);
	const double sx = 1.0 * surface->gdi.outputTargetWidth / (double)surface->gdi.mappedWidth;
	const double sy = 1.0 * surface->gdi.outputTargetHeight / (double)surface->gdi.mappedHeight;

	if (!(rects = region16_rects(&surface->gdi.invalidRegion, &nbRects)))
		return CHANNEL_RC_OK;

	xf_lock_x11(xfc);
	for (UINT32 x = 0; x < nbRects; x++)
	{
		const RECTANGLE_16* rect = &rects[x];
		const UINT32 nXSrc = rect->left;
		const UINT32 nYSrc = rect->top;
		const UINT32 swidth = rect->right - nXSrc;
		const UINT32 sheight = rect->bottom - nYSrc;
		const UINT32 nXDst = (UINT32)lround(1.0 * surfaceX + nXSrc * sx);
		const UINT32 nYDst = (UINT32)lround(1.0 * surfaceY + nYSrc * sy);
		const UINT32 dwidth = (UINT32)lround(1.0 * swidth * sx);
		const UINT32 dheight = (UINT32)lround(1.0 * sheight * sy);

		if (surface->stage)
		{
			if (!freerdp_image_scale(surface->stage, gdi->dstFormat, surface->stageScanline, nXSrc,
			                         nYSrc, dwidth, dheight, surface->gdi.data, surface->gdi.format,
			                         surface->gdi.scanline, nXSrc, nYSrc, swidth, sheight))
				goto fail;
		}

		if (xfc->remote_app)
		{
			xf_surface_put_image(xfc, surface, xfc->primary, WINPR_ASSERTING_INT_CAST(int, nXSrc),
				             WINPR_ASSERTING_INT_CAST(int, nYSrc), WINPR_ASSERTING_INT_CAST(int, nXDst),
				             WINPR_ASSERTING_INT_CAST(int, nYDst), dwidth, dheight);
			xf_rail_paint_surface(xfc, surface->gdi.windowId, rect);
		}
		else
#ifdef WITH_XRENDER
		    if (freerdp_settings_get_bool(settings, FreeRDP_SmartSizing) ||
		        freerdp_settings_get_bool(settings, FreeRDP_MultiTouchGestures))
		{
			xf_surface_put_image(xfc, surface, xfc->primary, WINPR_ASSERTING_INT_CAST(int, nXSrc),
				             WINPR_ASSERTING_INT_CAST(int, nYSrc), WINPR_ASSERTING_INT_CAST(int, nXDst),
				             WINPR_ASSERTING_INT_CAST(int, nYDst), dwidth, dheight);
			xf_draw_screen(xfc, WINPR_ASSERTING_INT_CAST(int32_t, nXDst),
			               WINPR_ASSERTING_INT_CAST(int32_t, nYDst),
			               WINPR_ASSERTING_INT_CAST(int32_t, dwidth),
			               WINPR_ASSERTING_INT_CAST(int32_t, dheight));
		}
		else
#endif
		{
			xf_surface_put_image(xfc, surface, xfc->drawable, WINPR_ASSERTING_INT_CAST(int, nXSrc),
				             WINPR_ASSERTING_INT_CAST(int, nYSrc), WINPR_ASSERTING_INT_CAST(int, nXDst),
				             WINPR_ASSERTING_INT_CAST(int, nYDst), dwidth, dheight);
		}
	}

	rc = CHANNEL_RC_OK;
fail:
	region16_clear(&surface->gdi.invalidRegion);
	LogDynAndXSetClipMask(xfc->log, xfc->display, xfc->gc, None);
	LogDynAndXSync(xfc->log, xfc->display, False);
	xf_unlock_x11(xfc);
	return rc;
}

static UINT xf_WindowUpdate(RdpgfxClientContext* context, xfGfxSurface* surface)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(surface);
	return IFCALLRESULT(CHANNEL_RC_OK, context->UpdateWindowFromSurface, context, &surface->gdi);
}

static UINT xf_UpdateSurfaces(RdpgfxClientContext* context)
{
	UINT16 count = 0;
	UINT status = CHANNEL_RC_OK;
	UINT16* pSurfaceIds = nullptr;
	rdpGdi* gdi = (rdpGdi*)context->custom;
	xfContext* xfc = nullptr;

	if (!gdi)
		return status;

	if (gdi->suppressOutput)
		return CHANNEL_RC_OK;

	xfc = (xfContext*)gdi->context;
	EnterCriticalSection(&context->mux);
	status = context->GetSurfaceIds(context, &pSurfaceIds, &count);
	if (status != CHANNEL_RC_OK)
		goto fail;

	for (UINT32 index = 0; index < count; index++)
	{
		xfGfxSurface* surface = (xfGfxSurface*)context->GetSurfaceData(context, pSurfaceIds[index]);

		if (!surface)
			continue;

		/* If UpdateSurfaceArea callback is available, the output has already been updated. */
		if (context->UpdateSurfaceArea)
		{
			if (surface->gdi.handleInUpdateSurfaceArea)
				continue;
		}

		if (surface->gdi.outputMapped)
			status = xf_OutputUpdate(xfc, surface);
		else if (surface->gdi.windowMapped)
			status = xf_WindowUpdate(context, surface);

		if (status != CHANNEL_RC_OK)
			break;
	}

fail:
	free(pSurfaceIds);
	LeaveCriticalSection(&context->mux);
	return status;
}

UINT xf_OutputExpose(xfContext* xfc, UINT32 x, UINT32 y, UINT32 width, UINT32 height)
{
	UINT16 count = 0;
	UINT status = ERROR_INTERNAL_ERROR;
	RECTANGLE_16 invalidRect = WINPR_C_ARRAY_INIT;
	RECTANGLE_16 intersection = WINPR_C_ARRAY_INIT;
	UINT16* pSurfaceIds = nullptr;
	RdpgfxClientContext* context = nullptr;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->common.context.gdi);

	context = xfc->common.context.gdi->gfx;
	WINPR_ASSERT(context);

	invalidRect.left = WINPR_ASSERTING_INT_CAST(UINT16, x);
	invalidRect.top = WINPR_ASSERTING_INT_CAST(UINT16, y);
	invalidRect.right = WINPR_ASSERTING_INT_CAST(UINT16, x + width);
	invalidRect.bottom = WINPR_ASSERTING_INT_CAST(UINT16, y + height);
	status = context->GetSurfaceIds(context, &pSurfaceIds, &count);

	if (status != CHANNEL_RC_OK)
		goto fail;

	if (!TryEnterCriticalSection(&context->mux))
	{
		free(pSurfaceIds);
		return CHANNEL_RC_OK;
	}
	for (UINT32 index = 0; index < count; index++)
	{
		RECTANGLE_16 surfaceRect = WINPR_C_ARRAY_INIT;
		xfGfxSurface* surface = (xfGfxSurface*)context->GetSurfaceData(context, pSurfaceIds[index]);

		if (!surface || (!surface->gdi.outputMapped && !surface->gdi.windowMapped))
			continue;

		surfaceRect.left = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.outputOriginX);
		surfaceRect.top = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.outputOriginY);
		surfaceRect.right = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.outputOriginX +
		                                                         surface->gdi.outputTargetWidth);
		surfaceRect.bottom = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.outputOriginY +
		                                                          surface->gdi.outputTargetHeight);

		if (rectangles_intersection(&invalidRect, &surfaceRect, &intersection))
		{
			/* Invalid rects are specified relative to surface origin */
			intersection.left -= surfaceRect.left;
			intersection.top -= surfaceRect.top;
			intersection.right -= surfaceRect.left;
			intersection.bottom -= surfaceRect.top;
#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
			if (surface->vaapiDirectReady && xf_vaapi_render_allowed(xfc, surface))
			{
				if (!region16_union_rect(&surface->vaapiInvalidRegion,
				                         &surface->vaapiInvalidRegion, &intersection))
				{
					free(pSurfaceIds);
					LeaveCriticalSection(&context->mux);

					goto fail;
				}
				surface->vaapiFramePending = TRUE;
				continue;
			}
#endif
			if (!region16_union_rect(&surface->gdi.invalidRegion, &surface->gdi.invalidRegion,
			                         &intersection))
			{
				free(pSurfaceIds);
				LeaveCriticalSection(&context->mux);

				goto fail;
			}
		}
	}

	free(pSurfaceIds);
	LeaveCriticalSection(&context->mux);
	IFCALLRET(context->UpdateSurfaces, status, context);

	if (status != CHANNEL_RC_OK)
		goto fail;

fail:
	return status;
}

static UINT32 x11_pad_scanline(UINT32 scanline, UINT32 inPad)
{
	/* Ensure X11 alignment is met */
	if (inPad > 0)
	{
		const UINT32 align = inPad / 8;
		const UINT32 pad = align - scanline % align;

		if (align != pad)
			scanline += pad;
	}

	/* 16 byte alignment is required for ASM optimized code */
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
	size_t size = 0;
	xfGfxSurface* surface = nullptr;
	rdpGdi* gdi = (rdpGdi*)context->custom;
	xfContext* xfc = (xfContext*)gdi->context;
	surface = (xfGfxSurface*)calloc(1, sizeof(xfGfxSurface));

	if (!surface)
		return CHANNEL_RC_NO_MEMORY;

	surface->gdi.codecs = context->codecs;

	if (!surface->gdi.codecs)
	{
		WLog_ERR(TAG, "global GDI codecs aren't set");
		goto out_free;
	}

	surface->gdi.surfaceId = createSurface->surfaceId;
	surface->gdi.width = x11_pad_scanline(createSurface->width, 0);
	surface->gdi.height = x11_pad_scanline(createSurface->height, 0);
	surface->gdi.mappedWidth = createSurface->width;
	surface->gdi.mappedHeight = createSurface->height;
	surface->gdi.outputTargetWidth = createSurface->width;
	surface->gdi.outputTargetHeight = createSurface->height;

	switch (createSurface->pixelFormat)
	{
		case GFX_PIXEL_FORMAT_ARGB_8888:
			surface->gdi.format = PIXEL_FORMAT_BGRA32;
			break;

		case GFX_PIXEL_FORMAT_XRGB_8888:
			surface->gdi.format = PIXEL_FORMAT_BGRX32;
			break;

		default:
			WLog_ERR(TAG, "unknown pixelFormat 0x%" PRIx32 "", createSurface->pixelFormat);
			ret = ERROR_INTERNAL_ERROR;
			goto out_free;
	}

	surface->gdi.scanline = surface->gdi.width * FreeRDPGetBytesPerPixel(surface->gdi.format);
	surface->gdi.scanline = x11_pad_scanline(surface->gdi.scanline,
	                                         WINPR_ASSERTING_INT_CAST(uint32_t, xfc->scanline_pad));
	size = 1ull * surface->gdi.scanline * surface->gdi.height;
	surface->gdi.data = (BYTE*)winpr_aligned_malloc(size, 16);

	if (!surface->gdi.data)
	{
		WLog_ERR(TAG, "unable to allocate GDI data");
		goto out_free;
	}

	ZeroMemory(surface->gdi.data, size);

	if (FreeRDPAreColorFormatsEqualNoAlpha(gdi->dstFormat, surface->gdi.format))
	{
		WINPR_ASSERT(xfc->depth != 0);
#ifdef WITH_XSHM
		BYTE* oldGdiData = surface->gdi.data;
		const BOOL useShm = xf_shm_create_surface_image(xfc, surface);
		if (useShm)
		{
			winpr_aligned_free(oldGdiData);
		}
		else
#endif
		{
			surface->image = LogDynAndXCreateImage(
			    xfc->log, xfc->display, xfc->visual, WINPR_ASSERTING_INT_CAST(uint32_t, xfc->depth),
			    ZPixmap, 0, (char*)surface->gdi.data, surface->gdi.mappedWidth,
			    surface->gdi.mappedHeight, xfc->scanline_pad,
			    WINPR_ASSERTING_INT_CAST(int, surface->gdi.scanline));
		}
	}
	else
	{
		UINT32 width = surface->gdi.width;
		UINT32 bytes = FreeRDPGetBytesPerPixel(gdi->dstFormat);
		surface->stageScanline = width * bytes;
		surface->stageScanline = x11_pad_scanline(
		    surface->stageScanline, WINPR_ASSERTING_INT_CAST(uint32_t, xfc->scanline_pad));
		size = 1ull * surface->stageScanline * surface->gdi.height;
		surface->stage = (BYTE*)winpr_aligned_malloc(size, 16);

		if (!surface->stage)
		{
			WLog_ERR(TAG, "unable to allocate stage buffer");
			goto out_free_gdidata;
		}

		ZeroMemory(surface->stage, size);
		WINPR_ASSERT(xfc->depth != 0);
		surface->image = LogDynAndXCreateImage(
		    xfc->log, xfc->display, xfc->visual, WINPR_ASSERTING_INT_CAST(uint32_t, xfc->depth),
		    ZPixmap, 0, (char*)surface->stage, surface->gdi.mappedWidth, surface->gdi.mappedHeight,
		    xfc->scanline_pad, WINPR_ASSERTING_INT_CAST(int, surface->stageScanline));
	}

	if (!surface->image)
	{
		WLog_ERR(TAG, "an error occurred when creating the XImage");
		goto error_surface_image;
	}

	surface->image->byte_order = LSBFirst;
	surface->image->bitmap_bit_order = LSBFirst;

	region16_init(&surface->gdi.invalidRegion);
#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
	region16_init(&surface->vaapiInvalidRegion);
#endif

	if (context->SetSurfaceData(context, surface->gdi.surfaceId, (void*)surface) != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "an error occurred during SetSurfaceData");
		goto error_set_surface_data;
	}

	return CHANNEL_RC_OK;
error_set_surface_data:
#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
	region16_uninit(&surface->vaapiInvalidRegion);
#endif
	region16_uninit(&surface->gdi.invalidRegion);
#ifdef WITH_XSHM
	if (!xf_shm_destroy_surface_image(xfc, surface))
#endif
	{
		surface->image->data = nullptr;
		XDestroyImage(surface->image);
	}
error_surface_image:
	winpr_aligned_free(surface->stage);
out_free_gdidata:
#ifdef WITH_XSHM
	if (!xf_shm_find_surface(surface))
#endif
		winpr_aligned_free(surface->gdi.data);
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
	rdpCodecs* codecs = nullptr;
	rdpGdi* gdi = (rdpGdi*)context->custom;
	xfContext* xfc = gdi ? (xfContext*)gdi->context : NULL;

	UINT status = 0;
	EnterCriticalSection(&context->mux);
	xfGfxSurface* surface =
	    (xfGfxSurface*)context->GetSurfaceData(context, deleteSurface->surfaceId);

	if (surface)
	{
		if (surface->gdi.windowMapped)
		{
			status = IFCALLRESULT(CHANNEL_RC_OK, context->UnmapWindowForSurface, context,
			                      surface->gdi.windowId);
			if (status != CHANNEL_RC_OK)
				goto fail;
		}

#ifdef WITH_GFX_H264
		h264_context_free(surface->gdi.h264);
#endif
#if defined(WITH_GFX_AV1)
		freerdp_av1_context_free(surface->gdi.av1);
#endif
#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
		h264_context_free(surface->vaapiH264);
		surface->vaapiH264 = NULL;
		region16_uninit(&surface->vaapiInvalidRegion);
#endif
#ifdef WITH_XSHM
		if (!xfc || !xf_shm_destroy_surface_image(xfc, surface))
#endif
		{
			surface->image->data = nullptr;
			XDestroyImage(surface->image);
			winpr_aligned_free(surface->gdi.data);
		}
		winpr_aligned_free(surface->stage);
		region16_uninit(&surface->gdi.invalidRegion);
		codecs = surface->gdi.codecs;
		free(surface);
	}

	status = context->SetSurfaceData(context, deleteSurface->surfaceId, nullptr);

	if (codecs && codecs->progressive)
		progressive_delete_surface_context(codecs->progressive, deleteSurface->surfaceId);

fail:
	LeaveCriticalSection(&context->mux);
	return status;
}

static UINT xf_UnmapWindowForSurface(RdpgfxClientContext* context, UINT64 windowID)
{
	WINPR_ASSERT(context);
	rdpGdi* gdi = (rdpGdi*)context->custom;
	WINPR_ASSERT(gdi);

	xfContext* xfc = (xfContext*)gdi->context;
	WINPR_ASSERT(gdi->context);

	if (freerdp_settings_get_bool(gdi->context->settings, FreeRDP_RemoteApplicationMode))
	{
		xfAppWindow* appWindow = xf_rail_get_window(xfc, windowID, FALSE);
		if (appWindow)
			xf_AppWindowDestroyImage(appWindow);
		xf_rail_return_window(appWindow, FALSE);
	}

	WLog_WARN(TAG, "function not implemented");
	return CHANNEL_RC_OK;
}

static UINT xf_UpdateWindowFromSurface(RdpgfxClientContext* context, gdiGfxSurface* surface)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(surface);

	rdpGdi* gdi = (rdpGdi*)context->custom;
	WINPR_ASSERT(gdi);

	xfContext* xfc = (xfContext*)gdi->context;
	WINPR_ASSERT(gdi->context);

	if (freerdp_settings_get_bool(gdi->context->settings, FreeRDP_RemoteApplicationMode))
		return xf_AppUpdateWindowFromSurface(xfc, surface);

	WLog_WARN(TAG, "function not implemented");
	return CHANNEL_RC_OK;
}

void xf_graphics_pipeline_init(xfContext* xfc, RdpgfxClientContext* gfx)
{
	rdpGdi* gdi = nullptr;
	const rdpSettings* settings = nullptr;
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(gfx);

	settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	gdi = xfc->common.context.gdi;

	gdi_graphics_pipeline_init(gdi, gfx);

	if (!freerdp_settings_get_bool(settings, FreeRDP_SoftwareGdi))
	{
		gfx->UpdateSurfaces = xf_UpdateSurfaces;
		gfx->CreateSurface = xf_CreateSurface;
		gfx->DeleteSurface = xf_DeleteSurface;

#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
		xfc->gfxSurfaceCommand = gfx->SurfaceCommand;
		gfx->SurfaceCommand = xf_SurfaceCommand;
#endif
	}

	gfx->UpdateWindowFromSurface = xf_UpdateWindowFromSurface;
	gfx->UnmapWindowForSurface = xf_UnmapWindowForSurface;
}

void xf_graphics_pipeline_uninit(xfContext* xfc, RdpgfxClientContext* gfx)
{
	WINPR_ASSERT(xfc);

	rdpGdi* gdi = xfc->common.context.gdi;
	gdi_graphics_pipeline_uninit(gdi, gfx);
}
