/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/crt.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/sysinfo.h>

#include "shadow.h"

void shadow_client_context_new(freerdp_peer* peer, rdpShadowClient* client)
{
	rdpSettings* settings;
	rdpShadowServer* server;

	server = (rdpShadowServer*) peer->ContextExtra;
	client->server = server;

	settings = peer->settings;

	settings->ColorDepth = 32;
	settings->NSCodec = TRUE;
	settings->RemoteFxCodec = TRUE;
	settings->BitmapCacheV3Enabled = TRUE;
	settings->FrameMarkerCommandEnabled = TRUE;
	settings->SurfaceFrameMarkerEnabled = TRUE;

	settings->RdpSecurity = TRUE;
	settings->TlsSecurity = TRUE;
	settings->NlaSecurity = FALSE;

	settings->CertificateFile = _strdup(server->CertificateFile);
	settings->PrivateKeyFile = _strdup(server->PrivateKeyFile);

	settings->RdpKeyFile = _strdup(settings->PrivateKeyFile);

	client->inLobby = TRUE;
	client->mayView = server->mayView;
	client->mayInteract = server->mayInteract;

	InitializeCriticalSectionAndSpinCount(&(client->lock), 4000);

	region16_init(&(client->invalidRegion));

	client->vcm = WTSOpenServerA((LPSTR) peer->context);

	client->StopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

	client->encoder = shadow_encoder_new(server);

	ArrayList_Add(server->clients, (void*) client);
}

void shadow_client_context_free(freerdp_peer* peer, rdpShadowClient* client)
{
	rdpShadowServer* server = client->server;

	ArrayList_Remove(server->clients, (void*) client);

	DeleteCriticalSection(&(client->lock));

	region16_uninit(&(client->invalidRegion));

	WTSCloseServer((HANDLE) client->vcm);

	CloseHandle(client->StopEvent);

	if (client->lobby)
	{
		shadow_surface_free(client->lobby);
		client->lobby = NULL;
	}

	if (client->encoder)
	{
		shadow_encoder_free(client->encoder);
		client->encoder = NULL;
	}
}

BOOL shadow_client_capabilities(freerdp_peer* peer)
{
	return TRUE;
}

BOOL shadow_client_post_connect(freerdp_peer* peer)
{
	int width, height;
	rdpSettings* settings;
	rdpShadowClient* client;
	rdpShadowSurface* lobby;
	RECTANGLE_16 invalidRect;

	client = (rdpShadowClient*) peer->context;
	settings = peer->settings;

	settings->DesktopWidth = client->server->screen->width;
	settings->DesktopHeight = client->server->screen->height;

	if (settings->ColorDepth == 24)
		settings->ColorDepth = 16; /* disable 24bpp */

	fprintf(stderr, "Client from %s is activated (%dx%d@%d)\n",
			peer->hostname, settings->DesktopWidth, settings->DesktopHeight, settings->ColorDepth);

	peer->update->DesktopResize(peer->update->context);

	shadow_client_channels_post_connect(client);

	width = settings->DesktopWidth;
	height = settings->DesktopHeight;

	invalidRect.left = 0;
	invalidRect.top = 0;
	invalidRect.right = width;
	invalidRect.bottom = height;

	region16_union_rect(&(client->invalidRegion), &(client->invalidRegion), &invalidRect);

	lobby = client->lobby = shadow_surface_new(client->server, 0, 0, width, height);

	if (!client->lobby)
		return FALSE;

	freerdp_image_fill(lobby->data, PIXEL_FORMAT_XRGB32, lobby->scanline,
			0, 0, lobby->width, lobby->height, 0x3BB9FF);

	region16_union_rect(&(lobby->invalidRegion), &(lobby->invalidRegion), &invalidRect);

	return TRUE;
}

BOOL shadow_client_activate(freerdp_peer* peer)
{
	rdpShadowClient* client;

	client = (rdpShadowClient*) peer->context;

	client->activated = TRUE;
	client->inLobby = client->mayView ? FALSE : TRUE;

	shadow_encoder_reset(client->encoder);

	return TRUE;
}

void shadow_client_surface_frame_acknowledge(rdpShadowClient* client, UINT32 frameId)
{
	SURFACE_FRAME* frame;
	wListDictionary* frameList;

	frameList = client->encoder->frameList;
	frame = (SURFACE_FRAME*) ListDictionary_GetItemValue(frameList, (void*) (size_t) frameId);

	if (frame)
	{
		ListDictionary_Remove(frameList, (void*) (size_t) frameId);
		free(frame);
	}
}

