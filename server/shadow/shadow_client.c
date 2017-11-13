/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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

struct _SHADOW_GFX_STATUS
{
	BOOL gfxOpened;
	BOOL gfxSurfaceCreated;
};
typedef struct _SHADOW_GFX_STATUS SHADOW_GFX_STATUS;

static INLINE BOOL shadow_client_rdpgfx_new_surface(rdpShadowClient* client)
{
	UINT error = CHANNEL_RC_OK;
	RDPGFX_CREATE_SURFACE_PDU createSurface;
	RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU surfaceToOutput;
	RdpgfxServerContext* context = client->rdpgfx;
	rdpSettings* settings = ((rdpContext*) client)->settings;
	createSurface.width = settings->DesktopWidth;
	createSurface.height = settings->DesktopHeight;
	createSurface.pixelFormat = GFX_PIXEL_FORMAT_XRGB_8888;
	createSurface.surfaceId = 0;
	surfaceToOutput.outputOriginX = 0;
	surfaceToOutput.outputOriginY = 0;
	surfaceToOutput.surfaceId = 0;
	surfaceToOutput.reserved = 0;
	IFCALLRET(context->CreateSurface, error, context, &createSurface);

	if (error)
	{
		WLog_ERR(TAG, "CreateSurface failed with error %"PRIu32"", error);
		return FALSE;
	}

	IFCALLRET(context->MapSurfaceToOutput, error, context, &surfaceToOutput);

	if (error)
	{
		WLog_ERR(TAG, "MapSurfaceToOutput failed with error %"PRIu32"", error);
		return FALSE;
	}

	return TRUE;
}

static INLINE BOOL shadow_client_rdpgfx_release_surface(rdpShadowClient* client)
{
	UINT error = CHANNEL_RC_OK;
	RDPGFX_DELETE_SURFACE_PDU pdu;
	RdpgfxServerContext* context = client->rdpgfx;
	pdu.surfaceId = 0;
	IFCALLRET(context->DeleteSurface, error, context, &pdu);

	if (error)
	{
		WLog_ERR(TAG, "DeleteSurface failed with error %"PRIu32"", error);
		return FALSE;
	}

	return TRUE;
}

static INLINE BOOL shadow_client_rdpgfx_reset_graphic(rdpShadowClient* client)
{
	UINT error = CHANNEL_RC_OK;
	RDPGFX_RESET_GRAPHICS_PDU pdu;
	RdpgfxServerContext* context = client->rdpgfx;
	rdpSettings* settings = ((rdpContext*) client)->settings;
	pdu.width = settings->DesktopWidth;
	pdu.height = settings->DesktopHeight;
	pdu.monitorCount = client->subsystem->numMonitors;
	pdu.monitorDefArray = client->subsystem->monitors;
	IFCALLRET(context->ResetGraphics, error, context, &pdu);

	if (error)
	{
		WLog_ERR(TAG, "ResetGraphics failed with error %"PRIu32"", error);
		return FALSE;
	}

	return TRUE;
}

static INLINE void shadow_client_free_queued_message(void* obj)
{
	wMessage* message = (wMessage*)obj;

	if (message->Free)
	{
		message->Free(message);
		message->Free = NULL;
	}
}

static BOOL shadow_client_context_new(freerdp_peer* peer,
                                      rdpShadowClient* client)
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
	settings->SupportGraphicsPipeline = TRUE;
	settings->GfxH264 = FALSE;
	settings->DrawAllowSkipAlpha = TRUE;
	settings->DrawAllowColorSubsampling = TRUE;
	settings->DrawAllowDynamicColorFidelity = TRUE;
	settings->CompressionLevel = PACKET_COMPR_TYPE_RDP6;

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

static void shadow_client_context_free(freerdp_peer* peer,
                                       rdpShadowClient* client)
{
	rdpShadowServer* server = client->server;
	ArrayList_Remove(server->clients, (void*) client);

	if (client->encoder)
	{
		shadow_encoder_free(client->encoder);
		client->encoder = NULL;
	}

	/* Clear queued messages and free resource */
	MessageQueue_Clear(client->MsgQueue);
	MessageQueue_Free(client->MsgQueue);
	WTSCloseServer((HANDLE) client->vcm);
	client->vcm = NULL;
	region16_uninit(&(client->invalidRegion));
	DeleteCriticalSection(&(client->lock));
}

static void shadow_client_message_free(wMessage* message)
{
	switch (message->id)
	{
		case SHADOW_MSG_IN_REFRESH_REQUEST_ID:
			/* Refresh request do not have message to free */
			break;

		default:
			WLog_ERR(TAG, "Unknown message id: %"PRIu32"", message->id);
			free(message->wParam);
			break;
	}
}

static INLINE void shadow_client_mark_invalid(rdpShadowClient* client,
        int numRects, const RECTANGLE_16* rects)
{
	int index;
	RECTANGLE_16 screenRegion;
	rdpSettings* settings = ((rdpContext*) client)->settings;
	EnterCriticalSection(&(client->lock));

	/* Mark client invalid region. No rectangle means full screen */
	if (numRects > 0)
	{
		for (index = 0; index < numRects; index++)
		{
			region16_union_rect(&(client->invalidRegion), &(client->invalidRegion),
			                    &rects[index]);
		}
	}
	else
	{
		screenRegion.left = 0;
		screenRegion.top = 0;
		screenRegion.right = settings->DesktopWidth;
		screenRegion.bottom = settings->DesktopHeight;
		region16_union_rect(&(client->invalidRegion),
		                    &(client->invalidRegion), &screenRegion);
	}

	LeaveCriticalSection(&(client->lock));
}

/**
 * Function description
 * Recalculate client desktop size and update to rdpSettings
 *
 * @return TRUE if width/height changed.
 */
