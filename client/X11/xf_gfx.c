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
#include <inttypes.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>

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
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
#include <va/va.h>
#include <va/va_x11.h>
#include <va/va_vpp.h>
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
static int g_xf_shm_last_request_code = 0;
static int g_xf_shm_last_minor_code = 0;
static BOOL g_xf_shm_runtime_disabled = FALSE;
static BOOL g_xf_shm_runtime_checked = FALSE;
static BOOL g_xf_shm_runtime_available = FALSE;
static BOOL g_xf_shm_runtime_ok_logged = FALSE;
static BOOL g_xf_shm_runtime_fail_logged = FALSE;
static BOOL g_xf_shm_runtime_put_fail_logged = FALSE;
static BOOL g_xf_shm_runtime_put_checked = FALSE;
static int g_xf_shm_runtime_major = 0;
static int g_xf_shm_runtime_minor = 0;
static Bool g_xf_shm_runtime_pixmaps = False;
static int (*g_xf_shm_old_error_handler)(Display*, XErrorEvent*) = NULL;

static int xf_shm_temp_error_handler(Display* display, XErrorEvent* event)
{
	WINPR_UNUSED(display);

	if (g_xf_shm_error_base >= 0)
	{
		if (g_xf_shm_last_error == 0)
		{
			g_xf_shm_last_error = event->error_code;
			g_xf_shm_last_request_code = event->request_code;
			g_xf_shm_last_minor_code = event->minor_code;
		}
		return 0;
	}

	if (g_xf_shm_old_error_handler)
		return g_xf_shm_old_error_handler(display, event);

	return 0;
}


static BOOL xf_shm_runtime_query(xfContext* xfc)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->display);

	if (g_xf_shm_runtime_disabled)
		return FALSE;

	if (g_xf_shm_runtime_checked)
		return g_xf_shm_runtime_available;

	g_xf_shm_runtime_checked = TRUE;

	if (!XShmQueryExtension(xfc->display))
	{
		WLog_INFO(TAG, "XShm runtime: MIT-SHM extension not available on this X connection");
		g_xf_shm_runtime_available = FALSE;
		return FALSE;
	}

	if (!XShmQueryVersion(xfc->display, &g_xf_shm_runtime_major, &g_xf_shm_runtime_minor,
	                     &g_xf_shm_runtime_pixmaps))
	{
		WLog_WARN(TAG, "XShm runtime: XShmQueryVersion failed on this X connection");
		g_xf_shm_runtime_available = FALSE;
		return FALSE;
	}

	g_xf_shm_runtime_available = TRUE;
	return TRUE;
}

static BOOL xf_shm_attach_checked(xfContext* xfc, XShmSegmentInfo* shminfo)
{
	int majorOpcode = 0;
	int eventBase = 0;
	int errorBase = 0;
	Bool attached = False;
	BOOL rc = FALSE;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->display);
	WINPR_ASSERT(shminfo);

	xf_lock_x11(xfc);
	if (!XQueryExtension(xfc->display, "MIT-SHM", &majorOpcode, &eventBase, &errorBase))
		goto out;

	g_xf_shm_error_base = errorBase;
	g_xf_shm_last_error = 0;
	g_xf_shm_last_request_code = 0;
	g_xf_shm_last_minor_code = 0;
	g_xf_shm_old_error_handler = XSetErrorHandler(xf_shm_temp_error_handler);

	attached = XShmAttach(xfc->display, shminfo);
	XSync(xfc->display, False);

	XSetErrorHandler(g_xf_shm_old_error_handler);
	g_xf_shm_old_error_handler = NULL;
	g_xf_shm_error_base = -1;

	rc = attached && (g_xf_shm_last_error == 0);
out:
	xf_unlock_x11(xfc);
	return rc;
}

static BOOL xf_shm_call_checked(xfContext* xfc, void (*fn)(Display*, void*), void* arg, BOOL callerLocked)
{
	int majorOpcode = 0;
	int eventBase = 0;
	int errorBase = 0;
	BOOL rc = FALSE;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(xfc->display);
	WINPR_ASSERT(fn);

	if (!callerLocked)
		xf_lock_x11(xfc);

	if (!XQueryExtension(xfc->display, "MIT-SHM", &majorOpcode, &eventBase, &errorBase))
		goto out;

	g_xf_shm_error_base = errorBase;
	g_xf_shm_last_error = 0;
	g_xf_shm_last_request_code = 0;
	g_xf_shm_last_minor_code = 0;
	g_xf_shm_old_error_handler = XSetErrorHandler(xf_shm_temp_error_handler);

	fn(xfc->display, arg);
	XSync(xfc->display, False);

	XSetErrorHandler(g_xf_shm_old_error_handler);
	g_xf_shm_old_error_handler = NULL;
	g_xf_shm_error_base = -1;

	rc = (g_xf_shm_last_error == 0);
out:
	if (!callerLocked)
		xf_unlock_x11(xfc);
	return rc;
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

	if (!xf_shm_runtime_query(xfc))
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
	                                NULL, shminfo, surface->gdi.width,
	                                surface->gdi.height);
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

	if (!xf_shm_attach_checked(xfc, shminfo))
	{
		if (!g_xf_shm_runtime_fail_logged)
		{
			WLog_WARN(TAG,
			          "XShm runtime: attach failed on this X connection, disabling XShm "
			          "version=%d.%d sharedPixmaps=%s lastError=%d request=%d minor=%d",
			          g_xf_shm_runtime_major, g_xf_shm_runtime_minor,
			          g_xf_shm_runtime_pixmaps ? "yes" : "no", g_xf_shm_last_error,
			          g_xf_shm_last_request_code, g_xf_shm_last_minor_code);
			g_xf_shm_runtime_fail_logged = TRUE;
		}
		g_xf_shm_runtime_disabled = TRUE;
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
		(void)xf_shm_call_checked(xfc, xf_shm_detach_call, &detach, FALSE);
		shmdt(shminfo->shmaddr);
		shmctl(shminfo->shmid, IPC_RMID, NULL);
		image->data = NULL;
		XDestroyImage(image);
		free(shminfo);
		return FALSE;
	}

	if (!g_xf_shm_runtime_ok_logged)
	{
		WLog_INFO(TAG,
		          "XShm runtime: enabled version=%d.%d sharedPixmaps=%s image=%ux%u bpl=%d size=%zu",
		          g_xf_shm_runtime_major, g_xf_shm_runtime_minor,
		          g_xf_shm_runtime_pixmaps ? "yes" : "no", surface->gdi.width,
		          surface->gdi.height, image->bytes_per_line, size);
		g_xf_shm_runtime_ok_logged = TRUE;
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
	(void)xf_shm_call_checked(xfc, xf_shm_detach_call, &detach, FALSE);

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

		if (!g_xf_shm_runtime_put_checked)
		{
			if (xf_shm_call_checked(xfc, xf_shm_put_call, &put, TRUE))
			{
				g_xf_shm_runtime_put_checked = TRUE;
				return;
			}

			/* XShmPutImage can fail asynchronously with BadShmSeg on some X setups.
			 * Disable XShm for the remaining lifetime of this process and fall back
			 * to plain XPutImage using the same local image buffer. */
			if (!g_xf_shm_runtime_put_fail_logged)
			{
				WLog_WARN(TAG,
				          "XShm runtime: XShmPutImage failed, disabling XShm lastError=%d request=%d minor=%d",
				          g_xf_shm_last_error, g_xf_shm_last_request_code,
				          g_xf_shm_last_minor_code);
				g_xf_shm_runtime_put_fail_logged = TRUE;
			}
			g_xf_shm_runtime_disabled = TRUE;
		}
		else
		{
			xf_shm_put_call(xfc->display, &put);
			return;
		}
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

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

typedef struct xf_vaapi_readback_cache_entry
{
	const xfGfxSurface* surface;
	VADisplay dpy;
	BOOL imageValid;
	VAImage image;
	UINT32 width;
	UINT32 height;
	UINT32 fourcc;
	BYTE* staging;
	size_t stagingSize;
	size_t stagingPitch;
	struct xf_vaapi_readback_cache_entry* next;
} xfVaapiReadbackCacheEntry;

static xfVaapiReadbackCacheEntry* g_xf_vaapi_readback_cache = NULL;

static void xf_vaapi_readback_cache_reset(xfVaapiReadbackCacheEntry* entry)
{
	if (!entry)
		return;

	if (entry->imageValid && entry->dpy)
		vaDestroyImage(entry->dpy, entry->image.image_id);

	if (entry->staging && (entry->stagingSize > 0))
		munmap(entry->staging, entry->stagingSize);

	entry->dpy = NULL;
	entry->imageValid = FALSE;
	entry->image = (VAImage)WINPR_C_ARRAY_INIT;
	entry->width = 0;
	entry->height = 0;
	entry->fourcc = 0;
	entry->staging = NULL;
	entry->stagingSize = 0;
	entry->stagingPitch = 0;
}

static xfVaapiReadbackCacheEntry* xf_vaapi_readback_cache_find(const xfGfxSurface* surface)
{
	for (xfVaapiReadbackCacheEntry* cur = g_xf_vaapi_readback_cache; cur; cur = cur->next)
	{
		if (cur->surface == surface)
			return cur;
	}

	return NULL;
}

static xfVaapiReadbackCacheEntry* xf_vaapi_readback_cache_get(const xfGfxSurface* surface)
{
	xfVaapiReadbackCacheEntry* entry = xf_vaapi_readback_cache_find(surface);
	if (entry)
		return entry;

	entry = (xfVaapiReadbackCacheEntry*)calloc(1, sizeof(xfVaapiReadbackCacheEntry));
	if (!entry)
		return NULL;

	entry->surface = surface;
	entry->next = g_xf_vaapi_readback_cache;
	g_xf_vaapi_readback_cache = entry;
	return entry;
}

static void xf_vaapi_readback_cache_destroy(const xfGfxSurface* surface)
{
	xfVaapiReadbackCacheEntry** cur = &g_xf_vaapi_readback_cache;

	while (*cur)
	{
		xfVaapiReadbackCacheEntry* entry = *cur;
		if (entry->surface == surface)
		{
			*cur = entry->next;
			xf_vaapi_readback_cache_reset(entry);
			free(entry);
			return;
		}
		cur = &entry->next;
	}
}

static void xf_vaapi_readback_cache_destroy_all(void)
{
	while (g_xf_vaapi_readback_cache)
	{
		xfVaapiReadbackCacheEntry* entry = g_xf_vaapi_readback_cache;
		g_xf_vaapi_readback_cache = entry->next;
		xf_vaapi_readback_cache_reset(entry);
		free(entry);
	}
}

static BOOL xf_vaapi_copy_rows(BYTE* dst, size_t dstPitch, const BYTE* src, size_t srcPitch,
                               size_t rowBytes, UINT32 height)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(src);

	if ((rowBytes == 0) || (height == 0))
		return FALSE;

	if ((dstPitch == rowBytes) && (srcPitch == rowBytes))
	{
		memcpy(dst, src, rowBytes * (size_t)height);
		return TRUE;
	}

	for (UINT32 y = 0; y < height; y++)
	{
		const BYTE* srcRow = src + (size_t)y * srcPitch;
		BYTE* dstRow = dst + (size_t)y * dstPitch;
		memcpy(dstRow, srcRow, rowBytes);
	}

	return TRUE;
}

static size_t xf_vaapi_align_size(size_t value, size_t alignment)
{
	return (value + alignment - 1) & ~(alignment - 1);
}

