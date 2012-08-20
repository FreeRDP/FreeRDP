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

#include <stdlib.h>
#include <winpr/tchar.h>
#include <winpr/windows.h>

#include "wf_info.h"
#include "wf_mirage.h"

wfInfo * wf_info_init(wfInfo * info)
{
	if(!info)
	{
		info = (wfInfo*)malloc(sizeof(wfInfo)); //free this on shutdown
		memset(info, 0, sizeof(wfInfo));

		info->mutex = CreateMutex( 
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex

		if (info->mutex == NULL) 
		{
			_tprintf(_T("CreateMutex error: %d\n"), GetLastError());
		}

		info->encodeMutex = CreateMutex( 
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex

		if (info->encodeMutex == NULL) 
		{
			_tprintf(_T("CreateMutex error: %d\n"), GetLastError());
		}

		info->can_send_mutex = CreateMutex( 
        NULL,              // default security attributes
        FALSE,             // initially not owned
        NULL);             // unnamed mutex

		if (info->can_send_mutex == NULL) 
		{
			_tprintf(_T("CreateMutex error: %d\n"), GetLastError());
		}
	}

	return info;
}

void wf_info_mirror_init(wfInfo * info, wfPeerContext* context)
{
	DWORD dRes;


	dRes = WaitForSingleObject( 
            info->mutex,    // handle to mutex
            INFINITE);  // no time-out interval

	switch(dRes)
	{
		case WAIT_OBJECT_0:
			if(info->subscribers == 0)
			{
				//only the first peer needs to call this.
				context->wfInfo = info;
				wf_check_disp_devices(context->wfInfo);
				wf_disp_device_set_attatch(context->wfInfo, 1);
				wf_update_mirror_drv(context->wfInfo, 0);
				wf_map_mirror_mem(context->wfInfo);

				context->rfx_context = rfx_context_new();
				context->rfx_context->mode = RLGR3;
				context->rfx_context->width = context->wfInfo->width;
				context->rfx_context->height = context->wfInfo->height;

				rfx_context_set_pixel_format(context->rfx_context, RDP_PIXEL_FORMAT_B8G8R8A8);
				context->s = stream_new(65536);

				printf("Start Encoder\n");
				ReleaseMutex(info->encodeMutex);
			}
			++info->subscribers;

			if (! ReleaseMutex(info->mutex)) 
            { 
                _tprintf(_T("Error releasing mutex\n"));
            } 

			break;

		default:
			_tprintf(_T("Error waiting for mutex: %d\n"), dRes);
	}

}


//todo: i think i can replace all the context->info here with info
//in fact i may not even care about subscribers
void wf_info_subscriber_release(wfInfo* info, wfPeerContext* context)
{
	DWORD dRes;

	WaitForSingleObject(info->mutex, INFINITE); 
	if (context && (info->subscribers == 1))
	{
		--info->subscribers;
		//only the last peer needs to call this
		wf_mirror_cleanup(context->wfInfo);
		wf_disp_device_set_attatch(context->wfInfo, 0);
		wf_update_mirror_drv(context->wfInfo, 1);

		stream_free(context->s);
		rfx_context_free(context->rfx_context);

		printf("Stop encoder\n");
		dRes = WaitForSingleObject(info->encodeMutex, INFINITE);

		switch (dRes) 
        {
            // The thread got ownership of the mutex
            case WAIT_OBJECT_0: 
				printf("Thread %d locked encodeMutex...\n", GetCurrentThreadId());
                break; 

            // The thread got ownership of an abandoned mutex
            // The database is in an indeterminate state
            default: 
                printf("Something else happened!!! dRes = %d\n", dRes); 
        }
    }	
	else
	{
		--info->subscribers;
	}

	ReleaseMutex(info->mutex);

	/***************
	Note: if we released the last subscriber,
	block the encoder until next subscriber
	***************/
	
}


BOOL wf_info_has_subscribers(wfInfo* info)
{
	int subs;

	WaitForSingleObject(info->mutex, INFINITE); 
	subs = info->subscribers;
	ReleaseMutex(info->mutex);

	if(info->subscribers > 0)
		return true;
	return false;
}


BOOL wf_info_have_updates(wfInfo* info)
{
	BOOL ret;
	ret = true;
	WaitForSingleObject(info->mutex, INFINITE); 
	if(info->nextUpdate == info->lastUpdate)
		ret = false;
	ReleaseMutex(info->mutex);

	return ret;
}


void wf_info_updated(wfInfo* info)
{

	WaitForSingleObject(info->mutex, INFINITE); 
	info->lastUpdate = info->nextUpdate;
	ReleaseMutex(info->mutex);
}


void wf_info_update_changes(wfInfo* info)
{
	GETCHANGESBUF* buf;

	WaitForSingleObject(info->mutex, INFINITE); 
	buf = (GETCHANGESBUF*)info->changeBuffer;
	info->nextUpdate = buf->buffer->counter;
	ReleaseMutex(info->mutex);
}


void wf_info_find_invalid_region(wfInfo* info)
{
	int i;
	GETCHANGESBUF* buf;

	WaitForSingleObject(info->mutex, INFINITE); 
	buf = (GETCHANGESBUF*)info->changeBuffer;
	for(i = info->lastUpdate; i != info->nextUpdate; i = (i+1) % MAXCHANGES_BUF )
	{
		info->invalid_x1 = min(info->invalid_x1, buf->buffer->pointrect[i].rect.left);
		info->invalid_x2 = max(info->invalid_x2, buf->buffer->pointrect[i].rect.right);
		info->invalid_y1 = min(info->invalid_y1, buf->buffer->pointrect[i].rect.top);
		info->invalid_y2 = max(info->invalid_y2, buf->buffer->pointrect[i].rect.bottom);
	}
	ReleaseMutex(info->mutex);
}


void wf_info_clear_invalid_region(wfInfo* info)
{
	
	WaitForSingleObject(info->mutex, INFINITE); 
	info->lastUpdate = info->nextUpdate;
	info->invalid_x1 = info->width;
	info->invalid_x2 = 0;
	info->invalid_y1 = info->height;
	info->invalid_y2 = 0;
	ReleaseMutex(info->mutex);
}

BOOL wf_info_have_invalid_region(wfInfo* info)
{
	if((info->invalid_x1 >= info->invalid_x2) || (info->invalid_y1 >= info->invalid_y2))
		return false;
	return true;
}