static INLINE BOOL shadow_client_recalc_desktop_size(rdpShadowClient* client)
{
	int width, height;
	rdpShadowServer* server = client->server;
	rdpSettings* settings = client->context.settings;
	RECTANGLE_16 viewport = {0, 0, server->surface->width, server->surface->height};

	if (server->shareSubRect)
	{
		rectangles_intersection(&viewport, &(server->subRect), &viewport);
	}

	width = viewport.right - viewport.left;
	height = viewport.bottom - viewport.top;

	if (settings->DesktopWidth != (UINT32)width
	    || settings->DesktopHeight != (UINT32)height)
	{
		settings->DesktopWidth = width;
		settings->DesktopHeight = height;
		return TRUE;
	}

	return FALSE;
}

static BOOL shadow_client_capabilities(freerdp_peer* peer)
{
	rdpShadowSubsystem* subsystem;
	rdpShadowClient* client;
	BOOL ret = TRUE;
	client = (rdpShadowClient*) peer->context;
	subsystem = client->server->subsystem;
	IFCALLRET(subsystem->ClientCapabilities, ret, subsystem, client);

	if (!ret)
		WLog_WARN(TAG, "subsystem->ClientCapabilities failed");

	/* Recalculate desktop size regardless whether previous call fail
	 * or not. Make sure we send correct width/height to client */
	(void)shadow_client_recalc_desktop_size(client);
	return ret;
}

static BOOL shadow_client_post_connect(freerdp_peer* peer)
{
	int authStatus;
	rdpSettings* settings;
	rdpShadowClient* client;
	rdpShadowServer* server;
	rdpShadowSubsystem* subsystem;
	client = (rdpShadowClient*) peer->context;
	settings = peer->settings;
	server = client->server;
	subsystem = server->subsystem;

	if (settings->ColorDepth == 24)
		settings->ColorDepth = 16; /* disable 24bpp */

	if (settings->MultifragMaxRequestSize < 0x3F0000)
		settings->NSCodec =
		    FALSE; /* NSCodec compressor does not support fragmentation yet */

	WLog_INFO(TAG, "Client from %s is activated (%"PRIu32"x%"PRIu32"@%"PRIu32")",
	          peer->hostname, settings->DesktopWidth,
	          settings->DesktopHeight, settings->ColorDepth);

	/* Resize client if necessary */
	if (shadow_client_recalc_desktop_size(client))
	{
		peer->update->DesktopResize(peer->update->context);
		WLog_INFO(TAG, "Client from %s is resized (%"PRIu32"x%"PRIu32"@%"PRIu32")",
		          peer->hostname, settings->DesktopWidth,
		          settings->DesktopHeight, settings->ColorDepth);
	}

	if (shadow_client_channels_post_connect(client) != CHANNEL_RC_OK)
		return FALSE;

	shadow_client_mark_invalid(client, 0, NULL);
	authStatus = -1;

	if (settings->Username && settings->Password)
		settings->AutoLogonEnabled = TRUE;

	if (server->authentication && !settings->NlaSecurity)
	{
		if (subsystem->Authenticate)
		{
			authStatus = subsystem->Authenticate(subsystem, client,
			                                     settings->Username, settings->Domain, settings->Password);
		}

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
        RECTANGLE_16* dst, const RECTANGLE_16* src, UINT32 numRects)
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
		if (src != dst)
		{
			CopyMemory(dst, src, numRects * sizeof(RECTANGLE_16));
		}
	}
}

static BOOL shadow_client_refresh_request(rdpShadowClient* client)
{
	wMessage message = { 0 };
	wMessagePipe* MsgPipe = client->subsystem->MsgPipe;
	message.id = SHADOW_MSG_IN_REFRESH_REQUEST_ID;
	message.wParam = NULL;
	message.lParam = NULL;
	message.context = (void*) client;
	message.Free = NULL;
	return MessageQueue_Dispatch(MsgPipe->In, &message);
}

static BOOL shadow_client_refresh_rect(rdpShadowClient* client, BYTE count,
                                       RECTANGLE_16* areas)
{
	RECTANGLE_16* rects;

	/* It is invalid if we have area count but no actual area */
	if (count && !areas)
		return FALSE;

	if (count)
	{
		rects = (RECTANGLE_16*) calloc(count, sizeof(RECTANGLE_16));

		if (!rects)
		{
			return FALSE;
		}

		shadow_client_convert_rects(client, rects, areas, count);
		shadow_client_mark_invalid(client, count, rects);
		free(rects);
	}
	else
	{
		shadow_client_mark_invalid(client, 0, NULL);
	}

	return shadow_client_refresh_request(client);
}

static BOOL shadow_client_suppress_output(rdpShadowClient* client, BYTE allow,
        RECTANGLE_16* area)
{
	RECTANGLE_16 region;
	client->suppressOutput = allow ? FALSE : TRUE;

	if (allow)
	{
		if (area)
		{
			shadow_client_convert_rects(client, &region, area, 1);
			shadow_client_mark_invalid(client, 1, &region);
		}
		else
		{
			shadow_client_mark_invalid(client, 0, NULL);
		}
	}

	return shadow_client_refresh_request(client);
}

static BOOL shadow_client_activate(freerdp_peer* peer)
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

	if (shadow_encoder_reset(client->encoder) < 0)
	{
		WLog_ERR(TAG, "Failed to reset encoder");
		return FALSE;
	}

	/* Update full screen in next update */
	return shadow_client_refresh_rect(client, 0, NULL);
}