/* Datiscum 38 diagnostic build.
 * PutSurface probing and mid-stream fallback to the original AVC420 decoder
 * are disabled. This build keeps the custom VAAPI decoder path but removes
 * vaGetImage from the presentation path. VPP renders into one persistent
 * USER_PTR RGB32 surface backed by mmap memory; the existing X11 path then
 * copies that stable buffer into GDI/XShm. Diagnostics use FreeRDP's normal
 * logging and are visible with /log-level:TRACE.
 */
static BOOL xf_vaapi_trace_enabled(void)
{
	static wLog* log = NULL;
	if (!log)
		log = WLog_Get(TAG);
	return WLog_IsLevelActive(log, WLOG_TRACE);
}

static BOOL xf_vaapi_diag_enabled(void)
{
	return xf_vaapi_trace_enabled();
}

#define XF_VAAPI_DIAG_INFO(...) \
	do \
	{ \
		WLog_Print_tag(TAG, WLOG_TRACE, __VA_ARGS__); \
	} while (0)

static UINT64 xf_vaapi_now_us(void)
{
	struct timespec ts = { 0 };
	if (!xf_vaapi_trace_enabled())
		return 0;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		return 0;
	return ((UINT64)ts.tv_sec * 1000000ULL) + ((UINT64)ts.tv_nsec / 1000ULL);
}

static UINT64 xf_vaapi_elapsed_us(UINT64 start)
{
	const UINT64 now = xf_vaapi_now_us();
	if ((start == 0) || (now < start))
		return 0;
	return now - start;
}

static void xf_vaapi_perf_stage(const char* stage, UINT32 surfaceId, UINT64 durationUs)
{
	WLog_Print_tag(TAG, WLOG_TRACE, "VAAPI16 PERF: stage=%s surface=%u duration_us=%" PRIu64,
	               stage ? stage : "?", surfaceId, durationUs);
}

static void xf_vaapi_perf_decision(const char* where, UINT32 surfaceId, const char* decision)
{
	WLog_Print_tag(TAG, WLOG_TRACE, "VAAPI16 PERF: decision=%s surface=%u where=%s",
	               decision ? decision : "?", surfaceId, where ? where : "?");
}

static BOOL xf_vaapi_diag_looks_annexb(const BYTE* data, UINT32 size)
{
	if (!data)
		return FALSE;

	if ((size >= 5) && (data[0] == 0x00) && (data[1] == 0x00) &&
	    (data[2] == 0x00) && (data[3] == 0x01))
		return TRUE;

	if ((size >= 4) && (data[0] == 0x00) && (data[1] == 0x00) &&
	    (data[2] == 0x01))
		return TRUE;

	return FALSE;
}

static BYTE xf_vaapi_diag_first_nal_type(const BYTE* data, UINT32 size)
{
	if (!data)
		return 0xff;

	if ((size >= 5) && (data[0] == 0x00) && (data[1] == 0x00) &&
	    (data[2] == 0x00) && (data[3] == 0x01))
		return data[4] & 0x1f;

	if ((size >= 4) && (data[0] == 0x00) && (data[1] == 0x00) &&
	    (data[2] == 0x01))
		return data[3] & 0x1f;

	return 0xff;
}

static void xf_vaapi_diag_log_rects(const char* prefix, const RECTANGLE_16* rects, UINT32 nbRects)
{
	if (!xf_vaapi_diag_enabled())
		return;

	if (!prefix)
		prefix = "VAAPI diag rects";

	XF_VAAPI_DIAG_INFO( "%s: count=%" PRIu32, prefix, nbRects);

	if (!rects)
		return;

	for (UINT32 index = 0; (index < nbRects) && (index < 4); index++)
	{
		const RECTANGLE_16* r = &rects[index];
		XF_VAAPI_DIAG_INFO(
		          "%s[%" PRIu32 "]: left=%" PRIu16 " top=%" PRIu16
		          " right=%" PRIu16 " bottom=%" PRIu16
		          " width=%" PRIu16 " height=%" PRIu16,
		          prefix, index, r->left, r->top, r->right, r->bottom,
		          (UINT16)(r->right - r->left), (UINT16)(r->bottom - r->top));
	}
}


static UINT64 xf_vaapi_diag_hash_bytes(const BYTE* data, size_t length)
{
	UINT64 hash = UINT64_C(1469598103934665603);

	if (!data)
		return 0;

	for (size_t x = 0; x < length; x++)
	{
		hash ^= data[x];
		hash *= UINT64_C(1099511628211);
	}

	return hash;
}

static UINT64 xf_vaapi_diag_hash_rows(const BYTE* data, size_t pitch, UINT32 width,
                                      UINT32 height, size_t bytesPerPixel)
{
	UINT64 hash = UINT64_C(1469598103934665603);
	const size_t rowBytes = (size_t)width * bytesPerPixel;

	if (!data || (pitch == 0) || (rowBytes == 0) || (height == 0))
		return 0;

	/* Sample enough rows to catch pitch/row-shift problems without making TRACE unusable. */
	const UINT32 step = (height > 64) ? (height / 64) : 1;
	for (UINT32 y = 0; y < height; y += step)
	{
		const BYTE* row = data + (size_t)y * pitch;
		const size_t sample = (rowBytes > 4096) ? 4096 : rowBytes;
		const UINT64 rowHash = xf_vaapi_diag_hash_bytes(row, sample);
		hash ^= rowHash + ((UINT64)y << 32) + (UINT64)sample;
		hash *= UINT64_C(1099511628211);
	}

	return hash;
}

static void xf_vaapi_diag_log_bgrx_samples(const char* prefix, const BYTE* data, size_t pitch,
                                           UINT32 width, UINT32 height)
{
	if (!xf_vaapi_diag_enabled())
		return;

	if (!prefix)
		prefix = "VAAPI BGRX samples";

	if (!data || (pitch == 0) || (width == 0) || (height == 0))
	{
		XF_VAAPI_DIAG_INFO( "%s: unavailable data=%p pitch=%zu width=%u height=%u", prefix,
		          (const void*)data, pitch, width, height);
		return;
	}

	const UINT32 xs[5] = { 0, width / 4, width / 2, (width > 0) ? (width - 1) : 0,
	                     (width > 8) ? 8 : 0 };
	const UINT32 ys[5] = { 0, 0, height / 2, (height > 0) ? (height - 1) : 0,
	                     (height > 8) ? 8 : 0 };
	const size_t rowBytes = (size_t)width * 4;
	const UINT32 midY = height / 2;
	const UINT32 lastY = height - 1;
	const UINT64 row0 = xf_vaapi_diag_hash_bytes(data, rowBytes);
	const UINT64 rowM = xf_vaapi_diag_hash_bytes(data + (size_t)midY * pitch, rowBytes);
	const UINT64 rowL = xf_vaapi_diag_hash_bytes(data + (size_t)lastY * pitch, rowBytes);
	const UINT64 sampled = xf_vaapi_diag_hash_rows(data, pitch, width, height, 4);

	XF_VAAPI_DIAG_INFO(
	          "%s: data=%p pitch=%zu width=%u height=%u rowBytes=%zu "
	          "hash_row0=0x%016" PRIx64 " hash_rowMid=0x%016" PRIx64
	          " hash_rowLast=0x%016" PRIx64 " hash_sampled=0x%016" PRIx64,
	          prefix, (const void*)data, pitch, width, height, rowBytes, row0, rowM, rowL,
	          sampled);

	for (UINT32 i = 0; i < 5; i++)
	{
		const UINT32 x = xs[i];
		const UINT32 y = ys[i];
		if ((x >= width) || (y >= height))
			continue;

		const BYTE* px = data + (size_t)y * pitch + (size_t)x * 4;
		const UINT32 raw = ((UINT32)px[0]) | ((UINT32)px[1] << 8) | ((UINT32)px[2] << 16) |
		                 ((UINT32)px[3] << 24);
		XF_VAAPI_DIAG_INFO(
		          "%s[%u]: x=%u y=%u raw=0x%08" PRIx32
		          " bytes=%02x,%02x,%02x,%02x B=%u G=%u R=%u X=%u",
		          prefix, i, x, y, raw, px[0], px[1], px[2], px[3], px[0], px[1], px[2],
		          px[3]);
	}
}

static void xf_vaapi_diag_log_ximage(const char* prefix, const XImage* image)
{
	if (!xf_vaapi_diag_enabled())
		return;

	if (!prefix)
		prefix = "XImage diag";

	if (!image)
	{
		XF_VAAPI_DIAG_INFO( "%s: image=NULL", prefix);
		return;
	}

	XF_VAAPI_DIAG_INFO(
	          "%s: image=%p data=%p width=%d height=%d xoffset=%d format=%d "
	          "byte_order=%d bitmap_unit=%d bitmap_bit_order=%d bitmap_pad=%d "
	          "depth=%d bpp=%d bytes_per_line=%d masks r=0x%lx g=0x%lx b=0x%lx obdata=%p",
	          prefix, (const void*)image, (const void*)image->data, image->width, image->height,
	          image->xoffset, image->format, image->byte_order, image->bitmap_unit,
	          image->bitmap_bit_order, image->bitmap_pad, image->depth, image->bits_per_pixel,
	          image->bytes_per_line, image->red_mask, image->green_mask, image->blue_mask,
	          image->obdata);
}

static void xf_vaapi_diag_log_h264_payload(const char* prefix, const BYTE* data, UINT32 size)
{
	if (!xf_vaapi_diag_enabled())
		return;

	if (!prefix)
		prefix = "VAAPI H264 payload";

	UINT32 nalCount = 0;
	UINT32 sps = 0;
	UINT32 pps = 0;
	UINT32 sei = 0;
	UINT32 idr = 0;
	UINT32 nonIdr = 0;
	UINT32 other = 0;
	UINT32 firstNal = 0xff;
	UINT32 lastNal = 0xff;
	UINT32 maxNalBytes = 0;
	UINT32 maxNalType = 0xff;
	UINT32 pos = 0;

	while (data && (pos + 3 < size))
	{
		UINT32 start = UINT32_MAX;
		UINT32 prefixLen = 0;

		for (; pos + 3 < size; pos++)
		{
			if ((pos + 4 <= size) && (data[pos] == 0x00) && (data[pos + 1] == 0x00) &&
			    (data[pos + 2] == 0x00) && (data[pos + 3] == 0x01))
			{
				start = pos;
				prefixLen = 4;
				break;
			}
			if ((data[pos] == 0x00) && (data[pos + 1] == 0x00) && (data[pos + 2] == 0x01))
			{
				start = pos;
				prefixLen = 3;
				break;
			}
		}

		if (start == UINT32_MAX)
			break;

		const UINT32 nalStart = start + prefixLen;
		if (nalStart >= size)
			break;

		UINT32 next = nalStart + 1;
		for (; next + 3 < size; next++)
		{
			if ((next + 4 <= size) && (data[next] == 0x00) && (data[next + 1] == 0x00) &&
			    (data[next + 2] == 0x00) && (data[next + 3] == 0x01))
				break;
			if ((data[next] == 0x00) && (data[next + 1] == 0x00) && (data[next + 2] == 0x01))
				break;
		}

		const UINT32 nalBytes = (next > nalStart) ? (next - nalStart) : 0;
		const UINT32 nalType = data[nalStart] & 0x1f;
		if (firstNal == 0xff)
			firstNal = nalType;
		lastNal = nalType;
		nalCount++;

		switch (nalType)
		{
			case 1:
				nonIdr++;
				break;
			case 5:
				idr++;
				break;
			case 6:
				sei++;
				break;
			case 7:
				sps++;
				break;
			case 8:
				pps++;
				break;
			default:
				other++;
				break;
		}

		if (nalBytes > maxNalBytes)
		{
			maxNalBytes = nalBytes;
			maxNalType = nalType;
		}

		pos = next;
	}

	XF_VAAPI_DIAG_INFO(
	          "%s: size=%" PRIu32 " annexb=%d first_nal=%u last_nal=%u "
	          "nal_count=%" PRIu32 " sps=%" PRIu32 " pps=%" PRIu32
	          " sei=%" PRIu32 " idr=%" PRIu32 " nonidr=%" PRIu32
	          " other=%" PRIu32 " max_nal_type=%u max_nal_bytes=%" PRIu32
	          " hash=0x%016" PRIx64,
	          prefix, size, xf_vaapi_diag_looks_annexb(data, size) ? 1 : 0, firstNal, lastNal,
	          nalCount, sps, pps, sei, idr, nonIdr, other, maxNalType, maxNalBytes,
	          xf_vaapi_diag_hash_bytes(data, size));
}

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




