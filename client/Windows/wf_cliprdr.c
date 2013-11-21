/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Clipboard Redirection
 *
 * Copyright 2012 Jason Champion
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

#include <assert.h>
#include <winpr/crt.h>

#include <freerdp/utils/event.h>
#include <winpr/stream.h>
#include <freerdp/client/cliprdr.h>

#include "wf_cliprdr.h"

/* this macro will update _p pointer */
#define Read_UINT32(_p, _v) do { _v = \
	(UINT32)(*_p) + \
	(((UINT32)(*(_p + 1))) << 8) + \
	(((UINT32)(*(_p + 2))) << 16) + \
	(((UINT32)(*(_p + 3))) << 24); \
	_p += 4; } while (0)

/* this macro will NOT update _p pointer */
#define Write_UINT32(_p, _v) do { \
	*(_p) = (_v) & 0xFF; \
	*(_p + 1) = ((_v) >> 8) & 0xFF; \
	*(_p + 2) = ((_v) >> 16) & 0xFF; \
	*(_p + 3) = ((_v) >> 24) & 0xFF; } while (0)


static UINT32 get_local_format_id_by_name(cliprdrContext *cliprdr, void *format_name)
{
	formatMapping *map;
	int i;

	for (i = 0; i < cliprdr->map_size; i++)
	{
		map = &cliprdr->format_mappings[i];
		if ((cliprdr->capabilities & CAPS_USE_LONG_FORMAT_NAMES) != 0)
		{
			if (map->name)
			{
				if (memcmp(map->name, format_name, wcslen((LPCWSTR)format_name)) == 0)
					return map->local_format_id;
			}
		}
	}

	return 0;
}

static UINT32 get_remote_format_id(cliprdrContext *cliprdr, UINT32 local_format)
{
	formatMapping *map;
	int i;

	for (i = 0; i < cliprdr->map_size; i++)
	{
		map = &cliprdr->format_mappings[i];
		if (map->local_format_id == local_format)
		{
			return map->remote_format_id;
		}
	}

	return 0;
}

static void map_ensure_capacity(cliprdrContext *cliprdr)
{
	if (cliprdr->map_size >= cliprdr->map_capacity)
	{
		cliprdr->format_mappings = (formatMapping *)realloc(cliprdr->format_mappings,
			sizeof(formatMapping) * cliprdr->map_capacity * 2);
		cliprdr->map_capacity *= 2;
	}
}

static void clear_format_map(cliprdrContext *cliprdr)
{
	formatMapping *map;
	int i;

	if (cliprdr->format_mappings)
	{
		for (i = 0; i < cliprdr->map_capacity; i++)
		{
			map = &cliprdr->format_mappings[i];
			map->remote_format_id = 0;
			map->local_format_id = 0;

			if (map->name)
			{
				free(map->name);
				map->name = NULL;
			}
		}
	}

	cliprdr->map_size= 0;
}

static void cliprdr_send_format_list(cliprdrContext *cliprdr)
{
	RDP_CB_FORMAT_LIST_EVENT *cliprdr_event;
	BYTE *format_data;
	int format = 0;
	int data_size;
	int format_count;
	int len = 0;
	int namelen;

	if (!OpenClipboard(cliprdr->hwndClipboard))
	{
		DEBUG_CLIPRDR("OpenClipboard failed with 0x%x", GetLastError());
		return;
	}

	format_count = CountClipboardFormats();
	data_size = format_count * (4 + MAX_PATH * 2);

	format_data = (BYTE *)calloc(1, data_size);
	assert(format_data != NULL);

	while (format = EnumClipboardFormats(format))
	{
		Write_UINT32(format_data + len, format);
		len += 4;
		if ((cliprdr->capabilities & CAPS_USE_LONG_FORMAT_NAMES) != 0)
		{
			if (format >= CF_MAX)
			{
				namelen = GetClipboardFormatNameW(format, (LPWSTR)(format_data + len), MAX_PATH);
				len += namelen * sizeof(WCHAR);
			}
			len += 2;							/* end of Unicode string */
		}
		else
		{
			if (format >= CF_MAX)
			{
				static wchar_t wName[MAX_PATH] = {0};
				int wLen;

				ZeroMemory(wName, MAX_PATH*2);
				wLen = GetClipboardFormatNameW(format, wName, MAX_PATH);
				if (wLen < 16)
				{
					memcpy(format_data + len, wName, wLen * sizeof(WCHAR));
				}
				else
				{
					memcpy(format_data + len, wName, 32);	/* truncate the long name to 32 bytes */
				}
			}
			len += 32;
		}
	}

	CloseClipboard();

	cliprdr_event = (RDP_CB_FORMAT_LIST_EVENT *) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FormatList, NULL, NULL);

	cliprdr_event->raw_format_data = (BYTE *)calloc(1, len);
	assert(cliprdr_event->raw_format_data != NULL);

	CopyMemory(cliprdr_event->raw_format_data, format_data, len);
	cliprdr_event->raw_format_data_size = len;

	free(format_data);

	freerdp_channels_send_event(cliprdr->channels, (wMessage *) cliprdr_event);
}