static BOOL shadow_client_logon(freerdp_peer* peer,
                                SEC_WINNT_AUTH_IDENTITY* identity,
                                BOOL automatic)
{
	char* user = NULL;
	char* domain = NULL;
	char* password = NULL;
	rdpSettings* settings = peer->settings;

	if (identity->Flags & SEC_WINNT_AUTH_IDENTITY_UNICODE)
	{
		if (identity->User)
			ConvertFromUnicode(CP_UTF8, 0, identity->User, identity->UserLength, &user, 0,
			                   NULL, NULL);

		if (identity->Domain)
			ConvertFromUnicode(CP_UTF8, 0, identity->Domain, identity->DomainLength,
			                   &domain, 0, NULL, NULL);

		if (identity->Password)
			ConvertFromUnicode(CP_UTF8, 0, identity->Password, identity->PasswordLength,
			                   &user, 0, NULL, NULL);
	}
	else
	{
		if (identity->User)
			user = _strdup((char*) identity->User);

		if (identity->Domain)
			domain = _strdup((char*) identity->Domain);

		if (identity->Password)
			password = _strdup((char*) identity->Password);
	}

	if ((identity->User && !user) || (identity->Domain && !domain)
	    || (identity->Password && !password))
	{
		free(user);
		free(domain);
		free(password);
		return FALSE;
	}

	if (user)
	{
		free(settings->Username);
		settings->Username = user;
		user = NULL;
	}

	if (domain)
	{
		free(settings->Domain);
		settings->Domain = domain;
		domain = NULL;
	}

	if (password)
	{
		free(settings->Password);
		settings->Password = password;
		password = NULL;
	}

	free(user);
	free(domain);
	free(password);
	return TRUE;
}

static INLINE void shadow_client_common_frame_acknowledge(
    rdpShadowClient* client, UINT32 frameId)
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
}

static BOOL shadow_client_surface_frame_acknowledge(rdpShadowClient* client,
        UINT32 frameId)
{
	shadow_client_common_frame_acknowledge(client, frameId);
	/*
	 * Reset queueDepth for legacy none RDPGFX acknowledge
	 */
	client->encoder->queueDepth = QUEUE_DEPTH_UNAVAILABLE;
	return TRUE;
}

static UINT shadow_client_rdpgfx_frame_acknowledge(RdpgfxServerContext* context,
        RDPGFX_FRAME_ACKNOWLEDGE_PDU* frameAcknowledge)
{
	rdpShadowClient* client = (rdpShadowClient*)context->custom;
	shadow_client_common_frame_acknowledge(client, frameAcknowledge->frameId);
	client->encoder->queueDepth = frameAcknowledge->queueDepth;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT shadow_client_rdpgfx_caps_advertise(RdpgfxServerContext* context,
        RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise)
{
	UINT16 index;
	rdpSettings* settings = context->rdpcontext->settings;
	UINT32 flags = 0;
	/* Request full screen update for new gfx channel */
	shadow_client_refresh_rect((rdpShadowClient*)context->custom, 0, NULL);

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

		if (currentCaps->version == RDPGFX_CAPVERSION_103)
		{
			RDPGFX_CAPSET caps = *currentCaps;
			RDPGFX_CAPS_CONFIRM_PDU pdu;
			pdu.capsSet = &caps;

			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
#ifndef WITH_GFX_H264
				settings->GfxH264 = FALSE;
				pdu.capsSet->flags |= RDPGFX_CAPS_FLAG_AVC_DISABLED;
#else
				settings->GfxAVC444 = settings->GfxH264 = !(flags & RDPGFX_CAPS_FLAG_AVC_DISABLED);
#endif
			}

			return context->CapsConfirm(context, &pdu);
		}
	}

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

		if (currentCaps->version == RDPGFX_CAPVERSION_102)
		{
			RDPGFX_CAPSET caps = *currentCaps;
			RDPGFX_CAPS_CONFIRM_PDU pdu;
			pdu.capsSet = &caps;

			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
#ifndef WITH_GFX_H264
				settings->GfxH264 = FALSE;
				pdu.capsSet->flags |= RDPGFX_CAPS_FLAG_AVC_DISABLED;
#else
				settings->GfxAVC444 = settings->GfxH264 = !(flags & RDPGFX_CAPS_FLAG_AVC_DISABLED);
#endif
			}

			return context->CapsConfirm(context, &pdu);
		}
	}

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

		if (currentCaps->version == RDPGFX_CAPVERSION_10)
		{
			RDPGFX_CAPSET caps = *currentCaps;
			RDPGFX_CAPS_CONFIRM_PDU pdu;
			pdu.capsSet = &caps;

			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
#ifndef WITH_GFX_H264
				settings->GfxH264 = FALSE;
				pdu.capsSet->flags |= RDPGFX_CAPS_FLAG_AVC_DISABLED;
#else
				settings->GfxAVC444 = settings->GfxH264 = !(flags & RDPGFX_CAPS_FLAG_AVC_DISABLED);
#endif
			}

			return context->CapsConfirm(context, &pdu);
		}
	}

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

		if (currentCaps->version == RDPGFX_CAPVERSION_81)
		{			
			RDPGFX_CAPSET caps = *currentCaps;
			RDPGFX_CAPS_CONFIRM_PDU pdu;
			pdu.capsSet = &caps;

			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxAVC444 = FALSE;
				settings->GfxThinClient = (flags & RDPGFX_CAPS_FLAG_THINCLIENT);
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);				
#ifndef WITH_GFX_H264
				settings->GfxH264 = FALSE;
				pdu.capsSet->flags &= ~RDPGFX_CAPS_FLAG_AVC420_ENABLED;
#else
				settings->GfxH264 = (flags & RDPGFX_CAPS_FLAG_AVC420_ENABLED);
#endif
			}

			return context->CapsConfirm(context, &pdu);
		}
	}

	for (index = 0; index < capsAdvertise->capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

		if (currentCaps->version == RDPGFX_CAPVERSION_8)
		{
			RDPGFX_CAPSET caps = *currentCaps;
			RDPGFX_CAPS_CONFIRM_PDU pdu;
			pdu.capsSet = &caps;

			if (settings)
			{
				flags = pdu.capsSet->flags;
				settings->GfxThinClient = (flags & RDPGFX_CAPS_FLAG_THINCLIENT);
				settings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);
			}

			return context->CapsConfirm(context, &pdu);
		}
	}

	return CHANNEL_RC_UNSUPPORTED_VERSION;
}