static BYTE xf_vaapi_clip_u8(INT32 v)
{
	if (v < 0)
		return 0;
	if (v > 255)
		return 255;
	return (BYTE)v;
}

static void xf_vaapi_nv12_to_bgrx(BYTE* dst, size_t dstPitch, const BYTE* yPlane,
                                  const BYTE* uvPlane, size_t yPitch, size_t uvPitch,
                                  UINT32 width, UINT32 height)
{
	WINPR_ASSERT(dst);
	WINPR_ASSERT(yPlane);
	WINPR_ASSERT(uvPlane);

	for (UINT32 y = 0; y < height; y++)
	{
		const BYTE* yRow = yPlane + (size_t)y * yPitch;
		const BYTE* uvRow = uvPlane + (size_t)(y / 2) * uvPitch;
		BYTE* dstRow = dst + (size_t)y * dstPitch;

		for (UINT32 x = 0; x < width; x++)
		{
			const UINT32 uvx = (x & ~1U);
			const INT32 yy = (INT32)yRow[x];
			const INT32 uu = (INT32)uvRow[uvx + 0];
			const INT32 vv = (INT32)uvRow[uvx + 1];
			const INT32 c = yy - 16;
			const INT32 d = uu - 128;
			const INT32 e = vv - 128;
			const INT32 cc = (c < 0) ? 0 : c;
			const INT32 r = (298 * cc + 409 * e + 128) >> 8;
			const INT32 g = (298 * cc - 100 * d - 208 * e + 128) >> 8;
			const INT32 b = (298 * cc + 516 * d + 128) >> 8;
			BYTE* px = dstRow + (size_t)x * 4;

			px[0] = xf_vaapi_clip_u8(b);
			px[1] = xf_vaapi_clip_u8(g);
			px[2] = xf_vaapi_clip_u8(r);
			px[3] = 0xff;
		}
	}
}

static BOOL xf_vaapi_getimage_nv12_to_gdi(xfGfxSurface* surface,
                                          const H264_VAAPI_SURFACE* vaSurface)
{
	const UINT64 perfStart = xf_vaapi_now_us();
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

	const UINT32 selectedFourcc = XF_VA_FOURCC('N', 'V', '1', '2');
	if (!cachedFmtValid || (cachedFmtDpy != dpy))
	{
		if (!xf_vaapi_find_image_format(dpy, selectedFourcc, &cachedFmt))
		{
			WLog_WARN(TAG, "VAAPI getimage: NV12 VAImage format unavailable");
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
			WLog_WARN(TAG, "VAAPI getimage: vaCreateImage(NV12) failed status=%d (%s)", status,
			          vaErrorStr(status));
			return FALSE;
		}

		cachedImageDpy = dpy;
		cachedImageWidth = width;
		cachedImageHeight = height;
		cachedImageValid = TRUE;
	}

	UINT64 perfCall = xf_vaapi_now_us();
	VAStatus status = vaSyncSurface(dpy, vaId);
	xf_vaapi_perf_stage("nv12_vaSyncSurface", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI getimage: vaSyncSurface(NV12) failed status=%d (%s)", status,
		          vaErrorStr(status));
		return FALSE;
	}

	perfCall = xf_vaapi_now_us();
	status = vaGetImage(dpy, vaId, 0, 0, width, height, cachedImage.image_id);
	xf_vaapi_perf_stage("nv12_vaGetImage", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI getimage: vaGetImage(NV12) failed status=%d (%s)", status,
		          vaErrorStr(status));
		return FALSE;
	}

	XF_VAAPI_DIAG_INFO(
	          "VAAPI NV12 image layout: image=%ux%u id=%u buf=%u dataSize=%u numPlanes=%u "
	          "fourcc=%c%c%c%c bpp=%u depth=%u byteOrder=%u "
	          "pitch0=%u pitch1=%u pitch2=%u offset0=%u offset1=%u offset2=%u",
	          cachedImage.width, cachedImage.height, cachedImage.image_id, cachedImage.buf,
	          cachedImage.data_size, cachedImage.num_planes,
	          (char)(cachedImage.format.fourcc & 0xff),
	          (char)((cachedImage.format.fourcc >> 8) & 0xff),
	          (char)((cachedImage.format.fourcc >> 16) & 0xff),
	          (char)((cachedImage.format.fourcc >> 24) & 0xff),
	          cachedImage.format.bits_per_pixel, cachedImage.format.depth,
	          cachedImage.format.byte_order, cachedImage.pitches[0], cachedImage.pitches[1],
	          cachedImage.pitches[2], cachedImage.offsets[0], cachedImage.offsets[1],
	          cachedImage.offsets[2]);

	if ((cachedImage.num_planes < 2) || (cachedImage.pitches[0] < width) ||
	    (cachedImage.pitches[1] < width) || (surface->gdi.scanline < (width * 4U)))
	{
		WLog_WARN(TAG,
		          "VAAPI getimage: invalid NV12 layout planes=%u pitch0=%u pitch1=%u dstPitch=%u width=%u",
		          cachedImage.num_planes, cachedImage.pitches[0], cachedImage.pitches[1],
		          surface->gdi.scanline, width);
		return FALSE;
	}

	void* mapped = NULL;
	perfCall = xf_vaapi_now_us();
	status = vaMapBuffer(dpy, cachedImage.buf, &mapped);
	xf_vaapi_perf_stage("nv12_vaMapBuffer", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));
	if (status != VA_STATUS_SUCCESS || !mapped)
	{
		WLog_WARN(TAG, "VAAPI getimage: vaMapBuffer(NV12) failed status=%d (%s) mapped=%p", status,
		          vaErrorStr(status), mapped);
		return FALSE;
	}

	const BYTE* base = (const BYTE*)mapped;
	const BYTE* yPlane = base + cachedImage.offsets[0];
	const BYTE* uvPlane = base + cachedImage.offsets[1];
	BYTE* dst = surface->gdi.data;

	xf_vaapi_diag_log_bgrx_samples("GDI destination before NV12 convert", dst,
	                              surface->gdi.scanline, width, height);
	XF_VAAPI_DIAG_INFO(
	          "VAAPI NV12 convert: y=%p uv=%p dst=%p yPitch=%u uvPitch=%u dstPitch=%u width=%u height=%u",
	          (const void*)yPlane, (const void*)uvPlane, (void*)dst, cachedImage.pitches[0],
	          cachedImage.pitches[1], surface->gdi.scanline, width, height);

	perfCall = xf_vaapi_now_us();
	xf_vaapi_nv12_to_bgrx(dst, surface->gdi.scanline, yPlane, uvPlane,
	                      cachedImage.pitches[0], cachedImage.pitches[1], width, height);
	xf_vaapi_perf_stage("nv12_cpu_convert_bgrx", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));

	xf_vaapi_diag_log_bgrx_samples("GDI destination after NV12 convert", dst,
	                              surface->gdi.scanline, width, height);

	XF_VAAPI_DIAG_INFO(
	          "VAAPI NV12 convert done: vaSurface=%ux%u gdiMapped=%ux%u image=%ux%u "
	          "dstPitch=%u dstHash=0x%016" PRIx64,
	          vaSurface->width, vaSurface->height, surface->gdi.mappedWidth,
	          surface->gdi.mappedHeight, width, height, surface->gdi.scanline,
	          xf_vaapi_diag_hash_rows(dst, surface->gdi.scanline, width, height, 4));

	vaUnmapBuffer(dpy, cachedImage.buf);
	xf_vaapi_perf_stage("nv12_getimage_to_gdi_total", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfStart));
	return TRUE;
}

static BOOL g_xf_vaapi_bgrx_probe_done = FALSE;
static BOOL g_xf_vaapi_force_nv12_output = FALSE;

static UINT32 g_xf_vaapi_vpp_target_fourcc = 0;
static BOOL g_xf_vaapi_vpp_userptr_unsupported = FALSE;
static Display* g_xf_vaapi_xdisplay = NULL;
static VADisplay g_xf_vaapi_display = NULL;
static BOOL g_xf_vaapi_display_initialized = FALSE;
static int g_xf_vaapi_display_major = 0;
static int g_xf_vaapi_display_minor = 0;

