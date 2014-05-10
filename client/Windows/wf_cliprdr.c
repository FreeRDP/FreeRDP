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

#include <Strsafe.h>

#include "wf_cliprdr.h"

#define WM_CLIPRDR_MESSAGE  (WM_USER + 156)
#define OLE_SETCLIPBOARD    1

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

BOOL wf_create_file_obj(cliprdrContext *cliprdr, IDataObject **ppDataObject);
void wf_destroy_file_obj(IDataObject *instance);

static UINT32 get_local_format_id_by_name(cliprdrContext *cliprdr, void *format_name)
{
	formatMapping *map;
	int i;

	for (i = 0; i < cliprdr->map_size; i++)
	{
		map = &cliprdr->format_mappings[i];
		if ((cliprdr->capabilities & CB_USE_LONG_FORMAT_NAMES) != 0)
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

static INLINE BOOL file_transferring(cliprdrContext *cliprdr)
{
	return !!get_local_format_id_by_name(cliprdr, L"FileGroupDescriptorW");
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

	return local_format;
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

int cliprdr_send_tempdir(cliprdrContext *cliprdr)
{
	RDP_CB_TEMPDIR_EVENT *cliprdr_event;

	cliprdr_event = (RDP_CB_TEMPDIR_EVENT *)freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_TemporaryDirectory, NULL, NULL);

	if (!cliprdr_event)
		return -1;

	GetEnvironmentVariableW(L"TEMP", (LPWSTR)cliprdr_event->dirname, 260);

	return freerdp_channels_send_event(cliprdr->channels, (wMessage *)cliprdr_event);
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
	int stream_file_transferring = FALSE;

	if (!OpenClipboard(cliprdr->hwndClipboard))
	{
		DEBUG_CLIPRDR("OpenClipboard failed with 0x%x", GetLastError());
		return;
	}

	format_count = CountClipboardFormats();
	data_size = format_count * (4 + MAX_PATH * 2);

	format_data = (BYTE*) calloc(1, data_size);
	assert(format_data != NULL);

	while (format = EnumClipboardFormats(format))
	{
		Write_UINT32(format_data + len, format);
		len += 4;
		if ((cliprdr->capabilities & CB_USE_LONG_FORMAT_NAMES) != 0)
		{
			if (format >= CF_MAX)
			{
				namelen = GetClipboardFormatNameW(format, (LPWSTR)(format_data + len), MAX_PATH);

				if ((wcscmp((LPWSTR)(format_data + len), L"FileNameW") == 0) ||
					(wcscmp((LPWSTR)(format_data + len), L"FileName") == 0) || (wcscmp((LPWSTR)(format_data + len), CFSTR_FILEDESCRIPTORW) == 0)) {
					stream_file_transferring = TRUE;
				}

				len += namelen * sizeof(WCHAR);
			}
			len += 2;							/* end of Unicode string */
		}
		else
		{
			if (format >= CF_MAX)
			{
				static WCHAR wName[MAX_PATH] = {0};
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

	if (stream_file_transferring)
	{
		cliprdr_event->raw_format_data_size = 4 + 42;
		cliprdr_event->raw_format_data = (BYTE*) calloc(1, cliprdr_event->raw_format_data_size);
		format = RegisterClipboardFormatW(L"FileGroupDescriptorW");
		Write_UINT32(cliprdr_event->raw_format_data, format);
		wcscpy_s((WCHAR*)(cliprdr_event->raw_format_data + 4),
			(cliprdr_event->raw_format_data_size - 4) / 2, L"FileGroupDescriptorW");
	}
	else
	{
		cliprdr_event->raw_format_data = (BYTE*) calloc(1, len);
		assert(cliprdr_event->raw_format_data != NULL);

		CopyMemory(cliprdr_event->raw_format_data, format_data, len);
		cliprdr_event->raw_format_data_size = len;
	}
	free(format_data);

	freerdp_channels_send_event(cliprdr->channels, (wMessage*) cliprdr_event);
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

	ret = freerdp_channels_send_event(cliprdr->channels, (wMessage *)cliprdr_event);

	if (ret != 0)
		return -1;

	WaitForSingleObject(cliprdr->response_data_event, INFINITE);
	ResetEvent(cliprdr->response_data_event);

	return 0;
}

int cliprdr_send_lock(cliprdrContext *cliprdr)
{
	RDP_CB_LOCK_CLIPDATA_EVENT *cliprdr_event;
	int ret;

	if ((cliprdr->capabilities & CB_CAN_LOCK_CLIPDATA) == 1) {
		cliprdr_event = (RDP_CB_LOCK_CLIPDATA_EVENT *)freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_LockClipdata, NULL, NULL);

		if (!cliprdr_event)
			return -1;

		cliprdr_event->clipDataId = 0;

		ret = freerdp_channels_send_event(cliprdr->channels, (wMessage *)cliprdr_event);

		if (ret != 0)
			return -1;
	}

	return 0;
}

int cliprdr_send_unlock(cliprdrContext *cliprdr)
{
	RDP_CB_UNLOCK_CLIPDATA_EVENT *cliprdr_event;
	int ret;

	if ((cliprdr->capabilities & CB_CAN_LOCK_CLIPDATA) == 1) {
		cliprdr_event = (RDP_CB_UNLOCK_CLIPDATA_EVENT *)freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_UnLockClipdata, NULL, NULL);

		if (!cliprdr_event)
			return -1;

		cliprdr_event->clipDataId = 0;

		ret = freerdp_channels_send_event(cliprdr->channels, (wMessage *)cliprdr_event);

		if (ret != 0)
			return -1;
	}

	return 0;
}

int cliprdr_send_request_filecontents(cliprdrContext *cliprdr, void *streamid,
		int index, int flag, DWORD positionhigh, DWORD positionlow, ULONG nreq)
{
	RDP_CB_FILECONTENTS_REQUEST_EVENT *cliprdr_event;
	int ret;

	cliprdr_event = (RDP_CB_FILECONTENTS_REQUEST_EVENT *)freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FilecontentsRequest, NULL, NULL);

	if (!cliprdr_event)
		return -1;

	cliprdr_event->streamId      = (UINT32)streamid;
	cliprdr_event->lindex        = index;
	cliprdr_event->dwFlags       = flag;
	cliprdr_event->nPositionLow  = positionlow;
	cliprdr_event->nPositionHigh = positionhigh;
	cliprdr_event->cbRequested   = nreq;
	cliprdr_event->clipDataId    = 0;

	ret = freerdp_channels_send_event(cliprdr->channels, (wMessage *)cliprdr_event);

	if (ret != 0)
		return -1;

	WaitForSingleObject(cliprdr->req_fevent, INFINITE);
	ResetEvent(cliprdr->req_fevent);

	return 0;
}

int cliprdr_send_response_filecontents(cliprdrContext *cliprdr, UINT32 streamid, UINT32 size, BYTE *data)
{
	RDP_CB_FILECONTENTS_RESPONSE_EVENT *cliprdr_event;
	int ret;

	cliprdr_event = (RDP_CB_FILECONTENTS_RESPONSE_EVENT *)freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_FilecontentsResponse, NULL, NULL);

	if (!cliprdr_event)
		return -1;

	cliprdr_event->streamId = streamid;
	cliprdr_event->size = size;
	cliprdr_event->data = data;

	ret = freerdp_channels_send_event(cliprdr->channels, (wMessage *)cliprdr_event);

	if (ret != 0)
		return -1;

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
				if ((GetClipboardOwner() != cliprdr->hwndClipboard) && (S_FALSE == OleIsCurrentClipboard(cliprdr->data_obj)))
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

			if (SetClipboardData((UINT) wParam, cliprdr->hmem) == NULL)
			{
				DEBUG_CLIPRDR("SetClipboardData failed with 0x%x", GetLastError());
				cliprdr->hmem = GlobalFree(cliprdr->hmem);
			}
			/* Note: GlobalFree() is not needed when success */
			break;

		case WM_CLIPRDR_MESSAGE:
			switch (wParam)
			{
				case OLE_SETCLIPBOARD:
					if (wf_create_file_obj(cliprdr, &cliprdr->data_obj))
						if (OleSetClipboard(cliprdr->data_obj) != S_OK)
							wf_destroy_file_obj(cliprdr->data_obj);
					break;

				default:
					break;
			}
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
	int ret;
	MSG msg;
	BOOL mcode;
	cliprdrContext* cliprdr = (cliprdrContext*) arg;

	OleInitialize(0);

	if ((ret = create_cliprdr_window(cliprdr)) != 0)
	{
		DEBUG_CLIPRDR("error: create clipboard window failed.");
		return NULL;
	}

	while ((mcode = GetMessage(&msg, 0, 0, 0)) != 0)
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

	OleUninitialize();

	return NULL;
}

static void clear_file_array(cliprdrContext *cliprdr)
{
	int i;

	/* clear file_names array */
	if (cliprdr->file_names)
	{
		for (i = 0; i < cliprdr->nFiles; i++)
		{
			if (cliprdr->file_names[i])
			{
				free(cliprdr->file_names[i]);
				cliprdr->file_names[i] = NULL;
			}
		}
	}

	/* clear fileDescriptor array */
	if (cliprdr->fileDescriptor)
	{
		for (i = 0; i < cliprdr->nFiles; i++)
		{
			if (cliprdr->fileDescriptor[i])
			{
				free(cliprdr->fileDescriptor[i]);
				cliprdr->fileDescriptor[i] = NULL;
			}
		}
	}

	cliprdr->nFiles = 0;
}

void wf_cliprdr_init(wfContext* wfc, rdpChannels* channels)
{
	cliprdrContext *cliprdr;

	if (!wfc->instance->settings->RedirectClipboard)
	{
		wfc->cliprdr_context = NULL;
		fprintf(stderr, "clipboard is not redirected.\n");
		return;
	}

	wfc->cliprdr_context = (cliprdrContext *) calloc(1, sizeof(cliprdrContext));
	cliprdr = (cliprdrContext *) wfc->cliprdr_context;
	assert(cliprdr != NULL);

	cliprdr->channels = channels;
	cliprdr->channel_initialized = FALSE;

	cliprdr->map_capacity = 32;
	cliprdr->map_size = 0;

	cliprdr->format_mappings = (formatMapping *)calloc(1, sizeof(formatMapping) * cliprdr->map_capacity);
	assert(cliprdr->format_mappings != NULL);

	cliprdr->file_array_size = 32;
	cliprdr->file_names = (wchar_t **)calloc(1, cliprdr->file_array_size * sizeof(wchar_t *));
	cliprdr->fileDescriptor = (FILEDESCRIPTORW **)calloc(1, cliprdr->file_array_size * sizeof(FILEDESCRIPTORW *));

	cliprdr->response_data_event = CreateEvent(NULL, TRUE, FALSE, L"response_data_event");
	assert(cliprdr->response_data_event != NULL);

	cliprdr->req_fevent = CreateEvent(NULL, TRUE, FALSE, L"request_filecontents_event");
	cliprdr->ID_FILEDESCRIPTORW = RegisterClipboardFormatW(CFSTR_FILEDESCRIPTORW);
	cliprdr->ID_FILECONTENTS = RegisterClipboardFormatW(CFSTR_FILECONTENTS);
	cliprdr->ID_PREFERREDDROPEFFECT = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);

	cliprdr->cliprdr_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)cliprdr_thread_func, cliprdr, 0, NULL);
	assert(cliprdr->cliprdr_thread != NULL);
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

	clear_file_array(cliprdr);
	clear_format_map(cliprdr);

	if (cliprdr->file_names)
		free(cliprdr->file_names);
	if (cliprdr->fileDescriptor)
		free(cliprdr->fileDescriptor);
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

	cliprdr_send_tempdir(cliprdr);

	cliprdr->channel_initialized = TRUE;

	cliprdr_send_format_list(wfc->cliprdr_context);
}

