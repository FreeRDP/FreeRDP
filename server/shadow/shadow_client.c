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
#include <winpr/interlocked.h>

#include <freerdp/log.h>

#include "shadow.h"

#define TAG CLIENT_TAG("shadow")

static void shadow_client_free_queued_message(void *obj)
{
	wMessage *message = (wMessage*)obj;
	if (message->Free)
	{
		message->Free(message);
		message->Free = NULL;
	}
}

BOOL shadow_client_context_new(freerdp_peer* peer, rdpShadowClient* client)
{
	rdpSettings* settings;
	rdpShadowServer* server;
	const wObject cb = { NULL, NULL, NULL, shadow_client_free_queued_message, NULL };

	server = (rdpShadowServer*) peer->ContextExtra;
	client->server = server;
	client->subsystem = server->subsystem;

	settings = peer->settings;

	settings->ColorDepth = 32;
	settings->NSCodec = TRUE;
	settings->RemoteFxCodec = TRUE;
	settings->BitmapCacheV3Enabled = TRUE;
	settings->FrameMarkerCommandEnabled = TRUE;
	settings->SurfaceFrameMarkerEnabled = TRUE;
	settings->SupportGraphicsPipeline = FALSE;

	settings->DrawAllowSkipAlpha = TRUE;
	settings->DrawAllowColorSubsampling = TRUE;
	settings->DrawAllowDynamicColorFidelity = TRUE;

	settings->CompressionLevel = PACKET_COMPR_TYPE_RDP6;

	settings->RdpSecurity = TRUE;
	settings->TlsSecurity = TRUE;
	settings->NlaSecurity = FALSE;

	if (!(settings->CertificateFile = _strdup(server->CertificateFile)))
		goto fail_cert_file;
	if (!(settings->PrivateKeyFile = _strdup(server->PrivateKeyFile)))
		goto fail_privkey_file;
	if (!(settings->RdpKeyFile = _strdup(settings->PrivateKeyFile)))
		goto fail_rdpkey_file;


	if (server->ipcSocket)
	{
		settings->LyncRdpMode = TRUE;
		settings->CompressionEnabled = FALSE;
	}

	client->inLobby = TRUE;
	client->mayView = server->mayView;
	client->mayInteract = server->mayInteract;

	if (!InitializeCriticalSectionAndSpinCount(&(client->lock), 4000))
		goto fail_client_lock;

	region16_init(&(client->invalidRegion));

	client->vcm = WTSOpenServerA((LPSTR) peer->context);
	if (!client->vcm || client->vcm == INVALID_HANDLE_VALUE)
		goto fail_open_server;

	if (!(client->MsgQueue = MessageQueue_New(&cb)))
		goto fail_message_queue;

	if (!(client->encoder = shadow_encoder_new(client)))
		goto fail_encoder_new;

	if (ArrayList_Add(server->clients, (void*) client) >= 0)
		return TRUE;

	shadow_encoder_free(client->encoder);
	client->encoder = NULL;
fail_encoder_new:
	MessageQueue_Free(client->MsgQueue);
	client->MsgQueue = NULL;
fail_message_queue:
	WTSCloseServer((HANDLE) client->vcm);
	client->vcm = NULL;
fail_open_server:
	DeleteCriticalSection(&(client->lock));
fail_client_lock:
	free(settings->RdpKeyFile);
	settings->RdpKeyFile = NULL;
fail_rdpkey_file:
	free(settings->PrivateKeyFile);
	settings->PrivateKeyFile = NULL;
fail_privkey_file:
	free(settings->CertificateFile);
	settings->CertificateFile = NULL;
fail_cert_file:

	return FALSE;
}

void shadow_client_context_free(freerdp_peer* peer, rdpShadowClient* client)
{
	rdpShadowServer* server = client->server;

	ArrayList_Remove(server->clients, (void*) client);

	DeleteCriticalSection(&(client->lock));

	region16_uninit(&(client->invalidRegion));

	WTSCloseServer((HANDLE) client->vcm);

    /* Clear queued messages and free resource */
	MessageQueue_Clear(client->MsgQueue);
	MessageQueue_Free(client->MsgQueue);

	if (client->encoder)
	{
		shadow_encoder_free(client->encoder);
		client->encoder = NULL;
	}
}

void shadow_client_message_free(wMessage* message)
{
	switch(message->id)
	{
		case SHADOW_MSG_IN_REFRESH_OUTPUT_ID:
			free(((SHADOW_MSG_IN_REFRESH_OUTPUT*)message->wParam)->rects);
			free(message->wParam);
			break;

		case SHADOW_MSG_IN_SUPPRESS_OUTPUT_ID:
			free(message->wParam);
			break;

		default:
			WLog_ERR(TAG, "Unknown message id: %u", message->id);
			free(message->wParam);
			break;
	}
}