static BOOL xf_vaapi_getimage_bgrx_to_gdi(xfGfxSurface* surface,
                                          const H264_VAAPI_SURFACE* vaSurface)
{
	const UINT64 perfStart = xf_vaapi_now_us();
	WINPR_ASSERT(surface);
	WINPR_ASSERT(vaSurface);

	VADisplay dpy = (VADisplay)vaSurface->display;
	VASurfaceID vaId = (VASurfaceID)vaSurface->surface;
	if (!dpy || !surface->gdi.data || (vaSurface->width == 0) || (vaSurface->height == 0))
		return FALSE;

	static VADisplay cachedFmtDpy = NULL;
	static BOOL cachedFmtValid = FALSE;
	static VAImageFormat cachedFmt = WINPR_C_ARRAY_INIT;
	static UINT32 cachedFmtFourcc = 0;

	const UINT32 selectedFourcc = g_xf_vaapi_vpp_target_fourcc ?
	    g_xf_vaapi_vpp_target_fourcc : XF_VA_FOURCC('B', 'G', 'R', 'X');
	if (!cachedFmtValid || (cachedFmtDpy != dpy) || (cachedFmtFourcc != selectedFourcc))
	{
		if (!xf_vaapi_find_image_format(dpy, selectedFourcc, &cachedFmt))
		{
			WLog_WARN(TAG, "VAAPI getimage: selected RGB32 VAImage format unavailable");
			return FALSE;
		}

		cachedFmtDpy = dpy;
		cachedFmtFourcc = selectedFourcc;
		cachedFmtValid = TRUE;
	}

	const UINT32 width = MIN(vaSurface->width, surface->gdi.mappedWidth);
	const UINT32 height = MIN(vaSurface->height, surface->gdi.mappedHeight);
	if ((width == 0) || (height == 0))
		return FALSE;

	const size_t bytesPerPixel = 4;
	const size_t rowBytes = (size_t)width * bytesPerPixel;
	const size_t dstPitch = (size_t)surface->gdi.scanline;
	if (dstPitch < rowBytes)
	{
		WLog_WARN(TAG,
		          "VAAPI getimage: invalid GDI pitch dstScanline=%zu rowBytes=%zu",
		          dstPitch, rowBytes);
		return FALSE;
	}

	xfVaapiReadbackCacheEntry* cache = xf_vaapi_readback_cache_get(surface);
	if (!cache)
		return FALSE;

	if (cache->imageValid && ((cache->dpy != dpy) || (cache->width != width) ||
	                         (cache->height != height) || (cache->fourcc != selectedFourcc) ||
	                         (cache->stagingPitch != rowBytes)))
	{
		xf_vaapi_readback_cache_reset(cache);
	}

	if (!cache->imageValid)
	{
		VAStatus status = vaCreateImage(dpy, &cachedFmt, width, height, &cache->image);
		if (status != VA_STATUS_SUCCESS)
		{
			WLog_WARN(TAG, "VAAPI getimage: vaCreateImage(RGB32 cached) failed status=%d (%s)",
			          status, vaErrorStr(status));
			return FALSE;
		}

		cache->stagingPitch = rowBytes;
		cache->stagingSize = rowBytes * (size_t)height;
		cache->staging = (BYTE*)mmap(NULL, cache->stagingSize, PROT_READ | PROT_WRITE,
		                             MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		if (cache->staging == MAP_FAILED)
		{
			cache->staging = NULL;
			vaDestroyImage(dpy, cache->image.image_id);
			cache->image = (VAImage)WINPR_C_ARRAY_INIT;
			WLog_WARN(TAG, "VAAPI getimage: persistent mmap staging allocation failed size=%zu",
			          cache->stagingSize);
			return FALSE;
		}

		cache->dpy = dpy;
		cache->width = width;
		cache->height = height;
		cache->fourcc = selectedFourcc;
		cache->imageValid = TRUE;
		ZeroMemory(cache->staging, cache->stagingSize);

		WLog_INFO(TAG,
		          "VAAPI readback cache: created surface=%u image=%ux%u fourcc=%c%c%c%c "
		          "buf=%u staging=%p stagingPitch=%zu stagingSize=%zu",
		          surface->gdi.surfaceId, width, height,
		          (char)(selectedFourcc & 0xff), (char)((selectedFourcc >> 8) & 0xff),
		          (char)((selectedFourcc >> 16) & 0xff), (char)((selectedFourcc >> 24) & 0xff),
		          cache->image.buf, (void*)cache->staging, cache->stagingPitch,
		          cache->stagingSize);
	}

	UINT64 perfCall = xf_vaapi_now_us();
	VAStatus status = vaSyncSurface(dpy, vaId);
	xf_vaapi_perf_stage("rgb32_vaSyncSurface", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI getimage: vaSyncSurface failed status=%d (%s)", status,
		          vaErrorStr(status));
		return FALSE;
	}

	perfCall = xf_vaapi_now_us();
	status = vaGetImage(dpy, vaId, 0, 0, width, height, cache->image.image_id);
	xf_vaapi_perf_stage("rgb32_vaGetImage", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI getimage: vaGetImage(RGB32 cached) failed status=%d (%s)",
		          status, vaErrorStr(status));
		return FALSE;
	}

	XF_VAAPI_DIAG_INFO(
	          "VAAPI image layout: image=%ux%u id=%u buf=%u dataSize=%u numPlanes=%u "
	          "fourcc=%c%c%c%c bpp=%u depth=%u byteOrder=%u "
	          "pitch0=%u pitch1=%u pitch2=%u offset0=%u offset1=%u offset2=%u "
	          "masks r=0x%08x g=0x%08x b=0x%08x a=0x%08x staging=%p stagingSize=%zu",
	          cache->image.width, cache->image.height, cache->image.image_id, cache->image.buf,
	          cache->image.data_size, cache->image.num_planes,
	          (char)(cache->image.format.fourcc & 0xff),
	          (char)((cache->image.format.fourcc >> 8) & 0xff),
	          (char)((cache->image.format.fourcc >> 16) & 0xff),
	          (char)((cache->image.format.fourcc >> 24) & 0xff),
	          cache->image.format.bits_per_pixel, cache->image.format.depth,
	          cache->image.format.byte_order, cache->image.pitches[0], cache->image.pitches[1],
	          cache->image.pitches[2], cache->image.offsets[0], cache->image.offsets[1],
	          cache->image.offsets[2], cache->image.format.red_mask, cache->image.format.green_mask,
	          cache->image.format.blue_mask, cache->image.format.alpha_mask, (void*)cache->staging,
	          cache->stagingSize);

	void* mapped = NULL;
	perfCall = xf_vaapi_now_us();
	status = vaMapBuffer(dpy, cache->image.buf, &mapped);
	xf_vaapi_perf_stage("rgb32_vaMapBuffer", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));
	if (status != VA_STATUS_SUCCESS || !mapped)
	{
		WLog_WARN(TAG, "VAAPI getimage: vaMapBuffer failed status=%d (%s) mapped=%p", status,
		          vaErrorStr(status), mapped);
		return FALSE;
	}

	const size_t srcPitch = (size_t)cache->image.pitches[0];
	if ((cache->image.num_planes < 1) || (cache->image.format.bits_per_pixel != 32) ||
	    (srcPitch < rowBytes) || !cache->staging || (cache->stagingPitch < rowBytes))
	{
		WLog_WARN(TAG,
		          "VAAPI getimage: invalid cached layout planes=%u bpp=%u srcPitch=%zu "
		          "stagingPitch=%zu rowBytes=%zu",
		          cache->image.num_planes, cache->image.format.bits_per_pixel, srcPitch,
		          cache->stagingPitch, rowBytes);
		vaUnmapBuffer(dpy, cache->image.buf);
		return FALSE;
	}

	const BYTE* src = (const BYTE*)mapped + cache->image.offsets[0];
	BYTE* staging = cache->staging;
	BYTE* dst = surface->gdi.data;

	xf_vaapi_diag_log_bgrx_samples("VAAPI source RGB32 before staging copy", src, srcPitch,
	                                width, height);
	xf_vaapi_diag_log_bgrx_samples("GDI destination before staging copy", dst, dstPitch,
	                                width, height);

	perfCall = xf_vaapi_now_us();
	xf_vaapi_copy_rows(staging, cache->stagingPitch, src, srcPitch, rowBytes, height);
	xf_vaapi_perf_stage("rgb32_cpu_memcpy_to_staging", surface->gdi.surfaceId,
	                     xf_vaapi_elapsed_us(perfCall));

	UINT64 srcHash = 0;
	if (xf_vaapi_diag_enabled())
		srcHash = xf_vaapi_diag_hash_rows(src, srcPitch, width, height, bytesPerPixel);

	vaUnmapBuffer(dpy, cache->image.buf);

	perfCall = xf_vaapi_now_us();
	xf_vaapi_copy_rows(dst, dstPitch, staging, cache->stagingPitch, rowBytes, height);
	xf_vaapi_perf_stage("rgb32_staging_memcpy_to_gdi", surface->gdi.surfaceId,
	                     xf_vaapi_elapsed_us(perfCall));

	xf_vaapi_diag_log_bgrx_samples("GDI destination after staging copy", dst, dstPitch,
	                                width, height);

	UINT64 stagingHash = 0;
	UINT64 dstHash = 0;
	if (xf_vaapi_diag_enabled())
	{
		stagingHash = xf_vaapi_diag_hash_rows(staging, cache->stagingPitch, width, height,
		                                      bytesPerPixel);
		dstHash = xf_vaapi_diag_hash_rows(dst, dstPitch, width, height, bytesPerPixel);
	}

	XF_VAAPI_DIAG_INFO(
	          "VAAPI RGB32 cached copy done: vaSurface=%ux%u gdiMapped=%ux%u image=%ux%u "
	          "rowBytes=%zu srcPitch=%zu stagingPitch=%zu dstPitch=%zu srcOffset=%u "
	          "dstData=%p staging=%p srcData=%p srcHash=0x%016" PRIx64
	          " stagingHash=0x%016" PRIx64 " dstHash=0x%016" PRIx64,
	          vaSurface->width, vaSurface->height, surface->gdi.mappedWidth,
	          surface->gdi.mappedHeight, width, height, rowBytes, srcPitch, cache->stagingPitch,
	          dstPitch, cache->image.offsets[0], (void*)surface->gdi.data,
	          (void*)cache->staging, (const void*)src, srcHash, stagingHash, dstHash);

	xf_vaapi_perf_stage("rgb32_getimage_to_gdi_total", surface->gdi.surfaceId,
	                     xf_vaapi_elapsed_us(perfStart));
	return TRUE;
}

#ifndef VA_INVALID_SURFACE
#define VA_INVALID_SURFACE ((VASurfaceID)VA_INVALID_ID)
#endif

static BOOL xf_vaapi_vpp_bgrx_target_supported(VADisplay dpy)
{
	static VADisplay cachedDpy = NULL;
	static BOOL cachedValid = FALSE;
	static BOOL cachedSupported = FALSE;
	static UINT32 cachedFourcc = 0;

	if (!dpy)
		return FALSE;

	if (cachedValid && (cachedDpy == dpy))
	{
		g_xf_vaapi_vpp_target_fourcc = cachedFourcc;
		return cachedSupported;
	}

	cachedDpy = dpy;
	cachedValid = TRUE;
	cachedSupported = FALSE;
	cachedFourcc = 0;
	g_xf_vaapi_vpp_target_fourcc = 0;

	VAConfigAttrib cfgAttrib = WINPR_C_ARRAY_INIT;
	cfgAttrib.type = VAConfigAttribRTFormat;
	VAStatus status = vaGetConfigAttributes(dpy, VAProfileNone, VAEntrypointVideoProc,
	                                        &cfgAttrib, 1);
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI VPP: vaGetConfigAttributes(VideoProc) failed status=%d (%s)",
		          status, vaErrorStr(status));
		return FALSE;
	}

	if ((cfgAttrib.value == VA_ATTRIB_NOT_SUPPORTED) ||
	    ((cfgAttrib.value & VA_RT_FORMAT_RGB32) == 0))
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: RGB32 render target unsupported by VideoProc cfg value=0x%08x",
		          cfgAttrib.value);
		return FALSE;
	}

	VAConfigAttrib createAttrib = WINPR_C_ARRAY_INIT;
	createAttrib.type = VAConfigAttribRTFormat;
	createAttrib.value = VA_RT_FORMAT_RGB32;

	VAConfigID config = VA_INVALID_ID;
	status = vaCreateConfig(dpy, VAProfileNone, VAEntrypointVideoProc,
	                        &createAttrib, 1, &config);
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI VPP: vaCreateConfig(VideoProc/RGB32) failed status=%d (%s)",
		          status, vaErrorStr(status));
		return FALSE;
	}

	unsigned int numAttrs = 0;
	status = vaQuerySurfaceAttributes(dpy, config, NULL, &numAttrs);
	if ((status != VA_STATUS_SUCCESS) || (numAttrs == 0))
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: vaQuerySurfaceAttributes count failed status=%d (%s) numAttrs=%u",
		          status, vaErrorStr(status), numAttrs);
		vaDestroyConfig(dpy, config);
		return FALSE;
	}

	VASurfaceAttrib* attrs = (VASurfaceAttrib*)calloc(numAttrs, sizeof(VASurfaceAttrib));
	if (!attrs)
	{
		vaDestroyConfig(dpy, config);
		return FALSE;
	}

	status = vaQuerySurfaceAttributes(dpy, config, attrs, &numAttrs);
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI VPP: vaQuerySurfaceAttributes list failed status=%d (%s)",
		          status, vaErrorStr(status));
		free(attrs);
		vaDestroyConfig(dpy, config);
		return FALSE;
	}

	const UINT32 bgrx = XF_VA_FOURCC('B', 'G', 'R', 'X');
	const UINT32 bgra = XF_VA_FOURCC('B', 'G', 'R', 'A');
	BOOL sawBgra = FALSE;

	for (unsigned int i = 0; i < numAttrs; i++)
	{
		if ((attrs[i].type == VASurfaceAttribPixelFormat) &&
		    (attrs[i].value.type == VAGenericValueTypeInteger))
		{
			const UINT32 fourcc = (UINT32)attrs[i].value.value.i;
			XF_VAAPI_DIAG_INFO(
			          "VAAPI VPP: target pixel format candidate=%c%c%c%c flags=0x%08x",
			          (char)(fourcc & 0xff), (char)((fourcc >> 8) & 0xff),
			          (char)((fourcc >> 16) & 0xff), (char)((fourcc >> 24) & 0xff),
			          attrs[i].flags);

			if ((attrs[i].flags & VA_SURFACE_ATTRIB_SETTABLE) == 0)
				continue;

			if (fourcc == bgrx)
			{
				cachedSupported = TRUE;
				cachedFourcc = bgrx;
				break;
			}

			/* BGRA has the same byte order for the color channels as BGRX on little-endian
			 * X11/GDI BGRX32 surfaces. Only the fourth byte differs (alpha instead of unused X),
			 * and the X11 visual is depth 24, so that byte is ignored by the visible output.
			 */
			if (fourcc == bgra)
				sawBgra = TRUE;
		}
	}

	if (!cachedSupported && sawBgra)
	{
		cachedSupported = TRUE;
		cachedFourcc = bgra;
	}

	free(attrs);
	vaDestroyConfig(dpy, config);

	g_xf_vaapi_vpp_target_fourcc = cachedFourcc;

	if (cachedSupported)
	{
		XF_VAAPI_DIAG_INFO( "VAAPI VPP: verified RGB32 render target support using %c%c%c%c",
		          (char)(cachedFourcc & 0xff), (char)((cachedFourcc >> 8) & 0xff),
		          (char)((cachedFourcc >> 16) & 0xff),
		          (char)((cachedFourcc >> 24) & 0xff));
	}
	else
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: neither BGRX nor BGRA render target is advertised as settable");
	}

	return cachedSupported;
}