void shadow_client_suppress_output(rdpShadowClient* client, BYTE allow, RECTANGLE_16* area)
{

}

int shadow_client_send_surface_frame_marker(rdpShadowClient* client, UINT32 action, UINT32 id)
{
	SURFACE_FRAME_MARKER surfaceFrameMarker;
	rdpContext* context = (rdpContext*) client;
	rdpUpdate* update = context->update;

	surfaceFrameMarker.frameAction = action;
	surfaceFrameMarker.frameId = id;

	IFCALL(update->SurfaceFrameMarker, context, &surfaceFrameMarker);

	return 1;
}

int shadow_client_send_surface_bits(rdpShadowClient* client, rdpShadowSurface* surface, int nXSrc, int nYSrc, int nWidth, int nHeight)
{
	int i;
	wStream* s;
	int nSrcStep;
	BYTE* pSrcData;
	int numMessages;
	UINT32 frameId = 0;
	rdpUpdate* update;
	rdpContext* context;
	rdpSettings* settings;
	rdpShadowServer* server;
	rdpShadowEncoder* encoder;
	SURFACE_BITS_COMMAND cmd;

	context = (rdpContext*) client;
	update = context->update;
	settings = context->settings;

	server = client->server;
	encoder = client->encoder;

	pSrcData = surface->data;
	nSrcStep = surface->scanline;

	if (encoder->frameAck)
	{
		frameId = (UINT32) shadow_encoder_create_frame_id(encoder);
		shadow_client_send_surface_frame_marker(client, SURFACECMD_FRAMEACTION_BEGIN, frameId);
	}

	if (settings->RemoteFxCodec)
	{
		RFX_RECT rect;
		RFX_MESSAGE* messages;

		s = encoder->bs;

		rect.x = nXSrc;
		rect.y = nYSrc;
		rect.width = nWidth;
		rect.height = nHeight;

		messages = rfx_encode_messages(encoder->rfx, &rect, 1, pSrcData,
				surface->width, surface->height, nSrcStep, &numMessages,
				settings->MultifragMaxRequestSize);

		cmd.codecID = settings->RemoteFxCodecId;

		cmd.destLeft = 0;
		cmd.destTop = 0;
		cmd.destRight = surface->width;
		cmd.destBottom = surface->height;

		cmd.bpp = 32;
		cmd.width = surface->width;
		cmd.height = surface->height;

		for (i = 0; i < numMessages; i++)
		{
			Stream_SetPosition(s, 0);
			rfx_write_message(encoder->rfx, s, &messages[i]);
			rfx_message_free(encoder->rfx, &messages[i]);

			cmd.bitmapDataLength = Stream_GetPosition(s);
			cmd.bitmapData = Stream_Buffer(s);

			IFCALL(update->SurfaceBits, update->context, &cmd);
		}

		free(messages);
	}
	else if (settings->NSCodec)
	{
		NSC_MESSAGE* messages;

		s = encoder->bs;

		messages = nsc_encode_messages(encoder->nsc, pSrcData,
				nXSrc, nYSrc, nWidth, nHeight, nSrcStep,
				&numMessages, settings->MultifragMaxRequestSize);

		cmd.bpp = 32;
		cmd.codecID = settings->NSCodecId;

		for (i = 0; i < numMessages; i++)
		{
			Stream_SetPosition(s, 0);

			nsc_write_message(encoder->nsc, s, &messages[i]);
			nsc_message_free(encoder->nsc, &messages[i]);

			cmd.destLeft = messages[i].x;
			cmd.destTop = messages[i].y;
			cmd.destRight = messages[i].x + messages[i].width;
			cmd.destBottom = messages[i].y + messages[i].height;
			cmd.width = messages[i].width;
			cmd.height = messages[i].height;

			cmd.bitmapDataLength = Stream_GetPosition(s);
			cmd.bitmapData = Stream_Buffer(s);

			IFCALL(update->SurfaceBits, update->context, &cmd);
		}

		free(messages);
	}

	if (encoder->frameAck)
	{
		shadow_client_send_surface_frame_marker(client, SURFACECMD_FRAMEACTION_END, frameId);
	}

	return 1;
}

