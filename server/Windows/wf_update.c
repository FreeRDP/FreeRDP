/**
* FreeRDP: A Remote Desktop Protocol Client
* FreeRDP Windows Server
*
* Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>

#include <winpr/windows.h>

#include <freerdp/freerdp.h>
#include <freerdp/listener.h>

#include "wf_peer.h"
#include "wf_info.h"
#include "wf_mirage.h"

#include "wf_update.h"

DWORD WINAPI wf_update_thread(LPVOID lpParam)
{
	int index;
	int peerindex;
	DWORD fps;
	wfInfo* wfi;
	DWORD beg, end;
	DWORD diff, rate;

	wfi = (wfInfo*) lpParam;

	fps = wfi->framesPerSecond;
	rate = 1000 / fps;

	while (1)
	{
		beg = GetTickCount();

		if (wf_info_lock(wfi) > 0)
		{
			if (wfi->activePeerCount > 0)
			{
				wf_info_update_changes(wfi);

				if (wf_info_have_updates(wfi))
				{
					wf_update_encode(wfi);

					//printf("Start of parallel sending\n");
					index = 0;
					for (peerindex = 0; peerindex < wfi->peerCount; peerindex++)
					{
						for (; index < WF_INFO_MAXPEERS; index++)
						{
							if (wfi->peers[index] && wfi->peers[index]->activated)
							{
								//printf("Setting event for %d of %d\n", index + 1, wfi->activePeerCount);
								SetEvent(((wfPeerContext*) wfi->peers[index]->context)->updateEvent);
							}
						}

					}

					for (index = 0; index < wfi->activePeerCount; index++)
					{
						//printf("Waiting for %d of %d\n", index + 1, wfi->activePeerCount);
						//WaitForSingleObject(wfi->updateSemaphore, INFINITE);
						WaitForSingleObject(wfi->updateSemaphore, 1000);
					}

					//printf("End of parallel sending\n");

					wf_info_clear_invalid_region(wfi);
				}
			}

			wf_info_unlock(wfi);
		}

		end = GetTickCount();
		diff = end - beg;

		if (diff < rate)
		{
			Sleep(rate - diff);
		}
	}

	//printf("Exiting Update Thread\n");

	return 0;
}


void wf_update_encode(wfInfo* wfi)
{

	RFX_RECT rect;
	long height, width;
	BYTE* pDataBits = NULL;
	int stride;

	SURFACE_BITS_COMMAND* cmd;

	wf_info_find_invalid_region(wfi);

	cmd = &wfi->cmd;

	Stream_SetPosition(wfi->s, 0);

	wf_info_getScreenData(wfi, &width, &height, &pDataBits, &stride);

	rect.x = 0;
	rect.y = 0;
	rect.width = (UINT16) width;
	rect.height = (UINT16) height;

	//printf("x:%d y:%d w:%d h:%d\n", wfi->invalid.left, wfi->invalid.top, width, height);

	Stream_Clear(wfi->s);
	rfx_compose_message(wfi->rfx_context, wfi->s, &rect, 1,
		pDataBits, width, height, stride);

	wfi->frame_idx = wfi->rfx_context->frameIdx;

	cmd->destLeft = wfi->invalid.left;
	cmd->destTop = wfi->invalid.top;
	cmd->destRight = wfi->invalid.left + width;
	cmd->destBottom = wfi->invalid.top + height;

	cmd->bpp = 32;
	cmd->codecID = 3;
	cmd->width = width;
	cmd->height = height;
	cmd->bitmapDataLength = Stream_GetPosition(wfi->s);
	cmd->bitmapData = Stream_Buffer(wfi->s);
}

void wf_update_peer_send(wfInfo* wfi, wfPeerContext* context)
{
	freerdp_peer* client = ((rdpContext*) context)->peer;

	/* This happens when the RemoteFX encoder state is reset */

	if (wfi->frame_idx == 1)
		context->frame_idx = 0;

	/*
	* When a new client connects, it is possible that old frames from
	* from a previous encoding state remain. Those frames should be discarded
	* as they will cause an error condition in mstsc.
	*/

	if ((context->frame_idx + 1) != wfi->frame_idx)
	{
		/* This frame is meant to be discarded */

		if (context->frame_idx == 0)
			return;

		/* This is an unexpected error condition */

		printf("Unexpected Frame Index: Actual: %d Expected: %d\n",
			wfi->frame_idx, context->frame_idx + 1);
	}

	wfi->cmd.codecID = client->settings->RemoteFxCodecId;
	client->update->SurfaceBits(client->update->context, &wfi->cmd);
	context->frame_idx++;
}

void wf_update_encoder_reset(wfInfo* wfi)
{
	if (wf_info_lock(wfi) > 0)
	{
		printf("Resetting encoder\n");

		if (wfi->rfx_context)
		{
			rfx_context_reset(wfi->rfx_context);
		}
		else
		{
			wfi->rfx_context = rfx_context_new(TRUE);
			wfi->rfx_context->mode = RLGR3;
			wfi->rfx_context->width = wfi->servscreen_width;
			wfi->rfx_context->height = wfi->servscreen_height;
			rfx_context_set_pixel_format(wfi->rfx_context, RDP_PIXEL_FORMAT_B8G8R8A8);
			wfi->s = Stream_New(NULL, 0xFFFF);
		}

		wf_info_invalidate_full_screen(wfi);

		wf_info_unlock(wfi);
	}
}

void wf_update_peer_activate(wfInfo* wfi, wfPeerContext* context)
{
	if (wf_info_lock(wfi) > 0)
	{
		if (wfi->activePeerCount < 1)
		{
#ifndef WITH_WIN8
			wf_mirror_driver_activate(wfi);
#endif
			ResumeThread(wfi->updateThread);
		}

		wf_update_encoder_reset(wfi);
		wfi->activePeerCount++;

		printf("Activating Peer Updates: %d\n", wfi->activePeerCount);

		wf_info_unlock(wfi);
	}
}

void wf_update_peer_deactivate(wfInfo* wfi, wfPeerContext* context)
{
	if (wf_info_lock(wfi) > 0)
	{
		freerdp_peer* client = ((rdpContext*) context)->peer;

		if (client->activated)
		{
			if (wfi->activePeerCount <= 1)
			{
				wf_mirror_driver_deactivate(wfi);
			}

			client->activated = FALSE;
			wfi->activePeerCount--;

			printf("Deactivating Peer Updates: %d\n", wfi->activePeerCount);
		}

		wf_info_unlock(wfi);
	}
}
