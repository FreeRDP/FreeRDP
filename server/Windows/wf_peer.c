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

#include "wf_input.h"
#include "wf_mirage.h"
#include "wf_dxgi.h"

#include "wf_peer.h"

BOOL win8;



void wf_peer_context_new(freerdp_peer* client, wfPeerContext* context)
{
	context->info = wf_info_get_instance();

	if(win8)
	{
		wf_info_get_screen_info(context->info);
		wf_dxgi_init(context->info);
	}
	else
	{
		wf_info_mirror_init(context->info, context);
	}
}

void wf_peer_context_free(freerdp_peer* client, wfPeerContext* context)
{
	if(win8)
	{
		wf_dxgi_cleanup(context->info);
	}
	else
	{
		wf_info_subscriber_release(context->info, context);
	}
}


static DWORD WINAPI wf_peer_mirror_monitor(LPVOID lpParam)
{
	DWORD fps;
	wfInfo* wfi;
	DWORD beg, end;
	DWORD diff, rate;
	freerdp_peer* client;
	wfPeerContext* context;

	fps = 1;
	rate = 1000 / fps;
	client = (freerdp_peer*) lpParam;
	context = (wfPeerContext*) client->context;
	wfi = context->info;
	
	while (1)
	{
		beg = GetTickCount();

		if (wf_info_lock(wfi) > 0)
		{
			if (wf_info_has_subscribers(wfi))
			{
				wf_info_update_changes(wfi);

				if (wf_info_have_updates(wfi))
				{
					//todo: changeme
					if(win8)
						wf_dxgi_encode(client);
					wf_rfx_encode(client);
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
	
	wf_info_lock(wfi);
	wfi->threadCount--;
	wf_info_unlock(wfi);

	return 0;
}

void wf_dxgi_encode(freerdp_peer* client)
{
	STREAM* s;
	wfInfo* wfi;
	long offset;
	RFX_RECT rect;
	long height, width;
	rdpUpdate* update;
	wfPeerContext* wfp;
	GETCHANGESBUF* buf;
	SURFACE_BITS_COMMAND* cmd;

	///
	
	BYTE* MetaDataBuffer = NULL;
	UINT MeinMetaDataSize = 0;
	UINT BufSize;
	UINT i;
	HRESULT hr;
	IDXGIResource* DesktopResource = NULL;
    DXGI_OUTDUPL_FRAME_INFO FrameInfo;

	BYTE* DirtyRects;

	UINT dirty;
	RECT* pRect;
	///

	wfp = (wfPeerContext*) client->context;
	wfi = wfp->info;

	if (client->activated == FALSE)
		return;
	
	if (wfp->activated == FALSE)
		return;

	//wf_info_find_invalid_region(wfi);

	_tprintf(_T("\nTrying to acquire a frame...\n"));
	hr = wfi->dxgi.DeskDupl->lpVtbl->AcquireNextFrame(wfi->dxgi.DeskDupl, 800, &FrameInfo, &DesktopResource);
	_tprintf(_T("hr = %#0X\n"), hr);
	if (hr == DXGI_ERROR_WAIT_TIMEOUT)
	{
		_tprintf(_T("Timeout\n"));
		return;
	}

	if (FAILED(hr))
	{
		_tprintf(_T("Failed to acquire next frame\n"));
		return;
	}
	_tprintf(_T("Gut!\n"));

	///////////////////////////////////////////////

		
	_tprintf(_T("Trying to QI for ID3D11Texture2D...\n"));
	hr = DesktopResource->lpVtbl->QueryInterface(DesktopResource, &IID_ID3D11Texture2D, (void**) &wfi->dxgi.AcquiredDesktopImage);
	DesktopResource->lpVtbl->Release(DesktopResource);
	DesktopResource = NULL;
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to QI for ID3D11Texture2D from acquired IDXGIResource\n"));
			return;
	}
	_tprintf(_T("Gut!\n"));

	_tprintf(_T("FrameInfo\n"));
	_tprintf(_T("\tAccumulated Frames: %d\n"), FrameInfo.AccumulatedFrames);
	_tprintf(_T("\tCoalesced Rectangles: %d\n"), FrameInfo.RectsCoalesced);
	_tprintf(_T("\tMetadata buffer size: %d\n"), FrameInfo.TotalMetadataBufferSize);


	if(FrameInfo.TotalMetadataBufferSize)
	{

		if (FrameInfo.TotalMetadataBufferSize > MeinMetaDataSize)
		{
			if (MetaDataBuffer)
			{
				free(MetaDataBuffer);
				MetaDataBuffer = NULL;
			}
			MetaDataBuffer = (BYTE*) malloc(FrameInfo.TotalMetadataBufferSize);
			if (!MetaDataBuffer)
			{
				MeinMetaDataSize = 0;
				_tprintf(_T("Failed to allocate memory for metadata\n"));
				return;
			}
			MeinMetaDataSize = FrameInfo.TotalMetadataBufferSize;
		}

		BufSize = FrameInfo.TotalMetadataBufferSize;

		// Get move rectangles
		hr = wfi->dxgi.DeskDupl->lpVtbl->GetFrameMoveRects(wfi->dxgi.DeskDupl, BufSize, (DXGI_OUTDUPL_MOVE_RECT*) MetaDataBuffer, &BufSize);
		if (FAILED(hr))
		{
			_tprintf(_T("Failed to get frame move rects\n"));
			return;
		}
		_tprintf(_T("Move rects: %d\n"), BufSize / sizeof(DXGI_OUTDUPL_MOVE_RECT));

		DirtyRects = MetaDataBuffer + BufSize;
		BufSize = FrameInfo.TotalMetadataBufferSize - BufSize;

			// Get dirty rectangles
		hr = wfi->dxgi.DeskDupl->lpVtbl->GetFrameDirtyRects(wfi->dxgi.DeskDupl, BufSize, (RECT*) DirtyRects, &BufSize);
		if (FAILED(hr))
		{
			_tprintf(_T("Failed to get frame dirty rects\n"));
			return;
		}
		dirty = BufSize / sizeof(RECT);
		_tprintf(_T("Dirty rects: %d\n"), dirty);

		pRect = (RECT*) DirtyRects;
		for(i = 0; i<dirty; ++i)
		{
			_tprintf(_T("\tRect: (%d, %d), (%d, %d)\n"),
				pRect->left,
				pRect->top,
				pRect->right,
				pRect->bottom);

			++pRect;
		}
			

	}


	hr = wfi->dxgi.DeskDupl->lpVtbl->ReleaseFrame(wfi->dxgi.DeskDupl);
	if (FAILED(hr))
	{
		_tprintf(_T("Failed to release frame\n"));
		return;
	}


	////////////////////////
	////////////////////////

	update = client->update;
	cmd = &update->surface_bits_command;
	buf = (GETCHANGESBUF*) wfi->changeBuffer;

	width = (wfi->invalid.right - wfi->invalid.left) + 1;
	height = (wfi->invalid.bottom - wfi->invalid.top) + 1;

	stream_clear(wfi->s);
	stream_set_pos(wfi->s, 0);
	s = wfi->s;

	rect.x = 0;
	rect.y = 0;
	rect.width = (uint16) width;
	rect.height = (uint16) height;

	offset = (4 * wfi->invalid.left) + (wfi->invalid.top * wfi->width * 4);

	rfx_compose_message(wfi->rfx_context, s, &rect, 1,
			((uint8*) (buf->Userbuffer)) + offset, width, height, wfi->width * 4);

	cmd->destLeft = wfi->invalid.left;
	cmd->destTop = wfi->invalid.top;
	cmd->destRight = wfi->invalid.left + width;
	cmd->destBottom = wfi->invalid.top + height;

	cmd->bpp = 32;
	cmd->codecID = client->settings->rfx_codec_id;
	cmd->width = width;
	cmd->height = height;
	cmd->bitmapDataLength = stream_get_length(s);
	cmd->bitmapData = stream_get_head(s);

	wfi->updatePending = TRUE;
}


void wf_rfx_encode(freerdp_peer* client)
{
	STREAM* s;
	wfInfo* wfi;
	long offset;
	RFX_RECT rect;
	long height, width;
	rdpUpdate* update;
	wfPeerContext* wfp;
	GETCHANGESBUF* buf;
	SURFACE_BITS_COMMAND* cmd;

	wfp = (wfPeerContext*) client->context;
	wfi = wfp->info;

	if (client->activated == FALSE)
		return;
	
	if (wfp->activated == FALSE)
		return;

	wf_info_find_invalid_region(wfi);

	update = client->update;
	cmd = &update->surface_bits_command;
	buf = (GETCHANGESBUF*) wfi->changeBuffer;

	width = (wfi->invalid.right - wfi->invalid.left) + 1;
	height = (wfi->invalid.bottom - wfi->invalid.top) + 1;

	stream_clear(wfi->s);
	stream_set_pos(wfi->s, 0);
	s = wfi->s;

	rect.x = 0;
	rect.y = 0;
	rect.width = (uint16) width;
	rect.height = (uint16) height;

	offset = (4 * wfi->invalid.left) + (wfi->invalid.top * wfi->width * 4);

	rfx_compose_message(wfi->rfx_context, s, &rect, 1,
			((uint8*) (buf->Userbuffer)) + offset, width, height, wfi->width * 4);

	cmd->destLeft = wfi->invalid.left;
	cmd->destTop = wfi->invalid.top;
	cmd->destRight = wfi->invalid.left + width;
	cmd->destBottom = wfi->invalid.top + height;

	cmd->bpp = 32;
	cmd->codecID = client->settings->rfx_codec_id;
	cmd->width = width;
	cmd->height = height;
	cmd->bitmapDataLength = stream_get_length(s);
	cmd->bitmapData = stream_get_head(s);

	wfi->updatePending = TRUE;
}

void wf_peer_init(freerdp_peer* client)
{
	wfInfo* wfi;

	client->context_size = sizeof(wfPeerContext);
	client->ContextNew = (psPeerContextNew) wf_peer_context_new;
	client->ContextFree = (psPeerContextFree) wf_peer_context_free;
	
	freerdp_peer_context_new(client);

	wfi = ((wfPeerContext*) client->context)->info;

	if(win8)
	{
	}
	else
	{
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
	}
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
	wfInfo* wfi;
	wfPeerContext* context = (wfPeerContext*) client->context;

	wfi = context->info;

	if (wf_info_lock(wfi) > 0)
	{
		if (wfi->updatePending)
		{
			wfi->lastUpdate = wfi->nextUpdate;

			client->update->SurfaceBits(client->update->context, &client->update->surface_bits_command);

			wfi->updatePending = FALSE;
		}

		wf_info_unlock(wfi);
	}
}

void wf_detect_win_ver()
{
	OSVERSIONINFOEX osvi;
	SYSTEM_INFO si;
	BOOL bOsVersionInfoEx;

	ZeroMemory(&si, sizeof(SYSTEM_INFO));
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));

	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO*) &osvi);

	if(bOsVersionInfoEx == 0 ) return;

	if ( VER_PLATFORM_WIN32_NT==osvi.dwPlatformId && 
        osvi.dwMajorVersion > 4 )
	{
		if ( osvi.dwMajorVersion == 6 && 
			osvi.dwMinorVersion == 2)
			win8 = TRUE;
		printf("Detected Windows 8\n");
		return;
	}

	printf("platform = %d\n Version = %d.%d\n", osvi.dwPlatformId, osvi.dwMajorVersion, osvi.dwMinorVersion);
	win8 = FALSE;
}

	