/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Client
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __WFREERDP_H
#define __WFREERDP_H

#include <winpr/windows.h>

#include <freerdp/freerdp.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/region.h>
#include <freerdp/cache/cache.h>
#include <freerdp/codec/color.h>
#include <freerdp/utils/debug.h>
#include <freerdp/channels/channels.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/codec/nsc.h>

#include "wf_event.h"

struct wf_bitmap
{
	rdpBitmap _bitmap;
	HDC hdc;
	HBITMAP bitmap;
	HBITMAP org_bitmap;
	BYTE* pdata;
};
typedef struct wf_bitmap wfBitmap;

struct wf_pointer
{
	rdpPointer pointer;
	HCURSOR cursor;
};
typedef struct wf_pointer wfPointer;

typedef struct wf_info wfInfo;

struct wf_context
{
	rdpContext _p;

	wfInfo* wfi;
};
typedef struct wf_context wfContext;

struct wf_info
{
	int width;
	int height;
	int offset_x;
	int offset_y;
	int fs_toggle;
	int fullscreen;
	int percentscreen;
	char window_title[64];

	HWND hwnd;
	POINT diff;
	HGDI_DC hdc;
	UINT16 srcBpp;
	UINT16 dstBpp;
	freerdp* instance;
	wfBitmap* primary;
	wfBitmap* drawing;
	HCLRCONV clrconv;
	HCURSOR cursor;
	HBRUSH brush;
	HBRUSH org_brush;
	RECT update_rect;

	wfBitmap* tile;
	RFX_CONTEXT* rfx_context;
	NSC_CONTEXT* nsc_context;

	BOOL sw_gdi;
};

int wf_create_console(void);

void wf_context_new(freerdp* instance, rdpContext* context);
void wf_context_free(freerdp* instance, rdpContext* context);

void wf_sw_begin_paint(rdpContext* context);
void wf_sw_end_paint(rdpContext* context);
void wf_sw_desktop_resize(rdpContext* context);

void wf_hw_begin_paint(rdpContext* context);
void wf_hw_end_paint(rdpContext* context);
void wf_hw_desktop_resize(rdpContext* context);

BOOL wf_pre_connect(freerdp* instance);
BOOL wf_post_connect(freerdp* instance);

BOOL wf_authenticate(freerdp* instance, char** username, char** password, char** domain);
BOOL wf_verify_certificate(freerdp* instance, char* subject, char* issuer, char* fingerprint);

int wf_receive_channel_data(freerdp* instance, int channelId, BYTE* data, int size, int flags, int total_size);

DWORD WINAPI kbd_thread_func(LPVOID lpParam);
DWORD WINAPI thread_func(LPVOID lpParam);

#endif
