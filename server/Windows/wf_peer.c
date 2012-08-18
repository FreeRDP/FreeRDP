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
#include <winpr/tchar.h>
#include <winpr/windows.h>
#include <freerdp/listener.h>
#include <freerdp/utils/sleep.h>
#include <freerdp/codec/rfx.h>
#include <freerdp/utils/stream.h>

#include "wf_mirage.h"

#include "wf_peer.h"

//extern wfInfo * wfInfoSingleton;

void wf_peer_context_new(freerdp_peer* client, wfPeerContext* context)
{
	DWORD dRes;
	wfInfoSingleton = wf_info_init(wfInfoSingleton);
	wf_info_mirror_init(wfInfoSingleton, context);
}

void wf_peer_context_free(freerdp_peer* client, wfPeerContext* context)
{
	wf_info_subscriber_release(wfInfoSingleton, context);
}

static DWORD WINAPI wf_peer_mirror_monitor(LPVOID lpParam)
{
	DWORD dRes;
	DWORD start, end, diff;
	DWORD rate;
	
	freerdp_peer* client;
	unsigned long i;

	rate = 1000;
	client = (freerdp_peer*)lpParam;
	
	//todo: make sure we dont encode after no clients
	while(1)
	{

		start = GetTickCount();


		if(wf_info_has_subscribers(wfInfoSingleton))
		{
			wf_info_update_changes(wfInfoSingleton);
			if(wf_info_have_updates(wfInfoSingleton))
			{
				wf_info_find_invalid_region(wfInfoSingleton);
				//printf("Fake Encode!\n");
				wf_rfx_encode(client);
			}			
		}

		end = GetTickCount();
		diff = end - start;
		if(diff < rate)
		{
			printf("sleeping for %d ms...\n", rate - diff);
			Sleep(rate - diff);
		}
		
	}

	_tprintf(_T("monitor thread terminating...\n"));
	return 0;
}

/*
void wf_peer_rfx_update(freerdp_peer* client, int x, int y, int width, int height)
{
	STREAM* s;
	wfInfo* wfi;
	RFX_RECT rect;
	rdpUpdate* update;
	wfPeerContext* wfp;
	SURFACE_BITS_COMMAND* cmd;
	GETCHANGESBUF* buf;
	
	update = client->update;
	wfp = (wfPeerContext*) client->context;
	cmd = &update->surface_bits_command;
	wfi = wfp->wfInfo;
	buf = (GETCHANGESBUF*)wfi->changeBuffer;

	if( (wfp->activated == false) ||
		( (wfi->nextUpdate - wfi->lastUpdate) == 0) )
		return;

	printf("encode %d\n", wfi->nextUpdate - wfi->lastUpdate);
	printf("\tinvlaid region = (%d, %d), (%d, %d)\n", wfi->invalid_x1, wfi->invalid_y1, wfi->invalid_x2, wfi->invalid_y2);
	wfi->lastUpdate = wfi->nextUpdate;

	if ( (wfi->invalid_x1 >= wfi->invalid_x2) ||
		(wfi->invalid_y1 >= wfi->invalid_y2) )
		return;

	stream_clear(wfp->s);
	stream_set_pos(wfp->s, 0);
	s = wfp->s;
		
	rect.x = wfi->invalid_x1;
	rect.y = wfi->invalid_y1;
	rect.width = wfi->invalid_x2;
	rect.height = wfi->invalid_y2;

	wfi->invalid_x1 = wfi->width;
	wfi->invalid_x2 = 0;
	wfi->invalid_y1 = wfi->height;
	wfi->invalid_y2 = 0;

	rfx_compose_message(wfp->rfx_context, s, &rect, 1,
			(uint8*) buf->Userbuffer, width, height, width * 4);

	cmd->destLeft = x;
	cmd->destTop = y;
	cmd->destRight = x + width;
	cmd->destBottom = y + height;

	cmd->bpp = 32;
	cmd->codecID = client->settings->rfx_codec_id;
	cmd->width = width;
	cmd->height = height;
	cmd->bitmapDataLength = stream_get_length(s);
	cmd->bitmapData = stream_get_head(s);

	update->SurfaceBits(update->context, cmd);
}
*/