int cliprdr_send_data_request(cliprdrContext *cliprdr, UINT32 format)
{
	RDP_CB_DATA_REQUEST_EVENT *cliprdr_event;
	int ret;

	cliprdr_event = (RDP_CB_DATA_REQUEST_EVENT *)freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_DataRequest, NULL, NULL);

	if (!cliprdr_event)
		return -1;

	cliprdr_event->format = get_remote_format_id(cliprdr, format);
	if (cliprdr_event->format == 0)
		return -1;

	ret = freerdp_channels_send_event(cliprdr->channels, (wMessage *)cliprdr_event);

	if (ret != 0)
		return -1;

	WaitForSingleObject(cliprdr->response_data_event, INFINITE);
	ResetEvent(cliprdr->response_data_event);

	return 0;
}

static LRESULT CALLBACK cliprdr_proc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	static cliprdrContext *cliprdr = NULL;

	switch (Msg)
	{
		case WM_CREATE:
			cliprdr = (cliprdrContext *)((CREATESTRUCT *)lParam)->lpCreateParams;
			cliprdr->hwndNextViewer = SetClipboardViewer(hWnd);

			if (cliprdr->hwndNextViewer == NULL && GetLastError() != 0)
			{
				DEBUG_CLIPRDR("error: SetClipboardViewer failed with 0x%0x.", GetLastError());
			}
			cliprdr->hwndClipboard = hWnd;
			break;

		case WM_CLOSE:
			ChangeClipboardChain(hWnd, cliprdr->hwndNextViewer);
			break;

		case WM_CHANGECBCHAIN:
			if (cliprdr->hwndNextViewer == (HWND)wParam)
			{
				cliprdr->hwndNextViewer = (HWND)lParam;
			}
			else if (cliprdr->hwndNextViewer != NULL)
			{
				SendMessage(cliprdr->hwndNextViewer, Msg, wParam, lParam);
			}
			break;

		case WM_DRAWCLIPBOARD:
			if (cliprdr->channel_initialized)
			{
				if (GetClipboardOwner() != cliprdr->hwndClipboard)
				{
						if (!cliprdr->hmem)
						{
							cliprdr->hmem = GlobalFree(cliprdr->hmem);
						}
						cliprdr_send_format_list(cliprdr);
				}
			}
			if (cliprdr->hwndNextViewer != NULL && cliprdr->hwndNextViewer != hWnd)
				SendMessage(cliprdr->hwndNextViewer, Msg, wParam, lParam);
			break;

		case WM_RENDERALLFORMATS:
			/* discard all contexts in clipboard */
			if (!OpenClipboard(cliprdr->hwndClipboard))
			{
				DEBUG_CLIPRDR("OpenClipboard failed with 0x%x", GetLastError());
				break;
			}
			EmptyClipboard();
			CloseClipboard();
			break;

		case WM_RENDERFORMAT:
			if (cliprdr_send_data_request(cliprdr, (UINT32)wParam) != 0)
			{
				DEBUG_CLIPRDR("error: cliprdr_send_data_request failed.");
				break;
			}

			if (SetClipboardData(wParam, cliprdr->hmem) == NULL)
			{
				DEBUG_CLIPRDR("SetClipboardData failed with 0x%x", GetLastError());
				cliprdr->hmem = GlobalFree(cliprdr->hmem);
			}
			/* Note: GlobalFree() is not needed when success */
			break;

		case WM_CLIPBOARDUPDATE:
		case WM_DESTROYCLIPBOARD:
		case WM_ASKCBFORMATNAME:
		case WM_HSCROLLCLIPBOARD:
		case WM_PAINTCLIPBOARD:
		case WM_SIZECLIPBOARD:
		case WM_VSCROLLCLIPBOARD:
		default:
			return DefWindowProc(hWnd, Msg, wParam, lParam);
	}

	return 0;
}
static int create_cliprdr_window(cliprdrContext *cliprdr)
{
	WNDCLASSEX wnd_cls;

	ZeroMemory(&wnd_cls, sizeof(WNDCLASSEX));
	wnd_cls.cbSize        = sizeof(WNDCLASSEX);
	wnd_cls.style         = CS_OWNDC;
	wnd_cls.lpfnWndProc   = cliprdr_proc;
	wnd_cls.cbClsExtra    = 0;
	wnd_cls.cbWndExtra    = 0;
	wnd_cls.hIcon         = NULL;
	wnd_cls.hCursor       = NULL;
	wnd_cls.hbrBackground = NULL;
	wnd_cls.lpszMenuName  = NULL;
	wnd_cls.lpszClassName = L"ClipboardHiddenMessageProcessor";
	wnd_cls.hInstance     = GetModuleHandle(NULL);
	wnd_cls.hIconSm       = NULL;
	RegisterClassEx(&wnd_cls);

	cliprdr->hwndClipboard = CreateWindowEx(WS_EX_LEFT,
		L"ClipboardHiddenMessageProcessor",
		L"rdpclip",
		0, 0, 0, 0, 0, HWND_MESSAGE, NULL, GetModuleHandle(NULL), cliprdr);

	if (cliprdr->hwndClipboard == NULL)
	{
		DEBUG_CLIPRDR("error: CreateWindowEx failed with %x.", GetLastError());
		return -1;
	}

	return 0;
}