int shadow_client_send_bitmap_update(rdpShadowClient* client, rdpShadowSurface* surface, int nXSrc, int nYSrc, int nWidth, int nHeight)
{
	BYTE* data;
	BYTE* buffer;
	int i, j, k;
	wStream* s;
	wStream* ts;
	int e, lines;
	int rows, cols;
	int nSrcStep;
	BYTE* pSrcData;
	rdpUpdate* update;
	rdpContext* context;
	rdpSettings* settings;
	int MaxRegionWidth;
	int MaxRegionHeight;
	BITMAP_DATA* bitmapData;
	BITMAP_UPDATE bitmapUpdate;
	rdpShadowServer* server;
	rdpShadowEncoder* encoder;

	context = (rdpContext*) client;
	update = context->update;
	settings = context->settings;

	server = client->server;
	encoder = client->encoder;

	pSrcData = surface->data;
	nSrcStep = surface->scanline;

	MaxRegionWidth = 64 * 4;
	MaxRegionHeight = 64 * 1;

	if ((nXSrc % 4) != 0)
	{
		nWidth += (nXSrc % 4);
		nXSrc -= (nXSrc % 4);
	}

	if ((nYSrc % 4) != 0)
	{
		nHeight += (nYSrc % 4);
		nYSrc -= (nYSrc % 4);
	}

	if ((nWidth * nHeight) > (MaxRegionWidth * MaxRegionHeight))
	{
		int nXSrcSub;
		int nYSrcSub;
		int nWidthSub;
		int nHeightSub;
		rows = (nWidth + (MaxRegionWidth - (nWidth % MaxRegionWidth))) / MaxRegionWidth;
		cols = (nHeight + (MaxRegionHeight - (nHeight % MaxRegionHeight))) / MaxRegionHeight;

		for (i = 0; i < rows; i++)
		{
			for (j = 0; j < cols; j++)
			{
				nXSrcSub = nXSrc + (i * MaxRegionWidth);
				nYSrcSub = nYSrc + (j * MaxRegionHeight);

				nWidthSub = (i < (rows - 1)) ? MaxRegionWidth : nWidth - (i * MaxRegionWidth);
				nHeightSub = (j < (cols - 1)) ? MaxRegionHeight : nHeight - (j * MaxRegionHeight);

				if ((nWidthSub * nHeightSub) > 0)
				{
					shadow_client_send_bitmap_update(client, surface, nXSrcSub, nYSrcSub, nWidthSub, nHeightSub);
				}
			}
		}

		return 1;
	}

	rows = (nWidth + (64 - (nWidth % 64))) / 64;
	cols = (nHeight + (64 - (nHeight % 64))) / 64;

	k = 0;
	bitmapUpdate.count = bitmapUpdate.number = rows * cols;
	bitmapData = (BITMAP_DATA*) malloc(sizeof(BITMAP_DATA) * bitmapUpdate.number);
	bitmapUpdate.rectangles = bitmapData;

	if (!bitmapData)
		return -1;

	if ((nWidth % 4) != 0)
	{
		nXSrc -= (nWidth % 4);
		nWidth += (nWidth % 4);
	}

	if ((nHeight % 4) != 0)
	{
		nYSrc -= (nHeight % 4);
		nHeight += (nHeight % 4);
	}

	for (i = 0; i < rows; i++)
	{
		for (j = 0; j < cols; j++)
		{
			nWidth = (i < (rows - 1)) ? 64 : nWidth - (i * 64);
			nHeight = (j < (cols - 1)) ? 64 : nHeight - (j * 64);

			bitmapData[k].bitsPerPixel = 16;
			bitmapData[k].width = nWidth;
			bitmapData[k].height = nHeight;
			bitmapData[k].destLeft = nXSrc + (i * 64);
			bitmapData[k].destTop = nYSrc + (j * 64);
			bitmapData[k].destRight = bitmapData[k].destLeft + nWidth - 1;
			bitmapData[k].destBottom = bitmapData[k].destTop + nHeight - 1;
			bitmapData[k].compressed = TRUE;

			if (((nWidth * nHeight) > 0) && (nWidth >= 4) && (nHeight >= 4))
			{
				UINT32 srcFormat = PIXEL_FORMAT_RGB32;

				e = nWidth % 4;

				if (e != 0)
					e = 4 - e;

				s = encoder->bs;
				ts = encoder->bts;

				Stream_SetPosition(s, 0);
				Stream_SetPosition(ts, 0);

				data = surface->data;
				data = &data[(bitmapData[k].destTop * nSrcStep) +
				             (bitmapData[k].destLeft * 4)];

				srcFormat = PIXEL_FORMAT_RGB32;

				if (settings->ColorDepth > 24)
				{
					int dstSize;

					buffer = encoder->grid[k];

					buffer = freerdp_bitmap_compress_planar(encoder->planar,
							data, srcFormat, nWidth, nHeight, nSrcStep, buffer, &dstSize);

					bitmapData[k].bitmapDataStream = buffer;
					bitmapData[k].bitmapLength = dstSize;

					bitmapData[k].bitsPerPixel = 32;
					bitmapData[k].cbScanWidth = nWidth * 4;
					bitmapData[k].cbUncompressedSize = nWidth * nHeight * 4;
				}
				else
				{
					int bytesPerPixel = 2;
					UINT32 dstFormat = PIXEL_FORMAT_RGB16;

					if (settings->ColorDepth == 15)
					{
						bytesPerPixel = 2;
						dstFormat = PIXEL_FORMAT_RGB15;
					}
					else if (settings->ColorDepth == 24)
					{
						bytesPerPixel = 3;
						dstFormat = PIXEL_FORMAT_XRGB32;
					}

					buffer = encoder->grid[k];

					freerdp_image_copy(buffer, dstFormat, -1, 0, 0, nWidth, nHeight,
							data, srcFormat, nSrcStep, 0, 0);

					lines = freerdp_bitmap_compress((char*) buffer, nWidth, nHeight, s,
							settings->ColorDepth, 64 * 64 * 4, nHeight - 1, ts, e);

					Stream_SealLength(s);

					bitmapData[k].bitmapDataStream = Stream_Buffer(s);
					bitmapData[k].bitmapLength = Stream_Length(s);

					buffer = encoder->grid[k];
					CopyMemory(buffer, bitmapData[k].bitmapDataStream, bitmapData[k].bitmapLength);
					bitmapData[k].bitmapDataStream = buffer;

					bitmapData[k].bitsPerPixel = settings->ColorDepth;
					bitmapData[k].cbScanWidth = nWidth * bytesPerPixel;
					bitmapData[k].cbUncompressedSize = nWidth * nHeight * bytesPerPixel;
				}

				bitmapData[k].cbCompFirstRowSize = 0;
				bitmapData[k].cbCompMainBodySize = bitmapData[k].bitmapLength;

				k++;
			}
		}
	}

	bitmapUpdate.count = bitmapUpdate.number = k;

	IFCALL(update->BitmapUpdate, context, &bitmapUpdate);

	free(bitmapData);

	return 1;
}