typedef struct xf_vaapi_vpp_state
{
	VADisplay dpy;
	VAConfigID config;
	VAContextID context;
	VASurfaceID target;
	UINT32 targetWidth;
	UINT32 targetHeight;
	UINT32 bufferHeight;
	UINT32 fourcc;
	BOOL valid;
	BOOL userptrBacked;
	BYTE* userptr;
	size_t userptrSize;
	size_t userptrPitch;
	uintptr_t userptrHandle;
	VASurfaceAttribExternalBuffers externalBuffers;
	struct xf_vaapi_vpp_state* next;
} xfVaapiVppState;

#define XF_VAAPI_VPP_CACHE_MAX 8

static xfVaapiVppState* g_xf_vaapi_vpp_cache = NULL;

static void xf_vaapi_vpp_state_init(xfVaapiVppState* state)
{
	WINPR_ASSERT(state);

	state->dpy = NULL;
	state->config = VA_INVALID_ID;
	state->context = VA_INVALID_ID;
	state->target = VA_INVALID_SURFACE;
	state->targetWidth = 0;
	state->targetHeight = 0;
	state->bufferHeight = 0;
	state->fourcc = 0;
	state->valid = FALSE;
	state->userptrBacked = FALSE;
	state->userptr = NULL;
	state->userptrSize = 0;
	state->userptrPitch = 0;
	state->userptrHandle = 0;
	state->externalBuffers = (VASurfaceAttribExternalBuffers)WINPR_C_ARRAY_INIT;
	state->next = NULL;
}

static void xf_vaapi_vpp_state_destroy(xfVaapiVppState* state)
{
	xfVaapiVppState* next = NULL;

	if (!state)
		return;

	next = state->next;
	if (state->dpy)
	{
		if (state->context != VA_INVALID_ID)
			vaDestroyContext(state->dpy, state->context);
		if (state->target != VA_INVALID_SURFACE)
			vaDestroySurfaces(state->dpy, &state->target, 1);
		if (state->config != VA_INVALID_ID)
			vaDestroyConfig(state->dpy, state->config);
	}

	if (state->userptr && (state->userptrSize > 0))
		munmap(state->userptr, state->userptrSize);

	xf_vaapi_vpp_state_init(state);
	state->next = next;
}

static xfVaapiVppState* xf_vaapi_vpp_cache_get(VADisplay dpy, UINT32 targetWidth,
                                                UINT32 targetHeight, UINT32 bufferHeight,
                                                UINT32 fourcc)
{
	xfVaapiVppState** cur = &g_xf_vaapi_vpp_cache;

	while (*cur)
	{
		xfVaapiVppState* entry = *cur;
		if (entry->valid && (entry->dpy == dpy) &&
		    (entry->targetWidth == targetWidth) &&
		    (entry->targetHeight == targetHeight) &&
		    (entry->bufferHeight == bufferHeight) && (entry->fourcc == fourcc))
		{
			if (entry != g_xf_vaapi_vpp_cache)
			{
				*cur = entry->next;
				entry->next = g_xf_vaapi_vpp_cache;
				g_xf_vaapi_vpp_cache = entry;
			}
			return entry;
		}
		cur = &entry->next;
	}

	return NULL;
}

static void xf_vaapi_vpp_cache_insert(xfVaapiVppState* state)
{
	UINT32 count = 0;
	xfVaapiVppState** cur = NULL;

	WINPR_ASSERT(state);

	state->next = g_xf_vaapi_vpp_cache;
	g_xf_vaapi_vpp_cache = state;

	cur = &g_xf_vaapi_vpp_cache;
	while (*cur)
	{
		count++;
		if (count > XF_VAAPI_VPP_CACHE_MAX)
		{
			xfVaapiVppState* stale = *cur;
			*cur = stale->next;
			xf_vaapi_vpp_state_destroy(stale);
			free(stale);
			continue;
		}
		cur = &(*cur)->next;
	}
}

static void xf_vaapi_vpp_destroy(void)
{
	while (g_xf_vaapi_vpp_cache)
	{
		xfVaapiVppState* entry = g_xf_vaapi_vpp_cache;
		g_xf_vaapi_vpp_cache = entry->next;
		xf_vaapi_vpp_state_destroy(entry);
		free(entry);
	}

	g_xf_vaapi_vpp_target_fourcc = 0;
}

static BOOL xf_vaapi_ensure_display(xfContext* xfc, xfGfxSurface* surface)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);
	WINPR_ASSERT(xfc->display);

	if (g_xf_vaapi_display_initialized)
	{
		if (g_xf_vaapi_xdisplay != xfc->display)
		{
			WLog_WARN(TAG,
			          "VAAPI direct: refusing to reuse global VA display across different X displays");
			return FALSE;
		}

		surface->vaDisplay = g_xf_vaapi_display;
		return TRUE;
	}

	g_xf_vaapi_display = vaGetDisplay(xfc->display);
	if (!g_xf_vaapi_display)
	{
		WLog_WARN(TAG, "VAAPI direct: vaGetDisplay returned NULL");
		return FALSE;
	}

	const VAStatus vaStatus =
	    vaInitialize(g_xf_vaapi_display, &g_xf_vaapi_display_major, &g_xf_vaapi_display_minor);
	if (vaStatus != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI direct: vaInitialize failed status=%d", vaStatus);
		g_xf_vaapi_display = NULL;
		return FALSE;
	}

	g_xf_vaapi_xdisplay = xfc->display;
	g_xf_vaapi_display_initialized = TRUE;
	surface->vaDisplay = g_xf_vaapi_display;
	XF_VAAPI_DIAG_INFO("VAAPI direct: vaInitialize ok version=%d.%d display=%p",
	                   g_xf_vaapi_display_major, g_xf_vaapi_display_minor,
	                   (void*)g_xf_vaapi_display);
	return TRUE;
}

static void xf_vaapi_terminate_display(void)
{
	xf_vaapi_vpp_destroy();
	if (g_xf_vaapi_display_initialized && g_xf_vaapi_display)
		vaTerminate(g_xf_vaapi_display);

	g_xf_vaapi_display = NULL;
	g_xf_vaapi_xdisplay = NULL;
	g_xf_vaapi_display_initialized = FALSE;
	g_xf_vaapi_display_major = 0;
	g_xf_vaapi_display_minor = 0;
	g_xf_vaapi_vpp_userptr_unsupported = FALSE;
}