static INLINE UINT32 rdpgfx_estimate_h264_avc420(
    RDPGFX_AVC420_BITMAP_STREAM* havc420)
{
	/* H264 metadata + H264 stream. See rdpgfx_write_h264_avc420 */
	return sizeof(UINT32) /* numRegionRects */
	       + 10 /* regionRects + quantQualityVals */
	       * havc420->meta.numRegionRects
	       + havc420->length;
}

/**
 * Function description
 *
 * @return TRUE on success
 */
static BOOL shadow_client_send_surface_gfx(rdpShadowClient* client,
        const BYTE* pSrcData, int nSrcStep, int nXSrc, int nYSrc, int nWidth, int nHeight)
{
	UINT error = CHANNEL_RC_OK;
	rdpContext* context = (rdpContext*) client;
	rdpSettings* settings;
	rdpShadowEncoder* encoder;
	RDPGFX_SURFACE_COMMAND cmd;
	RDPGFX_START_FRAME_PDU cmdstart;
	RDPGFX_END_FRAME_PDU cmdend;
	SYSTEMTIME sTime;

	if (!context || !pSrcData)
		return FALSE;

	settings = context->settings;
	encoder = client->encoder;

	if (!settings || !encoder)
		return FALSE;

	cmdstart.frameId = shadow_encoder_create_frame_id(encoder);
	GetSystemTime(&sTime);
	cmdstart.timestamp = sTime.wHour << 22 | sTime.wMinute << 16 |
	                     sTime.wSecond << 10 | sTime.wMilliseconds;
	cmdend.frameId = cmdstart.frameId;
	cmd.surfaceId = 0;
	cmd.codecId = 0;
	cmd.contextId = 0;
	cmd.format = PIXEL_FORMAT_BGRX32;
	cmd.left = nXSrc;
	cmd.top = nYSrc;
	cmd.right = cmd.left + nWidth;
	cmd.bottom = cmd.top + nHeight;
	cmd.width = nWidth;
	cmd.height = nHeight;
	cmd.length = 0;
	cmd.data = NULL;
	cmd.extra = NULL;

	if (settings->GfxAVC444)
	{
		RDPGFX_AVC444_BITMAP_STREAM avc444;
		RECTANGLE_16 regionRect;
		RDPGFX_H264_QUANT_QUALITY quantQualityVal;

		if (shadow_encoder_prepare(encoder, FREERDP_CODEC_AVC444) < 0)
		{
			WLog_ERR(TAG, "Failed to prepare encoder FREERDP_CODEC_AVC444");
			return FALSE;
		}

		if (avc420_compress(encoder->h264, pSrcData, cmd.format, nSrcStep,
		                    nWidth, nHeight, &avc444.bitstream[0].data,
		                    &avc444.bitstream[0].length) < 0)
		{
			WLog_ERR(TAG, "avc420_compress failed for avc444");
			return FALSE;
		}

		regionRect.left = cmd.left;
		regionRect.top = cmd.top;
		regionRect.right = cmd.right;
		regionRect.bottom = cmd.bottom;
		quantQualityVal.qp = encoder->h264->QP;
		quantQualityVal.r = 0;
		quantQualityVal.p = 0;
		quantQualityVal.qualityVal = 100 - quantQualityVal.qp;
		avc444.bitstream[0].meta.numRegionRects = 1;
		avc444.bitstream[0].meta.regionRects = &regionRect;
		avc444.bitstream[0].meta.quantQualityVals = &quantQualityVal;
		avc444.LC = 1;
		avc444.cbAvc420EncodedBitstream1 = rdpgfx_estimate_h264_avc420(&avc444.bitstream[0]);

		cmd.codecId = RDPGFX_CODECID_AVC444;
		cmd.extra = (void*)&avc444;

		IFCALLRET(client->rdpgfx->SurfaceFrameCommand, error, client->rdpgfx, &cmd,
		          &cmdstart, &cmdend);

		if (error)
		{
			WLog_ERR(TAG, "SurfaceFrameCommand failed with error %"PRIu32"", error);
			return FALSE;
		}
	}
	else if (settings->GfxH264)
	{
		RDPGFX_AVC420_BITMAP_STREAM avc420;
		RECTANGLE_16 regionRect;
		RDPGFX_H264_QUANT_QUALITY quantQualityVal;

		if (shadow_encoder_prepare(encoder, FREERDP_CODEC_AVC420) < 0)
		{
			WLog_ERR(TAG, "Failed to prepare encoder FREERDP_CODEC_AVC420");
			return FALSE;
		}

		if (avc420_compress(encoder->h264, pSrcData, cmd.format, nSrcStep,
		                nWidth, nHeight, &avc420.data, &avc420.length) < 0)
		{
			WLog_ERR(TAG, "avc420_compress failed");
			return FALSE;
		}

		cmd.codecId = RDPGFX_CODECID_AVC420;
		cmd.extra = (void*)&avc420;
		regionRect.left = cmd.left;
		regionRect.top = cmd.top;
		regionRect.right = cmd.right;
		regionRect.bottom = cmd.bottom;
		quantQualityVal.qp = encoder->h264->QP;
		quantQualityVal.r = 0;
		quantQualityVal.p = 0;
		quantQualityVal.qualityVal = 100 - quantQualityVal.qp;
		avc420.meta.numRegionRects = 1;
		avc420.meta.regionRects = &regionRect;
		avc420.meta.quantQualityVals = &quantQualityVal;
		IFCALLRET(client->rdpgfx->SurfaceFrameCommand, error, client->rdpgfx, &cmd,
		          &cmdstart, &cmdend);

		if (error)
		{
			WLog_ERR(TAG, "SurfaceFrameCommand failed with error %"PRIu32"", error);
			return FALSE;
		}
	}

	return TRUE;
}

/**
 * Function description
 *
 * @return TRUE on success
 */