int shadow_client_send_surface_update(rdpShadowClient* client)
{
	int status = -1;
	int nXSrc, nYSrc;
	int nWidth, nHeight;
	rdpContext* context;
	rdpSettings* settings;
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	rdpShadowEncoder* encoder;
	REGION16 invalidRegion;
	RECTANGLE_16 surfaceRect;
	const RECTANGLE_16* extents;

	context = (rdpContext*) client;
	settings = context->settings;
	server = client->server;
	encoder = client->encoder;

	surface = client->inLobby ? client->lobby : server->surface;

	EnterCriticalSection(&(client->lock));

	region16_init(&invalidRegion);
	region16_copy(&invalidRegion, &(client->invalidRegion));
	region16_clear(&(client->invalidRegion));

	LeaveCriticalSection(&(client->lock));

	surfaceRect.left = surface->x;
	surfaceRect.top = surface->y;
	surfaceRect.right = surface->x + surface->width;
	surfaceRect.bottom = surface->y + surface->height;

	region16_intersect_rect(&invalidRegion, &invalidRegion, &surfaceRect);

	if (region16_is_empty(&invalidRegion))
	{
		region16_uninit(&invalidRegion);
		return 1;
	}

	extents = region16_extents(&invalidRegion);

	nXSrc = extents->left - surface->x;
	nYSrc = extents->top - surface->y;
	nWidth = extents->right - extents->left;
	nHeight = extents->bottom - extents->top;

	//printf("shadow_client_send_surface_update: x: %d y: %d width: %d height: %d right: %d bottom: %d\n",
	//	nXSrc, nYSrc, nWidth, nHeight, nXSrc + nWidth, nYSrc + nHeight);

	if (settings->RemoteFxCodec || settings->NSCodec)
	{
		if (settings->RemoteFxCodec)
			shadow_encoder_prepare(encoder, SHADOW_CODEC_REMOTEFX);
		else if (settings->NSCodec)
			shadow_encoder_prepare(encoder, SHADOW_CODEC_NSCODEC);

		status = shadow_client_send_surface_bits(client, surface, nXSrc, nYSrc, nWidth, nHeight);
	}
	else
	{
		shadow_encoder_prepare(encoder, SHADOW_CODEC_BITMAP);

		status = shadow_client_send_bitmap_update(client, surface, nXSrc, nYSrc, nWidth, nHeight);
	}

	region16_uninit(&invalidRegion);

	return status;
}