void wf_rfx_encode(freerdp_peer* client)
{
	STREAM* s;
	wfInfo* wfi;
	RFX_RECT rect;
	rdpUpdate* update;
	wfPeerContext* wfp;
	SURFACE_BITS_COMMAND* cmd;
	GETCHANGESBUF* buf;
	long height, width;
	int dRes;
	
	update = client->update;
	wfp = (wfPeerContext*) client->context;
	cmd = &update->surface_bits_command;
	wfi = wfp->wfInfo;
	buf = (GETCHANGESBUF*)wfi->changeBuffer;

	
	if( (wfp->activated == false) )//||		( (wfi->nextUpdate - wfi->lastUpdate) == 0) )
		return;

	if ( (wfi->invalid_x1 >= wfi->invalid_x2) || (wfi->invalid_y1 >= wfi->invalid_y2) )
			return;

	dRes = WaitForSingleObject(wfInfoSingleton->encodeMutex, INFINITE);
	switch(dRes)
	{
	case WAIT_OBJECT_0:

		printf("encode %d\n", wfi->nextUpdate - wfi->lastUpdate);
		printf("\tinvlaid region = (%d, %d), (%d, %d)\n", wfi->invalid_x1, wfi->invalid_y1, wfi->invalid_x2, wfi->invalid_y2);
	

		wfi->lastUpdate = wfi->nextUpdate;

		width = wfi->invalid_x2 - wfi->invalid_x1;
		height = wfi->invalid_y2 - wfi->invalid_y1;

		stream_clear(wfp->s);
		stream_set_pos(wfp->s, 0);
		s = wfp->s;
		
		rect.x = wfi->invalid_x1;
		rect.y = wfi->invalid_y1;
		rect.width = width;
		rect.height = width;


		rfx_compose_message(wfp->rfx_context, s, &rect, 1,
				(uint8*) buf->Userbuffer, width, height, width * 4);

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
		break;

	default: 
        printf("\n\nSomething else happened!!! dRes = %d\n", dRes);
	}

}

void wf_peer_init(freerdp_peer* client)
{
	DWORD dRes;

	client->context_size = sizeof(wfPeerContext);
	client->ContextNew = (psPeerContextNew) wf_peer_context_new;
	client->ContextFree = (psPeerContextFree) wf_peer_context_free;
	freerdp_peer_context_new(client);

	_tprintf(_T("Trying to create a monitor thread...\n"));

	if (CreateThread(NULL, 0, wf_peer_mirror_monitor, client, 0, NULL) != 0)
		_tprintf(_T("Created!\n"));

	/*
	if(wfInfoSingleton)
	{
		dRes = WaitForSingleObject(
			wfInfoSingleton->mutex,    // handle to mutex
            INFINITE);  // no time-out interval
		switch(dRes)
		{
			case WAIT_OBJECT_0:
				if(wfInfoSingleton->threadCnt == 0)
				{
					if (CreateThread(NULL, 0, wf_peer_mirror_monitor, client, 0, NULL) != 0)
						_tprintf(_T("Created!\n"));

					++wfInfoSingleton->threadCnt;
				}

				if (! ReleaseMutex(wfInfoSingleton->mutex)) 
				{ 
					_tprintf(_T("Error releasing mutex\n"));
				} 

				break;

			default:
				_tprintf(_T("Error waiting for mutex: %d\n"), dRes);
		}
		
	}
	*/
		
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

	//are we currently encoding?
	dRes = WaitForSingleObject(wfInfoSingleton->encodeMutex, INFINITE);
	switch(dRes)
	{
	case WAIT_OBJECT_0:
		//are there changes to send?
		if(!wf_info_have_updates(wfInfoSingleton))
			return;

		wf_info_updated(wfInfoSingleton);
		printf("\tSend...\n");
		update->SurfaceBits(update->context, &update->surface_bits_command);

		break;

	default: 
        printf("Something else happened!!! dRes = %d\n", dRes);
	}

}