static BOOL shadow_client_send_surface_bits(rdpShadowClient* client,
        BYTE* pSrcData, int nSrcStep, int nXSrc, int nYSrc, int nWidth, int nHeight)
{
	BOOL ret = TRUE;
	int i;
	BOOL first;
	BOOL last;
	wStream* s;
	int numMessages;
	UINT32 frameId = 0;
	rdpUpdate* update;
	rdpContext* context = (rdpContext*) client;
	rdpSettings* settings;
	rdpShadowEncoder* encoder;
	SURFACE_BITS_COMMAND cmd;

	if (!context || !pSrcData)
		return FALSE;

	update = context->update;
	settings = context->settings;
	encoder = client->encoder;

	if (!update || !settings || !encoder)
		return FALSE;

	if (encoder->frameAck)
		frameId = shadow_encoder_create_frame_id(encoder);

	if (settings->RemoteFxCodec)
	{
		RFX_RECT rect;
		RFX_MESSAGE* messages;
		RFX_RECT* messageRects = NULL;

		if (shadow_encoder_prepare(encoder, FREERDP_CODEC_REMOTEFX) < 0)
		{
			WLog_ERR(TAG, "Failed to prepare encoder FREERDP_CODEC_REMOTEFX");
			return FALSE;
		}

		s = encoder->bs;
		rect.x = nXSrc;
		rect.y = nYSrc;
		rect.width = nWidth;
		rect.height = nHeight;

		if (!(messages = rfx_encode_messages(encoder->rfx, &rect, 1, pSrcData,
		                                     settings->DesktopWidth, settings->DesktopHeight, nSrcStep, &numMessages,
		                                     settings->MultifragMaxRequestSize)))
		{
			WLog_ERR(TAG, "rfx_encode_messages failed");
			return FALSE;
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

				WLog_ERR(TAG, "rfx_write_message failed");
				ret = FALSE;
				break;
			}

			rfx_message_free(encoder->rfx, &messages[i]);
			cmd.bitmapDataLength = Stream_GetPosition(s);
			cmd.bitmapData = Stream_Buffer(s);
			first = (i == 0) ? TRUE : FALSE;
			last = ((i + 1) == numMessages) ? TRUE : FALSE;

			if (!encoder->frameAck)
				IFCALLRET(update->SurfaceBits, ret, update->context, &cmd);
			else
				IFCALLRET(update->SurfaceFrameBits, ret, update->context, &cmd, first, last,
				          frameId);

			if (!ret)
			{
				WLog_ERR(TAG, "Send surface bits(RemoteFxCodec) failed");
				break;
			}
		}

		free(messageRects);
		free(messages);
	}
	else if (settings->NSCodec)
	{
		if (shadow_encoder_prepare(encoder, FREERDP_CODEC_NSCODEC) < 0)
		{
			WLog_ERR(TAG, "Failed to prepare encoder FREERDP_CODEC_NSCODEC");
			return FALSE;
		}

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
			IFCALLRET(update->SurfaceBits, ret, update->context, &cmd);
		else
			IFCALLRET(update->SurfaceFrameBits, ret, update->context, &cmd, first, last,
			          frameId);

		if (!ret)
		{
			WLog_ERR(TAG, "Send surface bits(NSCodec) failed");
		}
	}

	return ret;
}

/**
 * Function description
 *
 * @return TRUE on success
 */
static BOOL shadow_client_send_bitmap_update(rdpShadowClient* client,
        BYTE* pSrcData, int nSrcStep, int nXSrc, int nYSrc, int nWidth, int nHeight)
{
	BOOL ret = TRUE;
	BYTE* data;
	BYTE* buffer;
	int yIdx, xIdx, k;
	int rows, cols;
	UINT32 DstSize;
	UINT32 SrcFormat;
	BITMAP_DATA* bitmap;
	rdpUpdate* update;
	rdpContext* context = (rdpContext*) client;
	rdpSettings* settings;
	UINT32 maxUpdateSize;
	UINT32 totalBitmapSize;
	UINT32 updateSizeEstimate;
	BITMAP_DATA* bitmapData;
	BITMAP_UPDATE bitmapUpdate;
	rdpShadowEncoder* encoder;

	if (!context || !pSrcData)
		return FALSE;

	update = context->update;
	settings = context->settings;
	encoder = client->encoder;

	if (!update || !settings || !encoder)
		return FALSE;

	maxUpdateSize = settings->MultifragMaxRequestSize;

	if (settings->ColorDepth < 32)
	{
		if (shadow_encoder_prepare(encoder, FREERDP_CODEC_INTERLEAVED) < 0)
		{
			WLog_ERR(TAG, "Failed to prepare encoder FREERDP_CODEC_INTERLEAVED");
			return FALSE;
		}
	}
	else
	{
		if (shadow_encoder_prepare(encoder, FREERDP_CODEC_PLANAR) < 0)
		{
			WLog_ERR(TAG, "Failed to prepare encoder FREERDP_CODEC_PLANAR");
			return FALSE;
		}
	}

	SrcFormat = PIXEL_FORMAT_BGRX32;

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

	if (!(bitmapData = (BITMAP_DATA*) calloc(bitmapUpdate.number, sizeof(BITMAP_DATA))))
		return FALSE;

	bitmapUpdate.rectangles = bitmapData;

	if ((nWidth % 4) != 0)
	{
		nWidth += (4 - (nWidth % 4));
	}

	if ((nHeight % 4) != 0)
	{
		nHeight += (4 - (nHeight % 4));
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
				interleaved_compress(encoder->interleaved, buffer, &DstSize, bitmap->width,
				                     bitmap->height,
				                     pSrcData, SrcFormat, nSrcStep, bitmap->destLeft, bitmap->destTop, NULL,
				                     bitsPerPixel);
				bitmap->bitmapDataStream = buffer;
				bitmap->bitmapLength = DstSize;
				bitmap->bitsPerPixel = bitsPerPixel;
				bitmap->cbScanWidth = bitmap->width * bytesPerPixel;
				bitmap->cbUncompressedSize = bitmap->width * bitmap->height * bytesPerPixel;
			}
			else
			{
				UINT32 dstSize;
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
			fragBitmapData = (BITMAP_DATA*) calloc(k, sizeof(BITMAP_DATA));

		if (!fragBitmapData)
		{
			WLog_ERR(TAG, "Failed to allocate memory for fragBitmapData");
			ret = FALSE;
			goto out;
		}

		bitmapUpdate.rectangles = fragBitmapData;
		i = j = 0;
		updateSize = 1024;

		while (i < k)
		{
			newUpdateSize = updateSize + (bitmapData[i].bitmapLength + 16);

			if (newUpdateSize < maxUpdateSize)
			{
				CopyMemory(&fragBitmapData[j++], &bitmapData[i++], sizeof(BITMAP_DATA));
				updateSize = newUpdateSize;
			}

			if ((newUpdateSize >= maxUpdateSize) || (i + 1) >= k)
			{
				bitmapUpdate.count = bitmapUpdate.number = j;
				IFCALLRET(update->BitmapUpdate, ret, context, &bitmapUpdate);

				if (!ret)
				{
					WLog_ERR(TAG, "BitmapUpdate failed");
					break;
				}

				updateSize = 1024;
				j = 0;
			}
		}

		free(fragBitmapData);
	}
	else
	{
		IFCALLRET(update->BitmapUpdate, ret, context, &bitmapUpdate);

		if (!ret)
		{
			WLog_ERR(TAG, "BitmapUpdate failed");
		}
	}

out:
	free(bitmapData);
	return ret;
}

