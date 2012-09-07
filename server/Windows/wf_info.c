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

#include <winpr/tchar.h>
#include <winpr/windows.h>

#include "wf_info.h"
#include "wf_update.h"
#include "wf_mirage.h"

static wfInfo* wfInfoInstance = NULL;

int wf_info_lock(wfInfo* wfi)
{
	DWORD dRes;

	dRes = WaitForSingleObject(wfi->mutex, INFINITE);

	switch (dRes)
	{
		case WAIT_ABANDONED:
		case WAIT_OBJECT_0:
			return TRUE;
			break;

		case WAIT_TIMEOUT:
			return FALSE;
			break;

		case WAIT_FAILED:
			printf("wf_info_lock failed with 0x%08X\n", GetLastError());
			return -1;
			break;
	}

	return -1;
}

int wf_info_try_lock(wfInfo* wfi, DWORD dwMilliseconds)
{
	DWORD dRes;

	dRes = WaitForSingleObject(wfi->mutex, dwMilliseconds);

	switch (dRes)
	{
		case WAIT_ABANDONED:
		case WAIT_OBJECT_0:
			return TRUE;
			break;

		case WAIT_TIMEOUT:
			return FALSE;
			break;

		case WAIT_FAILED:
			printf("wf_info_try_lock failed with 0x%08X\n", GetLastError());
			return -1;
			break;
	}

	return -1;
}

int wf_info_unlock(wfInfo* wfi)
{
	if (ReleaseMutex(wfi->mutex) == 0)
	{
		printf("wf_info_unlock failed with 0x%08X\n", GetLastError());
		return -1;
	}

	return TRUE;
}

wfInfo* wf_info_init()
{
	wfInfo* wfi;

	wfi = (wfInfo*) malloc(sizeof(wfInfo));
	ZeroMemory(wfi, sizeof(wfInfo));

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
			_tprintf(_T("CreateMutex error: %d\n"), GetLastError());
		}

		wfi->updateEvent = CreateEvent(0, 1, 0, 0);

		wfi->updateThread = CreateThread(NULL, 0, wf_update_thread, wfi, CREATE_SUSPENDED, NULL);

		if (!wfi->updateThread)
		{
			_tprintf(_T("Failed to create update thread\n"));
		}

		wfi->peers = (wfPeerContext**) malloc(sizeof(wfPeerContext*) * 32);

		wfi->framesPerSecond = 24;

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

		if (status == ERROR_SUCCESS)
		{
			if (RegQueryValueEx(hKey, _T("FramesPerSecond"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
				wfi->framesPerSecond = dwValue;
				
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

void wf_update_encoder_init(wfInfo* wfi)
{
	wfi->rfx_context = rfx_context_new();
	wfi->rfx_context->mode = RLGR3;
	wfi->rfx_context->width = wfi->width;
	wfi->rfx_context->height = wfi->height;
	rfx_context_set_pixel_format(wfi->rfx_context, RDP_PIXEL_FORMAT_B8G8R8A8);
	wfi->s = stream_new(0xFFFF);
}

void wf_update_encoder_uninit(wfInfo* wfi)
{
	if (wfi->rfx_context != NULL)
	{
		rfx_context_free(wfi->rfx_context);
		wfi->rfx_context = NULL;
		stream_free(wfi->s);
	}
}

void wf_update_encoder_reinit(wfInfo* wfi)
{
	wf_update_encoder_uninit(wfi);
	wf_update_encoder_init(wfi);
}

void wf_info_peer_register(wfInfo* wfi, wfPeerContext* context)
{
	if (wf_info_lock(wfi) > 0)
	{
		context->info = wfi;

		if (wfi->peerCount < 1)
		{
			wf_mirror_driver_find_display_device(wfi);
			wf_mirror_driver_display_device_attach(wfi, 1);
			wf_mirror_driver_update(wfi, FALSE);
			wf_mirror_driver_map_memory(wfi);

			//wf_update_encoder_init(wfi);
		}

		wf_update_encoder_reinit(wfi);
		wfi->peers[wfi->peerCount++] = context;

		wf_info_unlock(wfi);
	}
}

void wf_info_peer_unregister(wfInfo* wfi, wfPeerContext* context)
{
	if (wf_info_lock(wfi) > 0)
	{
		if (wfi->peerCount <= 1)
		{
			wfi->peers[--(wfi->peerCount)] = NULL;

			wf_mirror_driver_cleanup(wfi);
			wf_mirror_driver_display_device_attach(wfi, 0);
			wf_mirror_driver_update(wfi, 1);

			//wf_update_encoder_uninit(wfi);
		}	
		else
		{
			wfi->peers[--(wfi->peerCount)] = NULL;
		}

		wf_info_unlock(wfi);
	}
}

BOOL wf_info_have_updates(wfInfo* wfi)
{
	if (wfi->nextUpdate == wfi->lastUpdate)
		return FALSE;

	return TRUE;
}

void wf_info_update_changes(wfInfo* wfi)
{
	GETCHANGESBUF* buf;

	buf = (GETCHANGESBUF*) wfi->changeBuffer;
	wfi->nextUpdate = buf->buffer->counter;
}

void wf_info_find_invalid_region(wfInfo* wfi)
{
	int i;
	GETCHANGESBUF* buf;

	buf = (GETCHANGESBUF*) wfi->changeBuffer;

	for (i = wfi->lastUpdate; i != wfi->nextUpdate; i = (i + 1) % MAXCHANGES_BUF)
	{
		UnionRect(&wfi->invalid, &wfi->invalid, &buf->buffer->pointrect[i].rect);
	}

	if (wfi->invalid.left < 0)
		wfi->invalid.left = 0;

	if (wfi->invalid.top < 0)
		wfi->invalid.top = 0;

	if (wfi->invalid.right >= wfi->width)
		wfi->invalid.right = wfi->width - 1;

	if (wfi->invalid.bottom >= wfi->height)
		wfi->invalid.bottom = wfi->height - 1;
}

void wf_info_clear_invalid_region(wfInfo* wfi)
{
	wfi->lastUpdate = wfi->nextUpdate;
	SetRectEmpty(&wfi->invalid);
}

BOOL wf_info_have_invalid_region(wfInfo* wfi)
{
	return IsRectEmpty(&wfi->invalid);
}