BOOL shadow_client_capabilities(freerdp_peer* peer)
{
	return TRUE;
}

static INLINE void shadow_client_calc_desktop_size(rdpShadowServer* server, int* pWidth, int* pHeight)
{
	RECTANGLE_16 viewport = {0, 0, server->screen->width, server->screen->height};

	if (server->shareSubRect)
	{
		rectangles_intersection(&viewport, &(server->subRect), &viewport); 
	}

	(*pWidth) = viewport.right - viewport.left;
	(*pHeight) = viewport.bottom - viewport.top;
}

BOOL shadow_client_post_connect(freerdp_peer* peer)
{
	int authStatus;
	int width, height;
	rdpSettings* settings;
	rdpShadowClient* client;
	rdpShadowServer* server;
	RECTANGLE_16 invalidRect;
	rdpShadowSubsystem* subsystem;

	client = (rdpShadowClient*) peer->context;
	settings = peer->settings;
	server = client->server;
	subsystem = server->subsystem;

	shadow_client_calc_desktop_size(server, &width, &height);
	settings->DesktopWidth = width;
	settings->DesktopHeight = height;

	if (settings->ColorDepth == 24)
		settings->ColorDepth = 16; /* disable 24bpp */

	if (settings->MultifragMaxRequestSize < 0x3F0000)
		settings->NSCodec = FALSE; /* NSCodec compressor does not support fragmentation yet */

	WLog_ERR(TAG, "Client from %s is activated (%dx%d@%d)",
			peer->hostname, settings->DesktopWidth, settings->DesktopHeight, settings->ColorDepth);

	peer->update->DesktopResize(peer->update->context);

	shadow_client_channels_post_connect(client);

	invalidRect.left = 0;
	invalidRect.top = 0;
	invalidRect.right = server->screen->width;
	invalidRect.bottom = server->screen->height;

	region16_union_rect(&(client->invalidRegion), &(client->invalidRegion), &invalidRect);

	authStatus = -1;

	if (settings->Username && settings->Password)
		settings->AutoLogonEnabled = TRUE;

	if (server->authentication)
	{
		if (subsystem->Authenticate)
		{
			authStatus = subsystem->Authenticate(subsystem,
					settings->Username, settings->Domain, settings->Password);
		}
	}

	if (server->authentication)
	{
		if (authStatus < 0)
		{
			WLog_ERR(TAG, "client authentication failure: %d", authStatus);
			return FALSE;
		}
	}

	if (subsystem->ClientConnect)
	{
		return subsystem->ClientConnect(subsystem, client);
	}

	return TRUE;
}

/* Convert rects in sub rect coordinate to client/surface coordinate */
static INLINE void shadow_client_convert_rects(rdpShadowClient* client, 
		RECTANGLE_16* dst, RECTANGLE_16* src, UINT32 numRects)
{
	if (client->server->shareSubRect)
	{
		int i = 0;
		UINT16 offsetX = client->server->subRect.left;
		UINT16 offsetY = client->server->subRect.top;

		for (i = 0; i < numRects; i++)
		{
			dst[i].left = src[i].left + offsetX;
			dst[i].right = src[i].right + offsetX;
			dst[i].top = src[i].top + offsetY;
			dst[i].bottom = src[i].bottom + offsetY;
		}
	}
	else
	{
		CopyMemory(dst, src, numRects * sizeof(RECTANGLE_16));
	}
}

BOOL shadow_client_refresh_rect(rdpShadowClient* client, BYTE count, RECTANGLE_16* areas)
{
	wMessage message = { 0 };
	SHADOW_MSG_IN_REFRESH_OUTPUT* wParam;
	wMessagePipe* MsgPipe = client->subsystem->MsgPipe;

	if (count && !areas)
		return FALSE;

	if (!(wParam = (SHADOW_MSG_IN_REFRESH_OUTPUT*) calloc(1, sizeof(SHADOW_MSG_IN_REFRESH_OUTPUT))))
		return FALSE;

	wParam->numRects = (UINT32) count;

	if (wParam->numRects)
	{
		wParam->rects = (RECTANGLE_16*) calloc(wParam->numRects, sizeof(RECTANGLE_16));

		if (!wParam->rects)
		{
			free (wParam);
			return FALSE;
		}

		shadow_client_convert_rects(client, wParam->rects, areas, wParam->numRects);
	}

	message.id = SHADOW_MSG_IN_REFRESH_OUTPUT_ID;
	message.wParam = (void*) wParam;
	message.lParam = NULL;
	message.context = (void*) client;
	message.Free = shadow_client_message_free;

	return MessageQueue_Dispatch(MsgPipe->In, &message);
}