static void *cliprdr_thread_func(void *arg)
{
	cliprdrContext *cliprdr = (cliprdrContext *)arg;
	BOOL mcode;
	MSG msg;
	int ret;
	HRESULT result;

	if ((ret = create_cliprdr_window(cliprdr)) != 0)
	{
		DEBUG_CLIPRDR("error: create clipboard window failed.");
		return NULL;
	}

	while ((mcode = GetMessage(&msg, 0, 0, 0) != 0))
	{
		if (mcode == -1)
		{
			DEBUG_CLIPRDR("error: clipboard thread GetMessage failed.");
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return NULL;
}

void wf_cliprdr_init(wfContext* wfc, rdpChannels* channels)
{
	cliprdrContext *cliprdr;

	wfc->cliprdr_context = (cliprdrContext *) calloc(1, sizeof(cliprdrContext));
	cliprdr = (cliprdrContext *) wfc->cliprdr_context;
	assert(cliprdr != NULL);

	cliprdr->channels = channels;
	cliprdr->channel_initialized = FALSE;

	cliprdr->map_capacity = 32;
	cliprdr->map_size = 0;

	cliprdr->format_mappings = (formatMapping *)calloc(1, sizeof(formatMapping) * cliprdr->map_capacity);
	assert(cliprdr->format_mappings != NULL);

	cliprdr->response_data_event = CreateEvent(NULL, TRUE, FALSE, L"response_data_event");
	assert(cliprdr->response_data_event != NULL);

	cliprdr->cliprdr_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)cliprdr_thread_func, cliprdr, 0, NULL);
	assert(cliprdr->cliprdr_thread != NULL);

	return;
}

void wf_cliprdr_uninit(wfContext* wfc)
{
	cliprdrContext *cliprdr = (cliprdrContext *) wfc->cliprdr_context;

	if (!cliprdr)
		return;

	if (cliprdr->hwndClipboard)
		PostMessage(cliprdr->hwndClipboard, WM_QUIT, 0, 0);

	if (cliprdr->cliprdr_thread)
	{
		WaitForSingleObject(cliprdr->cliprdr_thread, INFINITE);
		CloseHandle(cliprdr->cliprdr_thread);
	}

	if (cliprdr->response_data_event)
		CloseHandle(cliprdr->response_data_event);

	clear_format_map(cliprdr);

	if (cliprdr->format_mappings)
		free(cliprdr->format_mappings);

	free(cliprdr);
}

