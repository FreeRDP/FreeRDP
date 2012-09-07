/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/windows.h>

#include <freerdp/freerdp.h>
#include <freerdp/listener.h>

#include "wf_peer.h"
#include "wf_mirage.h"

#include "wf_update.h"

DWORD WINAPI wf_update_thread(LPVOID lpParam)
{
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
			if (wfi->peerCount > 0)
			{
				wf_info_update_changes(wfi);

				if (wf_info_have_updates(wfi))
				{
					wf_update_encode(wfi);
					SetEvent(wfi->updateEvent);
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

	return 0;
}

void wf_update_encode(wfInfo* wfi)
{
	long offset;
	RFX_RECT rect;
	long height, width;
	GETCHANGESBUF* changes;
	SURFACE_BITS_COMMAND* cmd;

	wf_info_find_invalid_region(wfi);

	cmd = &wfi->cmd;
	changes = (GETCHANGESBUF*) wfi->changeBuffer;

	width = (wfi->invalid.right - wfi->invalid.left) + 1;
	height = (wfi->invalid.bottom - wfi->invalid.top) + 1;

	stream_clear(wfi->s);
	stream_set_pos(wfi->s, 0);

	rect.x = 0;
	rect.y = 0;
	rect.width = (uint16) width;
	rect.height = (uint16) height;

	offset = (4 * wfi->invalid.left) + (wfi->invalid.top * wfi->width * 4);

	rfx_compose_message(wfi->rfx_context, wfi->s, &rect, 1,
			((uint8*) (changes->Userbuffer)) + offset, width, height, wfi->width * 4);

	cmd->destLeft = wfi->invalid.left;
	cmd->destTop = wfi->invalid.top;
	cmd->destRight = wfi->invalid.left + width;
	cmd->destBottom = wfi->invalid.top + height;

	cmd->bpp = 32;
	cmd->codecID = 3;
	cmd->width = width;
	cmd->height = height;
	cmd->bitmapDataLength = stream_get_length(wfi->s);
	cmd->bitmapData = stream_get_head(wfi->s);

	wfi->updatePending = TRUE;
}

void wf_update_send(wfInfo* wfi)
{
	if (wf_info_lock(wfi) > 0)
	{
		if (wfi->updatePending)
		{
			int index;
			freerdp_peer* client;

			for (index = 0; index < wfi->peerCount; index++)
			{
				client = ((rdpContext*) wfi->peers[index])->peer;
				wfi->cmd.codecID = client->settings->rfx_codec_id;
				client->update->SurfaceBits(client->update->context, &wfi->cmd);
			}

			wfi->lastUpdate = wfi->nextUpdate;
			wfi->updatePending = FALSE;
		}

		wf_info_unlock(wfi);
	}
}
