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

#include <winpr/tchar.h>
#include <winpr/windows.h>

#include <freerdp/listener.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/utils/stream.h>

#include "wf_mirage.h"

#include "wf_peer.h"

void wf_peer_context_new(freerdp_peer* client, wfPeerContext* context)
{
#ifndef WITH_WIN8
	context->info = wf_info_get_instance();
	wf_info_mirror_init(context->info, context);
#endif
}

void wf_peer_context_free(freerdp_peer* client, wfPeerContext* context)
{
#ifndef WITH_WIN8
	wf_info_subscriber_release(context->info, context);
#endif
}

static DWORD WINAPI wf_peer_mirror_monitor(LPVOID lpParam)
{
	DWORD fps;
	wfInfo* wfi;
	DWORD beg, end;
	DWORD diff, rate;
	freerdp_peer* client;
	wfPeerContext* context;

	fps = 24;
	rate = 1000 / fps;
	client = (freerdp_peer*) lpParam;
	context = (wfPeerContext*) client->context;
	wfi = context->info;
	
	while (1)
	{
		beg = GetTickCount();

		wf_info_lock(wfi);

		if (wf_info_has_subscribers(wfi))
		{
			wf_info_update_changes(wfi);

			if (wf_info_have_updates(wfi))
				wf_rfx_encode(client);
		}

		wf_info_unlock(wfi);

		end = GetTickCount();
		diff = end - beg;

		if (diff < rate)
		{
			Sleep(rate - diff);
		}
	}
	
	wf_info_lock(wfi);
	wfi->threadCount--;
	wf_info_unlock(wfi);

	return 0;
}

void wf_rfx_encode(freerdp_peer* client)
{
	int dRes;
	STREAM* s;
	wfInfo* wfi;
	long offset;
	RFX_RECT rect;
	long height, width;
	rdpUpdate* update;
	wfPeerContext* wfp;
	GETCHANGESBUF* buf;
	SURFACE_BITS_COMMAND* cmd;

#ifdef WITH_DOUBLE_BUFFERING
	uint16 i;
	int scanline;
	BYTE* srcp;
	BYTE* dstp;
#endif

	if (client->activated == FALSE)
		return;
	
	wfp = (wfPeerContext*) client->context;
	wfi = wfp->info;

	dRes = WaitForSingleObject(wfi->encodeMutex, INFINITE);

	switch (dRes)
	{
		case WAIT_ABANDONED:
		case WAIT_OBJECT_0:

			wf_info_find_invalid_region(wfi);

			if ((wfp->activated == false) ||
				(wf_info_has_subscribers(wfi) == false) ||
				!wf_info_have_invalid_region(wfi) ||
				(wfi->enc_data == true) )
			{
				ReleaseMutex(wfi->encodeMutex);
				break;
			}

			update = client->update;
			cmd = &update->surface_bits_command;
			buf = (GETCHANGESBUF*) wfi->changeBuffer;

			width = (wfi->invalid_x2 - wfi->invalid_x1) + 1;
			height = (wfi->invalid_y2 - wfi->invalid_y1) + 1;

			stream_clear(wfp->s);
			stream_set_pos(wfp->s, 0);
			s = wfp->s;
		
			rect.x = 0;
			rect.y = 0;
			rect.width = (uint16) width;
			rect.height = (uint16) height;
			
#ifndef WITH_DOUBLE_BUFFERING
			offset = (4 * wfi->invalid_x1) + (wfi->invalid_y1 * wfi->width * 4);

			
			rfx_compose_message(wfp->rfx_context, s, &rect, 1,
					((uint8*) (buf->Userbuffer)) + offset, width, height, wfi->width * 4);
#else			
			scanline = (wfi->width * 4);
			offset = (wfi->invalid_y1 * scanline) + (wfi->invalid_x1 * 4);
			srcp = (BYTE*) buf->Userbuffer + offset;
			dstp = (BYTE*) wfi->primary_buffer + offset;

			for (i = 0; i < height; i++)
			{
				memcpy(dstp, srcp, width * 4);
				dstp += scanline;
				srcp += scanline;
			}
			
			rfx_compose_message(wfp->rfx_context, s, &rect, 1,
				wfi->primary_buffer + offset, width, height, wfi->width * 4);
#endif

			cmd->destLeft = wfi->invalid_x1;
			cmd->destTop = wfi->invalid_y1;
			cmd->destRight = wfi->invalid_x1 + width;
			cmd->destBottom = wfi->invalid_y1 + height;

			cmd->bpp = 32;
			cmd->codecID = client->settings->rfx_codec_id;
			cmd->width = width;
			cmd->height = height;
			cmd->bitmapDataLength = stream_get_length(s);
			cmd->bitmapData = stream_get_head(s);

			wfi->enc_data = true;
			ReleaseMutex(wfi->encodeMutex);
			break;

		case WAIT_TIMEOUT:

			ReleaseMutex(wfi->encodeMutex);
			break;

		default:
			break;
	}

}