static void wf_cliprdr_process_cb_clip_caps_event(wfContext *wfc, RDP_CB_CLIP_CAPS *caps_event)
{
	cliprdrContext *cliprdr = (cliprdrContext *)wfc->cliprdr_context;

	cliprdr->capabilities = caps_event->capabilities;
}

static void wf_cliprdr_process_cb_monitor_ready_event(wfContext *wfc, RDP_CB_MONITOR_READY_EVENT *ready_event)
{
	cliprdrContext *cliprdr = (cliprdrContext *)wfc->cliprdr_context;

	cliprdr->channel_initialized = TRUE;

	cliprdr_send_format_list(wfc->cliprdr_context);
}

static void wf_cliprdr_process_cb_data_request_event(wfContext *wfc, RDP_CB_DATA_REQUEST_EVENT *event)
{
	cliprdrContext *cliprdr = (cliprdrContext *)wfc->cliprdr_context;
	RDP_CB_DATA_RESPONSE_EVENT *responce_event;
	HANDLE hClipdata;
	int size = 0;
	char *buff = NULL;
	char *globlemem = NULL;
	UINT32 local_format;

	local_format = event->format;

	if (local_format == 0x9)			/* FORMAT_ID_PALETTE */
	{
		/* TODO: implement this */
		DEBUG_CLIPRDR("FORMAT_ID_PALETTE is not supported yet.");
	}
	else if (local_format == 0x3)		/* FORMAT_ID_MATEFILE */
	{
		/* TODO: implement this */
		DEBUG_CLIPRDR("FORMAT_ID_MATEFILE is not supported yet.");
	}
	else if (local_format == RegisterClipboardFormatW(L"FileGroupDescriptorW"))
	{
		/* TODO: implement this */
		DEBUG_CLIPRDR("FileGroupDescriptorW is not supported yet.");
	}
	else
	{
		if (!OpenClipboard(cliprdr->hwndClipboard))
		{
			DEBUG_CLIPRDR("OpenClipboard failed with 0x%x", GetLastError());
			return;
		}

		hClipdata = GetClipboardData(event->format);
		if (!hClipdata)
		{
			DEBUG_CLIPRDR("GetClipboardData failed.");
			CloseClipboard();
			return;
		}

		globlemem = (char *)GlobalLock(hClipdata);
		size = GlobalSize(hClipdata);

		buff = (char *)malloc(size);
		memcpy(buff, globlemem, size);

		GlobalUnlock(hClipdata);

		CloseClipboard();
	}

	responce_event = (RDP_CB_DATA_RESPONSE_EVENT *)freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_DataResponse, NULL, NULL);

	responce_event->data = (BYTE *)buff;
	responce_event->size = size;

	freerdp_channels_send_event(cliprdr->channels, (wMessage *) responce_event);

	/* Note: don't free buff here. */
}