/**
 * Function description
 *
 * @return TRUE on success (or nothing need to be updated)
 */
static BOOL shadow_client_send_surface_update(rdpShadowClient* client,
        SHADOW_GFX_STATUS* pStatus)
{
	BOOL ret = TRUE;
	int nXSrc, nYSrc;
	int nWidth, nHeight;
	rdpContext* context = (rdpContext*) client;
	rdpSettings* settings;
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	REGION16 invalidRegion;
	RECTANGLE_16 surfaceRect;
	const RECTANGLE_16* extents;
	BYTE* pSrcData;
	int nSrcStep;
	int index;
	UINT32 numRects = 0;
	const RECTANGLE_16* rects;

	if (!context || !pStatus)
		return FALSE;

	settings = context->settings;
	server = client->server;

	if (!settings || !server)
		return FALSE;

	surface = client->inLobby ? server->lobby : server->surface;

	if (!surface)
		return FALSE;

	EnterCriticalSection(&(client->lock));
	region16_init(&invalidRegion);
	region16_copy(&invalidRegion, &(client->invalidRegion));
	region16_clear(&(client->invalidRegion));
	LeaveCriticalSection(&(client->lock));
	rects = region16_rects(&(surface->invalidRegion), &numRects);

	for (index = 0; index < numRects; index++)
	{
		region16_union_rect(&invalidRegion, &invalidRegion, &rects[index]);
	}

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
		/* No image region need to be updated. Success */
		goto out;
	}

	extents = region16_extents(&invalidRegion);
	nXSrc = extents->left;
	nYSrc = extents->top;
	nWidth = extents->right - extents->left;
	nHeight = extents->bottom - extents->top;
	pSrcData = surface->data;
	nSrcStep = surface->scanline;

	/* Move to new pSrcData / nXSrc / nYSrc according to sub rect */
	if (server->shareSubRect)
	{
		int subX, subY;
		subX = server->subRect.left;
		subY = server->subRect.top;
		nXSrc -= subX;
		nYSrc -= subY;
		pSrcData = &pSrcData[(subY * nSrcStep) + (subX * 4)];
	}

	//WLog_INFO(TAG, "shadow_client_send_surface_update: x: %d y: %d width: %d height: %d right: %d bottom: %d",
	//	nXSrc, nYSrc, nWidth, nHeight, nXSrc + nWidth, nYSrc + nHeight);

	if (settings->SupportGraphicsPipeline &&
	    settings->GfxH264 &&
	    pStatus->gfxOpened)
	{
		/* GFX/h264 always full screen encoded */
		nWidth = settings->DesktopWidth;
		nHeight = settings->DesktopHeight;

		/* Create primary surface if have not */
		if (!pStatus->gfxSurfaceCreated)
		{
			/* Only init surface when we have h264 supported */
			if (!(ret = shadow_client_rdpgfx_reset_graphic(client)))
				goto out;

			if (!(ret = shadow_client_rdpgfx_new_surface(client)))
				goto out;

			pStatus->gfxSurfaceCreated = TRUE;
		}

		ret = shadow_client_send_surface_gfx(client, pSrcData, nSrcStep, 0, 0, nWidth,
		                                     nHeight);
	}
	else if (settings->RemoteFxCodec || settings->NSCodec)
	{
		ret = shadow_client_send_surface_bits(client, pSrcData, nSrcStep, nXSrc, nYSrc,
		                                      nWidth, nHeight);
	}
	else
	{
		ret = shadow_client_send_bitmap_update(client, pSrcData, nSrcStep, nXSrc, nYSrc,
		                                       nWidth, nHeight);
	}

out:
	region16_uninit(&invalidRegion);
	return ret;
}

/**
 * Function description
 * Notify client for resize. The new desktop width/height
 * should have already been updated in rdpSettings.
 *
 * @return TRUE on success
 */
