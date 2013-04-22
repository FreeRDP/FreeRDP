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

#ifndef __WF_INTERFACE_H
#define __WF_INTERFACE_H

#include <winpr/windows.h>

#include <freerdp/api.h>
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
#include <freerdp/client/file.h>

#include "wf_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// Callback type codes. Move elsewhere?
#define CALLBACK_TYPE_PARAM_CHANGE		0x01
#define CALLBACK_TYPE_CONNECTED			0x02
#define CALLBACK_TYPE_DISCONNECTED		0x03

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

typedef void (CALLBACK * callbackFunc)(wfInfo* wfi, int callback_type, DWORD param1, DWORD param2);

struct wf_info
{
	rdpClient* client;

	int width;
	int height;
	int offset_x;
	int offset_y;
	int fs_toggle;
	int fullscreen;
	int percentscreen;
	char window_title[64];
	int client_width;
	int client_height;

	HANDLE thread;
	HANDLE keyboardThread;

	HICON icon;
	HWND hWndParent;
	HINSTANCE hInstance;
	WNDCLASSEX wndClass;
	LPCTSTR wndClassName;
	HCURSOR hDefaultCursor;

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
	RECT scale_update_rect;

	wfBitmap* tile;
	DWORD mainThreadId;
	DWORD keyboardThreadId;
	RFX_CONTEXT* rfx_context;
	NSC_CONTEXT* nsc_context;

	BOOL sw_gdi;
	callbackFunc callback_func;

	rdpFile* connectionRdpFile;
};

/**
 * Client Interface
 */

#define cfInfo	wfInfo

FREERDP_API int freerdp_client_global_init();
FREERDP_API int freerdp_client_global_uninit();

FREERDP_API int freerdp_client_start(wfInfo* cfi);
FREERDP_API int freerdp_client_stop(wfInfo* cfi);

FREERDP_API int freerdp_client_focus_in(wfInfo* cfi);
FREERDP_API int freerdp_client_focus_out(wfInfo* cfi);

FREERDP_API int freerdp_client_set_window_size(wfInfo* cfi, int width, int height);

FREERDP_API cfInfo* freerdp_client_new(int argc, char** argv);
FREERDP_API int freerdp_client_free(wfInfo* cfi);

FREERDP_API BOOL freerdp_client_get_param_bool(wfInfo* cfi, int id);
FREERDP_API int freerdp_client_set_param_bool(wfInfo* cfi, int id, BOOL param);

FREERDP_API UINT32 freerdp_client_get_param_uint32(wfInfo* cfi, int id);
FREERDP_API int freerdp_client_set_param_uint32(wfInfo* cfi, int id, UINT32 param);

FREERDP_API UINT64 freerdp_client_get_param_uint64(wfInfo* cfi, int id);
FREERDP_API int freerdp_client_set_param_uint64(wfInfo* cfi, int id, UINT64 param);

FREERDP_API char* freerdp_client_get_param_string(wfInfo* cfi, int id);
FREERDP_API int freerdp_client_set_param_string(wfInfo* cfi, int id, char* param);

FREERDP_API int freerdp_client_set_callback_function(wfInfo* cfi, callbackFunc callbackFunc);

FREERDP_API int freerdp_client_load_settings_from_rdp_file(wfInfo* cfi, char* filename);
FREERDP_API int freerdp_client_save_settings_to_rdp_file(wfInfo* cfi, char* filename);

#ifdef __cplusplus
}
#endif

#endif
