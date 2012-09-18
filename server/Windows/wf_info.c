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
#include "wf_dxgi.h"

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

	OSVERSIONINFOEX osvi;
	SYSTEM_INFO si;
	BOOL bOsVersionInfoEx;



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

		wfi->updateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		wfi->updateSemaphore = CreateSemaphore(NULL, 0, 32, NULL);

		wfi->updateThread = CreateThread(NULL, 0, wf_update_thread, wfi, CREATE_SUSPENDED, NULL);

		if (!wfi->updateThread)
		{
			_tprintf(_T("Failed to create update thread\n"));
		}

		wfi->peers = (freerdp_peer**) malloc(sizeof(freerdp_peer*) * 32);

		wfi->framesPerSecond = 10;

		status = RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("Software\\FreeRDP\\Server"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey);

		if (status == ERROR_SUCCESS)
		{
			if (RegQueryValueEx(hKey, _T("FramesPerSecond"), NULL, &dwType, (BYTE*) &dwValue, &dwSize) == ERROR_SUCCESS)
				wfi->framesPerSecond = dwValue;
				
		}

		RegCloseKey(hKey);

		//detect windows version
		ZeroMemory(&si, sizeof(SYSTEM_INFO));
		ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));

		osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
		bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO*) &osvi);

		wfi->win8 = FALSE;
		if(bOsVersionInfoEx != 0 )
		{
			if ( VER_PLATFORM_WIN32_NT==osvi.dwPlatformId && 
			osvi.dwMajorVersion > 4 )
			{
				if ( osvi.dwMajorVersion == 6 && 
					osvi.dwMinorVersion == 2)
				{
					wfi->win8 = TRUE;
				}
			}
		}
	}

	return wfi;
}

wfInfo* wf_info_get_instance()
{
	if (wfInfoInstance == NULL)
		wfInfoInstance = wf_info_init();

	return wfInfoInstance;
}


void wf_info_peer_register(wfInfo* wfi, wfPeerContext* context)
{
	if (wf_info_lock(wfi) > 0)
	{
		context->info = wfi;
		context->updateEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		if(wfi->win8 && 
			(wfi->peerCount == 0))
		{
			wf_dxgi_init(wfi);
		}
		else
		{
			wf_mirror_driver_activate(wfi);
		}

		wfi->peers[wfi->peerCount++] = ((rdpContext*) context)->peer;

		printf("Registering Peer: %d\n", wfi->peerCount);

		wf_info_unlock(wfi);
	}
}

void wf_info_peer_unregister(wfInfo* wfi, wfPeerContext* context)
{
	if (wf_info_lock(wfi) > 0)
	{
		wfi->peers[--(wfi->peerCount)] = NULL;
		CloseHandle(context->updateEvent);

		printf("Unregistering Peer: %d\n", wfi->peerCount);

		if(wfi->win8 && 
			(wfi->peerCount == 0))
		{
			wf_dxgi_cleanup(wfi);
		}

		wf_info_unlock(wfi);
	}
}

BOOL wf_info_have_updates(wfInfo* wfi)
{
	if(wfi->win8)
	{
		if(wfi->framesWaiting == 0)
			return FALSE;
	}
	else
	{
		if (wfi->nextUpdate == wfi->lastUpdate)
			return FALSE;
	}

	return TRUE;
}

void wf_info_update_changes(wfInfo* wfi)
{
	if(wfi->win8)
	{
		wf_dxgi_nextFrame(wfi, wfi->framesPerSecond / 1000);
	}
	else
	{
		GETCHANGESBUF* buf;

		buf = (GETCHANGESBUF*) wfi->changeBuffer;
		wfi->nextUpdate = buf->buffer->counter;
	}
}

void wf_info_find_invalid_region(wfInfo* wfi)
{
	if(wfi->win8)
	{
		wf_dxgi_getInvalidRegion(&wfi->invalid);
	}
	else
	{
		int i;
		GETCHANGESBUF* buf;

		buf = (GETCHANGESBUF*) wfi->changeBuffer;

		for (i = wfi->lastUpdate; i != wfi->nextUpdate; i = (i + 1) % MAXCHANGES_BUF)
		{
			UnionRect(&wfi->invalid, &wfi->invalid, &buf->buffer->pointrect[i].rect);
		}
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

void wf_info_invalidate_full_screen(wfInfo* wfi)
{
	SetRect(&wfi->invalid, 0, 0, wfi->width, wfi->height);
}

BOOL wf_info_have_invalid_region(wfInfo* wfi)
{
	return IsRectEmpty(&wfi->invalid);
}

void wf_info_getScreenData(wfInfo* wfi, uint8** pBits, int* pitch)
{
	if(wfi->win8)
	{
		wf_dxgi_getPixelData(wfi, pBits, pitch, &wfi->invalid);
	}
	else
	{
		long offset;
		GETCHANGESBUF* changes;
		changes = (GETCHANGESBUF*) wfi->changeBuffer;

		offset = (4 * wfi->invalid.left) + (wfi->invalid.top * wfi->width * 4);
		*pBits = ((uint8*) (changes->Userbuffer)) + offset;
		*pitch = wfi->width * 4;
	}
}