int shadow_client_surface_update(rdpShadowClient* client, REGION16* region)
{
	int index;
	int numRects = 0;
	const RECTANGLE_16* rects;

	EnterCriticalSection(&(client->lock));

	rects = region16_rects(region, &numRects);

	for (index = 0; index < numRects; index++)
	{
		region16_union_rect(&(client->invalidRegion), &(client->invalidRegion), &rects[index]);
	}

	LeaveCriticalSection(&(client->lock));

	return 1;
}

void* shadow_client_thread(rdpShadowClient* client)
{
	DWORD status;
	DWORD nCount;
	HANDLE events[32];
	HANDLE StopEvent;
	HANDLE ClientEvent;
	HANDLE ChannelEvent;
	HANDLE UpdateEvent;
	freerdp_peer* peer;
	rdpSettings* settings;
	rdpShadowServer* server;
	rdpShadowScreen* screen;
	rdpShadowEncoder* encoder;
	rdpShadowSubsystem* subsystem;

	server = client->server;
	screen = server->screen;
	encoder = client->encoder;
	subsystem = server->subsystem;

	peer = ((rdpContext*) client)->peer;
	settings = peer->settings;

	peer->Capabilities = shadow_client_capabilities;
	peer->PostConnect = shadow_client_post_connect;
	peer->Activate = shadow_client_activate;

	shadow_input_register_callbacks(peer->input);

	peer->Initialize(peer);

	peer->update->SurfaceFrameAcknowledge = (pSurfaceFrameAcknowledge)
			shadow_client_surface_frame_acknowledge;
	peer->update->SuppressOutput = (pSuppressOutput) shadow_client_suppress_output;

	StopEvent = client->StopEvent;
	UpdateEvent = subsystem->updateEvent;
	ClientEvent = peer->GetEventHandle(peer);
	ChannelEvent = WTSVirtualChannelManagerGetEventHandle(client->vcm);

	while (1)
	{
		nCount = 0;
		events[nCount++] = StopEvent;
		events[nCount++] = UpdateEvent;
		events[nCount++] = ClientEvent;
		events[nCount++] = ChannelEvent;

		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(StopEvent, 0) == WAIT_OBJECT_0)
		{
			if (WaitForSingleObject(UpdateEvent, 0) == WAIT_OBJECT_0)
			{
				EnterSynchronizationBarrier(&(subsystem->barrier), 0);
			}

			break;
		}

		if (WaitForSingleObject(UpdateEvent, 0) == WAIT_OBJECT_0)
		{
			if (client->activated)
			{
				int index;
				int numRects = 0;
				const RECTANGLE_16* rects;

				rects = region16_rects(&(subsystem->invalidRegion), &numRects);

				for (index = 0; index < numRects; index++)
				{
					region16_union_rect(&(client->invalidRegion), &(client->invalidRegion), &rects[index]);
				}

				shadow_client_send_surface_update(client);
			}

			EnterSynchronizationBarrier(&(subsystem->barrier), 0);

			while (WaitForSingleObject(UpdateEvent, 0) == WAIT_OBJECT_0);
		}

		if (WaitForSingleObject(ClientEvent, 0) == WAIT_OBJECT_0)
		{
			if (!peer->CheckFileDescriptor(peer))
			{
				fprintf(stderr, "Failed to check FreeRDP file descriptor\n");
				break;
			}
		}

		if (WaitForSingleObject(ChannelEvent, 0) == WAIT_OBJECT_0)
		{
			if (WTSVirtualChannelManagerCheckFileDescriptor(client->vcm) != TRUE)
			{
				fprintf(stderr, "WTSVirtualChannelManagerCheckFileDescriptor failure\n");
				break;
			}
		}
	}

	peer->Disconnect(peer);
	
	freerdp_peer_context_free(peer);
	freerdp_peer_free(peer);

	ExitThread(0);

	return NULL;
}

void shadow_client_accepted(freerdp_listener* listener, freerdp_peer* peer)
{
	rdpShadowClient* client;
	rdpShadowServer* server;

	server = (rdpShadowServer*) listener->info;

	peer->ContextExtra = (void*) server;
	peer->ContextSize = sizeof(rdpShadowClient);
	peer->ContextNew = (psPeerContextNew) shadow_client_context_new;
	peer->ContextFree = (psPeerContextFree) shadow_client_context_free;
	freerdp_peer_context_new(peer);

	client = (rdpShadowClient*) peer->context;

	client->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
			shadow_client_thread, client, 0, NULL);
}
