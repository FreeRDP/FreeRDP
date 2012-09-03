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
		wfi->mutex = CreateMutex(NULL, FALSE, NULL);

		if (wfi->mutex == NULL) 
		{
			_tprintf(_T("CreateMutex error: %d\n"), GetLastError());
		}

		wfi->updateEvent = CreateEvent(0, 1, 0, 0);
	}

	return wfi;
}

wfInfo* wf_info_get_instance()
{
	if (wfInfoInstance == NULL)
		wfInfoInstance = wf_info_init();

	return wfInfoInstance;
}

void wf_info_mirror_init(wfInfo* wfi, wfPeerContext* context)
{
	if (wf_info_lock(wfi) > 0)
	{
		if (wfi->subscribers < 1)
		{
			context->info = wfi;
			if(!wf_check_disp_devices(wfi))
			{
				_tprintf(_T("Failed to load Mirror driver\n"));
				exit(1);
			}
			wf_disp_device_set_attach_mode(wfi, 1);
			wf_update_mirror_drv(wfi, 0);
			wf_map_mirror_mem(wfi);

			wfi->rfx_context = rfx_context_new();
			wfi->rfx_context->mode = RLGR3;
			wfi->rfx_context->width = wfi->width;
			wfi->rfx_context->height = wfi->height;

			rfx_context_set_pixel_format(wfi->rfx_context, RDP_PIXEL_FORMAT_B8G8R8A8);
			wfi->s = stream_new(65536);
		}

		wfi->subscribers++;

		wf_info_unlock(wfi);
	}
}

/**
 * TODO: i think i can replace all the context->info here with info
 * in fact it may not even care about subscribers
 */

void wf_info_subscriber_release(wfInfo* wfi, wfPeerContext* context)
{
	if (wf_info_lock(wfi) > 0)
	{
		if (context && (wfi->subscribers == 1))
		{
			wfi->subscribers--;
			/* only the last peer needs to call this */
			wf_mirror_cleanup(context->info);
			wf_disp_device_set_attach_mode(context->info, FALSE);
			wf_update_mirror_drv(context->info, 1);

			stream_free(wfi->s);
			rfx_context_free(wfi->rfx_context);
		}	
		else
		{
			wfi->subscribers--;
		}

		wf_info_unlock(wfi);
	}

	/**
	 * Note: if we released the last subscriber,
	 * block the encoder until next subscriber
	 */	
}

BOOL wf_info_has_subscribers(wfInfo* wfi)
{
	if (wfi->subscribers > 0)
		return TRUE;

	return FALSE;
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