/* DATISCUM VAAPI safety stand 41: USER_PTR uses storage-sized VA surfaces and visible crop regions. */
static BOOL xf_vaapi_vpp_prepare(VADisplay dpy, UINT32 targetWidth, UINT32 targetHeight,
                                 UINT32 bufferHeight,
                                 xfVaapiVppState** ppState)
{
	xfVaapiVppState* vpp = NULL;
	UINT32 targetFourcc = 0;

	WINPR_ASSERT(ppState);
	*ppState = NULL;

	/*
	 * targetWidth/targetHeight describe the VA storage surface, not the visible
	 * RDP image. Cropping to the visible image is done later via
	 * VAProcPipelineParameterBuffer::output_region and by copying only the
	 * visible rectangle back to GDI.
	 */
	if (!dpy || (targetWidth == 0) || (targetHeight == 0) || (bufferHeight < targetHeight))
		return FALSE;

	/*
	 * USER_PTR is a performance optimization only. Some iHD/libva versions
	 * advertise RGB32 VideoProc targets but still reject RGB32 USER_PTR external
	 * buffers with VA_STATUS_ERROR_ALLOCATION_FAILED. Do not reject the whole
	 * custom VAAPI path for that case; fall back to a normal VA RGB32 target and
	 * read/map that target back into the GDI/XShm buffer after VPP.
	 */

	if (!xf_vaapi_vpp_bgrx_target_supported(dpy))
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: not enabling VAAPI output because RGB32 target was not verified");
		g_xf_vaapi_vpp_userptr_unsupported = TRUE;
		return FALSE;
	}

	targetFourcc = g_xf_vaapi_vpp_target_fourcc;
	if (targetFourcc == 0)
	{
		WLog_WARN(TAG, "VAAPI VPP: target RGB32 fourcc was not selected");
		g_xf_vaapi_vpp_userptr_unsupported = TRUE;
		return FALSE;
	}

	vpp = xf_vaapi_vpp_cache_get(dpy, targetWidth, targetHeight, bufferHeight, targetFourcc);
	if (vpp)
	{
		*ppState = vpp;
		return TRUE;
	}

	vpp = (xfVaapiVppState*)calloc(1, sizeof(xfVaapiVppState));
	if (!vpp)
		return FALSE;
	xf_vaapi_vpp_state_init(vpp);
	vpp->dpy = dpy;
	vpp->fourcc = targetFourcc;

	VAConfigAttrib cfgAttrib = WINPR_C_ARRAY_INIT;
	cfgAttrib.type = VAConfigAttribRTFormat;
	cfgAttrib.value = VA_RT_FORMAT_RGB32;

	VAStatus status = vaCreateConfig(dpy, VAProfileNone, VAEntrypointVideoProc,
	                                &cfgAttrib, 1, &vpp->config);
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI VPP: vaCreateConfig failed status=%d (%s)", status,
		          vaErrorStr(status));
		g_xf_vaapi_vpp_userptr_unsupported = TRUE;
		xf_vaapi_vpp_state_destroy(vpp);
		free(vpp);
		return FALSE;
	}

	const size_t rowBytes = (size_t)targetWidth * 4U;
	const size_t pitch = xf_vaapi_align_size(rowBytes, 128);
	if ((pitch == 0) || ((size_t)bufferHeight > (SIZE_MAX / pitch)))
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: invalid userptr target=%ux%u bufferHeight=%u pitch=%zu",
		          targetWidth, targetHeight, bufferHeight, pitch);
		xf_vaapi_vpp_state_destroy(vpp);
		free(vpp);
		return FALSE;
	}

	vpp->userptrPitch = pitch;
	vpp->userptrSize = pitch * (size_t)bufferHeight;
	if (vpp->userptrSize > UINT_MAX)
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: userptr size too large target=%ux%u bufferHeight=%u pitch=%zu size=%zu; trying native VA target",
		          targetWidth, targetHeight, bufferHeight, pitch, vpp->userptrSize);
		g_xf_vaapi_vpp_userptr_unsupported = TRUE;
	}

	if (!g_xf_vaapi_vpp_userptr_unsupported)
	{
		vpp->userptr = (BYTE*)mmap(NULL, vpp->userptrSize,
		                         PROT_READ | PROT_WRITE,
		                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
		if (vpp->userptr == MAP_FAILED)
		{
			vpp->userptr = NULL;
			WLog_WARN(TAG,
			          "VAAPI VPP: userptr mmap failed target=%ux%u bufferHeight=%u size=%zu; trying native VA target",
			          targetWidth, targetHeight, bufferHeight, vpp->userptrSize);
			g_xf_vaapi_vpp_userptr_unsupported = TRUE;
		}
	}

	if (vpp->userptr && !g_xf_vaapi_vpp_userptr_unsupported)
	{
		ZeroMemory(vpp->userptr, vpp->userptrSize);
		vpp->userptrHandle = (uintptr_t)vpp->userptr;
		vpp->externalBuffers = (VASurfaceAttribExternalBuffers)WINPR_C_ARRAY_INIT;
		vpp->externalBuffers.pixel_format = targetFourcc;
		vpp->externalBuffers.width = targetWidth;
		vpp->externalBuffers.height = targetHeight;
		vpp->externalBuffers.data_size = (unsigned int)vpp->userptrSize;
		vpp->externalBuffers.num_planes = 1;
		vpp->externalBuffers.pitches[0] = (unsigned int)vpp->userptrPitch;
		vpp->externalBuffers.offsets[0] = 0;
		vpp->externalBuffers.buffers = &vpp->userptrHandle;
		vpp->externalBuffers.num_buffers = 1;
		vpp->externalBuffers.flags = 0;

		VASurfaceAttrib attrs[3] = { 0 };
		attrs[0].type = VASurfaceAttribPixelFormat;
		attrs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
		attrs[0].value.type = VAGenericValueTypeInteger;
		attrs[0].value.value.i = (INT32)targetFourcc;
		attrs[1].type = VASurfaceAttribMemoryType;
		attrs[1].flags = VA_SURFACE_ATTRIB_SETTABLE;
		attrs[1].value.type = VAGenericValueTypeInteger;
		attrs[1].value.value.i = VA_SURFACE_ATTRIB_MEM_TYPE_USER_PTR;
		attrs[2].type = VASurfaceAttribExternalBufferDescriptor;
		attrs[2].flags = VA_SURFACE_ATTRIB_SETTABLE;
		attrs[2].value.type = VAGenericValueTypePointer;
		attrs[2].value.value.p = &vpp->externalBuffers;

		status = vaCreateSurfaces(dpy, VA_RT_FORMAT_RGB32, targetWidth, targetHeight,
		                          &vpp->target, 1, attrs, 3);
		if (status == VA_STATUS_SUCCESS)
		{
			vpp->userptrBacked = TRUE;
		}
		else
		{
			WLog_WARN(TAG,
			          "VAAPI VPP: userptr RGB32 target failed status=%d (%s) target=%ux%u bufferHeight=%u fourcc=%c%c%c%c pitch=%zu size=%zu; retrying with native VA target",
			          status, vaErrorStr(status), targetWidth, targetHeight, bufferHeight,
			          (char)(targetFourcc & 0xff),
			          (char)((targetFourcc >> 8) & 0xff),
			          (char)((targetFourcc >> 16) & 0xff),
			          (char)((targetFourcc >> 24) & 0xff), pitch, vpp->userptrSize);
			g_xf_vaapi_vpp_userptr_unsupported = TRUE;
			munmap(vpp->userptr, vpp->userptrSize);
			vpp->userptr = NULL;
			vpp->userptrSize = 0;
			vpp->userptrHandle = 0;
			vpp->externalBuffers = (VASurfaceAttribExternalBuffers)WINPR_C_ARRAY_INIT;
		}
	}

	if (vpp->target == VA_INVALID_SURFACE)
	{
		VASurfaceAttrib attrs[1] = { 0 };
		attrs[0].type = VASurfaceAttribPixelFormat;
		attrs[0].flags = VA_SURFACE_ATTRIB_SETTABLE;
		attrs[0].value.type = VAGenericValueTypeInteger;
		attrs[0].value.value.i = (INT32)targetFourcc;

		status = vaCreateSurfaces(dpy, VA_RT_FORMAT_RGB32, targetWidth, targetHeight,
		                          &vpp->target, 1, attrs, 1);
		if (status != VA_STATUS_SUCCESS)
		{
			WLog_WARN(TAG,
			          "VAAPI VPP: native RGB32 target failed status=%d (%s) target=%ux%u fourcc=%c%c%c%c; falling back before consuming AVC420",
			          status, vaErrorStr(status), targetWidth, targetHeight,
			          (char)(targetFourcc & 0xff),
			          (char)((targetFourcc >> 8) & 0xff),
			          (char)((targetFourcc >> 16) & 0xff),
			          (char)((targetFourcc >> 24) & 0xff));
			xf_vaapi_vpp_state_destroy(vpp);
			free(vpp);
			return FALSE;
		}
	}

	status = vaCreateContext(dpy, vpp->config, targetWidth, targetHeight, VA_PROGRESSIVE,
	                         &vpp->target, 1, &vpp->context);
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: vaCreateContext failed status=%d (%s) target=%ux%u bufferHeight=%u",
		          status, vaErrorStr(status), targetWidth, targetHeight, bufferHeight);
		g_xf_vaapi_vpp_userptr_unsupported = TRUE;
		xf_vaapi_vpp_state_destroy(vpp);
		free(vpp);
		return FALSE;
	}

	vpp->targetWidth = targetWidth;
	vpp->targetHeight = targetHeight;
	vpp->bufferHeight = bufferHeight;
	vpp->valid = TRUE;
	xf_vaapi_vpp_cache_insert(vpp);
	*ppState = vpp;

	WLog_INFO(TAG,
	          "VAAPI VPP v41: ready source NV12/VAAPI -> %s RGB32 target=%ux%u bufferHeight=%u config=%u context=%u targetSurface=%u ptr=%p pitch=%zu size=%zu",
	          vpp->userptrBacked ? "USER_PTR" : "native", targetWidth, targetHeight,
	          bufferHeight, vpp->config, vpp->context, vpp->target,
	          (void*)vpp->userptr, vpp->userptrPitch, vpp->userptrSize);
	return TRUE;
}

static BOOL xf_vaapi_process_vpp_rgb32(xfGfxSurface* surface,
                                      const H264_VAAPI_SURFACE* vaSurface,
                                      H264_VAAPI_SURFACE* vppTarget,
                                      xfVaapiVppState** ppVpp)
{
	WINPR_ASSERT(surface);
	WINPR_ASSERT(vaSurface);
	WINPR_ASSERT(vppTarget);
	WINPR_ASSERT(ppVpp);
	*ppVpp = NULL;

	VADisplay dpy = (VADisplay)vaSurface->display;
	VASurfaceID source = (VASurfaceID)vaSurface->surface;
	const UINT32 visibleWidth = surface->gdi.mappedWidth;
	const UINT32 visibleHeight = surface->gdi.mappedHeight;
	const UINT32 codedWidth = surface->gdi.width;
	const UINT32 codedHeight = surface->gdi.height;
	/*
	 * VAAPI/iHD may reject USER_PTR surfaces whose geometry is the cropped
	 * visible RDP size. Use the padded/coded GDI storage dimensions for
	 * vaCreateSurfaces/vaCreateContext/externalBuffers and crop explicitly in
	 * the VPP pipeline/output copy below.
	 */
	const UINT32 targetWidth = codedWidth;
	const UINT32 targetHeight = codedHeight;
	const UINT32 bufferHeight = codedHeight;
	const size_t rowBytes = (size_t)visibleWidth * 4U;

	if (!dpy || !surface->gdi.data || (visibleWidth == 0) || (visibleHeight == 0) ||
	    (codedWidth < visibleWidth) || (codedHeight < visibleHeight) ||
	    (targetWidth < visibleWidth) || (targetHeight < visibleHeight) ||
	    (bufferHeight < targetHeight) ||
	    (vaSurface->width < visibleWidth) || (vaSurface->height < visibleHeight) ||
	    (surface->gdi.scanline < rowBytes))
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: invalid geometry dpy=%p decoded=%ux%u visible=%ux%u coded=%ux%u target=%ux%u bufferHeight=%u dstPitch=%u rowBytes=%zu",
		          dpy, vaSurface->width, vaSurface->height, visibleWidth, visibleHeight,
		          codedWidth, codedHeight, targetWidth, targetHeight, bufferHeight,
		          surface->gdi.scanline, rowBytes);
		return FALSE;
	}

	xfVaapiVppState* vpp = NULL;
	UINT64 perfCall = xf_vaapi_now_us();
	if (!xf_vaapi_vpp_prepare(dpy, targetWidth, targetHeight, bufferHeight, &vpp))
		return FALSE;
	xf_vaapi_perf_stage("vpp_prepare", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));

	VARectangle srcRect = WINPR_C_ARRAY_INIT;
	srcRect.x = 0;
	srcRect.y = 0;
	srcRect.width = WINPR_ASSERTING_INT_CAST(unsigned short, visibleWidth);
	srcRect.height = WINPR_ASSERTING_INT_CAST(unsigned short, visibleHeight);

	VARectangle dstRect = WINPR_C_ARRAY_INIT;
	dstRect.x = 0;
	dstRect.y = 0;
	dstRect.width = WINPR_ASSERTING_INT_CAST(unsigned short, visibleWidth);
	dstRect.height = WINPR_ASSERTING_INT_CAST(unsigned short, visibleHeight);

	VAProcPipelineParameterBuffer params = WINPR_C_ARRAY_INIT;
	params.surface = source;
	params.surface_region = &srcRect;
	params.output_region = &dstRect;

	VABufferID paramBuf = VA_INVALID_ID;
	perfCall = xf_vaapi_now_us();
	VAStatus status = vaCreateBuffer(dpy, vpp->context,
	                                VAProcPipelineParameterBufferType,
	                                sizeof(params), 1, &params, &paramBuf);
	xf_vaapi_perf_stage("vpp_vaCreateBuffer", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI VPP: vaCreateBuffer pipeline failed status=%d (%s)",
		          status, vaErrorStr(status));
		return FALSE;
	}

	perfCall = xf_vaapi_now_us();
	status = vaBeginPicture(dpy, vpp->context, vpp->target);
	xf_vaapi_perf_stage("vpp_vaBeginPicture", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI VPP: vaBeginPicture failed status=%d (%s)", status,
		          vaErrorStr(status));
		vaDestroyBuffer(dpy, paramBuf);
		return FALSE;
	}

	perfCall = xf_vaapi_now_us();
	status = vaRenderPicture(dpy, vpp->context, &paramBuf, 1);
	xf_vaapi_perf_stage("vpp_vaRenderPicture", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI VPP: vaRenderPicture failed status=%d (%s)", status,
		          vaErrorStr(status));
		vaEndPicture(dpy, vpp->context);
		vaDestroyBuffer(dpy, paramBuf);
		return FALSE;
	}

	perfCall = xf_vaapi_now_us();
	status = vaEndPicture(dpy, vpp->context);
	xf_vaapi_perf_stage("vpp_vaEndPicture", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfCall));
	vaDestroyBuffer(dpy, paramBuf);
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI VPP: vaEndPicture failed status=%d (%s)", status,
		          vaErrorStr(status));
		return FALSE;
	}

	vppTarget->display = dpy;
	vppTarget->surface = vpp->target;
	vppTarget->width = visibleWidth;
	vppTarget->height = visibleHeight;
	*ppVpp = vpp;

	XF_VAAPI_DIAG_INFO(
	          "VAAPI VPP: processed source=%u decoded=%ux%u visible=%ux%u coded=%ux%u target=%ux%u bufferHeight=%u -> RGB32 targetSurface=%u copy=%ux%u dstPitch=%u userPitch=%zu",
	          source, vaSurface->width, vaSurface->height, visibleWidth, visibleHeight,
	          codedWidth, codedHeight, targetWidth, targetHeight, bufferHeight,
	          vpp->target, visibleWidth, visibleHeight, surface->gdi.scanline,
	          vpp->userptrPitch);
	return TRUE;
}

