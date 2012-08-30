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

	wfInfoSingleton = wf_info_init(wfInfoSingleton);
	wf_info_mirror_init(wfInfoSingleton, context);
}

void wf_peer_context_free(freerdp_peer* client, wfPeerContext* context)
{
	wf_info_subscriber_release(wfInfoSingleton, context);
}

static DWORD WINAPI wf_peer_mirror_monitor(LPVOID lpParam)
{
	DWORD fps;
	DWORD beg, end;
	DWORD diff, rate;
	freerdp_peer* client;

	fps = 24;
	rate = 1000 / fps;
	client = (freerdp_peer*) lpParam;
	
	while (1)
	{
		beg = GetTickCount();

		wf_info_lock(INFINITE);

		if (wf_info_has_subscribers(wfInfoSingleton))
		{

			wf_info_update_changes(wfInfoSingleton);
			if (wf_info_have_updates(wfInfoSingleton))
			{
				wf_rfx_encode(client);
			}
		}
		else
		{
			wf_info_unlock();
		}

		wf_info_unlock();

		end = GetTickCount();
		diff = end - beg;

		if (diff < rate)
		{
			Sleep(rate - diff);
		}
	}
	

	_tprintf(_T("monitor thread terminating...\n"));
	wf_info_set_thread_count(wfInfoSingleton, wf_info_get_thread_count(wfInfoSingleton) - 1);

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

#ifdef ROFLBUFFER
	uint16 i;
	int delta;
	int scanline;
	BYTE* srcp;
	BYTE* dstp;
#endif

	if(client->activated == FALSE)
		return;
	wfp = (wfPeerContext*) client->context;

	dRes = WaitForSingleObject(wfInfoSingleton->encodeMutex, INFINITE);

	switch (dRes)
	{

		case WAIT_ABANDONED:

			printf("\n\nwf_rfx_encode: Got ownership of abandoned mutex... resuming...\n");			
			//no break

		case WAIT_OBJECT_0:

			wf_info_find_invalid_region(wfInfoSingleton);

			if( (wfp->activated == false) ||
				(wf_info_has_subscribers(wfInfoSingleton) == false) ||
				!wf_info_have_invalid_region(wfInfoSingleton) ||
				(wfInfoSingleton->enc_data == true) )
			{
				ReleaseMutex(wfInfoSingleton->encodeMutex);
				break;
			}

			update = client->update;
			cmd = &update->surface_bits_command;
			wfi = wfp->wfInfo;
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

			//printf("Encoding: left:%d top:%d right:%d bottom:%d width:%d height:%d\n",
			//	wfi->invalid_x1, wfi->invalid_y1, wfi->invalid_x2, wfi->invalid_y2, width, height);

			
#ifndef ROFLBUFFER
			offset = (4 * wfi->invalid_x1) + (wfi->invalid_y1 * wfi->width * 4);

			
			rfx_compose_message(wfp->rfx_context, s, &rect, 1,
					((uint8*) (buf->Userbuffer)) + offset, width, height, wfi->width * 4);
#else			
			
			//memcpy(wfi->roflbuffer, ((uint8*) (buf->Userbuffer)) + offset, 4 * width * height);

			//to copy the region we must copy HxWxB bytes per line and skip 4*(screen_w - x2)


			/*delta = 0;
			for(i = 0; i < height; ++i)
			{
				memcpy(wfi->roflbuffer + offset + delta, ((uint8*) (buf->Userbuffer)) + offset + delta, 4 * width);
				delta += (4 * width) + (4 * (wfi->width - wfi->invalid_x2) + (4 * wfi->invalid_x1));
			}*/
			
			scanline = (wfi->width * 4);
			offset = (wfi->invalid_y1 * scanline) + (wfi->invalid_x1 * 4);
			srcp = (BYTE*) buf->Userbuffer + offset;
			dstp = (BYTE*) wfi->roflbuffer + offset;
			Sleep(100);
			for (i = 0; i < height; i++)
			{
				memcpy(dstp, srcp, width * 4);
				dstp += scanline;
				srcp += scanline;
			}
			
			rfx_compose_message(wfp->rfx_context, s, &rect, 1,
				wfi->roflbuffer + offset, width, height, wfi->width * 4);

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
			ReleaseMutex(wfInfoSingleton->encodeMutex);
			break;

		case WAIT_TIMEOUT:

			ReleaseMutex(wfInfoSingleton->encodeMutex);
			break;

		default:
			printf("\n\nwf_rfx_encode: Something else happened!!! dRes = %d\n", dRes);
			break;
	}

}

void wf_peer_init(freerdp_peer* client)
{
	client->context_size = sizeof(wfPeerContext);
	client->ContextNew = (psPeerContextNew) wf_peer_context_new;
	client->ContextFree = (psPeerContextFree) wf_peer_context_free;
	freerdp_peer_context_new(client);

	if (!wf_info_get_thread_count(wfInfoSingleton))
	{
		_tprintf(_T("Trying to create a monitor thread...\n"));

		if (CreateThread(NULL, 0, wf_peer_mirror_monitor, client, 0, NULL) != 0)
			_tprintf(_T("Created!\n"));

		wf_info_set_thread_count(wfInfoSingleton, wf_info_get_thread_count(wfInfoSingleton) + 1 );
	}
}

boolean wf_peer_post_connect(freerdp_peer* client)
{
	wfPeerContext* context = (wfPeerContext*) client->context;

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

	printf("But we will try resizing to %dx%d\n",
		wf_info_get_width(wfInfoSingleton),
		wf_info_get_height(wfInfoSingleton));

	client->settings->width = wf_info_get_width(wfInfoSingleton);
	client->settings->height = wf_info_get_height(wfInfoSingleton);

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

void wf_peer_send_changes(rdpUpdate* update)
{
	int dRes;

	/* are we currently encoding? */
	dRes = WaitForSingleObject(wfInfoSingleton->encodeMutex, 0);

	switch(dRes)
	{
		case WAIT_ABANDONED:

			printf("\n\nwf_peer_send_changes: Got ownership of abandoned mutex... resuming...\n");
			//no break;

		case WAIT_OBJECT_0:

			/* are there changes to send? */

			if (	((wf_info_lock(0) != 0)) ||
				!wf_info_have_updates(wfInfoSingleton) ||
				!wf_info_have_invalid_region(wfInfoSingleton) ||
				(wfInfoSingleton->enc_data == FALSE))
			{
				//we dont send
				wf_info_unlock();
				ReleaseMutex(wfInfoSingleton->encodeMutex);
				break;
			}

			wf_info_updated(wfInfoSingleton);

			update->SurfaceBits(update->context, &update->surface_bits_command);

			wfInfoSingleton->enc_data = FALSE;
			wf_info_unlock();
			ReleaseMutex(wfInfoSingleton->encodeMutex);
			break;

		case WAIT_TIMEOUT:
			break;

		default:
			printf("wf_peer_send_changes: Something else happened!!! dRes = %d\n", dRes);
			break;
	}
}