static void wf_cliprdr_process_cb_format_list_event(wfContext *wfc, RDP_CB_FORMAT_LIST_EVENT *event)
{
	cliprdrContext *cliprdr = (cliprdrContext *)wfc->cliprdr_context;
	UINT32 left_size = event->raw_format_data_size;
	int i = 0;
	BYTE *p;
	BYTE* end_mark;
	BOOL format_forbidden = FALSE;

	/* ignore the formats member in event struct, only parsing raw_format_data */

	p = event->raw_format_data;
	end_mark = p + event->raw_format_data_size;

	clear_format_map(cliprdr);

	if ((cliprdr->capabilities & CAPS_USE_LONG_FORMAT_NAMES) != 0)
	{
		while (left_size >= 6)
		{
			formatMapping *map;
			BYTE* tmp;
			int name_len;

			map = &cliprdr->format_mappings[i++];

			Read_UINT32(p, map->remote_format_id);
			map->name = NULL;

			/* get name_len */
			for (tmp = p, name_len = 0; tmp + 1 < end_mark; tmp += 2, name_len += 2)
			{
				if (*((unsigned short*) tmp) == 0)
					break;
			}

			if (name_len > 0)
			{
				map->name = malloc(name_len + 2);
				memcpy(map->name, p, name_len + 2);

				map->local_format_id = RegisterClipboardFormatW((LPCWSTR)map->name);
			}
			else
			{
				map->local_format_id = map->remote_format_id;
			}

			left_size -= name_len + 4 + 2;
			p += name_len + 2;          /* p's already +4 when Read_UINT32() is called. */

			cliprdr->map_size++;

			map_ensure_capacity(cliprdr);
		}
	}
	else
	{
		int k;

		for (k = 0; k < event->raw_format_data_size / 36; k++)
		{
			formatMapping *map;
			int name_len;

			map = &cliprdr->format_mappings[i++];

			Read_UINT32(p, map->remote_format_id);
			map->name = NULL;

			if (event->raw_format_unicode)
			{
				/* get name_len, in bytes, if the file name is truncated, no terminated null will be included. */
				for (name_len = 0; name_len < 32; name_len += 2)
				{
					if (*((unsigned short*) (p + name_len)) == 0)
						break;
				}

				if (name_len > 0)
				{
					map->name = calloc(1, name_len + 2);
					memcpy(map->name, p, name_len);
					map->local_format_id = RegisterClipboardFormatW((LPCWSTR)map->name);
				}
				else
				{
					map->local_format_id = map->remote_format_id;
				}
			}
			else
			{
				/* get name_len, in bytes, if the file name is truncated, no terminated null will be included. */
				for (name_len = 0; name_len < 32; name_len += 1)
				{
					if (*((unsigned char*) (p + name_len)) == 0)
						break;
				}

				if (name_len > 0)
				{
					map->name = calloc(1, name_len + 1);
					memcpy(map->name, p, name_len);
					map->local_format_id = RegisterClipboardFormatA((LPCSTR)map->name);
				}
				else
				{
					map->local_format_id = map->remote_format_id;
				}
			}

			p += 32;          /* p's already +4 when Read_UINT32() is called. */
			cliprdr->map_size++;
			map_ensure_capacity(cliprdr);
		}
	}

	if (!OpenClipboard(cliprdr->hwndClipboard))
	{
		DEBUG_CLIPRDR("OpenClipboard failed with 0x%x", GetLastError());
		return;
	}

	if (!EmptyClipboard())
	{
		DEBUG_CLIPRDR("EmptyClipboard failed with 0x%x", GetLastError());
		CloseClipboard();
		return;
	}

	for (i = 0; i < cliprdr->map_size; i++)
	{
		SetClipboardData(cliprdr->format_mappings[i].local_format_id, NULL);
	}

	CloseClipboard();
}

static void wf_cliprdr_process_cb_data_response_event(wfContext *wfc, RDP_CB_DATA_RESPONSE_EVENT *event)
{
	cliprdrContext *cliprdr = (cliprdrContext *)wfc->cliprdr_context;
	HANDLE hMem;
	char *buff;

	hMem = GlobalAlloc(GMEM_FIXED, event->size);
	buff = (char *) GlobalLock(hMem);
	memcpy(buff, event->data, event->size);
	GlobalUnlock(hMem);

	cliprdr->hmem = hMem;
	SetEvent(cliprdr->response_data_event);
}

void wf_process_cliprdr_event(wfContext *wfc, wMessage *event)
{
	switch (GetMessageType(event->id))
	{
		case CliprdrChannel_ClipCaps:
			wf_cliprdr_process_cb_clip_caps_event(wfc, (RDP_CB_CLIP_CAPS *)event);
			break;

		case CliprdrChannel_MonitorReady:
			wf_cliprdr_process_cb_monitor_ready_event(wfc, (RDP_CB_MONITOR_READY_EVENT *)event);
			break;

		case CliprdrChannel_FormatList:
			wf_cliprdr_process_cb_format_list_event(wfc, (RDP_CB_FORMAT_LIST_EVENT *) event);
			break;

		case CliprdrChannel_DataRequest:
			wf_cliprdr_process_cb_data_request_event(wfc, (RDP_CB_DATA_REQUEST_EVENT *) event);
			break;

		case CliprdrChannel_DataResponse:
			wf_cliprdr_process_cb_data_response_event(wfc, (RDP_CB_DATA_RESPONSE_EVENT *) event);
			break;

		default:
			break;
	}
}

BOOL wf_cliprdr_process_selection_notify(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

BOOL wf_cliprdr_process_selection_request(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

BOOL wf_cliprdr_process_selection_clear(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

BOOL wf_cliprdr_process_property_notify(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return TRUE;
}

void wf_cliprdr_check_owner(wfContext* wfc)
{

}