static BOOL xf_vaapi_copy_vpp_bgrx_to_gdi(xfGfxSurface* surface,
                                          const H264_VAAPI_SURFACE* vaSurface)
{
	const UINT64 perfStart = xf_vaapi_now_us();
	H264_VAAPI_SURFACE vppTarget = WINPR_C_ARRAY_INIT;
	xfVaapiVppState* vpp = NULL;
	if (!xf_vaapi_process_vpp_rgb32(surface, vaSurface, &vppTarget, &vpp))
		return FALSE;

	VADisplay dpy = (VADisplay)vppTarget.display;
	VASurfaceID target = (VASurfaceID)vppTarget.surface;
	const UINT32 width = vppTarget.width;
	const UINT32 height = vppTarget.height;
	const size_t rowBytes = (size_t)width * 4U;
	const size_t dstPitch = (size_t)surface->gdi.scanline;

	if (!dpy || (target == VA_INVALID_SURFACE) || !vpp ||
	    (width == 0) || (height == 0) ||
	    (width > surface->gdi.mappedWidth) || (height > surface->gdi.mappedHeight) ||
	    (width > vpp->targetWidth) || (height > vpp->targetHeight) ||
	    (height > vpp->bufferHeight) || (dstPitch < rowBytes))
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: invalid RGB32 copy target=%u copy=%ux%u mapped=%ux%u vaTarget=%ux%u bufferHeight=%u dstPitch=%zu rowBytes=%zu userptrBacked=%d",
		          target, width, height, surface->gdi.mappedWidth, surface->gdi.mappedHeight,
		          vpp ? vpp->targetWidth : 0, vpp ? vpp->targetHeight : 0,
		          vpp ? vpp->bufferHeight : 0, dstPitch, rowBytes,
		          (vpp && vpp->userptrBacked) ? 1 : 0);
		return FALSE;
	}

	if (!vpp->userptrBacked)
	{
		/* USER_PTR was rejected by the driver. The VPP result is still a valid
		 * native RGB32 VA surface, so use the existing RGB32 VAImage readback path.
		 */
		H264_VAAPI_SURFACE nativeTarget = WINPR_C_ARRAY_INIT;
		nativeTarget.display = dpy;
		nativeTarget.surface = target;
		nativeTarget.width = width;
		nativeTarget.height = height;
		xf_vaapi_perf_decision("copy_vpp_bgrx", surface->gdi.surfaceId,
		                       "native_rgb32_vaimage_readback");
		return xf_vaapi_getimage_bgrx_to_gdi(surface, &nativeTarget);
	}

	const BOOL userptrSizeOk = (vpp->userptrPitch > 0) &&
	    ((size_t)vpp->bufferHeight <= (SIZE_MAX / vpp->userptrPitch)) &&
	    (vpp->userptrSize >= (vpp->userptrPitch * (size_t)vpp->bufferHeight));

	if (!vpp->userptr || (vpp->userptrPitch < rowBytes) || !userptrSizeOk)
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: invalid userptr copy target=%u ptr=%p copy=%ux%u mapped=%ux%u vaTarget=%ux%u bufferHeight=%u dstPitch=%zu userPitch=%zu userSize=%zu rowBytes=%zu",
		          target, (void*)vpp->userptr, width, height,
		          surface->gdi.mappedWidth, surface->gdi.mappedHeight,
		          vpp->targetWidth, vpp->targetHeight, vpp->bufferHeight,
		          dstPitch, vpp->userptrPitch, vpp->userptrSize, rowBytes);
		return FALSE;
	}

	UINT64 perfCall = xf_vaapi_now_us();
	VAStatus status = vaSyncSurface(dpy, target);
	xf_vaapi_perf_stage("userptr_rgb32_vaSyncSurface", surface->gdi.surfaceId,
	                     xf_vaapi_elapsed_us(perfCall));
	if (status != VA_STATUS_SUCCESS)
	{
		WLog_WARN(TAG, "VAAPI VPP: userptr target sync failed status=%d (%s)",
		          status, vaErrorStr(status));
		return FALSE;
	}

	perfCall = xf_vaapi_now_us();
	xf_vaapi_copy_rows(surface->gdi.data, dstPitch, vpp->userptr,
	                   vpp->userptrPitch, rowBytes, height);
	xf_vaapi_perf_stage("userptr_rgb32_memcpy_to_gdi", surface->gdi.surfaceId,
	                     xf_vaapi_elapsed_us(perfCall));

	XF_VAAPI_DIAG_INFO(
	          "VAAPI VPP userptr copy done: copy=%ux%u gdiMapped=%ux%u vaTarget=%ux%u bufferHeight=%u rowBytes=%zu userPitch=%zu dstPitch=%zu userptr=%p dst=%p",
	          vppTarget.width, vppTarget.height, surface->gdi.mappedWidth,
	          surface->gdi.mappedHeight, vpp->targetWidth, vpp->targetHeight,
	          vpp->bufferHeight, rowBytes, vpp->userptrPitch, dstPitch,
	          (void*)vpp->userptr, (void*)surface->gdi.data);

	xf_vaapi_perf_stage("vpp_userptr_to_gdi_total", surface->gdi.surfaceId,
	                     xf_vaapi_elapsed_us(perfStart));
	return TRUE;
}

static BOOL xf_vaapi_copy_adaptive_to_gdi(xfGfxSurface* surface,
                                          const H264_VAAPI_SURFACE* vaSurface)
{
	WINPR_ASSERT(surface);
	WINPR_ASSERT(vaSurface);

	/* Direct decode-surface -> BGRX export intentionally remains in this file, but it is
	 * not selected automatically anymore. This build forces the VAAPI VPP path so that
	 * the decoded VAAPI surface is converted to a verified BGRX render target first. */
	const BOOL directBgrxSwitch = FALSE;
	const BOOL manualNv12FallbackSwitch = FALSE;
	if (directBgrxSwitch)
	{
		xf_vaapi_perf_decision("copy_adaptive", surface->gdi.surfaceId, "direct_bgrx_readback_16");
		return xf_vaapi_getimage_bgrx_to_gdi(surface, vaSurface);
	}
	if (manualNv12FallbackSwitch)
	{
		xf_vaapi_perf_decision("copy_adaptive", surface->gdi.surfaceId, "direct_nv12_readback_16");
		return xf_vaapi_getimage_nv12_to_gdi(surface, vaSurface);
	}

	xf_vaapi_perf_decision("copy_adaptive", surface->gdi.surfaceId, "vpp_rgb32_to_gdi_16");
	return xf_vaapi_copy_vpp_bgrx_to_gdi(surface, vaSurface);
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

static BOOL xf_vaapi_ensure_h264(xfContext* xfc, xfGfxSurface* surface)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);

	if (surface->vaapiDirectDisabled)
		return FALSE;

	if (surface->vaapiH264)
		return surface->vaapiDirectReady;

	if (!xf_vaapi_ensure_display(xfc, surface))
	{
		xf_vaapi_disable_direct(surface);
		return FALSE;
	}

	const UINT32 codedWidth = surface->gdi.width;
	const UINT32 codedHeight = surface->gdi.height;
	const UINT32 visibleWidth = surface->gdi.mappedWidth;
	const UINT32 visibleHeight = surface->gdi.mappedHeight;

	XF_VAAPI_DIAG_INFO("VAAPI direct: using VA display=%p surface=%u coded=%ux%u visible=%ux%u",
	                   surface->vaDisplay, surface->gdi.surfaceId, codedWidth, codedHeight,
	                   visibleWidth, visibleHeight);

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

	if (!h264_context_reset(surface->vaapiH264, codedWidth, codedHeight))
	{
		WLog_WARN(TAG, "VAAPI direct: ensure_h264 failed at h264_context_reset coded=%ux%u",
		          codedWidth, codedHeight);
		goto fail;
	}

	if (!h264_context_get_option(surface->vaapiH264, H264_CONTEXT_OPTION_HW_ACCEL))
	{
		WLog_WARN(TAG, "VAAPI direct: ensure_h264 failed at HW_ACCEL option");
		goto fail;
	}

	XF_VAAPI_DIAG_INFO( "VAAPI direct: H264 context ready surface=%u coded=%ux%u visible=%ux%u hwaccel=%d",
	          surface->gdi.surfaceId, codedWidth, codedHeight, visibleWidth, visibleHeight,
	          h264_context_get_option(surface->vaapiH264, H264_CONTEXT_OPTION_HW_ACCEL) ? 1 : 0);

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
	const UINT64 perfStart = xf_vaapi_now_us();
	UINT rc = CHANNEL_RC_OK;
	UINT32 nbRects = 0;
	const RECTANGLE_16* rects = nullptr;
	RECTANGLE_16 surfaceRect = WINPR_C_ARRAY_INIT;
	H264_VAAPI_SURFACE vaSurface = WINPR_C_ARRAY_INIT;
	BOOL copyDone = FALSE;
	BOOL dirtyDone = FALSE;

	WINPR_ASSERT(xfc);
	WINPR_ASSERT(surface);
	WINPR_ASSERT(handled);

	*handled = FALSE;

	if (!surface->vaapiFramePending)
		return CHANNEL_RC_OK;

	/* Keep vaapiFramePending set until the decoded VAAPI frame has really been
	 * copied into the GDI/XShm buffer and a GDI invalid region has been armed.
	 * Otherwise a transient VPP/userptr failure silently drops the frame. */

	if (!xf_vaapi_render_allowed(xfc, surface))
	{
		WLog_WARN(TAG,
		          "VAAPI direct: render no longer allowed; dropping pending frame");
		surface->vaapiFramePending = FALSE;
		region16_clear(&surface->vaapiInvalidRegion);
		goto out;
	}

	if (!h264_context_get_vaapi_surface(surface->vaapiH264, &vaSurface))
	{
		WLog_WARN(TAG, "VAAPI direct: no VAAPI surface available for userptr output");
		goto keep_pending;
	}

	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.mappedWidth);
	surfaceRect.bottom = WINPR_ASSERTING_INT_CAST(UINT16, surface->gdi.mappedHeight);

	if (!region16_intersect_rect(&surface->vaapiInvalidRegion, &surface->vaapiInvalidRegion,
	                             &surfaceRect))
	{
		WLog_WARN(TAG, "VAAPI direct: invalid region intersect failed");
		goto keep_pending;
	}

	rects = region16_rects(&surface->vaapiInvalidRegion, &nbRects);
	if (!rects || (nbRects == 0))
	{
		surface->vaapiFramePending = FALSE;
		region16_clear(&surface->vaapiInvalidRegion);
		goto out;
	}

	XF_VAAPI_DIAG_INFO(
	          "VAAPI OutputUpdate pending: mapped=%ux%u outputTarget=%ux%u "
	          "origin=%ux%u scanline=%u rects=%" PRIu32 " vaSurface=%ux%u surfaceId=%u "
	          "gdiFormat=0x%08" PRIx32 " image=%p data=%p stage=%p",
	          surface->gdi.mappedWidth, surface->gdi.mappedHeight,
	          surface->gdi.outputTargetWidth, surface->gdi.outputTargetHeight,
	          surface->gdi.outputOriginX, surface->gdi.outputOriginY, surface->gdi.scanline,
	          nbRects, vaSurface.width, vaSurface.height, vaSurface.surface, surface->gdi.format,
	          (void*)surface->image, surface->gdi.data, surface->stage);
	xf_vaapi_diag_log_ximage("VAAPI OutputUpdate XImage", surface->image);
	xf_vaapi_diag_log_rects("VAAPI OutputUpdate invalid", rects, nbRects);

	/* Stand 38: no vaPutSurface and no vaGetImage. The driver rejected direct
	 * X11 drawables with VA_STATUS_ERROR_INVALID_PARAMETER and vaGetImage costs
	 * ~600 ms per frame. Use VPP -> persistent USER_PTR RGB32 memory -> GDI/XShm. */

	xf_vaapi_perf_decision("output_update", surface->gdi.surfaceId, "vpp_userptr_then_xputimage_16");
	if (!xf_vaapi_copy_adaptive_to_gdi(surface, &vaSurface))
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: RGB32 VPP copy failed, keeping pending frame for retry");
		goto keep_pending;
	}
	copyDone = TRUE;

	/* The normal xf_OutputUpdate() path consumes surface->gdi.invalidRegion.
	 * We copied the full current decoded VAAPI frame into surface->gdi.data, so
	 * transfer the pending VAAPI invalid rects to the GDI invalid region. */
	region16_clear(&surface->gdi.invalidRegion);
	for (UINT32 x = 0; x < nbRects; x++)
	{
		if (!region16_union_rect(&surface->gdi.invalidRegion, &surface->gdi.invalidRegion,
		                         &rects[x]))
		{
			WLog_WARN(TAG, "VAAPI direct: failed to mark GDI invalid rect after userptr copy");
			rc = ERROR_INTERNAL_ERROR;
			goto keep_pending;
		}
	}
	dirtyDone = TRUE;

	{
		UINT32 gdiRects = 0;
		const RECTANGLE_16* gdiInvalid = region16_rects(&surface->gdi.invalidRegion, &gdiRects);
		XF_VAAPI_DIAG_INFO("VAAPI GDI invalid region armed: rects=%" PRIu32, gdiRects);
		xf_vaapi_diag_log_rects("VAAPI GDI invalid", gdiInvalid, gdiRects);
	}

	/* Do not set *handled=TRUE here. The existing X11/GDI output path must still
	 * run so that XPutImage draws the GDI buffer we just filled. */
	*handled = FALSE;
	surface->vaapiFramePending = FALSE;
	region16_clear(&surface->vaapiInvalidRegion);
	goto out;

