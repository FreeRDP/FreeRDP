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

		wfi->encodeMutex = CreateMutex(NULL, FALSE, NULL);

		if (wfi->encodeMutex == NULL) 
		{
			_tprintf(_T("CreateMutex error: %d\n"), GetLastError());
		}

		wfi->can_send_mutex = CreateMutex(NULL, FALSE, NULL);

		if (wfi->can_send_mutex == NULL) 
		{
			_tprintf(_T("CreateMutex error: %d\n"), GetLastError());
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

void wf_info_mirror_init(wfInfo* wfi, wfPeerContext* context)
{
	DWORD dRes;

	dRes = WaitForSingleObject(wfi->mutex, INFINITE);

	switch (dRes)
	{
		case WAIT_ABANDONED:
		case WAIT_OBJECT_0:

			if (wfi->subscribers < 1)
			{
				/* only the first peer needs to call this. */

				context->info = wfi;
				wf_check_disp_devices(context->info);
				wf_disp_device_set_attach_mode(context->info, 1);
				wf_update_mirror_drv(context->info, 0);
				wf_map_mirror_mem(context->info);

				context->rfx_context = rfx_context_new();
				context->rfx_context->mode = RLGR3;
				context->rfx_context->width = context->info->width;
				context->rfx_context->height = context->info->height;

				rfx_context_set_pixel_format(context->rfx_context, RDP_PIXEL_FORMAT_B8G8R8A8);
				context->s = stream_new(65536);

				context->info->primary_buffer = (BYTE*) malloc((context->info->width) * (context->info->height) * 4);

				ReleaseMutex(wfi->encodeMutex);
			}
			++wfi->subscribers;

			if (!ReleaseMutex(wfi->mutex)) 
			{
				_tprintf(_T("Error releasing mutex\n"));
			}

			break;

		default:
			_tprintf(_T("Error waiting for mutex: %d\n"), dRes);
			break;
	}
}


/**
 * TODO: i think i can replace all the context->info here with info
 * in fact it may not even care about subscribers
 */

void wf_info_subscriber_release(wfInfo* wfi, wfPeerContext* context)
{
	DWORD dRes;

	WaitForSingleObject(wfi->mutex, INFINITE); 

	if (context && (wfi->subscribers == 1))
	{
		dRes = WaitForSingleObject(wfi->encodeMutex, INFINITE);

		switch (dRes)
		{
			/* The thread got ownership of the mutex */
			
			case WAIT_OBJECT_0: 
				--wfi->subscribers;
				/* only the last peer needs to call this */
				wf_mirror_cleanup(context->info);
				wf_disp_device_set_attach_mode(context->info, FALSE);
				wf_update_mirror_drv(context->info, 1);

				stream_free(context->s);
				rfx_context_free(context->rfx_context);

				free(context->info->primary_buffer);
				break; 

			/**
			 * The thread got ownership of an abandoned mutex
			 * The database is in an indeterminate state
			 */
			default: 
				printf("wf_info_subscriber_release: Something else happened!!! dRes = %d\n", dRes);
				break;
		}
	}	
	else
	{
		--wfi->subscribers;
	}

	ReleaseMutex(wfi->mutex);

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

void wf_info_updated(wfInfo* wfi)
{
	wfi->lastUpdate = wfi->nextUpdate;	
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

	if (wfi->enc_data == FALSE)
	{
		wfi->invalid_x1 = wfi->width - 1;
		wfi->invalid_x2 = 0;
		wfi->invalid_y1 = wfi->height - 1;
		wfi->invalid_y2 = 0;
	}

	for (i = wfi->lastUpdate; i != wfi->nextUpdate; i = (i + 1) % MAXCHANGES_BUF)
	{
		wfi->invalid_x1 = min(wfi->invalid_x1, buf->buffer->pointrect[i].rect.left);
		wfi->invalid_x2 = max(wfi->invalid_x2, buf->buffer->pointrect[i].rect.right);
		wfi->invalid_y1 = min(wfi->invalid_y1, buf->buffer->pointrect[i].rect.top);
		wfi->invalid_y2 = max(wfi->invalid_y2, buf->buffer->pointrect[i].rect.bottom);
	}

	if (wfi->invalid_x1 < 0)
		wfi->invalid_x1 = 0;

	if (wfi->invalid_y1 < 0)
		wfi->invalid_y1 = 0;

	if (wfi->invalid_x2 >= wfi->width)
		wfi->invalid_x2 = wfi->width - 1;

	if (wfi->invalid_y2 >= wfi->height)
		wfi->invalid_y2 = wfi->height - 1;
}

void wf_info_clear_invalid_region(wfInfo* wfi)
{
	WaitForSingleObject(wfi->mutex, INFINITE);

	wfi->lastUpdate = wfi->nextUpdate;
	
	wfi->invalid_x1 = wfi->width - 1;
	wfi->invalid_x2 = 0;
	wfi->invalid_y1 = wfi->height - 1;
	wfi->invalid_y2 = 0;
	
	ReleaseMutex(wfi->mutex);
}

BOOL wf_info_have_invalid_region(wfInfo* wfi)
{
	if ((wfi->invalid_x1 >= wfi->invalid_x2) || (wfi->invalid_y1 >= wfi->invalid_y2))
		return FALSE;

	return TRUE;
}