static BOOL shadow_client_send_resize(rdpShadowClient* client,
                                      SHADOW_GFX_STATUS* pStatus)
{
	rdpContext* context = (rdpContext*) client;
	rdpSettings* settings;
	freerdp_peer* peer;

	if (!context || !pStatus)
		return FALSE;

	peer = context->peer;
	settings = context->settings;

	if (!peer || !settings)
		return FALSE;

	/**
	 * Unset client activated flag to avoid sending update message during
	 * resize. DesktopResize will reactive the client and
	 * shadow_client_activate would be invoked later.
	 */
	client->activated = FALSE;

	/* Close Gfx surfaces */
	if (pStatus->gfxSurfaceCreated)
	{
		if (!shadow_client_rdpgfx_release_surface(client))
			return FALSE;

		pStatus->gfxSurfaceCreated = FALSE;
	}

	/* Send Resize */
	if (!peer->update->DesktopResize(peer->update->context))
	{
		WLog_ERR(TAG, "DesktopResize failed");
		return FALSE;
	}

	/* Clear my invalidRegion. shadow_client_activate refreshes fullscreen */
	EnterCriticalSection(&(client->lock));
	region16_clear(&(client->invalidRegion));
	LeaveCriticalSection(&(client->lock));
	WLog_INFO(TAG, "Client from %s is resized (%"PRIu32"x%"PRIu32"@%"PRIu32")",
	          peer->hostname, settings->DesktopWidth, settings->DesktopHeight,
	          settings->ColorDepth);
	return TRUE;
}

/**
 * Function description
 * Mark invalid region for client
 *
 * @return TRUE on success
 */
BOOL shadow_client_surface_update(rdpShadowClient* client, REGION16* region)
{
	UINT32 numRects = 0;
	const RECTANGLE_16* rects;
	rects = region16_rects(region, &numRects);
	shadow_client_mark_invalid(client, numRects, rects);
	return TRUE;
}

/**
 * Function description
 * Only union invalid region from server surface
 *
 * @return TRUE on success
 */
static INLINE BOOL shadow_client_no_surface_update(rdpShadowClient* client,
        SHADOW_GFX_STATUS* pStatus)
{
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	server = client->server;
	surface = client->inLobby ? server->lobby : server->surface;
	return shadow_client_surface_update(client, &(surface->invalidRegion));
}

static int shadow_client_subsystem_process_message(rdpShadowClient* client,
        wMessage* message)
{
	rdpContext* context = (rdpContext*) client;
	rdpUpdate* update = context->update;

	/* FIXME: the pointer updates appear to be broken when used with bulk compression and mstsc */

	switch (message->id)
	{
		case SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID:
			{
				POINTER_POSITION_UPDATE pointerPosition;
				SHADOW_MSG_OUT_POINTER_POSITION_UPDATE* msg =
				    (SHADOW_MSG_OUT_POINTER_POSITION_UPDATE*) message->wParam;
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
				SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* msg =
				    (SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE*) message->wParam;
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
				SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES* msg = (SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES*)
				                                        message->wParam;

				if (client->activated && client->rdpsnd && client->rdpsnd->Activated)
				{
					client->rdpsnd->src_format = msg->audio_format;
					IFCALL(client->rdpsnd->SendSamples, client->rdpsnd, msg->buf, msg->nFrames,
					       msg->wTimestamp);
				}

				break;
			}

		case SHADOW_MSG_OUT_AUDIO_OUT_VOLUME_ID:
			{
				SHADOW_MSG_OUT_AUDIO_OUT_VOLUME* msg = (SHADOW_MSG_OUT_AUDIO_OUT_VOLUME*)
				                                       message->wParam;

				if (client->activated && client->rdpsnd && client->rdpsnd->Activated)
				{
					IFCALL(client->rdpsnd->SetVolume, client->rdpsnd, msg->left, msg->right);
				}

				break;
			}

		default:
			WLog_ERR(TAG, "Unknown message id: %"PRIu32"", message->id);
			break;
	}

	shadow_client_free_queued_message(message);
	return 1;
}