keep_pending:
	if (!copyDone || !dirtyDone)
	{
		surface->vaapiFramePending = TRUE;
		*handled = TRUE;
	}

out:
	xf_vaapi_perf_stage("output_update_vaapi_total", surface->gdi.surfaceId,
	                    xf_vaapi_elapsed_us(perfStart));
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
	xfVaapiVppState* preflightVpp = NULL;
	UINT32 codedWidth = 0;
	UINT32 codedHeight = 0;
	UINT32 visibleWidth = 0;
	UINT32 visibleHeight = 0;


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
	codedWidth = surface->gdi.width;
	codedHeight = surface->gdi.height;
	visibleWidth = surface->gdi.mappedWidth;
	visibleHeight = surface->gdi.mappedHeight;

	if ((visibleWidth == 0) || (visibleHeight == 0) ||
	    (codedWidth < visibleWidth) || (codedHeight < visibleHeight))
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: invalid surface geometry coded=%ux%u visible=%ux%u",
		          codedWidth, codedHeight, visibleWidth, visibleHeight);
		status = XF_VAAPI_FALLBACK;
		goto fail;
	}

	const BOOL allowed = xf_vaapi_render_allowed(xfc, surface);
	if (!allowed)
	{
		WLog_WARN(TAG,
		          "VAAPI VPP: AVC420 update not suitable for custom path; using original path before consuming payload");
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

		if (!xf_vaapi_vpp_prepare((VADisplay)surface->vaDisplay, codedWidth, codedHeight,
		                          codedHeight, &preflightVpp))
		{
			WLog_WARN(TAG,
			          "VAAPI VPP: USER_PTR RGB32 target not available for target=%ux%u bufferHeight=%u visible=%ux%u; disabling custom path before consuming payload",
			          codedWidth, codedHeight, codedHeight, visibleWidth, visibleHeight);
			xf_vaapi_disable_direct(surface);
			status = XF_VAAPI_FALLBACK;
			goto fail;
		}
	}
	XF_VAAPI_DIAG_INFO(
	          "VAAPI AVC420 payload: size=%" PRIu32
	          " surface=%" PRIu32 " coded=%ux%u visible=%ux%u out=%ux%u rects=%" PRIu32
	          " annexb=%d first_nal=%u first=%02x %02x %02x %02x",
	          bs->length, cmd->surfaceId, codedWidth, codedHeight,
	          visibleWidth, visibleHeight,
	          surface->gdi.outputTargetWidth, surface->gdi.outputTargetHeight,
	          meta->numRegionRects, xf_vaapi_diag_looks_annexb(bs->data, bs->length) ? 1 : 0,
	          (unsigned)xf_vaapi_diag_first_nal_type(bs->data, bs->length),
	          (bs->data && (bs->length > 0)) ? bs->data[0] : 0,
	          (bs->data && (bs->length > 1)) ? bs->data[1] : 0,
	          (bs->data && (bs->length > 2)) ? bs->data[2] : 0,
	          (bs->data && (bs->length > 3)) ? bs->data[3] : 0);
	xf_vaapi_diag_log_h264_payload("VAAPI AVC420 H264", bs->data, bs->length);
	xf_vaapi_diag_log_rects("VAAPI AVC420 region", meta->regionRects, meta->numRegionRects);

	XF_VAAPI_DIAG_INFO( "VAAPI AVC420 decode call: surface=%u coded=%ux%u visible=%ux%u payload=%" PRIu32
	              " h264ctx=%p",
	          surface->gdi.surfaceId, codedWidth, codedHeight, visibleWidth, visibleHeight,
	          bs->length, (void*)surface->vaapiH264);

	UINT64 perfDecode = xf_vaapi_now_us();
	/*
	 * The H264/VAAPI decode context was reset to the coded/padded surface
	 * dimensions. Keep the decode call on the same geometry; the RDP region
	 * rectangles remain visible-image coordinates and are clipped later.
	 */
	const INT32 rc = avc420_decompress_to_vaapi(
	    surface->vaapiH264, bs->data, bs->length, codedWidth, codedHeight,
	    meta->regionRects, meta->numRegionRects);
	xf_vaapi_perf_stage("avc420_decode_vaapi_16", surface->gdi.surfaceId, xf_vaapi_elapsed_us(perfDecode));
	XF_VAAPI_DIAG_INFO( "VAAPI AVC420 decode result: rc=%" PRId32 " surface=%u", rc,
	          surface->gdi.surfaceId);
	if (rc < 0)
	{
		WLog_WARN(TAG, "VAAPI direct: AVC420 decode failed rc=%" PRId32
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
	invalidRect.right = WINPR_ASSERTING_INT_CAST(UINT16, visibleWidth);
	invalidRect.bottom = WINPR_ASSERTING_INT_CAST(UINT16, visibleHeight);

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

#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
	XF_VAAPI_DIAG_INFO(
	          "GDI OutputUpdate draw: mapped=%ux%u outputTarget=%ux%u origin=%ux%u "
	          "scale=%f/%f scanline=%u rects=%" PRIu32 " image=%p data=%p drawable=%lu "
	          "stage=%p stageScanline=%u gdiFormat=0x%08" PRIx32,
	          surface->gdi.mappedWidth, surface->gdi.mappedHeight,
	          surface->gdi.outputTargetWidth, surface->gdi.outputTargetHeight,
	          surfaceX, surfaceY, sx, sy, surface->gdi.scanline, nbRects,
	          (void*)surface->image, surface->gdi.data, (unsigned long)xfc->drawable,
	          surface->stage, surface->stageScanline, surface->gdi.format);
	xf_vaapi_diag_log_ximage("GDI OutputUpdate XImage", surface->image);
	xf_vaapi_diag_log_bgrx_samples("GDI OutputUpdate buffer", surface->gdi.data,
	                              surface->gdi.scanline, surface->gdi.mappedWidth,
	                              surface->gdi.mappedHeight);
	xf_vaapi_diag_log_rects("GDI OutputUpdate invalid", rects, nbRects);
#endif

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

#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
		if (x < 8)
		{
			XF_VAAPI_DIAG_INFO(
			          "GDI OutputUpdate blit[%" PRIu32 "]: src=%ux%u size=%ux%u dst=%ux%u "
			          "dsize=%ux%u sx=%f sy=%f stage=%d imageBpl=%d imageBpp=%d",
			          x, nXSrc, nYSrc, swidth, sheight, nXDst, nYDst, dwidth, dheight,
			          sx, sy, surface->stage ? 1 : 0, surface->image ? surface->image->bytes_per_line : -1,
			          surface->image ? surface->image->bits_per_pixel : -1);
		}
#endif

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

#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
	XF_VAAPI_DIAG_INFO(
	          "GFX CreateSurface diag: id=%u request=%ux%u padded=%ux%u mapped=%ux%u "
	          "pixelFormat=0x%08" PRIx32 " gdiFormat=0x%08" PRIx32
	          " dstFormat=0x%08" PRIx32 " scanline=%u stage=%p stageScanline=%u "
	          "xfcDepth=%d scanlinePad=%d visualMasks r=0x%lx g=0x%lx b=0x%lx",
	          surface->gdi.surfaceId, createSurface->width, createSurface->height,
	          surface->gdi.width, surface->gdi.height, surface->gdi.mappedWidth,
	          surface->gdi.mappedHeight, createSurface->pixelFormat, surface->gdi.format,
	          gdi->dstFormat, surface->gdi.scanline, surface->stage, surface->stageScanline,
	          xfc->depth, xfc->scanline_pad, xfc->visual ? xfc->visual->red_mask : 0,
	          xfc->visual ? xfc->visual->green_mask : 0,
	          xfc->visual ? xfc->visual->blue_mask : 0);
	xf_vaapi_diag_log_ximage("GFX CreateSurface XImage", surface->image);
#endif

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
		xf_vaapi_readback_cache_destroy(surface);
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
	else
	{
#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
		XF_VAAPI_DIAG_INFO( "VAAPI direct: not installing X11 VAAPI direct path because /gdi:sw is active");
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

#if defined(WITH_VAAPI) && defined(WITH_X11_VAAPI)
	xf_vaapi_readback_cache_destroy_all();
	xf_vaapi_terminate_display();
#endif
}
