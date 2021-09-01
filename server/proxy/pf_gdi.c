/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
 * Copyright 2021 Armin Novak <anovak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#include <winpr/assert.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>
#include <freerdp/constants.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/bitmap.h>
#include <freerdp/freerdp.h>

#include <freerdp/server/proxy/proxy_log.h>

#include "pf_gdi.h"

#define TAG PROXY_TAG("gdi")

/* TODO: Figure how to use functions decleared in update.c */
static BOOL pf_gdi_set_bounds(rdpContext* context, const rdpBounds* bounds)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(bounds);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_dstblt(rdpContext* context, const DSTBLT_ORDER* dstblt)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(dstblt);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_patblt(rdpContext* context, PATBLT_ORDER* patblt)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(patblt);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_scrblt(rdpContext* context, const SCRBLT_ORDER* scrblt)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(scrblt);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_opaque_rect(rdpContext* context, const OPAQUE_RECT_ORDER* opaque_rect)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(opaque_rect);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_multi_opaque_rect(rdpContext* context,
                                     const MULTI_OPAQUE_RECT_ORDER* multi_opaque_rect)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(multi_opaque_rect);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_line_to(rdpContext* context, const LINE_TO_ORDER* line_to)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(line_to);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_polyline(rdpContext* context, const POLYLINE_ORDER* polyline)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(polyline);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_memblt(rdpContext* context, MEMBLT_ORDER* memblt)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(memblt);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_mem3blt(rdpContext* context, MEM3BLT_ORDER* mem3blt)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(mem3blt);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_polygon_sc(rdpContext* context, const POLYGON_SC_ORDER* polygon_sc)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(polygon_sc);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_polygon_cb(rdpContext* context, POLYGON_CB_ORDER* polygon_cb)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(polygon_cb);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_surface_frame_marker(rdpContext* context,
                                        const SURFACE_FRAME_MARKER* surface_frame_marker)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(surface_frame_marker);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

static BOOL pf_gdi_surface_bits(rdpContext* context, const SURFACE_BITS_COMMAND* cmd)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(cmd);

	WLog_INFO(TAG, __FUNCTION__);
	return TRUE;
}

void pf_gdi_register_update_callbacks(rdpUpdate* update)
{
	rdpPrimaryUpdate* primary;

	WINPR_ASSERT(update);
	primary = update->primary;
	WINPR_ASSERT(primary);

	update->SetBounds = pf_gdi_set_bounds;
	primary->DstBlt = pf_gdi_dstblt;
	primary->PatBlt = pf_gdi_patblt;
	primary->ScrBlt = pf_gdi_scrblt;
	primary->OpaqueRect = pf_gdi_opaque_rect;
	primary->MultiOpaqueRect = pf_gdi_multi_opaque_rect;
	primary->LineTo = pf_gdi_line_to;
	primary->Polyline = pf_gdi_polyline;
	primary->MemBlt = pf_gdi_memblt;
	primary->Mem3Blt = pf_gdi_mem3blt;
	primary->PolygonSC = pf_gdi_polygon_sc;
	primary->PolygonCB = pf_gdi_polygon_cb;
	update->SurfaceBits = pf_gdi_surface_bits;
	update->SurfaceFrameMarker = pf_gdi_surface_frame_marker;
}