static void* shadow_client_thread(rdpShadowClient* client)
{
	DWORD status;
	DWORD nCount;
	wMessage message;
	wMessage pointerPositionMsg;
	wMessage pointerAlphaMsg;
	wMessage audioVolumeMsg;
	HANDLE events[32];
	HANDLE ChannelEvent;
	void* UpdateSubscriber;
	HANDLE UpdateEvent;
	freerdp_peer* peer;
	rdpContext* context;
	rdpSettings* settings;
	rdpShadowServer* server;
	rdpShadowSubsystem* subsystem;
	wMessageQueue* MsgQueue = client->MsgQueue;
	/* This should only be visited in client thread */
	SHADOW_GFX_STATUS gfxstatus;
	gfxstatus.gfxOpened = FALSE;
	gfxstatus.gfxSurfaceCreated = FALSE;
	server = client->server;
	subsystem = server->subsystem;
	context = (rdpContext*) client;
	peer = context->peer;
	settings = peer->settings;
	peer->Capabilities = shadow_client_capabilities;
	peer->PostConnect = shadow_client_post_connect;
	peer->Activate = shadow_client_activate;
	peer->Logon = shadow_client_logon;
	shadow_input_register_callbacks(peer->input);
	peer->Initialize(peer);
	peer->update->RefreshRect = (pRefreshRect)shadow_client_refresh_rect;
	peer->update->SuppressOutput = (pSuppressOutput)shadow_client_suppress_output;
	peer->update->SurfaceFrameAcknowledge = (pSurfaceFrameAcknowledge)
	                                        shadow_client_surface_frame_acknowledge;

	if ((!client->vcm) || (!subsystem->updateEvent))
		goto out;

	UpdateSubscriber = shadow_multiclient_get_subscriber(subsystem->updateEvent);

	if (!UpdateSubscriber)
		goto out;

	UpdateEvent = shadow_multiclient_getevent(UpdateSubscriber);
	ChannelEvent = WTSVirtualChannelManagerGetEventHandle(client->vcm);

	while (1)
	{
		nCount = 0;
		events[nCount++] = UpdateEvent;
		{
			DWORD tmp = peer->GetEventHandles(peer, &events[nCount], 64 - nCount);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "Failed to get FreeRDP transport event handles");
				break;
			}

			nCount += tmp;
		}
		events[nCount++] = ChannelEvent;
		events[nCount++] = MessageQueue_Event(MsgQueue);
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
			break;

		if (WaitForSingleObject(UpdateEvent, 0) == WAIT_OBJECT_0)
		{
			/* The UpdateEvent means to start sending current frame. It is
			 * triggered from subsystem implementation and it should ensure
			 * that the screen and primary surface meta data (width, height,
			 * scanline, invalid region, etc) is not changed until it is reset
			 * (at shadow_multiclient_consume). As best practice, subsystem
			 * implementation should invoke shadow_subsystem_frame_update which
			 * triggers the event and then wait for completion */
			if (client->activated && !client->suppressOutput)
			{
				/* Send screen update or resize to this client */

				/* Check resize */
				if (shadow_client_recalc_desktop_size(client))
				{
					/* Screen size changed, do resize */
					if (!shadow_client_send_resize(client, &gfxstatus))
					{
						WLog_ERR(TAG, "Failed to send resize message");
						break;
					}
				}
				else
				{
					/* Send frame */
					if (!shadow_client_send_surface_update(client, &gfxstatus))
					{
						WLog_ERR(TAG, "Failed to send surface update");
						break;
					}
				}
			}
			else
			{
				/* Our client don't receive graphic updates. Just save the invalid region */
				if (!shadow_client_no_surface_update(client, &gfxstatus))
				{
					WLog_ERR(TAG, "Failed to handle surface update");
					break;
				}
			}

			/*
			 * The return value of shadow_multiclient_consume is whether or not
			 * the subscriber really consumes the event. It's not cared currently.
			 */
			(void)shadow_multiclient_consume(UpdateSubscriber);
		}

		if (!peer->CheckFileDescriptor(peer))
		{
			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			break;
		}
		else
		{
			if (WTSVirtualChannelManagerIsChannelJoined(client->vcm, "drdynvc"))
			{
				/* Dynamic channel status may have been changed after processing */
				if (WTSVirtualChannelManagerGetDrdynvcState(client->vcm) == DRDYNVC_STATE_NONE)
				{
					/* Call this routine to Initialize drdynvc channel */
					if (!WTSVirtualChannelManagerCheckFileDescriptor(client->vcm))
					{
						WLog_ERR(TAG, "Failed to initialize drdynvc channel");
						break;
					}
				}
				else if (WTSVirtualChannelManagerGetDrdynvcState(client->vcm) ==
				         DRDYNVC_STATE_READY)
				{
					/* Init RDPGFX dynamic channel */
					if (settings->SupportGraphicsPipeline && client->rdpgfx &&
					    !gfxstatus.gfxOpened)
					{
						client->rdpgfx->FrameAcknowledge = shadow_client_rdpgfx_frame_acknowledge;
						client->rdpgfx->CapsAdvertise = shadow_client_rdpgfx_caps_advertise;

						if (!client->rdpgfx->Open(client->rdpgfx))
						{
							WLog_WARN(TAG, "Failed to open GraphicsPipeline");
							settings->SupportGraphicsPipeline = FALSE;
						}

						gfxstatus.gfxOpened = TRUE;
						WLog_INFO(TAG, "Gfx Pipeline Opened");
					}
				}
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
			pointerPositionMsg.Free = NULL;
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

				switch (message.id)
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
	if (gfxstatus.gfxOpened)
	{
		if (gfxstatus.gfxSurfaceCreated)
		{
			if (!shadow_client_rdpgfx_release_surface(client))
				WLog_WARN(TAG, "GFX release surface failure!");
		}

		(void)client->rdpgfx->Close(client->rdpgfx);
	}

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

	if (!listener || !peer)
		return FALSE;

	server = (rdpShadowServer*) listener->info;
	peer->ContextExtra = (void*) server;
	peer->ContextSize = sizeof(rdpShadowClient);
	peer->ContextNew = (psPeerContextNew) shadow_client_context_new;
	peer->ContextFree = (psPeerContextFree) shadow_client_context_free;
	peer->settings = freerdp_settings_clone(server->settings);

	if (!freerdp_peer_context_new(peer))
		return FALSE;

	client = (rdpShadowClient*) peer->context;

	if (!(client->thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)
	                                    shadow_client_thread, client, 0, NULL)))
	{
		freerdp_peer_context_free(peer);
		return FALSE;
	}
	else
	{
		/* Close the thread handle to make it detached. */
		CloseHandle(client->thread);
		client->thread = NULL;
	}

	return TRUE;
}

static void shadow_msg_out_addref(wMessage* message)
{
	SHADOW_MSG_OUT* msg = (SHADOW_MSG_OUT*)message->wParam;
	InterlockedIncrement(&(msg->refCount));
}

static void shadow_msg_out_release(wMessage* message)
{
	SHADOW_MSG_OUT* msg = (SHADOW_MSG_OUT*)message->wParam;

	if (InterlockedDecrement(&(msg->refCount)) <= 0)
	{
		if (msg->Free)
			msg->Free(message->id, msg);
	}
}

static BOOL shadow_client_dispatch_msg(rdpShadowClient* client,
                                       wMessage* message)
{
	if (!client || !message)
		return FALSE;

	/* Add reference when it is posted */
	shadow_msg_out_addref(message);

	if (MessageQueue_Dispatch(client->MsgQueue, message))
		return TRUE;
	else
	{
		/* Release the reference since post failed */
		shadow_msg_out_release(message);
		return FALSE;
	}
}

BOOL shadow_client_post_msg(rdpShadowClient* client, void* context, UINT32 type,
                            SHADOW_MSG_OUT* msg, void* lParam)
{
	wMessage message = {0};
	message.context = context;
	message.id = type;
	message.wParam = (void*)msg;
	message.lParam = lParam;
	message.Free = shadow_msg_out_release;
	return shadow_client_dispatch_msg(client, &message);
}

int shadow_client_boardcast_msg(rdpShadowServer* server, void* context,
                                UINT32 type, SHADOW_MSG_OUT* msg, void* lParam)
{
	wMessage message = {0};
	rdpShadowClient* client = NULL;
	int count = 0;
	int index = 0;
	message.context = context;
	message.id = type;
	message.wParam = (void*)msg;
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