void wf_peer_init(freerdp_peer* client)
{
	wfInfo* wfi;

	client->context_size = sizeof(wfPeerContext);
	client->ContextNew = (psPeerContextNew) wf_peer_context_new;
	client->ContextFree = (psPeerContextFree) wf_peer_context_free;
	
	freerdp_peer_context_new(client);

	wfi = ((wfPeerContext*) client->context)->info;

#ifndef WITH_WIN8
	wf_info_lock(wfi);

	if (wfi->threadCount < 1)
	{
		if (CreateThread(NULL, 0, wf_peer_mirror_monitor, client, 0, NULL) != 0)
		{
			wfi->threadCount++;
			printf("started monitor thread\n");
		}
		else
		{
			_tprintf(_T("failed to create monitor thread\n"));
		}
	}

	wf_info_unlock(wfi);
#endif
}

boolean wf_peer_post_connect(freerdp_peer* client)
{
	wfInfo* wfi;
	wfPeerContext* context = (wfPeerContext*) client->context;

	wfi = context->info;

	/**
	 * This callback is called when the entire connection sequence is done, i.e. we've received the
	 * Font List PDU from the client and sent out the Font Map PDU.
	 * The server may start sending graphics output and receiving keyboard/mouse input after this
	 * callback returns.
	 */

	printf("Client %s is activated (osMajorType %d osMinorType %d)", client->local ? "(local)" : client->hostname,
		client->settings->os_major_type, client->settings->os_minor_type);

	if (client->settings->autologon)
	{
		printf(" and wants to login automatically as %s\\%s",
			client->settings->domain ? client->settings->domain : "",
			client->settings->username);

		/* A real server may perform OS login here if NLA is not executed previously. */
	}
	printf("\n");

	printf("Client requested desktop: %dx%dx%d\n",
		client->settings->width, client->settings->height, client->settings->color_depth);

	printf("But we will try resizing to %dx%d\n", wfi->width, wfi->height);

	client->settings->width = wfi->width;
	client->settings->height = wfi->height;

	client->update->DesktopResize(client->update->context);

	return true;
}

boolean wf_peer_activate(freerdp_peer* client)
{
	wfPeerContext* context = (wfPeerContext*) client->context;

	context->activated = true;

	return true;
}

void wf_peer_synchronize_event(rdpInput* input, uint32 flags)
{

}

void wf_peer_send_changes(freerdp_peer* client)
{
	int dRes;
	wfInfo* wfi;
	wfPeerContext* context = (wfPeerContext*) client->context;

	wfi = context->info;

	/* are we currently encoding? */
	dRes = WaitForSingleObject(wfi->encodeMutex, 0);

	switch (dRes)
	{
		case WAIT_ABANDONED:
		case WAIT_OBJECT_0:

			if (wf_info_try_lock(wfi, 100) != -1)
			{
				if ((!wf_info_have_updates(wfi) || !wf_info_have_invalid_region(wfi) || (wfi->enc_data == FALSE)))
				{
					wf_info_unlock(wfi);
					ReleaseMutex(wfi->encodeMutex);
					break;
				}

				wf_info_updated(wfi);

				client->update->SurfaceBits(client->update->context, &client->update->surface_bits_command);

				wfi->enc_data = FALSE;
				wf_info_unlock(wfi);
				ReleaseMutex(wfi->encodeMutex);
			}
			break;

		case WAIT_TIMEOUT:
			break;

		default:
			break;
	}
}
