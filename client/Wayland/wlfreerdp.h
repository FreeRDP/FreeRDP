/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Wayland Client
 *
 * Copyright 2014 Manuel Bachmann <tarnyko@tarnyko.net>
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

#ifndef FREERDP_CLIENT_WAYLAND_FREERDP_H
#define FREERDP_CLIENT_WAYLAND_FREERDP_H

#include <freerdp/client/rdpei.h>
#include <freerdp/gdi/gfx.h>
#include <freerdp/freerdp.h>
#include <freerdp/log.h>
#include <winpr/wtypes.h>
#include <uwac/uwac.h>

typedef struct wlf_clipboard wfClipboard;
typedef struct s_wlfDispContext wlfDispContext;

typedef struct
{
	rdpClientContext common;

	UwacDisplay* display;
	HANDLE displayHandle;
	UwacWindow* window;
	UwacSeat* seat;

	BOOL fullscreen;
	BOOL closed;
	BOOL focusing;

	/* Channels */
	wfClipboard* clipboard;
	wlfDispContext* disp;
	wLog* log;
	CRITICAL_SECTION critical;
	wArrayList* events;
} wlfContext;

BOOL wlf_scale_coordinates(rdpContext* context, UINT32* px, UINT32* py, BOOL fromLocalToRDP);
BOOL wlf_copy_image(const void* src, size_t srcStride, size_t srcWidth, size_t srcHeight, void* dst,
                    size_t dstStride, size_t dstWidth, size_t dstHeight, const RECTANGLE_16* area,
                    BOOL scale);

#endif /* FREERDP_CLIENT_WAYLAND_FREERDP_H */