static BOOL wf_cliprdr_get_file_contents(wchar_t *file_name, BYTE *buffer, int positionLow, int positionHigh, int nRequested, unsigned int *puSize)
{
	HANDLE hFile;
	DWORD nGet;

	if (file_name == NULL || buffer == NULL || puSize == NULL)
	{
		fprintf(stderr, "get file contents Invalid Arguments.\n");
		return FALSE;
	}
	hFile = CreateFileW(file_name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
	{
		return FALSE;
	}

	SetFilePointer(hFile, positionLow, (PLONG)&positionHigh, FILE_BEGIN);

	if (!ReadFile(hFile, buffer, nRequested, &nGet, NULL))
	{
		DWORD err = GetLastError();
		DEBUG_CLIPRDR("ReadFile failed with 0x%x.", err);
	}
	CloseHandle(hFile);

	*puSize = nGet;

	return TRUE;
}

/* path_name has a '\' at the end. e.g. c:\newfolder\, file_name is c:\newfolder\new.txt */
static FILEDESCRIPTORW *wf_cliprdr_get_file_descriptor(WCHAR* file_name, int pathLen)
{
	FILEDESCRIPTORW *fd;
	HANDLE hFile;

	fd = (FILEDESCRIPTORW*) malloc(sizeof(FILEDESCRIPTORW));

	if (!fd)
		return NULL;

	ZeroMemory(fd, sizeof(FILEDESCRIPTORW));

	hFile = CreateFileW(file_name, GENERIC_READ, FILE_SHARE_READ,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_BACKUP_SEMANTICS, NULL);

	if (hFile == INVALID_HANDLE_VALUE)
	{
		free(fd);
		return NULL;
	}

	fd->dwFlags = FD_ATTRIBUTES | FD_FILESIZE | FD_WRITESTIME | FD_PROGRESSUI;

	fd->dwFileAttributes = GetFileAttributes(file_name);

	if (!GetFileTime(hFile, NULL, NULL, &fd->ftLastWriteTime))
	{
		fd->dwFlags &= ~FD_WRITESTIME;
	}

	fd->nFileSizeLow = GetFileSize(hFile, &fd->nFileSizeHigh);

	wcscpy_s(fd->cFileName, sizeof(fd->cFileName) / 2, file_name + pathLen);
	CloseHandle(hFile);

	return fd;
}

static void wf_cliprdr_array_ensure_capacity(cliprdrContext *cliprdr)
{
	if (cliprdr->nFiles == cliprdr->file_array_size)
	{
		cliprdr->file_array_size *= 2;
		cliprdr->fileDescriptor = (FILEDESCRIPTORW **)realloc(cliprdr->fileDescriptor, cliprdr->file_array_size * sizeof(FILEDESCRIPTORW *));
		cliprdr->file_names = (wchar_t **)realloc(cliprdr->file_names, cliprdr->file_array_size * sizeof(wchar_t *));
	}
}

static void wf_cliprdr_add_to_file_arrays(cliprdrContext *cliprdr, WCHAR *full_file_name, int pathLen)
{
	/* add to name array */
	cliprdr->file_names[cliprdr->nFiles] = (LPWSTR) malloc(MAX_PATH);
	wcscpy_s(cliprdr->file_names[cliprdr->nFiles], MAX_PATH, full_file_name);

	/* add to descriptor array */
	cliprdr->fileDescriptor[cliprdr->nFiles] = wf_cliprdr_get_file_descriptor(full_file_name, pathLen);

	cliprdr->nFiles++;

	wf_cliprdr_array_ensure_capacity(cliprdr);
}

static void wf_cliprdr_traverse_directory(cliprdrContext *cliprdr, wchar_t *Dir, int pathLen)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;
	wchar_t DirSpec[MAX_PATH];

	StringCchCopy(DirSpec,MAX_PATH,Dir);
	StringCchCat(DirSpec,MAX_PATH,TEXT("\\*"));

	hFind = FindFirstFile(DirSpec,&FindFileData);

	if(hFind == INVALID_HANDLE_VALUE)
	{
		DEBUG_CLIPRDR("FindFirstFile failed with 0x%x.", GetLastError());
		return;
	}

	while(FindNextFile(hFind, &FindFileData))
	{
		if((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0
			&& wcscmp(FindFileData.cFileName,L".") == 0
			|| wcscmp(FindFileData.cFileName,L"..") == 0)
		{
			continue;
		}


		if((FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) !=0 )
		{
			wchar_t DirAdd[MAX_PATH];

			StringCchCopy(DirAdd,MAX_PATH,Dir);
			StringCchCat(DirAdd,MAX_PATH,TEXT("\\"));
			StringCchCat(DirAdd,MAX_PATH,FindFileData.cFileName);
			wf_cliprdr_add_to_file_arrays(cliprdr, DirAdd, pathLen);
			wf_cliprdr_traverse_directory(cliprdr, DirAdd, pathLen);
		}
		else
		{
			WCHAR fileName[MAX_PATH];

			StringCchCopy(fileName,MAX_PATH,Dir);
			StringCchCat(fileName,MAX_PATH,TEXT("\\"));
			StringCchCat(fileName,MAX_PATH,FindFileData.cFileName);

			wf_cliprdr_add_to_file_arrays(cliprdr, fileName, pathLen);
		}
	}

	FindClose(hFind);
}

static void wf_cliprdr_process_cb_data_request_event(wfContext* wfc, RDP_CB_DATA_REQUEST_EVENT* event)
{
	HANDLE hClipdata;
	int size = 0;
	char* buff = NULL;
	char* globlemem = NULL;
	UINT32 local_format;
	cliprdrContext* cliprdr = (cliprdrContext*) wfc->cliprdr_context;
	RDP_CB_DATA_RESPONSE_EVENT* response_event;

	local_format = event->format;

	if (local_format == FORMAT_ID_PALETTE)
	{
		/* TODO: implement this */
		DEBUG_CLIPRDR("FORMAT_ID_PALETTE is not supported yet.");
	}
	else if (local_format == FORMAT_ID_METAFILE)
	{
		/* TODO: implement this */
		DEBUG_CLIPRDR("FORMAT_ID_MATEFILE is not supported yet.");
	}
	else if (local_format == RegisterClipboardFormatW(L"FileGroupDescriptorW"))
	{
		HRESULT result;
		LPDATAOBJECT dataObj;
		FORMATETC format_etc;
		STGMEDIUM stg_medium;
		DROPFILES *dropFiles;

		int len;
		int i;
		wchar_t *wFileName;
		unsigned int uSize;

		DEBUG_CLIPRDR("file descriptors request.");
		result = OleGetClipboard(&dataObj);
		if (!SUCCEEDED(result))
		{
			DEBUG_CLIPRDR("OleGetClipboard failed.");
		}

		ZeroMemory(&format_etc, sizeof(FORMATETC));
		ZeroMemory(&stg_medium, sizeof(STGMEDIUM));

		/* try to get FileGroupDescriptorW struct from OLE */
		format_etc.cfFormat = local_format;
		format_etc.tymed = TYMED_HGLOBAL;
		format_etc.dwAspect = 1;
		format_etc.lindex = -1;
		format_etc.ptd = 0;

		result = IDataObject_GetData(dataObj, &format_etc, &stg_medium);

		if (SUCCEEDED(result))
		{
			DEBUG_CLIPRDR("Got FileGroupDescriptorW.");
			globlemem = (char *)GlobalLock(stg_medium.hGlobal);
			uSize = GlobalSize(stg_medium.hGlobal);
			size = uSize;
			buff = (char*) malloc(uSize);
			CopyMemory(buff, globlemem, uSize);
			GlobalUnlock(stg_medium.hGlobal);

			ReleaseStgMedium(&stg_medium);

			clear_file_array(cliprdr);
		}
		else
		{
			/* get DROPFILES struct from OLE */
			format_etc.cfFormat = CF_HDROP;
			format_etc.tymed = TYMED_HGLOBAL;
			format_etc.dwAspect = 1;
			format_etc.lindex = -1;

			result = IDataObject_GetData(dataObj, &format_etc, &stg_medium);
			if (!SUCCEEDED(result)) {
				DEBUG_CLIPRDR("dataObj->GetData failed.");
			}

			globlemem = (char *)GlobalLock(stg_medium.hGlobal);

			if (globlemem == NULL)
			{
				GlobalUnlock(stg_medium.hGlobal);

				ReleaseStgMedium(&stg_medium);
				cliprdr->nFiles = 0;

				goto exit;
			}
			uSize = GlobalSize(stg_medium.hGlobal);

			dropFiles = (DROPFILES *)malloc(uSize);
			memcpy(dropFiles, globlemem, uSize);

			GlobalUnlock(stg_medium.hGlobal);

			ReleaseStgMedium(&stg_medium);

			clear_file_array(cliprdr);

			if (dropFiles->fWide)
			{
				wchar_t *p;
				int str_len;
				int offset;
				int pathLen;

				/* dropFiles contains file names */
				for (wFileName = (wchar_t *)((char *)dropFiles + dropFiles->pFiles); (len = wcslen(wFileName)) > 0; wFileName += len + 1)
				{
					/* get path name */
					str_len = wcslen(wFileName);
					offset = str_len;
					/* find the last '\' in full file name */
					for (p = wFileName + offset; *p != L'\\'; p--)
					{
						;
					}
					p += 1;
					pathLen = wcslen(wFileName) - wcslen(p);

					wf_cliprdr_add_to_file_arrays(cliprdr, wFileName, pathLen);

					if ((cliprdr->fileDescriptor[cliprdr->nFiles - 1]->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
					{
						/* this is a directory */
						wf_cliprdr_traverse_directory(cliprdr, wFileName, pathLen);
					}
				}
			}
			else
			{
				char *p;
				for (p = (char *)((char *)dropFiles + dropFiles->pFiles); (len = strlen(p)) > 0; p += len + 1, cliprdr->nFiles++)
				{
					int cchWideChar;

					cchWideChar = MultiByteToWideChar(CP_ACP, MB_COMPOSITE, p, len, NULL, 0);
					cliprdr->file_names[cliprdr->nFiles] = (LPWSTR)malloc(cchWideChar);
					MultiByteToWideChar(CP_ACP, MB_COMPOSITE, p, len, cliprdr->file_names[cliprdr->nFiles], cchWideChar);

					if (cliprdr->nFiles == cliprdr->file_array_size)
					{
						cliprdr->file_array_size *= 2;
						cliprdr->fileDescriptor = (FILEDESCRIPTORW **)realloc(cliprdr->fileDescriptor, cliprdr->file_array_size * sizeof(FILEDESCRIPTORW *));
						cliprdr->file_names = (wchar_t **)realloc(cliprdr->file_names, cliprdr->file_array_size * sizeof(wchar_t *));
					}
				}
			}

exit:
			size = 4 + cliprdr->nFiles * sizeof(FILEDESCRIPTORW);
			buff = (char *)malloc(size);

			Write_UINT32(buff, cliprdr->nFiles);

			for (i = 0; i < cliprdr->nFiles; i++)
			{
				if (cliprdr->fileDescriptor[i])
				{
					memcpy(buff + 4 + i * sizeof(FILEDESCRIPTORW), cliprdr->fileDescriptor[i], sizeof(FILEDESCRIPTORW));
				}
			}
		}

		IDataObject_Release(dataObj);
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

		globlemem = (char*) GlobalLock(hClipdata);
		size = (int) GlobalSize(hClipdata);

		buff = (char*) malloc(size);
		memcpy(buff, globlemem, size);

		GlobalUnlock(hClipdata);

		CloseClipboard();
	}

	response_event = (RDP_CB_DATA_RESPONSE_EVENT*) freerdp_event_new(CliprdrChannel_Class,
			CliprdrChannel_DataResponse, NULL, NULL);

	response_event->data = (BYTE *)buff;
	response_event->size = size;

	freerdp_channels_send_event(cliprdr->channels, (wMessage*) response_event);

	/* Note: don't free buffer here. */
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

	if ((cliprdr->capabilities & CB_USE_LONG_FORMAT_NAMES) != 0)
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
		UINT32 k;

		for (k = 0; k < event->raw_format_data_size / 36; k++)
		{
			int name_len;
			formatMapping* map;

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

	if (file_transferring(cliprdr))
	{
		PostMessage(cliprdr->hwndClipboard, WM_CLIPRDR_MESSAGE, OLE_SETCLIPBOARD, 0);
	}
	else
	{
		if (!OpenClipboard(cliprdr->hwndClipboard))
			return;

		if (EmptyClipboard())
			for (i = 0; i < cliprdr->map_size; i++)
				SetClipboardData(cliprdr->format_mappings[i].local_format_id, NULL);

		CloseClipboard();
	}
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

static void wf_cliprdr_process_cb_filecontents_request_event(wfContext *wfc, RDP_CB_FILECONTENTS_REQUEST_EVENT *event)
{
	cliprdrContext		*cliprdr = (cliprdrContext *)wfc->cliprdr_context;
	UINT32				uSize = 0;
	BYTE				*pData = NULL;
	HRESULT				hRet = S_OK;
	FORMATETC			vFormatEtc;
	LPDATAOBJECT		pDataObj = NULL;
	STGMEDIUM			vStgMedium;
	LPSTREAM			pStream = NULL;
	BOOL				bIsStreamFile = TRUE;
	static LPSTREAM		pStreamStc = NULL;
	static UINT32		uStreamIdStc = 0;

	pData = (BYTE *)calloc(1, event->cbRequested);
	if (!pData)
		goto error;

	hRet = OleGetClipboard(&pDataObj);
	if (!SUCCEEDED(hRet))
	{
		fprintf(stderr, "filecontents: get ole clipboard failed.\n");
		goto error;
	}
	
	ZeroMemory(&vFormatEtc, sizeof(FORMATETC));
	ZeroMemory(&vStgMedium, sizeof(STGMEDIUM));

	vFormatEtc.cfFormat = cliprdr->ID_FILECONTENTS;
	vFormatEtc.tymed = TYMED_ISTREAM;
	vFormatEtc.dwAspect = 1;
	vFormatEtc.lindex = event->lindex;
	vFormatEtc.ptd = NULL;

	if (uStreamIdStc != event->streamId || pStreamStc == NULL)
	{
		LPENUMFORMATETC pEnumFormatEtc;
		ULONG CeltFetched;

		FORMATETC vFormatEtc2;
		if (pStreamStc != NULL)
		{
			IStream_Release(pStreamStc);
			pStreamStc = NULL;
		}

		bIsStreamFile = FALSE;

		hRet = IDataObject_EnumFormatEtc(pDataObj, DATADIR_GET, &pEnumFormatEtc);
		if (hRet == S_OK)
		{
			do {
				hRet = IEnumFORMATETC_Next(pEnumFormatEtc, 1, &vFormatEtc2, &CeltFetched);
				if (hRet == S_OK)
				{
					if (vFormatEtc2.cfFormat == cliprdr->ID_FILECONTENTS)
					{
						hRet = IDataObject_GetData(pDataObj, &vFormatEtc, &vStgMedium);
						if (hRet == S_OK)
						{
							pStreamStc = vStgMedium.pstm;
							uStreamIdStc = event->streamId;
							bIsStreamFile = TRUE;
						}
						break;
					}
				}
			} while (hRet == S_OK);
		}
	}

	if (bIsStreamFile == TRUE)
	{
		if (event->dwFlags == 0x00000001)            /* FILECONTENTS_SIZE */
		{
			STATSTG vStatStg;

			ZeroMemory(&vStatStg, sizeof(STATSTG));

			hRet = IStream_Stat(pStreamStc, &vStatStg, STATFLAG_NONAME);
			if (hRet == S_OK)
			{
				Write_UINT32(pData, vStatStg.cbSize.LowPart);
				Write_UINT32(pData + 4, vStatStg.cbSize.HighPart);
				uSize = event->cbRequested;
			}
		}
		else if (event->dwFlags == 0x00000002)     /* FILECONTENTS_RANGE */
		{
			LARGE_INTEGER dlibMove;
			ULARGE_INTEGER dlibNewPosition;

			dlibMove.HighPart = event->nPositionHigh;
			dlibMove.LowPart = event->nPositionLow;

			hRet = IStream_Seek(pStreamStc, dlibMove, STREAM_SEEK_SET, &dlibNewPosition);
			if (SUCCEEDED(hRet))
			{
				hRet = IStream_Read(pStreamStc, pData, event->cbRequested, (PULONG)&uSize);
			}

		}
	}
	else // is local file
	{
		if (event->dwFlags == 0x00000001)            /* FILECONTENTS_SIZE */
		{
			Write_UINT32(pData, cliprdr->fileDescriptor[event->lindex]->nFileSizeLow);
			Write_UINT32(pData + 4, cliprdr->fileDescriptor[event->lindex]->nFileSizeHigh);
			uSize = event->cbRequested;
		}
		else if (event->dwFlags == 0x00000002)     /* FILECONTENTS_RANGE */
		{
			BOOL bRet;

			bRet = wf_cliprdr_get_file_contents(cliprdr->file_names[event->lindex], pData,
				event->nPositionLow, event->nPositionHigh, event->cbRequested, &uSize);
			if (bRet == FALSE)
			{
				fprintf(stderr, "get file contents failed.\n");
				uSize = 0;
				goto error;
			}
		}
	}

	IDataObject_Release(pDataObj);

	if (uSize == 0)
	{
		free(pData);
		pData = NULL;
	}

	cliprdr_send_response_filecontents(cliprdr, event->streamId, uSize, pData);

	return;

error:
	if (pData)
	{
		free(pData);
		pData = NULL;
	}

	if (pDataObj)
	{
		IDataObject_Release(pDataObj);
		pDataObj = NULL;
	}
	fprintf(stderr, "filecontents: send failed response.\n");
	cliprdr_send_response_filecontents(cliprdr, event->streamId, 0, NULL);
	return;
}

static void wf_cliprdr_process_cb_filecontents_response_event(wfContext *wfc, RDP_CB_FILECONTENTS_RESPONSE_EVENT *event)
{
	cliprdrContext *cliprdr = (cliprdrContext *)wfc->cliprdr_context;

	cliprdr->req_fsize = event->size;
	cliprdr->req_fdata = (char *)malloc(event->size);
	memcpy(cliprdr->req_fdata, event->data, event->size);

	SetEvent(cliprdr->req_fevent);
}

static void wf_cliprdr_process_cb_lock_clipdata_event(wfContext *wfc, RDP_CB_LOCK_CLIPDATA_EVENT *event)
{

}

static void wf_cliprdr_process_cb_unlock_clipdata_event(wfContext *wfc, RDP_CB_UNLOCK_CLIPDATA_EVENT *event)
{

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

		case CliprdrChannel_FilecontentsRequest:
			wf_cliprdr_process_cb_filecontents_request_event(wfc, (RDP_CB_FILECONTENTS_REQUEST_EVENT *) event);
			break;

		case CliprdrChannel_FilecontentsResponse:
			wf_cliprdr_process_cb_filecontents_response_event(wfc, (RDP_CB_FILECONTENTS_RESPONSE_EVENT *) event);
			break;

		case CliprdrChannel_LockClipdata:
			wf_cliprdr_process_cb_lock_clipdata_event(wfc, (RDP_CB_LOCK_CLIPDATA_EVENT *) event);
			break;

		case CliprdrChannel_UnLockClipdata:
			wf_cliprdr_process_cb_unlock_clipdata_event(wfc, (RDP_CB_UNLOCK_CLIPDATA_EVENT *) event);
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