BOOL shadow_client_suppress_output(rdpShadowClient* client, BYTE allow, RECTANGLE_16* area)
{
	wMessage message = { 0 };
	SHADOW_MSG_IN_SUPPRESS_OUTPUT* wParam;
	wMessagePipe* MsgPipe = client->subsystem->MsgPipe;

	wParam = (SHADOW_MSG_IN_SUPPRESS_OUTPUT*) calloc(1, sizeof(SHADOW_MSG_IN_SUPPRESS_OUTPUT));
	if (!wParam)
		return FALSE;

	wParam->allow = (UINT32) allow;

	if (area)
		shadow_client_convert_rects(client, &(wParam->rect), area, 1);

	message.id = SHADOW_MSG_IN_SUPPRESS_OUTPUT_ID;
	message.wParam = (void*) wParam;
	message.lParam = NULL;
	message.context = (void*) client;
	message.Free = shadow_client_message_free;

	return MessageQueue_Dispatch(MsgPipe->In, &message);
}

BOOL shadow_client_activate(freerdp_peer* peer)
{
	rdpSettings* settings = peer->settings;
	rdpShadowClient* client = (rdpShadowClient*) peer->context;

	if (settings->ClientDir && (strcmp(settings->ClientDir, "librdp") == 0))
	{
		/* Hack for Mac/iOS/Android Microsoft RDP clients */

		settings->RemoteFxCodec = FALSE;

		settings->NSCodec = FALSE;
		settings->NSCodecAllowSubsampling = FALSE;

		settings->SurfaceFrameMarkerEnabled = FALSE;
	}

	client->activated = TRUE;
	client->inLobby = client->mayView ? FALSE : TRUE;

	shadow_encoder_reset(client->encoder);

	return shadow_client_refresh_rect(client, 0, NULL);
}

BOOL shadow_client_surface_frame_acknowledge(rdpShadowClient* client, UINT32 frameId)
{
	/*
     * Record the last client acknowledged frame id to 
	 * calculate how much frames are in progress.
	 * Some rdp clients (win7 mstsc) skips frame ACK if it is 
	 * inactive, we should not expect ACK for each frame. 
	 * So it is OK to calculate inflight frame count according to
	 * a latest acknowledged frame id.
     */
	client->encoder->lastAckframeId = frameId;

	return TRUE;
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
	BOOL first;
	BOOL last;
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

	if (server->shareSubRect)
	{
		int subX, subY;
		int subWidth, subHeight;

		subX = server->subRect.left;
		subY = server->subRect.top;
		subWidth = server->subRect.right - server->subRect.left;
		subHeight = server->subRect.bottom - server->subRect.top;

		nXSrc -= subX;
		nYSrc -= subY;
		pSrcData = &pSrcData[(subY * nSrcStep) + (subX * 4)];
	}

	if (encoder->frameAck)
		frameId = shadow_encoder_create_frame_id(encoder);

	if (settings->RemoteFxCodec)
	{
		RFX_RECT rect;
		RFX_MESSAGE* messages;
		RFX_RECT *messageRects = NULL;

		shadow_encoder_prepare(encoder, FREERDP_CODEC_REMOTEFX);

		s = encoder->bs;

		rect.x = nXSrc;
		rect.y = nYSrc;
		rect.width = nWidth;
		rect.height = nHeight;

		if (!(messages = rfx_encode_messages(encoder->rfx, &rect, 1, pSrcData,
				settings->DesktopWidth, settings->DesktopHeight, nSrcStep, &numMessages,
				settings->MultifragMaxRequestSize)))
		{
			return 0;
		}

		cmd.codecID = settings->RemoteFxCodecId;

		cmd.destLeft = 0;
		cmd.destTop = 0;
		cmd.destRight = settings->DesktopWidth;
		cmd.destBottom = settings->DesktopHeight;

		cmd.bpp = 32;
		cmd.width = settings->DesktopWidth;
		cmd.height = settings->DesktopHeight;
		cmd.skipCompression = TRUE;

		if (numMessages > 0)
			messageRects = messages[0].rects;

		for (i = 0; i < numMessages; i++)
		{
			Stream_SetPosition(s, 0);
			if (!rfx_write_message(encoder->rfx, s, &messages[i]))
			{
				while (i < numMessages)
				{
					rfx_message_free(encoder->rfx, &messages[i++]);
				}
				break;
			}
			rfx_message_free(encoder->rfx, &messages[i]);

			cmd.bitmapDataLength = Stream_GetPosition(s);
			cmd.bitmapData = Stream_Buffer(s);

			first = (i == 0) ? TRUE : FALSE;
			last = ((i + 1) == numMessages) ? TRUE : FALSE;

			if (!encoder->frameAck)
				IFCALL(update->SurfaceBits, update->context, &cmd);
			else
				IFCALL(update->SurfaceFrameBits, update->context, &cmd, first, last, frameId);
		}

		free(messageRects);
		free(messages);
	}
	else if (settings->NSCodec)
	{
		shadow_encoder_prepare(encoder, FREERDP_CODEC_NSCODEC);

		s = encoder->bs;
		Stream_SetPosition(s, 0);

		pSrcData = &pSrcData[(nYSrc * nSrcStep) + (nXSrc * 4)];

		nsc_compose_message(encoder->nsc, s, pSrcData, nWidth, nHeight, nSrcStep);

		cmd.bpp = 32;
		cmd.codecID = settings->NSCodecId;
		cmd.destLeft = nXSrc;
		cmd.destTop = nYSrc;
		cmd.destRight = cmd.destLeft + nWidth;
		cmd.destBottom = cmd.destTop + nHeight;
		cmd.width = nWidth;
		cmd.height = nHeight;

		cmd.bitmapDataLength = Stream_GetPosition(s);
		cmd.bitmapData = Stream_Buffer(s);

		first = TRUE;
		last = TRUE;

		if (!encoder->frameAck)
			IFCALL(update->SurfaceBits, update->context, &cmd);
		else
			IFCALL(update->SurfaceFrameBits, update->context, &cmd, first, last, frameId);
	}

	return 1;
}

