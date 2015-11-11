/**
* FreeRDP: A Remote Desktop Protocol Client
* FreeRDP Windows Server
*
* Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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

#include <stdlib.h>

#include <freerdp/build-config.h>

#include <winpr/tchar.h>
#include <winpr/windows.h>

#include "wf_info.h"
#include "wf_update.h"
#include "wf_mirage.h"
#include "wf_dxgi.h"

#include <freerdp/log.h>
#define TAG SERVER_TAG("windows")

#define SERVER_KEY "Software\\"FREERDP_VENDOR_STRING"\\" \
		FREERDP_PRODUCT_STRING"\\Server"

static wfInfo* wfInfoInstance = NULL;
static int _IDcount = 0;

BOOL wf_info_lock(wfInfo* wfi)
{
	DWORD dRes;

	dRes = WaitForSingleObject(wfi->mutex, INFINITE);

	switch (dRes)
	{
	case WAIT_ABANDONED:
	case WAIT_OBJECT_0:
		return TRUE;

	case WAIT_TIMEOUT:
		return FALSE;

	case WAIT_FAILED:
		WLog_ERR(TAG, "wf_info_lock failed with 0x%08X", GetLastError());
		return FALSE;
	}

	return FALSE;
}

BOOL wf_info_try_lock(wfInfo* wfi, DWORD dwMilliseconds)
{
	DWORD dRes;

	dRes = WaitForSingleObject(wfi->mutex, dwMilliseconds);

	switch (dRes)
	{
	case WAIT_ABANDONED:
	case WAIT_OBJECT_0:
		return TRUE;

	case WAIT_TIMEOUT:
		return FALSE;

	case WAIT_FAILED:
		WLog_ERR(TAG, "wf_info_try_lock failed with 0x%08X", GetLastError());
		return FALSE;
	}

	return FALSE;
}

BOOL wf_info_unlock(wfInfo* wfi)
{
	if (!ReleaseMutex(wfi->mutex))
	{
		WLog_ERR(TAG, "wf_info_unlock failed with 0x%08X", GetLastError());
		return FALSE;
	}

	return TRUE;
}

wfInfo* wf_info_init()
{
	wfInfo* wfi;

	wfi = (wfInfo*) calloc(1, sizeof(wfInfo));

	if (wfi != NULL)
	{
		HKEY hKey;
		LONG status;
		DWORD dwType;
		DWORD dwSize;
		DWORD dwValue;

		wfi->mutex = CreateMutex(NULL, FALSE, NULL);

		if (wfi->mutex == NULL)
		{
			WLog_ERR(TAG, "CreateMutex error: %d", GetLastError());
			free(wfi);
			return NULL;
		}

		wfi->updateSemaphore = CreateSemaphore(NULL, 0, 32, NULL);
		if (!wfi->updateSemaphore)
		{
			WLog_ERR(TAG, "CreateSemaphore error: %d", GetLastError());
			CloseHandle(wfi->mutex);
			free(wfi);
			return NULL;
		}

		wfi->updateThread = CreateThread(NULL, 0, wf_update_thread, wfi, CREATE_SUSPENDED, NULL);

		if (!wfi->updateThread)
		{
			WLog_ERR(TAG, "Failed to create update thread");
			CloseHandle(wfi->mutex);
			CloseHandle(wfi->updateSemaphore);
			free(wfi);
			return NULL;
		}

		wfi->peers = (freerdp_peer**) calloc(WF_INFO_MAXPEERS, sizeof(freerdp_peer*));
		if (!wfi->peers)
		{
			WLog_ERR(TAG, "Failed to allocate memory for peer");
			CloseHandle(wfi->mutex);
			CloseHandle(wfi->updateSemaphore);
			CloseHandle(wfi->updateThread);
			free(wfi);
			return NULL;
		}

		//Set FPS
		wfi->framesPerSecond = WF_INFO_DEFAULT_FPS;

		status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, SERVER_KEY, 0,
					KEY_READ | KEY_WOW64_64KEY, &hKey);
		if (status == ERROR_SUCCESS)
		{
			if (RegQueryValueEx(hKey, _T("FramesPerSecond"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
				wfi->framesPerSecond = dwValue;
		}
		RegCloseKey(hKey);

		//Set input toggle
		wfi->input_disabled = FALSE;

		status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, SERVER_KEY,
					0, KEY_READ | KEY_WOW64_64KEY, &hKey);
		if (status == ERROR_SUCCESS)
		{
			if (RegQueryValueEx(hKey, _T("DisableInput"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
			{
				if (dwValue != 0)
					wfi->input_disabled = TRUE;
			}
		}
		RegCloseKey(hKey);
	}

	return wfi;
}

wfInfo* wf_info_get_instance()
{
	if (wfInfoInstance == NULL)
		wfInfoInstance = wf_info_init();

	return wfInfoInstance;
}

BOOL wf_info_peer_register(wfInfo* wfi, wfPeerContext* context)
{
	int i;
	int peerId;

	if (!wfi || !context)
		return FALSE;

	if (!wf_info_lock(wfi))
		return FALSE;

	if (wfi->peerCount == WF_INFO_MAXPEERS)
		goto fail_peer_count;

	context->info = wfi;
	if (!(context->updateEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		goto fail_update_event;

	//get the offset of the top left corner of selected screen
	EnumDisplayMonitors(NULL, NULL, wf_info_monEnumCB, 0);
	_IDcount = 0;

#ifdef WITH_DXGI_1_2
	if (wfi->peerCount == 0)
		if (wf_dxgi_init(wfi) != 0)
			goto fail_driver_init;
#else
	if (!wf_mirror_driver_activate(wfi))
		goto fail_driver_init;
#endif
	//look through the array of peers until an empty slot
	for (i = 0; i < WF_INFO_MAXPEERS; ++i)
	{
		//empty index will be our peer id
		if (wfi->peers[i] == NULL)
		{
			peerId = i;
			break;
		}
	}

	wfi->peers[peerId] = ((rdpContext*) context)->peer;
	wfi->peers[peerId]->pId = peerId;
	wfi->peerCount++;

	WLog_INFO(TAG, "Registering Peer: id=%d #=%d", peerId, wfi->peerCount);
	wf_info_unlock(wfi);
	wfreerdp_server_peer_callback_event(peerId, WF_SRV_CALLBACK_EVENT_CONNECT);

	return TRUE;

fail_driver_init:
	CloseHandle(context->updateEvent);
	context->updateEvent = NULL;
fail_update_event:
fail_peer_count:
	context->socketClose = TRUE;
	wf_info_unlock(wfi);
	return FALSE;
}

void wf_info_peer_unregister(wfInfo* wfi, wfPeerContext* context)
{
	if (wf_info_lock(wfi))
	{
		int peerId;

		peerId = ((rdpContext*) context)->peer->pId;
		wfi->peers[peerId] = NULL;
		wfi->peerCount--;
		CloseHandle(context->updateEvent);
		WLog_INFO(TAG, "Unregistering Peer: id=%d, #=%d", peerId, wfi->peerCount);

#ifdef WITH_DXGI_1_2
		if (wfi->peerCount == 0)
			wf_dxgi_cleanup(wfi);
#endif

		wf_info_unlock(wfi);

		wfreerdp_server_peer_callback_event(peerId, WF_SRV_CALLBACK_EVENT_DISCONNECT);
	}
}

BOOL wf_info_have_updates(wfInfo* wfi)
{
#ifdef WITH_DXGI_1_2
	if (wfi->framesWaiting == 0)
		return FALSE;
#else
	if (wfi->nextUpdate == wfi->lastUpdate)
		return FALSE;
#endif
	return TRUE;
}

void wf_info_update_changes(wfInfo* wfi)
{
#ifdef WITH_DXGI_1_2
	wf_dxgi_nextFrame(wfi, wfi->framesPerSecond * 1000);
#else
	GETCHANGESBUF* buf;

	buf = (GETCHANGESBUF*) wfi->changeBuffer;
	wfi->nextUpdate = buf->buffer->counter;
#endif
}

void wf_info_find_invalid_region(wfInfo* wfi)
{
#ifdef WITH_DXGI_1_2
	wf_dxgi_getInvalidRegion(&wfi->invalid);
#else
	int i;
	GETCHANGESBUF* buf;

	buf = (GETCHANGESBUF*) wfi->changeBuffer;

	for (i = wfi->lastUpdate; i != wfi->nextUpdate; i = (i + 1) % MAXCHANGES_BUF)
	{
		LPRECT lpR = &buf->buffer->pointrect[i].rect;

		//need to make sure we only get updates from the selected screen
		if (	(lpR->left >= wfi->servscreen_xoffset) &&
			(lpR->right <= (wfi->servscreen_xoffset + wfi->servscreen_width) ) &&
			(lpR->top >= wfi->servscreen_yoffset) &&
			(lpR->bottom <= (wfi->servscreen_yoffset + wfi->servscreen_height) ) )
		{
			UnionRect(&wfi->invalid, &wfi->invalid, lpR);
		}
		else
		{
			continue;
		}
	}
#endif

	if (wfi->invalid.left < 0)
		wfi->invalid.left = 0;

	if (wfi->invalid.top < 0)
		wfi->invalid.top = 0;

	if (wfi->invalid.right >= wfi->servscreen_width)
		wfi->invalid.right = wfi->servscreen_width - 1;

	if (wfi->invalid.bottom >= wfi->servscreen_height)
		wfi->invalid.bottom = wfi->servscreen_height - 1;

	//WLog_DBG(TAG, "invalid region: (%d, %d), (%d, %d)", wfi->invalid.left, wfi->invalid.top, wfi->invalid.right, wfi->invalid.bottom);
}

void wf_info_clear_invalid_region(wfInfo* wfi)
{
	wfi->lastUpdate = wfi->nextUpdate;
	SetRectEmpty(&wfi->invalid);
}

void wf_info_invalidate_full_screen(wfInfo* wfi)
{
	SetRect(&wfi->invalid, 0, 0, wfi->servscreen_width, wfi->servscreen_height);
}

BOOL wf_info_have_invalid_region(wfInfo* wfi)
{
	return IsRectEmpty(&wfi->invalid);
}

void wf_info_getScreenData(wfInfo* wfi, long* width, long* height, BYTE** pBits, int* pitch)
{
	*width = (wfi->invalid.right - wfi->invalid.left);
	*height = (wfi->invalid.bottom - wfi->invalid.top);

#ifdef WITH_DXGI_1_2
	wf_dxgi_getPixelData(wfi, pBits, pitch, &wfi->invalid);
#else
	{
		long offset;
		GETCHANGESBUF* changes;
		changes = (GETCHANGESBUF*) wfi->changeBuffer;

		*width += 1;
		*height += 1;

		offset = (4 * wfi->invalid.left) + (wfi->invalid.top * wfi->virtscreen_width * 4);
		*pBits = ((BYTE*) (changes->Userbuffer)) + offset;
		*pitch = wfi->virtscreen_width * 4;
	}
#endif
}

BOOL CALLBACK wf_info_monEnumCB(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
	wfInfo * wfi;

	wfi = wf_info_get_instance();
	if (!wfi)
		return FALSE;

	if (_IDcount == wfi->screenID)
	{
		wfi->servscreen_xoffset = lprcMonitor->left;
		wfi->servscreen_yoffset = lprcMonitor->top;
	}

	_IDcount++;

	return TRUE;
}