int shadow_client_send_bitmap_update(rdpShadowClient* client, rdpShadowSurface* surface, int nXSrc, int nYSrc, int nWidth, int nHeight)
{
	BYTE* data;
	BYTE* buffer;
	int yIdx, xIdx, k;
	int rows, cols;
	int nSrcStep;
	BYTE* pSrcData;
	UINT32 DstSize;
	UINT32 SrcFormat;
	BITMAP_DATA* bitmap;
	rdpUpdate* update;
	rdpContext* context;
	rdpSettings* settings;
	UINT32 maxUpdateSize;
	UINT32 totalBitmapSize;
	UINT32 updateSizeEstimate;
	BITMAP_DATA* bitmapData;
	BITMAP_UPDATE bitmapUpdate;
	rdpShadowServer* server;
	rdpShadowEncoder* encoder;

	context = (rdpContext*) client;
	update = context->update;
	settings = context->settings;

	server = client->server;
	encoder = client->encoder;

	maxUpdateSize = settings->MultifragMaxRequestSize;

	if (settings->ColorDepth < 32)
		shadow_encoder_prepare(encoder, FREERDP_CODEC_INTERLEAVED);
	else
		shadow_encoder_prepare(encoder, FREERDP_CODEC_PLANAR);

	pSrcData = surface->data;
	nSrcStep = surface->scanline;
	SrcFormat = PIXEL_FORMAT_RGB32;

	if (server->shareSubRect)
	{
		int subX, subY;
		int subWidth, subHeight;

		subX = server->subRect.left;
		subY = server->subRect.top;
		subWidth = server->subRect.right - server->subRect.left;
		subHeight = server->subRect.bottom - server->subRect.top;

		nXSrc -= subX;
		nYSrc -= subY;
		pSrcData = &pSrcData[(subY * nSrcStep) + (subX * 4)];
	}

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

	rows = (nHeight / 64) + ((nHeight % 64) ? 1 : 0);
	cols = (nWidth / 64) + ((nWidth % 64) ? 1 : 0);

	k = 0;
	totalBitmapSize = 0;

	bitmapUpdate.count = bitmapUpdate.number = rows * cols;
	if (!(bitmapData = (BITMAP_DATA*) malloc(sizeof(BITMAP_DATA) * bitmapUpdate.number)))
		return -1;
	bitmapUpdate.rectangles = bitmapData;

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

	for (yIdx = 0; yIdx < rows; yIdx++)
	{
		for (xIdx = 0; xIdx < cols; xIdx++)
		{
			bitmap = &bitmapData[k];

			bitmap->width = 64;
			bitmap->height = 64;
			bitmap->destLeft = nXSrc + (xIdx * 64);
			bitmap->destTop = nYSrc + (yIdx * 64);

			if ((bitmap->destLeft + bitmap->width) > (nXSrc + nWidth))
				bitmap->width = (nXSrc + nWidth) - bitmap->destLeft;

			if ((bitmap->destTop + bitmap->height) > (nYSrc + nHeight))
				bitmap->height = (nYSrc + nHeight) - bitmap->destTop;

			bitmap->destRight = bitmap->destLeft + bitmap->width - 1;
			bitmap->destBottom = bitmap->destTop + bitmap->height - 1;
			bitmap->compressed = TRUE;

			if ((bitmap->width < 4) || (bitmap->height < 4))
				continue;

			if (settings->ColorDepth < 32)
			{
				int bitsPerPixel = settings->ColorDepth;
				int bytesPerPixel = (bitsPerPixel + 7) / 8;

				DstSize = 64 * 64 * 4;
				buffer = encoder->grid[k];

				interleaved_compress(encoder->interleaved, buffer, &DstSize, bitmap->width, bitmap->height,
						pSrcData, SrcFormat, nSrcStep, bitmap->destLeft, bitmap->destTop, NULL, bitsPerPixel);

				bitmap->bitmapDataStream = buffer;
				bitmap->bitmapLength = DstSize;
				bitmap->bitsPerPixel = bitsPerPixel;
				bitmap->cbScanWidth = bitmap->width * bytesPerPixel;
				bitmap->cbUncompressedSize = bitmap->width * bitmap->height * bytesPerPixel;
			}
			else
			{
				int dstSize;

				buffer = encoder->grid[k];
				data = &pSrcData[(bitmap->destTop * nSrcStep) + (bitmap->destLeft * 4)];

				buffer = freerdp_bitmap_compress_planar(encoder->planar, data, SrcFormat,
						bitmap->width, bitmap->height, nSrcStep, buffer, &dstSize);

				bitmap->bitmapDataStream = buffer;
				bitmap->bitmapLength = dstSize;
				bitmap->bitsPerPixel = 32;
				bitmap->cbScanWidth = bitmap->width * 4;
				bitmap->cbUncompressedSize = bitmap->width * bitmap->height * 4;
			}

			bitmap->cbCompFirstRowSize = 0;
			bitmap->cbCompMainBodySize = bitmap->bitmapLength;

			totalBitmapSize += bitmap->bitmapLength;
			k++;
		}
	}

	bitmapUpdate.count = bitmapUpdate.number = k;

	updateSizeEstimate = totalBitmapSize + (k * bitmapUpdate.count) + 16;

	if (updateSizeEstimate > maxUpdateSize)
	{
		UINT32 i, j;
		UINT32 updateSize;
		UINT32 newUpdateSize;
		BITMAP_DATA* fragBitmapData = NULL;

		if (k > 0)
			fragBitmapData = (BITMAP_DATA*) malloc(sizeof(BITMAP_DATA) * k);

		if (!fragBitmapData)
		{
			free(bitmapData);
			return -1;
		}
		bitmapUpdate.rectangles = fragBitmapData;

		i = j = 0;
		updateSize = 1024;

		while (i < k)
		{
			newUpdateSize = updateSize + (bitmapData[i].bitmapLength + 16);

			if ((newUpdateSize < maxUpdateSize) && ((i + 1) < k))
			{
				CopyMemory(&fragBitmapData[j++], &bitmapData[i++], sizeof(BITMAP_DATA));
				updateSize = newUpdateSize;
			}
			else
			{
				if ((i + 1) >= k)
				{
					CopyMemory(&fragBitmapData[j++], &bitmapData[i++], sizeof(BITMAP_DATA));
					updateSize = newUpdateSize;
				}
				
				bitmapUpdate.count = bitmapUpdate.number = j;
				IFCALL(update->BitmapUpdate, context, &bitmapUpdate);
				updateSize = 1024;
				j = 0;
			}
		}

		free(fragBitmapData);
	}
	else
	{
		IFCALL(update->BitmapUpdate, context, &bitmapUpdate);
	}

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

	surface = client->inLobby ? server->lobby : server->surface;

	EnterCriticalSection(&(client->lock));

	region16_init(&invalidRegion);
	region16_copy(&invalidRegion, &(client->invalidRegion));
	region16_clear(&(client->invalidRegion));

	LeaveCriticalSection(&(client->lock));

	surfaceRect.left = 0;
	surfaceRect.top = 0;
	surfaceRect.right = surface->width;
	surfaceRect.bottom = surface->height;

	region16_intersect_rect(&invalidRegion, &invalidRegion, &surfaceRect);

	if (server->shareSubRect)
	{
		region16_intersect_rect(&invalidRegion, &invalidRegion, &(server->subRect));
	}

	if (region16_is_empty(&invalidRegion))
	{
		region16_uninit(&invalidRegion);
		return 1;
	}

	extents = region16_extents(&invalidRegion);

	nXSrc = extents->left - 0;
	nYSrc = extents->top - 0;
	nWidth = extents->right - extents->left;
	nHeight = extents->bottom - extents->top;

	//WLog_INFO(TAG, "shadow_client_send_surface_update: x: %d y: %d width: %d height: %d right: %d bottom: %d",
	//	nXSrc, nYSrc, nWidth, nHeight, nXSrc + nWidth, nYSrc + nHeight);

	if (settings->RemoteFxCodec || settings->NSCodec)
	{
		status = shadow_client_send_surface_bits(client, surface, nXSrc, nYSrc, nWidth, nHeight);
	}
	else
	{
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

int shadow_client_subsystem_process_message(rdpShadowClient* client, wMessage* message)
{
	rdpContext* context = (rdpContext*) client;
	rdpUpdate* update = context->update;

	/* FIXME: the pointer updates appear to be broken when used with bulk compression and mstsc */

	switch(message->id)
	{
		case SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID:
		{
			POINTER_POSITION_UPDATE pointerPosition;
			SHADOW_MSG_OUT_POINTER_POSITION_UPDATE* msg = (SHADOW_MSG_OUT_POINTER_POSITION_UPDATE*) message->wParam;

			pointerPosition.xPos = msg->xPos;
			pointerPosition.yPos = msg->yPos;

			if (client->server->shareSubRect)
			{
				pointerPosition.xPos -= client->server->subRect.left;
				pointerPosition.yPos -= client->server->subRect.top;
			}

			if (client->activated)
			{
				if ((msg->xPos != client->pointerX) || (msg->yPos != client->pointerY))
				{
					IFCALL(update->pointer->PointerPosition, context, &pointerPosition);

					client->pointerX = msg->xPos;
					client->pointerY = msg->yPos;
				}
			}
			break;
		}
		case SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID:
		{
			POINTER_NEW_UPDATE pointerNew;
			POINTER_COLOR_UPDATE* pointerColor;
			POINTER_CACHED_UPDATE pointerCached;
			SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* msg = (SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE*) message->wParam;

			ZeroMemory(&pointerNew, sizeof(POINTER_NEW_UPDATE));

			pointerNew.xorBpp = 24;
			pointerColor = &(pointerNew.colorPtrAttr);

			pointerColor->cacheIndex = 0;
			pointerColor->xPos = msg->xHot;
			pointerColor->yPos = msg->yHot;
			pointerColor->width = msg->width;
			pointerColor->height = msg->height;
			pointerColor->lengthAndMask = msg->lengthAndMask;
			pointerColor->lengthXorMask = msg->lengthXorMask;
			pointerColor->xorMaskData = msg->xorMaskData;
			pointerColor->andMaskData = msg->andMaskData;

			pointerCached.cacheIndex = pointerColor->cacheIndex;

			if (client->activated)
			{
				IFCALL(update->pointer->PointerNew, context, &pointerNew);
				IFCALL(update->pointer->PointerCached, context, &pointerCached);
			}
			break;
		}
		case SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES_ID:
		{
			SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES* msg = (SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES*) message->wParam;
			if (client->activated && client->rdpsnd && client->rdpsnd->Activated)
			{
				client->rdpsnd->src_format = msg->audio_format;
				IFCALL(client->rdpsnd->SendSamples, client->rdpsnd, msg->buf, msg->nFrames, msg->wTimestamp);
			}
			break;
		}
		case SHADOW_MSG_OUT_AUDIO_OUT_VOLUME_ID:
		{
			SHADOW_MSG_OUT_AUDIO_OUT_VOLUME* msg = (SHADOW_MSG_OUT_AUDIO_OUT_VOLUME*) message->wParam;
			if (client->activated && client->rdpsnd && client->rdpsnd->Activated)
			{
				IFCALL(client->rdpsnd->SetVolume, client->rdpsnd, msg->left, msg->right);
			}
			break;
		}
		default:
			WLog_ERR(TAG, "Unknown message id: %u", message->id);
			break;
	}

	shadow_client_free_queued_message(message);

	return 1;
}

void* shadow_client_thread(rdpShadowClient* client)
{
	DWORD status;
	DWORD nCount;
	wMessage message;
	wMessage pointerPositionMsg;
	wMessage pointerAlphaMsg;
	wMessage audioVolumeMsg;
	HANDLE events[32];
	HANDLE ClientEvent;
	HANDLE ChannelEvent;
	void* UpdateSubscriber;
	HANDLE UpdateEvent;
	freerdp_peer* peer;
	rdpContext* context;
	rdpSettings* settings;
	rdpShadowServer* server;
	rdpShadowScreen* screen;
	rdpShadowEncoder* encoder;
	rdpShadowSubsystem* subsystem;
	wMessageQueue* MsgQueue = client->MsgQueue;

	server = client->server;
	screen = server->screen;
	encoder = client->encoder;
	subsystem = server->subsystem;

	context = (rdpContext*) client;
	peer = context->peer;
	settings = peer->settings;

	peer->Capabilities = shadow_client_capabilities;
	peer->PostConnect = shadow_client_post_connect;
	peer->Activate = shadow_client_activate;

	shadow_input_register_callbacks(peer->input);

	peer->Initialize(peer);

	peer->update->RefreshRect = (pRefreshRect)shadow_client_refresh_rect;
	peer->update->SuppressOutput = (pSuppressOutput)shadow_client_suppress_output;
	peer->update->SurfaceFrameAcknowledge = (pSurfaceFrameAcknowledge)shadow_client_surface_frame_acknowledge;

	if ((!client->vcm) || (!subsystem->updateEvent))
		goto out;

	UpdateSubscriber = shadow_multiclient_get_subscriber(subsystem->updateEvent);
	if (!UpdateSubscriber)
		goto out;

	UpdateEvent = shadow_multiclient_getevent(UpdateSubscriber);
	ClientEvent = peer->GetEventHandle(peer);
	ChannelEvent = WTSVirtualChannelManagerGetEventHandle(client->vcm);

	while (1)
	{
		nCount = 0;
		events[nCount++] = UpdateEvent;
		events[nCount++] = ClientEvent;
		events[nCount++] = ChannelEvent;
		events[nCount++] = MessageQueue_Event(MsgQueue);

		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (WaitForSingleObject(UpdateEvent, 0) == WAIT_OBJECT_0)
		{
			if (client->activated)
			{
				int index;
				int numRects = 0;
				const RECTANGLE_16* rects;
				int width, height;

				/* Check resize */
				shadow_client_calc_desktop_size(server, &width, &height);
				if (settings->DesktopWidth != (UINT32)width || settings->DesktopHeight != (UINT32)height)
				{
					/* Screen size changed, do resize */
					settings->DesktopWidth = width;
					settings->DesktopHeight = height;

					/**
					 * Unset client activated flag to avoid sending update message during
					 * resize. DesktopResize will reactive the client and 
					 * shadow_client_activate would be invoked later.
					 */
					client->activated = FALSE;

					/* Send Resize */
					peer->update->DesktopResize(peer->update->context); // update_send_desktop_resize

					/* Clear my invalidRegion. shadow_client_activate refreshes fullscreen */
					region16_clear(&(client->invalidRegion));

					WLog_ERR(TAG, "Client from %s is resized (%dx%d@%d)",
							peer->hostname, settings->DesktopWidth, settings->DesktopHeight, settings->ColorDepth);
				}
				else 
				{
					/* Send frame */
					rects = region16_rects(&(subsystem->invalidRegion), &numRects);

					for (index = 0; index < numRects; index++)
					{
						region16_union_rect(&(client->invalidRegion), &(client->invalidRegion), &rects[index]);
					}

					shadow_client_send_surface_update(client);
				}
			}

			/* 
			 * The return value of shadow_multiclient_consume is whether or not the subscriber really consumes the event.
			 * It's not cared currently.
			 */
			(void)shadow_multiclient_consume(UpdateSubscriber);
		}

		if (WaitForSingleObject(ClientEvent, 0) == WAIT_OBJECT_0)
		{
			if (!peer->CheckFileDescriptor(peer))
			{
				WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
				break;
			}
		}

		if (WaitForSingleObject(ChannelEvent, 0) == WAIT_OBJECT_0)
		{
			if (!WTSVirtualChannelManagerCheckFileDescriptor(client->vcm))
			{
				WLog_ERR(TAG, "WTSVirtualChannelManagerCheckFileDescriptor failure");
				break;
			}
		}

		if (WaitForSingleObject(MessageQueue_Event(MsgQueue), 0) == WAIT_OBJECT_0)
		{
			/* Drain messages. Pointer update could be accumulated. */
			pointerPositionMsg.id = 0;
			pointerPositionMsg.Free= NULL;
			pointerAlphaMsg.id = 0;
			pointerAlphaMsg.Free = NULL;
			audioVolumeMsg.id = 0;
			audioVolumeMsg.Free = NULL;
			while (MessageQueue_Peek(MsgQueue, &message, TRUE))
			{
				if (message.id == WMQ_QUIT)
				{
					break;
				}

				switch(message.id)
				{
					case SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID:
						/* Abandon previous message */
						shadow_client_free_queued_message(&pointerPositionMsg);
						CopyMemory(&pointerPositionMsg, &message, sizeof(wMessage));
						break;

					case SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID:
						/* Abandon previous message */
						shadow_client_free_queued_message(&pointerAlphaMsg);
						CopyMemory(&pointerAlphaMsg, &message, sizeof(wMessage));
						break;

					case SHADOW_MSG_OUT_AUDIO_OUT_VOLUME_ID:
						/* Abandon previous message */
						shadow_client_free_queued_message(&audioVolumeMsg);
						CopyMemory(&audioVolumeMsg, &message, sizeof(wMessage));
						break;

					default:
						shadow_client_subsystem_process_message(client, &message);
						break;
				}
			}

			if (message.id == WMQ_QUIT)
			{
				/* Release stored message */
				shadow_client_free_queued_message(&pointerPositionMsg);
				shadow_client_free_queued_message(&pointerAlphaMsg);
				shadow_client_free_queued_message(&audioVolumeMsg);
				break;
			}
			else
			{
				/* Process accumulated messages if needed */
				if (pointerPositionMsg.id)
				{
					shadow_client_subsystem_process_message(client, &pointerPositionMsg);
				}
				if (pointerAlphaMsg.id)
				{
					shadow_client_subsystem_process_message(client, &pointerAlphaMsg);
				}
				if (audioVolumeMsg.id)
				{
					shadow_client_subsystem_process_message(client, &audioVolumeMsg);
				}
			}
		}
	}

	/* Free channels early because we establish channels in post connect */
	shadow_client_channels_free(client);

	if (UpdateSubscriber)
	{
		shadow_multiclient_release_subscriber(UpdateSubscriber);
		UpdateSubscriber = NULL;
	}

	if (peer->connected && subsystem->ClientDisconnect)
	{
		subsystem->ClientDisconnect(subsystem, client);
	}

out:
	peer->Disconnect(peer);
	
	freerdp_peer_context_free(peer);
	freerdp_peer_free(peer);
	ExitThread(0);
	return NULL;
}

BOOL shadow_client_accepted(freerdp_listener* listener, freerdp_peer* peer)
{
	rdpShadowClient* client;
	rdpShadowServer* server;

	server = (rdpShadowServer*) listener->info;

	peer->ContextExtra = (void*) server;
	peer->ContextSize = sizeof(rdpShadowClient);
	peer->ContextNew = (psPeerContextNew) shadow_client_context_new;
	peer->ContextFree = (psPeerContextFree) shadow_client_context_free;

	if (!freerdp_peer_context_new(peer))
		return FALSE;

	client = (rdpShadowClient*) peer->context;

	if (!(client->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
			shadow_client_thread, client, 0, NULL)))
	{
		freerdp_peer_context_free(peer);
		return FALSE;
	}

	return TRUE;
}

static void shadow_msg_out_addref(wMessage* message)
{
	SHADOW_MSG_OUT* msg = (SHADOW_MSG_OUT *)message->wParam;
	InterlockedIncrement(&(msg->refCount));
}

static void shadow_msg_out_release(wMessage* message)
{
	SHADOW_MSG_OUT* msg = (SHADOW_MSG_OUT *)message->wParam;
	if (InterlockedDecrement(&(msg->refCount)) <= 0)
	{
		if (msg->Free)
			msg->Free(message->id, msg);
	}
}

static BOOL shadow_client_dispatch_msg(rdpShadowClient* client, wMessage* message)
{
	/* Add reference when it is posted */
	shadow_msg_out_addref(message);
	if (MessageQueue_Dispatch(client->MsgQueue, message))
	{
		return TRUE;
	}
	else
	{
		/* Release the reference since post failed */
		shadow_msg_out_release(message);
		return FALSE;
	}
}

BOOL shadow_client_post_msg(rdpShadowClient* client, void* context, UINT32 type, SHADOW_MSG_OUT* msg, void* lParam)
{
	wMessage message = {0};

	message.context = context;
	message.id = type;
	message.wParam = (void *)msg;
	message.lParam = lParam;
	message.Free = shadow_msg_out_release;

	return shadow_client_dispatch_msg(client, &message);
}

int shadow_client_boardcast_msg(rdpShadowServer* server, void* context, UINT32 type, SHADOW_MSG_OUT* msg, void* lParam)
{
	wMessage message = {0};
	rdpShadowClient* client = NULL;
	int count = 0;
	int index = 0;

	message.context = context;
	message.id = type;
	message.wParam = (void *)msg;
	message.lParam = lParam;
	message.Free = shadow_msg_out_release;

	/* First add reference as we reference it in this function.
     * Therefore it would not be free'ed during post. */
	shadow_msg_out_addref(&message);

	ArrayList_Lock(server->clients);
	for (index = 0; index < ArrayList_Count(server->clients); index++)
	{
		client = (rdpShadowClient*)ArrayList_GetItem(server->clients, index);
		if (shadow_client_dispatch_msg(client, &message))
		{
			count++;
		}
	}
	ArrayList_Unlock(server->clients);

    /* Release the reference for this function */
	shadow_msg_out_release(&message);

	return count;
}

int shadow_client_boardcast_quit(rdpShadowServer* server, int nExitCode)
{
	wMessageQueue* queue = NULL;
	int count = 0;
	int index = 0;

	ArrayList_Lock(server->clients);
	for (index = 0; index < ArrayList_Count(server->clients); index++)
	{
		queue = ((rdpShadowClient*)ArrayList_GetItem(server->clients, index))->MsgQueue;
		if (MessageQueue_PostQuit(queue, nExitCode))
		{
			count++;
		}
	}
	ArrayList_Unlock(server->clients);

	return count;
}
