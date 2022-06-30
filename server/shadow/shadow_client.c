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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/sysinfo.h>
#include <winpr/interlocked.h>

#include <freerdp/log.h>
#include <freerdp/channels/drdynvc.h>

#include "shadow.h"

#define TAG CLIENT_TAG("shadow")

typedef struct
{
	BOOL gfxOpened;
	BOOL gfxSurfaceCreated;
} SHADOW_GFX_STATUS;

static INLINE BOOL shadow_client_rdpgfx_new_surface(rdpShadowClient* client)
{
	UINT error = CHANNEL_RC_OK;
	RDPGFX_CREATE_SURFACE_PDU createSurface;
	RDPGFX_MAP_SURFACE_TO_OUTPUT_PDU surfaceToOutput;
	RdpgfxServerContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(client);
	context = client->rdpgfx;
	WINPR_ASSERT(context);
	settings = ((rdpContext*)client)->settings;
	WINPR_ASSERT(settings);

	WINPR_ASSERT(settings->DesktopWidth <= UINT16_MAX);
	WINPR_ASSERT(settings->DesktopHeight <= UINT16_MAX);
	createSurface.width = (UINT16)settings->DesktopWidth;
	createSurface.height = (UINT16)settings->DesktopHeight;
	createSurface.pixelFormat = GFX_PIXEL_FORMAT_XRGB_8888;
	createSurface.surfaceId = client->surfaceId;
	surfaceToOutput.outputOriginX = 0;
	surfaceToOutput.outputOriginY = 0;
	surfaceToOutput.surfaceId = client->surfaceId;
	surfaceToOutput.reserved = 0;
	IFCALLRET(context->CreateSurface, error, context, &createSurface);

	if (error)
	{
		WLog_ERR(TAG, "CreateSurface failed with error %" PRIu32 "", error);
		return FALSE;
	}

	IFCALLRET(context->MapSurfaceToOutput, error, context, &surfaceToOutput);

	if (error)
	{
		WLog_ERR(TAG, "MapSurfaceToOutput failed with error %" PRIu32 "", error);
		return FALSE;
	}

	return TRUE;
}

static INLINE BOOL shadow_client_rdpgfx_release_surface(rdpShadowClient* client)
{
	UINT error = CHANNEL_RC_OK;
	RDPGFX_DELETE_SURFACE_PDU pdu;
	RdpgfxServerContext* context;

	WINPR_ASSERT(client);

	context = client->rdpgfx;
	WINPR_ASSERT(context);

	pdu.surfaceId = client->surfaceId++;
	IFCALLRET(context->DeleteSurface, error, context, &pdu);

	if (error)
	{
		WLog_ERR(TAG, "DeleteSurface failed with error %" PRIu32 "", error);
		return FALSE;
	}

	return TRUE;
}

static INLINE BOOL shadow_client_rdpgfx_reset_graphic(rdpShadowClient* client)
{
	UINT error = CHANNEL_RC_OK;
	RDPGFX_RESET_GRAPHICS_PDU pdu = { 0 };
	RdpgfxServerContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->rdpgfx);

	context = client->rdpgfx;
	WINPR_ASSERT(context);

	settings = client->context.settings;
	WINPR_ASSERT(settings);

	pdu.width = settings->DesktopWidth;
	pdu.height = settings->DesktopHeight;
	pdu.monitorCount = client->subsystem->numMonitors;
	pdu.monitorDefArray = client->subsystem->monitors;
	IFCALLRET(context->ResetGraphics, error, context, &pdu);

	if (error)
	{
		WLog_ERR(TAG, "ResetGraphics failed with error %" PRIu32 "", error);
		return FALSE;
	}

	client->first_frame = TRUE;
	return TRUE;
}

static INLINE void shadow_client_free_queued_message(void* obj)
{
	wMessage* message = (wMessage*)obj;

	WINPR_ASSERT(message);
	if (message->Free)
	{
		message->Free(message);
		message->Free = NULL;
	}
}

static BOOL shadow_client_context_new(freerdp_peer* peer, rdpContext* context)
{
	BOOL NSCodec;
	const char bind_address[] = "bind-address,";
	rdpShadowClient* client = (rdpShadowClient*)context;
	rdpSettings* settings;
	const rdpSettings* srvSettings;
	rdpShadowServer* server;
	const wObject cb = { NULL, NULL, NULL, shadow_client_free_queued_message, NULL };

	WINPR_ASSERT(client);
	WINPR_ASSERT(peer);
	WINPR_ASSERT(peer->context);

	server = (rdpShadowServer*)peer->ContextExtra;
	WINPR_ASSERT(server);

	srvSettings = server->settings;
	WINPR_ASSERT(srvSettings);

	client->surfaceId = 1;
	client->server = server;
	client->subsystem = server->subsystem;
	WINPR_ASSERT(client->subsystem);

	settings = peer->context->settings;
	WINPR_ASSERT(settings);

	if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth,
	                                 freerdp_settings_get_uint32(srvSettings, FreeRDP_ColorDepth)))
		return FALSE;
	NSCodec = freerdp_settings_get_bool(srvSettings, FreeRDP_NSCodec);
	freerdp_settings_set_bool(settings, FreeRDP_NSCodec, NSCodec);
	settings->RemoteFxCodec = srvSettings->RemoteFxCodec;
	settings->BitmapCacheV3Enabled = TRUE;
	settings->FrameMarkerCommandEnabled = TRUE;
	settings->SurfaceFrameMarkerEnabled = TRUE;
	settings->SupportGraphicsPipeline = TRUE;
	settings->GfxH264 = srvSettings->GfxH264;
	settings->DrawAllowSkipAlpha = TRUE;
	settings->DrawAllowColorSubsampling = TRUE;
	settings->DrawAllowDynamicColorFidelity = TRUE;
	settings->CompressionLevel = PACKET_COMPR_TYPE_RDP6;

	if (!freerdp_settings_set_string(settings, FreeRDP_CertificateFile, server->CertificateFile))
		goto fail_cert_file;

	if (!freerdp_settings_set_string(settings, FreeRDP_PrivateKeyFile, server->PrivateKeyFile))
		goto fail_privkey_file;

	if (!freerdp_settings_set_string(settings, FreeRDP_RdpKeyFile, server->PrivateKeyFile))
		goto fail_rdpkey_file;
	if (server->ipcSocket && (strncmp(bind_address, server->ipcSocket,
	                                  strnlen(bind_address, sizeof(bind_address))) != 0))
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
	client->vcm = WTSOpenServerA((LPSTR)peer->context);

	if (!client->vcm || client->vcm == INVALID_HANDLE_VALUE)
		goto fail_open_server;

	if (!(client->MsgQueue = MessageQueue_New(&cb)))
		goto fail_message_queue;

	if (!(client->encoder = shadow_encoder_new(client)))
		goto fail_encoder_new;

	if (ArrayList_Append(server->clients, (void*)client))
		return TRUE;

	shadow_encoder_free(client->encoder);
	client->encoder = NULL;
fail_encoder_new:
	MessageQueue_Free(client->MsgQueue);
	client->MsgQueue = NULL;
fail_message_queue:
	WTSCloseServer((HANDLE)client->vcm);
	client->vcm = NULL;
fail_open_server:
	DeleteCriticalSection(&(client->lock));
fail_client_lock:
	freerdp_settings_set_string(settings, FreeRDP_RdpKeyFile, NULL);
fail_rdpkey_file:
	freerdp_settings_set_string(settings, FreeRDP_PrivateKeyFile, NULL);
fail_privkey_file:
	freerdp_settings_set_string(settings, FreeRDP_CertificateFile, NULL);
fail_cert_file:
	return FALSE;
}

static void shadow_client_context_free(freerdp_peer* peer, rdpContext* context)
{
	rdpShadowClient* client = (rdpShadowClient*)context;
	rdpShadowServer* server;

	WINPR_ASSERT(context);
	WINPR_UNUSED(peer);
	server = client->server;
	WINPR_ASSERT(server);

	WINPR_ASSERT(server->clients);
	ArrayList_Remove(server->clients, (void*)client);

	if (client->encoder)
	{
		shadow_encoder_free(client->encoder);
		client->encoder = NULL;
	}

	/* Clear queued messages and free resource */
	WINPR_ASSERT(client->MsgQueue);
	MessageQueue_Clear(client->MsgQueue);
	MessageQueue_Free(client->MsgQueue);
	WTSCloseServer((HANDLE)client->vcm);
	client->vcm = NULL;
	region16_uninit(&(client->invalidRegion));
	DeleteCriticalSection(&(client->lock));
}

static INLINE void shadow_client_mark_invalid(rdpShadowClient* client, UINT32 numRects,
                                              const RECTANGLE_16* rects)
{
	UINT32 index;
	RECTANGLE_16 screenRegion;
	rdpSettings* settings;

	WINPR_ASSERT(client);
	WINPR_ASSERT(rects || (numRects == 0));

	settings = client->context.settings;
	WINPR_ASSERT(settings);

	EnterCriticalSection(&(client->lock));

	/* Mark client invalid region. No rectangle means full screen */
	if (numRects > 0)
	{
		for (index = 0; index < numRects; index++)
		{
			region16_union_rect(&(client->invalidRegion), &(client->invalidRegion), &rects[index]);
		}
	}
	else
	{
		screenRegion.left = 0;
		screenRegion.top = 0;
		WINPR_ASSERT(settings->DesktopWidth <= UINT16_MAX);
		WINPR_ASSERT(settings->DesktopHeight <= UINT16_MAX);
		screenRegion.right = (UINT16)settings->DesktopWidth;
		screenRegion.bottom = (UINT16)settings->DesktopHeight;
		region16_union_rect(&(client->invalidRegion), &(client->invalidRegion), &screenRegion);
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
	INT32 width, height;
	rdpShadowServer* server;
	rdpSettings* settings;
	RECTANGLE_16 viewport = { 0 };

	WINPR_ASSERT(client);
	server = client->server;
	settings = client->context.settings;

	WINPR_ASSERT(server);
	WINPR_ASSERT(server->surface);
	WINPR_ASSERT(settings);

	WINPR_ASSERT(server->surface->width <= UINT16_MAX);
	WINPR_ASSERT(server->surface->height <= UINT16_MAX);
	viewport.right = (UINT16)server->surface->width;
	viewport.bottom = (UINT16)server->surface->height;

	if (server->shareSubRect)
	{
		rectangles_intersection(&viewport, &(server->subRect), &viewport);
	}

	width = viewport.right - viewport.left;
	height = viewport.bottom - viewport.top;

	WINPR_ASSERT(width >= 0);
	WINPR_ASSERT(width <= UINT16_MAX);
	WINPR_ASSERT(height >= 0);
	WINPR_ASSERT(height <= UINT16_MAX);
	if (settings->DesktopWidth != (UINT32)width || settings->DesktopHeight != (UINT32)height)
		return TRUE;

	return FALSE;
}

static BOOL shadow_client_capabilities(freerdp_peer* peer)
{
	rdpShadowSubsystem* subsystem;
	rdpShadowClient* client;
	BOOL ret = TRUE;

	WINPR_ASSERT(peer);

	client = (rdpShadowClient*)peer->context;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);

	subsystem = client->server->subsystem;
	WINPR_ASSERT(subsystem);

	IFCALLRET(subsystem->ClientCapabilities, ret, subsystem, client);

	if (!ret)
		WLog_WARN(TAG, "subsystem->ClientCapabilities failed");

	return ret;
}

static BOOL shadow_client_post_connect(freerdp_peer* peer)
{
	int authStatus;
	rdpSettings* settings;
	rdpShadowClient* client;
	rdpShadowServer* server;
	rdpShadowSubsystem* subsystem;

	WINPR_ASSERT(peer);

	client = (rdpShadowClient*)peer->context;
	WINPR_ASSERT(client);

	settings = peer->context->settings;
	WINPR_ASSERT(settings);

	server = client->server;
	WINPR_ASSERT(server);

	subsystem = server->subsystem;
	WINPR_ASSERT(subsystem);

	if (freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth) == 24)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 16)) /* disable 24bpp */
			return FALSE;
	}

	if (settings->MultifragMaxRequestSize < 0x3F0000)
	{
		BOOL rc = freerdp_settings_set_bool(
		    settings, FreeRDP_NSCodec,
		    FALSE); /* NSCodec compressor does not support fragmentation yet */
		WINPR_ASSERT(rc);
	}

	WLog_INFO(TAG, "Client from %s is activated (%" PRIu32 "x%" PRIu32 "@%" PRIu32 ")",
	          peer->hostname, settings->DesktopWidth, settings->DesktopHeight,
	          freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth));

	/* Resize client if necessary */
	if (shadow_client_recalc_desktop_size(client))
	{
		BOOL rc;
		rdpUpdate* update = peer->context->update;
		WINPR_ASSERT(update);
		WINPR_ASSERT(update->DesktopResize);

		rc = update->DesktopResize(update->context);
		WLog_INFO(TAG, "Client from %s is resized (%" PRIu32 "x%" PRIu32 "@%" PRIu32 ")",
		          peer->hostname, settings->DesktopWidth, settings->DesktopHeight,
		          freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth));
		return FALSE;
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
			authStatus = subsystem->Authenticate(subsystem, client, settings->Username,
			                                     settings->Domain, settings->Password);
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
static INLINE void shadow_client_convert_rects(rdpShadowClient* client, RECTANGLE_16* dst,
                                               const RECTANGLE_16* src, UINT32 numRects)
{
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);
	WINPR_ASSERT(dst);
	WINPR_ASSERT(src || (numRects == 0));

	if (client->server->shareSubRect)
	{
		UINT32 i = 0;
		UINT16 offsetX = client->server->subRect.left;
		UINT16 offsetY = client->server->subRect.top;

		for (i = 0; i < numRects; i++)
		{
			const RECTANGLE_16* s = &src[i];
			RECTANGLE_16* d = &dst[i];

			d->left = s->left + offsetX;
			d->right = s->right + offsetX;
			d->top = s->top + offsetY;
			d->bottom = s->bottom + offsetY;
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
	wMessagePipe* MsgPipe;

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->subsystem);

	MsgPipe = client->subsystem->MsgPipe;
	WINPR_ASSERT(MsgPipe);

	message.id = SHADOW_MSG_IN_REFRESH_REQUEST_ID;
	message.wParam = NULL;
	message.lParam = NULL;
	message.context = (void*)client;
	message.Free = NULL;
	return MessageQueue_Dispatch(MsgPipe->In, &message);
}

static BOOL shadow_client_refresh_rect(rdpContext* context, BYTE count, const RECTANGLE_16* areas)
{
	rdpShadowClient* client = (rdpShadowClient*)context;
	RECTANGLE_16* rects;

	/* It is invalid if we have area count but no actual area */
	if (count && !areas)
		return FALSE;

	if (count)
	{
		rects = (RECTANGLE_16*)calloc(count, sizeof(RECTANGLE_16));

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

static BOOL shadow_client_suppress_output(rdpContext* context, BYTE allow, const RECTANGLE_16* area)
{
	rdpShadowClient* client = (rdpShadowClient*)context;
	RECTANGLE_16 region;

	WINPR_ASSERT(client);

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
	rdpSettings* settings;
	rdpShadowClient* client;

	WINPR_ASSERT(peer);

	client = (rdpShadowClient*)peer->context;
	WINPR_ASSERT(client);

	settings = peer->context->settings;
	WINPR_ASSERT(settings);

	client->activated = TRUE;
	client->inLobby = client->mayView ? FALSE : TRUE;

	if (shadow_encoder_reset(client->encoder) < 0)
	{
		WLog_ERR(TAG, "Failed to reset encoder");
		return FALSE;
	}

	/* Update full screen in next update */
	return shadow_client_refresh_rect(&client->context, 0, NULL);
}

static BOOL shadow_client_logon(freerdp_peer* peer, const SEC_WINNT_AUTH_IDENTITY* identity,
                                BOOL automatic)
{
	BOOL rc = FALSE;
	char* user = NULL;
	char* domain = NULL;
	char* password = NULL;
	rdpSettings* settings;

	WINPR_UNUSED(automatic);

	WINPR_ASSERT(peer);
	WINPR_ASSERT(identity);

	WINPR_ASSERT(peer->context);

	settings = peer->context->settings;
	WINPR_ASSERT(settings);

	if (identity->Flags & SEC_WINNT_AUTH_IDENTITY_UNICODE)
	{
		if (identity->User)
		{
			int r;
			WINPR_ASSERT(identity->UserLength <= INT_MAX);
			r = ConvertFromUnicode(CP_UTF8, 0, identity->User, (int)identity->UserLength, &user, 0,
			                       NULL, NULL);
			WINPR_ASSERT(r > 0);
		}

		if (identity->Domain)
		{
			int r;
			WINPR_ASSERT(identity->DomainLength <= INT_MAX);
			r = ConvertFromUnicode(CP_UTF8, 0, identity->Domain, (int)identity->DomainLength,
			                       &domain, 0, NULL, NULL);
			WINPR_ASSERT(r > 0);
		}

		if (identity->Password)
		{
			int r;
			WINPR_ASSERT(identity->PasswordLength <= INT_MAX);
			r = ConvertFromUnicode(CP_UTF8, 0, identity->Password, (int)identity->PasswordLength,
			                       &password, 0, NULL, NULL);
			WINPR_ASSERT(r > 0);
		}
	}
	else
	{
		if (identity->User)
			user = _strdup((char*)identity->User);

		if (identity->Domain)
			domain = _strdup((char*)identity->Domain);

		if (identity->Password)
			password = _strdup((char*)identity->Password);
	}

	if ((identity->User && !user) || (identity->Domain && !domain) ||
	    (identity->Password && !password))
		goto fail;

	if (user)
		freerdp_settings_set_string(settings, FreeRDP_Username, user);

	if (domain)
		freerdp_settings_set_string(settings, FreeRDP_Domain, domain);

	if (password)
		freerdp_settings_set_string(settings, FreeRDP_Password, password);

	rc = TRUE;
fail:
	free(user);
	free(domain);
	free(password);
	return rc;
}

static INLINE void shadow_client_common_frame_acknowledge(rdpShadowClient* client, UINT32 frameId)
{
	/*
	 * Record the last client acknowledged frame id to
	 * calculate how much frames are in progress.
	 * Some rdp clients (win7 mstsc) skips frame ACK if it is
	 * inactive, we should not expect ACK for each frame.
	 * So it is OK to calculate inflight frame count according to
	 * a latest acknowledged frame id.
	 */
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->encoder);
	client->encoder->lastAckframeId = frameId;
}

static BOOL shadow_client_surface_frame_acknowledge(rdpContext* context, UINT32 frameId)
{
	rdpShadowClient* client = (rdpShadowClient*)context;
	shadow_client_common_frame_acknowledge(client, frameId);
	/*
	 * Reset queueDepth for legacy none RDPGFX acknowledge
	 */
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->encoder);
	client->encoder->queueDepth = QUEUE_DEPTH_UNAVAILABLE;
	return TRUE;
}

static UINT
shadow_client_rdpgfx_frame_acknowledge(RdpgfxServerContext* context,
                                       const RDPGFX_FRAME_ACKNOWLEDGE_PDU* frameAcknowledge)
{
	rdpShadowClient* client;

	WINPR_ASSERT(context);
	WINPR_ASSERT(frameAcknowledge);

	client = (rdpShadowClient*)context->custom;
	shadow_client_common_frame_acknowledge(client, frameAcknowledge->frameId);

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->encoder);
	client->encoder->queueDepth = frameAcknowledge->queueDepth;
	return CHANNEL_RC_OK;
}

static BOOL shadow_are_caps_filtered(const rdpSettings* settings, UINT32 caps)
{
	UINT32 filter;
	const UINT32 capList[] = { RDPGFX_CAPVERSION_8,   RDPGFX_CAPVERSION_81,
		                       RDPGFX_CAPVERSION_10,  RDPGFX_CAPVERSION_101,
		                       RDPGFX_CAPVERSION_102, RDPGFX_CAPVERSION_103,
		                       RDPGFX_CAPVERSION_104, RDPGFX_CAPVERSION_105,
		                       RDPGFX_CAPVERSION_106, RDPGFX_CAPVERSION_106_ERR,
		                       RDPGFX_CAPVERSION_107 };
	UINT32 x;

	WINPR_ASSERT(settings);
	filter = settings->GfxCapsFilter;

	for (x = 0; x < ARRAYSIZE(capList); x++)
	{
		if (caps == capList[x])
			return (filter & (1 << x)) != 0;
	}

	return TRUE;
}

static BOOL shadow_client_caps_test_version(RdpgfxServerContext* context, rdpShadowClient* client,
                                            BOOL h264, const RDPGFX_CAPSET* capsSets,
                                            UINT32 capsSetCount, UINT32 capsVersion, UINT* rc)
{
	UINT32 index;
	const rdpSettings* srvSettings;
	rdpSettings* clientSettings;

	WINPR_ASSERT(context);
	WINPR_ASSERT(client);
	WINPR_ASSERT(capsSets || (capsSetCount == 0));
	WINPR_ASSERT(rc);

	WINPR_ASSERT(context->rdpcontext);
	srvSettings = context->rdpcontext->settings;
	WINPR_ASSERT(srvSettings);

	clientSettings = client->context.settings;
	WINPR_ASSERT(clientSettings);

	if (shadow_are_caps_filtered(srvSettings, capsVersion))
		return FALSE;

	for (index = 0; index < capsSetCount; index++)
	{
		const RDPGFX_CAPSET* currentCaps = &capsSets[index];

		if (currentCaps->version == capsVersion)
		{
			UINT32 flags;
			BOOL planar = FALSE;
			BOOL rfx = FALSE;
			BOOL avc444v2 = FALSE;
			BOOL avc444 = FALSE;
			BOOL avc420 = FALSE;
			BOOL progressive = FALSE;
			RDPGFX_CAPSET caps = *currentCaps;
			RDPGFX_CAPS_CONFIRM_PDU pdu = { 0 };
			pdu.capsSet = &caps;

			flags = pdu.capsSet->flags;

			clientSettings->GfxSmallCache = (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE);

			avc444v2 = avc444 = !(flags & RDPGFX_CAPS_FLAG_AVC_DISABLED);
			if (!freerdp_settings_get_bool(srvSettings, FreeRDP_GfxAVC444v2) || !h264)
				avc444v2 = FALSE;
			freerdp_settings_set_bool(clientSettings, FreeRDP_GfxAVC444v2, avc444v2);
			if (!freerdp_settings_get_bool(srvSettings, FreeRDP_GfxAVC444) || !h264)
				avc444 = FALSE;
			freerdp_settings_set_bool(clientSettings, FreeRDP_GfxAVC444, avc444);
			if (!freerdp_settings_get_bool(srvSettings, FreeRDP_GfxH264) || !h264)
				avc420 = FALSE;
			freerdp_settings_set_bool(clientSettings, FreeRDP_GfxH264, avc420);

			progressive = freerdp_settings_get_bool(srvSettings, FreeRDP_GfxProgressive);
			freerdp_settings_set_bool(clientSettings, FreeRDP_GfxProgressive, progressive);
			progressive = freerdp_settings_get_bool(srvSettings, FreeRDP_GfxProgressiveV2);
			freerdp_settings_set_bool(clientSettings, FreeRDP_GfxProgressiveV2, progressive);

			rfx = freerdp_settings_get_bool(srvSettings, FreeRDP_RemoteFxCodec);
			freerdp_settings_set_bool(clientSettings, FreeRDP_RemoteFxCodec, rfx);

			planar = freerdp_settings_get_bool(srvSettings, FreeRDP_GfxPlanar);
			freerdp_settings_set_bool(clientSettings, FreeRDP_GfxPlanar, planar);

			if (!avc444v2 && !avc444 && !avc420)
				pdu.capsSet->flags |= RDPGFX_CAPS_FLAG_AVC_DISABLED;

			WINPR_ASSERT(context->CapsConfirm);
			*rc = context->CapsConfirm(context, &pdu);
			return TRUE;
		}
	}

	return FALSE;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT shadow_client_rdpgfx_caps_advertise(RdpgfxServerContext* context,
                                                const RDPGFX_CAPS_ADVERTISE_PDU* capsAdvertise)
{
	UINT16 index;
	UINT rc = ERROR_INTERNAL_ERROR;
	const rdpSettings* srvSettings;
	rdpSettings* clientSettings;
	BOOL h264 = FALSE;

	UINT32 flags = 0;
	rdpShadowClient* client;

	WINPR_ASSERT(context);
	WINPR_ASSERT(capsAdvertise);

	client = (rdpShadowClient*)context->custom;
	WINPR_ASSERT(client);
	WINPR_ASSERT(context->rdpcontext);

	srvSettings = context->rdpcontext->settings;
	WINPR_ASSERT(srvSettings);

	clientSettings = client->context.settings;
	WINPR_ASSERT(clientSettings);

#ifdef WITH_GFX_H264
	h264 =
	    (shadow_encoder_prepare(client->encoder, FREERDP_CODEC_AVC420 | FREERDP_CODEC_AVC444) >= 0);
#else
	freerdp_settings_set_bool(clientSettings, FreeRDP_GfxAVC444v2, FALSE);
	freerdp_settings_set_bool(clientSettings, FreeRDP_GfxAVC444, FALSE);
	freerdp_settings_set_bool(clientSettings, FreeRDP_GfxH264, FALSE);
#endif

	/* Request full screen update for new gfx channel */
	if (!shadow_client_refresh_rect(&client->context, 0, NULL))
		return rc;

	if (shadow_client_caps_test_version(context, client, h264, capsAdvertise->capsSets,
	                                    capsAdvertise->capsSetCount, RDPGFX_CAPVERSION_107, &rc))
		return rc;

	if (shadow_client_caps_test_version(context, client, h264, capsAdvertise->capsSets,
	                                    capsAdvertise->capsSetCount, RDPGFX_CAPVERSION_106, &rc))
		return rc;

	if (shadow_client_caps_test_version(context, client, h264, capsAdvertise->capsSets,
	                                    capsAdvertise->capsSetCount, RDPGFX_CAPVERSION_106_ERR,
	                                    &rc))
		return rc;

	if (shadow_client_caps_test_version(context, client, h264, capsAdvertise->capsSets,
	                                    capsAdvertise->capsSetCount, RDPGFX_CAPVERSION_105, &rc))
		return rc;

	if (shadow_client_caps_test_version(context, client, h264, capsAdvertise->capsSets,
	                                    capsAdvertise->capsSetCount, RDPGFX_CAPVERSION_104, &rc))
		return rc;

	if (shadow_client_caps_test_version(context, client, h264, capsAdvertise->capsSets,
	                                    capsAdvertise->capsSetCount, RDPGFX_CAPVERSION_103, &rc))
		return rc;

	if (shadow_client_caps_test_version(context, client, h264, capsAdvertise->capsSets,
	                                    capsAdvertise->capsSetCount, RDPGFX_CAPVERSION_102, &rc))
		return rc;

	if (shadow_client_caps_test_version(context, client, h264, capsAdvertise->capsSets,
	                                    capsAdvertise->capsSetCount, RDPGFX_CAPVERSION_101, &rc))
		return rc;

	if (shadow_client_caps_test_version(context, client, h264, capsAdvertise->capsSets,
	                                    capsAdvertise->capsSetCount, RDPGFX_CAPVERSION_10, &rc))
		return rc;

	if (!shadow_are_caps_filtered(srvSettings, RDPGFX_CAPVERSION_81))
	{
		for (index = 0; index < capsAdvertise->capsSetCount; index++)
		{
			const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

			if (currentCaps->version == RDPGFX_CAPVERSION_81)
			{
				RDPGFX_CAPSET caps = *currentCaps;
				RDPGFX_CAPS_CONFIRM_PDU pdu;
				pdu.capsSet = &caps;

				flags = pdu.capsSet->flags;

				freerdp_settings_set_bool(clientSettings, FreeRDP_GfxAVC444v2, FALSE);
				freerdp_settings_set_bool(clientSettings, FreeRDP_GfxAVC444, FALSE);

				freerdp_settings_set_bool(clientSettings, FreeRDP_GfxThinClient,
				                          (flags & RDPGFX_CAPS_FLAG_THINCLIENT));
				freerdp_settings_set_bool(clientSettings, FreeRDP_GfxSmallCache,
				                          (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE));

#ifndef WITH_GFX_H264
				freerdp_settings_set_bool(clientSettings, FreeRDP_GfxH264, FALSE);
				pdu.capsSet->flags &= ~RDPGFX_CAPS_FLAG_AVC420_ENABLED;
#else

				if (h264)
					freerdp_settings_set_bool(clientSettings, FreeRDP_GfxH264,
					                          (flags & RDPGFX_CAPS_FLAG_AVC420_ENABLED));
				else
					freerdp_settings_set_bool(clientSettings, FreeRDP_GfxH264, FALSE);
#endif

				WINPR_ASSERT(context->CapsConfirm);
				return context->CapsConfirm(context, &pdu);
			}
		}
	}

	if (!shadow_are_caps_filtered(srvSettings, RDPGFX_CAPVERSION_8))
	{
		for (index = 0; index < capsAdvertise->capsSetCount; index++)
		{
			const RDPGFX_CAPSET* currentCaps = &capsAdvertise->capsSets[index];

			if (currentCaps->version == RDPGFX_CAPVERSION_8)
			{
				RDPGFX_CAPSET caps = *currentCaps;
				RDPGFX_CAPS_CONFIRM_PDU pdu;
				pdu.capsSet = &caps;
				flags = pdu.capsSet->flags;

				freerdp_settings_set_bool(clientSettings, FreeRDP_GfxAVC444v2, FALSE);
				freerdp_settings_set_bool(clientSettings, FreeRDP_GfxAVC444, FALSE);
				freerdp_settings_set_bool(clientSettings, FreeRDP_GfxH264, FALSE);

				freerdp_settings_set_bool(clientSettings, FreeRDP_GfxThinClient,
				                          (flags & RDPGFX_CAPS_FLAG_THINCLIENT));
				freerdp_settings_set_bool(clientSettings, FreeRDP_GfxSmallCache,
				                          (flags & RDPGFX_CAPS_FLAG_SMALL_CACHE));

				WINPR_ASSERT(context->CapsConfirm);
				return context->CapsConfirm(context, &pdu);
			}
		}
	}

	return CHANNEL_RC_UNSUPPORTED_VERSION;
}

static INLINE UINT32 rdpgfx_estimate_h264_avc420(RDPGFX_AVC420_BITMAP_STREAM* havc420)
{
	/* H264 metadata + H264 stream. See rdpgfx_write_h264_avc420 */
	WINPR_ASSERT(havc420);
	return sizeof(UINT32) /* numRegionRects */
	       + 10           /* regionRects + quantQualityVals */
	             * havc420->meta.numRegionRects +
	       havc420->length;
}

/**
 * Function description
 *
 * @return TRUE on success
 */
static BOOL shadow_client_send_surface_gfx(rdpShadowClient* client, const BYTE* pSrcData,
                                           UINT32 nSrcStep, UINT32 SrcFormat, UINT16 nXSrc,
                                           UINT16 nYSrc, UINT16 nWidth, UINT16 nHeight)
{
	UINT32 id;
	UINT error = CHANNEL_RC_OK;
	const rdpContext* context = (const rdpContext*)client;
	const rdpSettings* settings;
	rdpShadowEncoder* encoder;
	RDPGFX_SURFACE_COMMAND cmd = { 0 };
	RDPGFX_START_FRAME_PDU cmdstart = { 0 };
	RDPGFX_END_FRAME_PDU cmdend = { 0 };
	SYSTEMTIME sTime = { 0 };

	if (!context || !pSrcData)
		return FALSE;

	settings = context->settings;
	encoder = client->encoder;

	if (!settings || !encoder)
		return FALSE;

	if (client->first_frame)
	{
		rfx_context_reset(encoder->rfx, nWidth, nHeight);
		client->first_frame = FALSE;
	}

	cmdstart.frameId = shadow_encoder_create_frame_id(encoder);
	GetSystemTime(&sTime);
	cmdstart.timestamp = (UINT32)(sTime.wHour << 22U | sTime.wMinute << 16U | sTime.wSecond << 10U |
	                              sTime.wMilliseconds);
	cmdend.frameId = cmdstart.frameId;
	cmd.surfaceId = client->surfaceId;
	cmd.format = PIXEL_FORMAT_BGRX32;
	cmd.left = nXSrc;
	cmd.top = nYSrc;
	cmd.right = cmd.left + nWidth;
	cmd.bottom = cmd.top + nHeight;
	cmd.width = nWidth;
	cmd.height = nHeight;

	id = freerdp_settings_get_uint32(settings, FreeRDP_RemoteFxCodecId);
	if (settings->GfxAVC444 || settings->GfxAVC444v2)
	{
		INT32 rc;
		RDPGFX_AVC444_BITMAP_STREAM avc444 = { 0 };
		RECTANGLE_16 regionRect = { 0 };
		BYTE version = settings->GfxAVC444v2 ? 2 : 1;

		if (shadow_encoder_prepare(encoder, FREERDP_CODEC_AVC444) < 0)
		{
			WLog_ERR(TAG, "Failed to prepare encoder FREERDP_CODEC_AVC444");
			return FALSE;
		}

		WINPR_ASSERT(cmd.left <= UINT16_MAX);
		WINPR_ASSERT(cmd.top <= UINT16_MAX);
		WINPR_ASSERT(cmd.right <= UINT16_MAX);
		WINPR_ASSERT(cmd.bottom <= UINT16_MAX);
		regionRect.left = (UINT16)cmd.left;
		regionRect.top = (UINT16)cmd.top;
		regionRect.right = (UINT16)cmd.right;
		regionRect.bottom = (UINT16)cmd.bottom;
		rc = avc444_compress(encoder->h264, pSrcData, cmd.format, nSrcStep, nWidth, nHeight,
		                     version, &regionRect, &avc444.LC, &avc444.bitstream[0].data,
		                     &avc444.bitstream[0].length, &avc444.bitstream[1].data,
		                     &avc444.bitstream[1].length, &avc444.bitstream[0].meta,
		                     &avc444.bitstream[1].meta);
		if (rc < 0)
		{
			WLog_ERR(TAG, "avc420_compress failed for avc444");
			return FALSE;
		}

		/* rc > 0 means new data */
		if (rc > 0)
		{
			avc444.cbAvc420EncodedBitstream1 = rdpgfx_estimate_h264_avc420(&avc444.bitstream[0]);
			cmd.codecId = settings->GfxAVC444v2 ? RDPGFX_CODECID_AVC444v2 : RDPGFX_CODECID_AVC444;
			cmd.extra = (void*)&avc444;
			IFCALLRET(client->rdpgfx->SurfaceFrameCommand, error, client->rdpgfx, &cmd, &cmdstart,
			          &cmdend);
		}

		free_h264_metablock(&avc444.bitstream[0].meta);
		free_h264_metablock(&avc444.bitstream[1].meta);
		if (error)
		{
			WLog_ERR(TAG, "SurfaceFrameCommand failed with error %" PRIu32 "", error);
			return FALSE;
		}
	}
	else if (settings->GfxH264)
	{
		INT32 rc;
		RDPGFX_AVC420_BITMAP_STREAM avc420 = { 0 };
		RECTANGLE_16 regionRect;

		if (shadow_encoder_prepare(encoder, FREERDP_CODEC_AVC420) < 0)
		{
			WLog_ERR(TAG, "Failed to prepare encoder FREERDP_CODEC_AVC420");
			return FALSE;
		}

		WINPR_ASSERT(cmd.left <= UINT16_MAX);
		WINPR_ASSERT(cmd.top <= UINT16_MAX);
		WINPR_ASSERT(cmd.right <= UINT16_MAX);
		WINPR_ASSERT(cmd.bottom <= UINT16_MAX);
		regionRect.left = (UINT16)cmd.left;
		regionRect.top = (UINT16)cmd.top;
		regionRect.right = (UINT16)cmd.right;
		regionRect.bottom = (UINT16)cmd.bottom;
		rc = avc420_compress(encoder->h264, pSrcData, cmd.format, nSrcStep, nWidth, nHeight,
		                     &regionRect, &avc420.data, &avc420.length, &avc420.meta);
		if (rc < 0)
		{
			WLog_ERR(TAG, "avc420_compress failed");
			return FALSE;
		}

		/* rc > 0 means new data */
		if (rc > 0)
		{
			cmd.codecId = RDPGFX_CODECID_AVC420;
			cmd.extra = (void*)&avc420;

			IFCALLRET(client->rdpgfx->SurfaceFrameCommand, error, client->rdpgfx, &cmd, &cmdstart,
			          &cmdend);
		}
		free_h264_metablock(&avc420.meta);

		if (error)
		{
			WLog_ERR(TAG, "SurfaceFrameCommand failed with error %" PRIu32 "", error);
			return FALSE;
		}
	}
	else if (freerdp_settings_get_bool(settings, FreeRDP_RemoteFxCodec) && (id != 0))
	{
		BOOL rc;
		wStream* s;
		RFX_RECT rect;

		if (shadow_encoder_prepare(encoder, FREERDP_CODEC_REMOTEFX) < 0)
		{
			WLog_ERR(TAG, "Failed to prepare encoder FREERDP_CODEC_REMOTEFX");
			return FALSE;
		}

		s = Stream_New(NULL, 1024);
		WINPR_ASSERT(s);

		WINPR_ASSERT(cmd.left <= UINT16_MAX);
		WINPR_ASSERT(cmd.top <= UINT16_MAX);
		WINPR_ASSERT(cmd.right <= UINT16_MAX);
		WINPR_ASSERT(cmd.bottom <= UINT16_MAX);
		rect.x = (UINT16)cmd.left;
		rect.y = (UINT16)cmd.top;
		rect.width = (UINT16)cmd.right - cmd.left;
		rect.height = (UINT16)cmd.bottom - cmd.top;

		rc = rfx_compose_message(encoder->rfx, s, &rect, 1, pSrcData, nWidth, nHeight, nSrcStep);

		if (!rc)
		{
			WLog_ERR(TAG, "rfx_compose_message failed");
			Stream_Free(s, TRUE);
			return FALSE;
		}

		/* rc > 0 means new data */
		if (rc > 0)
		{
			const size_t pos = Stream_GetPosition(s);
			WINPR_ASSERT(pos <= UINT32_MAX);

			cmd.codecId = RDPGFX_CODECID_CAVIDEO;
			cmd.data = Stream_Buffer(s);
			cmd.length = (UINT32)pos;

			IFCALLRET(client->rdpgfx->SurfaceFrameCommand, error, client->rdpgfx, &cmd, &cmdstart,
			          &cmdend);
		}

		Stream_Free(s, TRUE);
		if (error)
		{
			WLog_ERR(TAG, "SurfaceFrameCommand failed with error %" PRIu32 "", error);
			return FALSE;
		}
	}
	else if (freerdp_settings_get_bool(settings, FreeRDP_GfxProgressive))
	{
		INT32 rc;
		REGION16 region;
		RECTANGLE_16 regionRect;

		if (shadow_encoder_prepare(encoder, FREERDP_CODEC_PROGRESSIVE) < 0)
		{
			WLog_ERR(TAG, "Failed to prepare encoder FREERDP_CODEC_PROGRESSIVE");
			return FALSE;
		}

		WINPR_ASSERT(cmd.left <= UINT16_MAX);
		WINPR_ASSERT(cmd.top <= UINT16_MAX);
		WINPR_ASSERT(cmd.right <= UINT16_MAX);
		WINPR_ASSERT(cmd.bottom <= UINT16_MAX);
		regionRect.left = (UINT16)cmd.left;
		regionRect.top = (UINT16)cmd.top;
		regionRect.right = (UINT16)cmd.right;
		regionRect.bottom = (UINT16)cmd.bottom;
		region16_init(&region);
		region16_union_rect(&region, &region, &regionRect);
		rc = progressive_compress(encoder->progressive, pSrcData, nSrcStep * nHeight, cmd.format,
		                          nWidth, nHeight, nSrcStep, &region, &cmd.data, &cmd.length);
		region16_uninit(&region);
		if (rc < 0)
		{
			WLog_ERR(TAG, "progressive_compress failed");
			return FALSE;
		}

		/* rc > 0 means new data */
		if (rc > 0)
		{
			cmd.codecId = RDPGFX_CODECID_CAPROGRESSIVE;

			IFCALLRET(client->rdpgfx->SurfaceFrameCommand, error, client->rdpgfx, &cmd, &cmdstart,
			          &cmdend);
		}

		if (error)
		{
			WLog_ERR(TAG, "SurfaceFrameCommand failed with error %" PRIu32 "", error);
			return FALSE;
		}
	}
	else if (freerdp_settings_get_bool(settings, FreeRDP_GfxPlanar))
	{
		BOOL rc;
		const UINT32 w = cmd.right - cmd.left;
		const UINT32 h = cmd.bottom - cmd.top;
		const BYTE* src =
		    &pSrcData[cmd.top * nSrcStep + cmd.left * FreeRDPGetBytesPerPixel(SrcFormat)];
		if (shadow_encoder_prepare(encoder, FREERDP_CODEC_PLANAR) < 0)
		{
			WLog_ERR(TAG, "Failed to prepare encoder FREERDP_CODEC_PLANAR");
			return FALSE;
		}

		rc = freerdp_bitmap_planar_context_reset(encoder->planar, w, h);
		WINPR_ASSERT(rc);
		freerdp_planar_topdown_image(encoder->planar, TRUE);

		cmd.data = freerdp_bitmap_compress_planar(encoder->planar, src, SrcFormat, w, h, nSrcStep,
		                                          NULL, &cmd.length);
		WINPR_ASSERT(cmd.data || (cmd.length == 0));

		cmd.codecId = RDPGFX_CODECID_PLANAR;

		IFCALLRET(client->rdpgfx->SurfaceFrameCommand, error, client->rdpgfx, &cmd, &cmdstart,
		          &cmdend);
		free(cmd.data);
		if (error)
		{
			WLog_ERR(TAG, "SurfaceFrameCommand failed with error %" PRIu32 "", error);
			return FALSE;
		}
	}
	else
	{
		BOOL rc;
		const UINT32 w = cmd.right - cmd.left;
		const UINT32 h = cmd.bottom - cmd.top;
		const UINT32 length = w * 4 * h;
		BYTE* data = malloc(length);

		WINPR_ASSERT(data);

		rc = freerdp_image_copy(data, PIXEL_FORMAT_BGRA32, 0, 0, 0, w, h, pSrcData, SrcFormat,
		                        nSrcStep, cmd.left, cmd.top, NULL, 0);
		WINPR_ASSERT(rc);

		cmd.data = data;
		cmd.length = length;
		cmd.codecId = RDPGFX_CODECID_UNCOMPRESSED;

		IFCALLRET(client->rdpgfx->SurfaceFrameCommand, error, client->rdpgfx, &cmd, &cmdstart,
		          &cmdend);
		free(data);
		if (error)
		{
			WLog_ERR(TAG, "SurfaceFrameCommand failed with error %" PRIu32 "", error);
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
static BOOL shadow_client_send_surface_bits(rdpShadowClient* client, BYTE* pSrcData,
                                            UINT32 nSrcStep, UINT16 nXSrc, UINT16 nYSrc,
                                            UINT16 nWidth, UINT16 nHeight)
{
	BOOL ret = TRUE;
	size_t i;
	BOOL first;
	BOOL last;
	wStream* s;
	size_t numMessages;
	UINT32 frameId = 0;
	rdpUpdate* update;
	rdpContext* context = (rdpContext*)client;
	rdpSettings* settings;
	rdpShadowEncoder* encoder;
	SURFACE_BITS_COMMAND cmd = { 0 };
	UINT32 nsID, rfxID;

	if (!context || !pSrcData)
		return FALSE;

	update = context->update;
	settings = context->settings;
	encoder = client->encoder;

	if (!update || !settings || !encoder)
		return FALSE;

	if (encoder->frameAck)
		frameId = shadow_encoder_create_frame_id(encoder);

	nsID = freerdp_settings_get_uint32(settings, FreeRDP_NSCodecId);
	rfxID = freerdp_settings_get_uint32(settings, FreeRDP_RemoteFxCodecId);
	if (freerdp_settings_get_bool(settings, FreeRDP_RemoteFxCodec) && (rfxID != 0))
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

		if (!(messages = rfx_encode_messages(
		          encoder->rfx, &rect, 1, pSrcData, settings->DesktopWidth, settings->DesktopHeight,
		          nSrcStep, &numMessages, settings->MultifragMaxRequestSize)))
		{
			WLog_ERR(TAG, "rfx_encode_messages failed");
			return FALSE;
		}

		cmd.cmdType = CMDTYPE_STREAM_SURFACE_BITS;
		WINPR_ASSERT(rfxID <= UINT16_MAX);
		cmd.bmp.codecID = (UINT16)rfxID;
		cmd.destLeft = 0;
		cmd.destTop = 0;
		cmd.destRight = settings->DesktopWidth;
		cmd.destBottom = settings->DesktopHeight;
		cmd.bmp.bpp = 32;
		cmd.bmp.flags = 0;
		WINPR_ASSERT(settings->DesktopWidth <= UINT16_MAX);
		WINPR_ASSERT(settings->DesktopHeight <= UINT16_MAX);
		cmd.bmp.width = (UINT16)settings->DesktopWidth;
		cmd.bmp.height = (UINT16)settings->DesktopHeight;
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
			WINPR_ASSERT(Stream_GetPosition(s) <= UINT32_MAX);
			cmd.bmp.bitmapDataLength = (UINT32)Stream_GetPosition(s);
			cmd.bmp.bitmapData = Stream_Buffer(s);
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
	if (freerdp_settings_get_bool(settings, FreeRDP_NSCodec) && (nsID != 0))
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
		cmd.cmdType = CMDTYPE_SET_SURFACE_BITS;
		cmd.bmp.bpp = 32;
		WINPR_ASSERT(nsID <= UINT16_MAX);
		cmd.bmp.codecID = (UINT16)nsID;
		cmd.destLeft = nXSrc;
		cmd.destTop = nYSrc;
		cmd.destRight = cmd.destLeft + nWidth;
		cmd.destBottom = cmd.destTop + nHeight;
		cmd.bmp.width = nWidth;
		cmd.bmp.height = nHeight;
		WINPR_ASSERT(Stream_GetPosition(s) <= UINT32_MAX);
		cmd.bmp.bitmapDataLength = (UINT32)Stream_GetPosition(s);
		cmd.bmp.bitmapData = Stream_Buffer(s);
		first = TRUE;
		last = TRUE;

		if (!encoder->frameAck)
			IFCALLRET(update->SurfaceBits, ret, update->context, &cmd);
		else
			IFCALLRET(update->SurfaceFrameBits, ret, update->context, &cmd, first, last, frameId);

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
static BOOL shadow_client_send_bitmap_update(rdpShadowClient* client, BYTE* pSrcData,
                                             UINT32 nSrcStep, UINT16 nXSrc, UINT16 nYSrc,
                                             UINT16 nWidth, UINT16 nHeight)
{
	BOOL ret = TRUE;
	BYTE* data;
	BYTE* buffer;
	UINT32 k;
	UINT32 yIdx, xIdx;
	UINT32 rows, cols;
	UINT32 DstSize;
	UINT32 SrcFormat;
	BITMAP_DATA* bitmap;
	rdpUpdate* update;
	rdpContext* context = (rdpContext*)client;
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

	if (freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth) < 32)
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
	bitmapUpdate.number = rows * cols;

	if (!(bitmapData = (BITMAP_DATA*)calloc(bitmapUpdate.number, sizeof(BITMAP_DATA))))
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

			if ((INT64)(bitmap->destLeft + bitmap->width) > (nXSrc + nWidth))
				bitmap->width = (UINT32)(nXSrc + nWidth) - bitmap->destLeft;

			if ((INT64)(bitmap->destTop + bitmap->height) > (nYSrc + nHeight))
				bitmap->height = (UINT32)(nYSrc + nHeight) - bitmap->destTop;

			bitmap->destRight = bitmap->destLeft + bitmap->width - 1;
			bitmap->destBottom = bitmap->destTop + bitmap->height - 1;
			bitmap->compressed = TRUE;

			if ((bitmap->width < 4) || (bitmap->height < 4))
				continue;

			if (freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth) < 32)
			{
				UINT32 bitsPerPixel = freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth);
				UINT32 bytesPerPixel = (bitsPerPixel + 7) / 8;
				DstSize = 64 * 64 * 4;
				buffer = encoder->grid[k];
				interleaved_compress(encoder->interleaved, buffer, &DstSize, bitmap->width,
				                     bitmap->height, pSrcData, SrcFormat, nSrcStep,
				                     bitmap->destLeft, bitmap->destTop, NULL, bitsPerPixel);
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

				buffer =
				    freerdp_bitmap_compress_planar(encoder->planar, data, SrcFormat, bitmap->width,
				                                   bitmap->height, nSrcStep, buffer, &dstSize);
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

	bitmapUpdate.number = k;
	updateSizeEstimate = totalBitmapSize + (k * bitmapUpdate.number) + 16;

	if (updateSizeEstimate > maxUpdateSize)
	{
		UINT32 i, j;
		UINT32 updateSize;
		UINT32 newUpdateSize;
		BITMAP_DATA* fragBitmapData = NULL;

		if (k > 0)
			fragBitmapData = (BITMAP_DATA*)calloc(k, sizeof(BITMAP_DATA));

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
				bitmapUpdate.number = j;
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
static BOOL shadow_client_send_surface_update(rdpShadowClient* client, SHADOW_GFX_STATUS* pStatus)
{
	BOOL ret = TRUE;
	INT64 nXSrc, nYSrc;
	INT64 nWidth, nHeight;
	rdpContext* context = (rdpContext*)client;
	rdpSettings* settings;
	rdpShadowServer* server;
	rdpShadowSurface* surface;
	REGION16 invalidRegion;
	RECTANGLE_16 surfaceRect;
	const RECTANGLE_16* extents;
	BYTE* pSrcData;
	UINT32 nSrcStep, SrcFormat;
	UINT32 index;
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

	EnterCriticalSection(&surface->lock);
	rects = region16_rects(&(surface->invalidRegion), &numRects);

	for (index = 0; index < numRects; index++)
		region16_union_rect(&invalidRegion, &invalidRegion, &rects[index]);

	surfaceRect.left = 0;
	surfaceRect.top = 0;
	WINPR_ASSERT(surface->width <= UINT16_MAX);
	WINPR_ASSERT(surface->height <= UINT16_MAX);
	surfaceRect.right = (UINT16)surface->width;
	surfaceRect.bottom = (UINT16)surface->height;
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
	SrcFormat = surface->format;

	/* Move to new pSrcData / nXSrc / nYSrc according to sub rect */
	if (server->shareSubRect)
	{
		INT32 subX, subY;
		subX = server->subRect.left;
		subY = server->subRect.top;
		nXSrc -= subX;
		nYSrc -= subY;
		WINPR_ASSERT(nXSrc >= 0);
		WINPR_ASSERT(nXSrc <= UINT16_MAX);
		WINPR_ASSERT(nYSrc >= 0);
		WINPR_ASSERT(nYSrc <= UINT16_MAX);
		pSrcData = &pSrcData[((UINT16)subY * nSrcStep) + ((UINT16)subX * 4U)];
	}

	// WLog_INFO(TAG, "shadow_client_send_surface_update: x: %d y: %d width: %d height: %d right: %d
	// bottom: %d", 	nXSrc, nYSrc, nWidth, nHeight, nXSrc + nWidth, nYSrc + nHeight);

	if (settings->SupportGraphicsPipeline && pStatus->gfxOpened)
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

		WINPR_ASSERT(nWidth >= 0);
		WINPR_ASSERT(nWidth <= UINT16_MAX);
		WINPR_ASSERT(nHeight >= 0);
		WINPR_ASSERT(nHeight <= UINT16_MAX);
		ret = shadow_client_send_surface_gfx(client, pSrcData, nSrcStep, SrcFormat, 0, 0,
		                                     (UINT16)nWidth, (UINT16)nHeight);
	}
	else if (settings->RemoteFxCodec || freerdp_settings_get_bool(settings, FreeRDP_NSCodec))
	{
		WINPR_ASSERT(nXSrc >= 0);
		WINPR_ASSERT(nXSrc <= UINT16_MAX);
		WINPR_ASSERT(nYSrc >= 0);
		WINPR_ASSERT(nYSrc <= UINT16_MAX);
		WINPR_ASSERT(nWidth >= 0);
		WINPR_ASSERT(nWidth <= UINT16_MAX);
		WINPR_ASSERT(nHeight >= 0);
		WINPR_ASSERT(nHeight <= UINT16_MAX);
		ret = shadow_client_send_surface_bits(client, pSrcData, nSrcStep, (UINT16)nXSrc,
		                                      (UINT16)nYSrc, (UINT16)nWidth, (UINT16)nHeight);
	}
	else
	{
		WINPR_ASSERT(nXSrc >= 0);
		WINPR_ASSERT(nXSrc <= UINT16_MAX);
		WINPR_ASSERT(nYSrc >= 0);
		WINPR_ASSERT(nYSrc <= UINT16_MAX);
		WINPR_ASSERT(nWidth >= 0);
		WINPR_ASSERT(nWidth <= UINT16_MAX);
		WINPR_ASSERT(nHeight >= 0);
		WINPR_ASSERT(nHeight <= UINT16_MAX);
		ret = shadow_client_send_bitmap_update(client, pSrcData, nSrcStep, (UINT16)nXSrc,
		                                       (UINT16)nYSrc, (UINT16)nWidth, (UINT16)nHeight);
	}

out:
	LeaveCriticalSection(&surface->lock);
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
static BOOL shadow_client_send_resize(rdpShadowClient* client, SHADOW_GFX_STATUS* pStatus)
{
	rdpContext* context = (rdpContext*)client;
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
	WINPR_ASSERT(peer->context);
	WINPR_ASSERT(peer->context->update);
	WINPR_ASSERT(peer->context->update->DesktopResize);
	if (!peer->context->update->DesktopResize(peer->context->update->context))
	{
		WLog_ERR(TAG, "DesktopResize failed");
		return FALSE;
	}

	/* Clear my invalidRegion. shadow_client_activate refreshes fullscreen */
	EnterCriticalSection(&(client->lock));
	region16_clear(&(client->invalidRegion));
	LeaveCriticalSection(&(client->lock));
	WLog_INFO(TAG, "Client from %s is resized (%" PRIu32 "x%" PRIu32 "@%" PRIu32 ")",
	          peer->hostname, settings->DesktopWidth, settings->DesktopHeight,
	          freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth));
	return TRUE;
}

/**
 * Function description
 * Mark invalid region for client
 *
 * @return TRUE on success
 */
static BOOL shadow_client_surface_update(rdpShadowClient* client, REGION16* region)
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
	WINPR_UNUSED(pStatus);
	WINPR_ASSERT(client);
	server = client->server;
	WINPR_ASSERT(server);
	surface = client->inLobby ? server->lobby : server->surface;
	return shadow_client_surface_update(client, &(surface->invalidRegion));
}

static int shadow_client_subsystem_process_message(rdpShadowClient* client, wMessage* message)
{
	rdpContext* context = (rdpContext*)client;
	rdpUpdate* update;

	WINPR_ASSERT(message);
	WINPR_ASSERT(context);
	update = context->update;
	WINPR_ASSERT(update);

	/* FIXME: the pointer updates appear to be broken when used with bulk compression and mstsc */

	switch (message->id)
	{
		case SHADOW_MSG_OUT_POINTER_POSITION_UPDATE_ID:
		{
			POINTER_POSITION_UPDATE pointerPosition;
			const SHADOW_MSG_OUT_POINTER_POSITION_UPDATE* msg =
			    (const SHADOW_MSG_OUT_POINTER_POSITION_UPDATE*)message->wParam;
			pointerPosition.xPos = msg->xPos;
			pointerPosition.yPos = msg->yPos;

			WINPR_ASSERT(client->server);
			if (client->server->shareSubRect)
			{
				pointerPosition.xPos -= client->server->subRect.left;
				pointerPosition.yPos -= client->server->subRect.top;
			}

			if (client->activated)
			{
				if ((msg->xPos != client->pointerX) || (msg->yPos != client->pointerY))
				{
					WINPR_ASSERT(update->pointer);
					IFCALL(update->pointer->PointerPosition, context, &pointerPosition);
					client->pointerX = msg->xPos;
					client->pointerY = msg->yPos;
				}
			}

			break;
		}

		case SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID:
		{
			POINTER_NEW_UPDATE pointerNew = { 0 };
			POINTER_COLOR_UPDATE* pointerColor = { 0 };
			POINTER_CACHED_UPDATE pointerCached = { 0 };
			const SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE* msg =
			    (const SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE*)message->wParam;

			WINPR_ASSERT(msg);
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
			const SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES* msg =
			    (const SHADOW_MSG_OUT_AUDIO_OUT_SAMPLES*)message->wParam;

			WINPR_ASSERT(msg);

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
			const SHADOW_MSG_OUT_AUDIO_OUT_VOLUME* msg =
			    (const SHADOW_MSG_OUT_AUDIO_OUT_VOLUME*)message->wParam;

			if (client->activated && client->rdpsnd && client->rdpsnd->Activated)
			{
				IFCALL(client->rdpsnd->SetVolume, client->rdpsnd, msg->left, msg->right);
			}

			break;
		}

		default:
			WLog_ERR(TAG, "Unknown message id: %" PRIu32 "", message->id);
			break;
	}

	shadow_client_free_queued_message(message);
	return 1;
}

static DWORD WINAPI shadow_client_thread(LPVOID arg)
{
	rdpShadowClient* client = (rdpShadowClient*)arg;
	BOOL rc;
	DWORD status;
	DWORD nCount;
	wMessage message;
	wMessage pointerPositionMsg;
	wMessage pointerAlphaMsg;
	wMessage audioVolumeMsg;
	HANDLE events[32] = { 0 };
	HANDLE ChannelEvent;
	void* UpdateSubscriber;
	HANDLE UpdateEvent;
	freerdp_peer* peer;
	rdpContext* context;
	rdpSettings* settings;
	rdpShadowServer* server;
	rdpShadowSubsystem* subsystem;
	wMessageQueue* MsgQueue;
	/* This should only be visited in client thread */
	SHADOW_GFX_STATUS gfxstatus = { 0 };
	rdpUpdate* update;

	WINPR_ASSERT(client);

	MsgQueue = client->MsgQueue;
	WINPR_ASSERT(MsgQueue);

	server = client->server;
	WINPR_ASSERT(server);
	subsystem = server->subsystem;
	context = (rdpContext*)client;
	peer = context->peer;
	WINPR_ASSERT(peer);
	WINPR_ASSERT(peer->context);

	settings = peer->context->settings;
	WINPR_ASSERT(settings);

	peer->Capabilities = shadow_client_capabilities;
	peer->PostConnect = shadow_client_post_connect;
	peer->Activate = shadow_client_activate;
	peer->Logon = shadow_client_logon;
	shadow_input_register_callbacks(peer->context->input);

	rc = peer->Initialize(peer);
	WINPR_ASSERT(rc);

	update = peer->context->update;
	WINPR_ASSERT(update);

	update->RefreshRect = shadow_client_refresh_rect;
	update->SuppressOutput = shadow_client_suppress_output;
	update->SurfaceFrameAcknowledge = shadow_client_surface_frame_acknowledge;

	if ((!client->vcm) || (!subsystem->updateEvent))
		goto out;

	UpdateSubscriber = shadow_multiclient_get_subscriber(subsystem->updateEvent);

	if (!UpdateSubscriber)
		goto out;

	UpdateEvent = shadow_multiclient_getevent(UpdateSubscriber);
	WINPR_ASSERT(UpdateEvent);

	ChannelEvent = WTSVirtualChannelManagerGetEventHandle(client->vcm);
	WINPR_ASSERT(ChannelEvent);

	rc = freerdp_settings_set_bool(settings, FreeRDP_UnicodeInput, TRUE);
	WINPR_ASSERT(rc);
	rc = freerdp_settings_set_bool(settings, FreeRDP_HasHorizontalWheel, TRUE);
	WINPR_ASSERT(rc);
	rc = freerdp_settings_set_bool(settings, FreeRDP_HasExtendedMouseEvent, TRUE);
	WINPR_ASSERT(rc);

	while (1)
	{
		nCount = 0;
		events[nCount++] = UpdateEvent;
		{
			DWORD tmp = peer->GetEventHandles(peer, &events[nCount], 64 - nCount);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "Failed to get FreeRDP transport event handles");
				goto fail;
			}

			nCount += tmp;
		}
		events[nCount++] = ChannelEvent;
		events[nCount++] = MessageQueue_Event(MsgQueue);
		status = WaitForMultipleObjects(nCount, events, FALSE, INFINITE);

		if (status == WAIT_FAILED)
			goto fail;

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

		WINPR_ASSERT(peer->CheckFileDescriptor);
		if (!peer->CheckFileDescriptor(peer))
		{
			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			goto fail;
		}
		else
		{
			if (WTSVirtualChannelManagerIsChannelJoined(client->vcm, DRDYNVC_SVC_CHANNEL_NAME))
			{
				switch (WTSVirtualChannelManagerGetDrdynvcState(client->vcm))
				{
					/* Dynamic channel status may have been changed after processing */
					case DRDYNVC_STATE_NONE:

						/* Call this routine to Initialize drdynvc channel */
						if (!WTSVirtualChannelManagerCheckFileDescriptor(client->vcm))
						{
							WLog_ERR(TAG, "Failed to initialize drdynvc channel");
							goto fail;
						}

						break;

					case DRDYNVC_STATE_READY:
						if (client->audin &&
						    !IFCALLRESULT(TRUE, client->audin->IsOpen, client->audin))
						{
							if (!IFCALLRESULT(FALSE, client->audin->Open, client->audin))
							{
								WLog_ERR(TAG, "Failed to initialize audin channel");
								goto fail;
							}
						}

						/* Init RDPGFX dynamic channel */
						if (settings->SupportGraphicsPipeline && client->rdpgfx &&
						    !gfxstatus.gfxOpened)
						{
							client->rdpgfx->FrameAcknowledge =
							    shadow_client_rdpgfx_frame_acknowledge;
							client->rdpgfx->CapsAdvertise = shadow_client_rdpgfx_caps_advertise;

							if (!client->rdpgfx->Open(client->rdpgfx))
							{
								WLog_WARN(TAG, "Failed to open GraphicsPipeline");
								settings->SupportGraphicsPipeline = FALSE;
							}
							else
							{
								gfxstatus.gfxOpened = TRUE;
								WLog_INFO(TAG, "Gfx Pipeline Opened");
							}
						}

						break;

					default:
						break;
				}
			}
		}

		if (WaitForSingleObject(ChannelEvent, 0) == WAIT_OBJECT_0)
		{
			if (!WTSVirtualChannelManagerCheckFileDescriptor(client->vcm))
			{
				WLog_ERR(TAG, "WTSVirtualChannelManagerCheckFileDescriptor failure");
				goto fail;
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
						pointerPositionMsg = message;
						break;

					case SHADOW_MSG_OUT_POINTER_ALPHA_UPDATE_ID:
						/* Abandon previous message */
						shadow_client_free_queued_message(&pointerAlphaMsg);
						pointerAlphaMsg = message;
						break;

					case SHADOW_MSG_OUT_AUDIO_OUT_VOLUME_ID:
						/* Abandon previous message */
						shadow_client_free_queued_message(&audioVolumeMsg);
						audioVolumeMsg = message;
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
				goto fail;
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

fail:

	/* Free channels early because we establish channels in post connect */
	if (client->audin && !IFCALLRESULT(TRUE, client->audin->IsOpen, client->audin))
	{
		if (!IFCALLRESULT(FALSE, client->audin->Close, client->audin))
		{
			WLog_WARN(TAG, "AUDIN shutdown failure!");
		}
	}

	if (gfxstatus.gfxOpened)
	{
		if (gfxstatus.gfxSurfaceCreated)
		{
			if (!shadow_client_rdpgfx_release_surface(client))
				WLog_WARN(TAG, "GFX release surface failure!");
		}

		WINPR_ASSERT(client->rdpgfx);
		WINPR_ASSERT(client->rdpgfx->Close);
		rc = client->rdpgfx->Close(client->rdpgfx);
		WINPR_ASSERT(rc);
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
	WINPR_ASSERT(peer->Disconnect);
	peer->Disconnect(peer);
	freerdp_peer_context_free(peer);
	freerdp_peer_free(peer);
	ExitThread(0);
	return 0;
}

BOOL shadow_client_accepted(freerdp_listener* listener, freerdp_peer* peer)
{
	rdpShadowClient* client;
	rdpShadowServer* server;

	if (!listener || !peer)
		return FALSE;

	server = (rdpShadowServer*)listener->info;
	WINPR_ASSERT(server);

	peer->ContextExtra = (void*)server;
	peer->ContextSize = sizeof(rdpShadowClient);
	peer->ContextNew = shadow_client_context_new;
	peer->ContextFree = shadow_client_context_free;

	if (!freerdp_peer_context_new_ex(peer, server->settings))
		return FALSE;

	client = (rdpShadowClient*)peer->context;
	WINPR_ASSERT(client);

	if (!(client->thread = CreateThread(NULL, 0, shadow_client_thread, client, 0, NULL)))
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
	SHADOW_MSG_OUT* msg;

	WINPR_ASSERT(message);
	msg = (SHADOW_MSG_OUT*)message->wParam;
	WINPR_ASSERT(msg);

	InterlockedIncrement(&(msg->refCount));
}

static void shadow_msg_out_release(wMessage* message)
{
	SHADOW_MSG_OUT* msg;

	WINPR_ASSERT(message);
	msg = (SHADOW_MSG_OUT*)message->wParam;
	WINPR_ASSERT(msg);

	if (InterlockedDecrement(&(msg->refCount)) <= 0)
	{
		IFCALL(msg->Free, message->id, msg);
	}
}

static BOOL shadow_client_dispatch_msg(rdpShadowClient* client, wMessage* message)
{
	if (!client || !message)
		return FALSE;

	/* Add reference when it is posted */
	shadow_msg_out_addref(message);

	WINPR_ASSERT(client->MsgQueue);
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
	wMessage message = { 0 };
	message.context = context;
	message.id = type;
	message.wParam = (void*)msg;
	message.lParam = lParam;
	message.Free = shadow_msg_out_release;
	return shadow_client_dispatch_msg(client, &message);
}

int shadow_client_boardcast_msg(rdpShadowServer* server, void* context, UINT32 type,
                                SHADOW_MSG_OUT* msg, void* lParam)
{
	wMessage message = { 0 };
	rdpShadowClient* client = NULL;
	int count = 0;
	size_t index = 0;

	WINPR_ASSERT(server);
	WINPR_ASSERT(msg);

	message.context = context;
	message.id = type;
	message.wParam = (void*)msg;
	message.lParam = lParam;
	message.Free = shadow_msg_out_release;
	/* First add reference as we reference it in this function.
	 * Therefore it would not be free'ed during post. */
	shadow_msg_out_addref(&message);

	WINPR_ASSERT(server->clients);
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
	size_t index = 0;

	WINPR_ASSERT(server);
	WINPR_ASSERT(server->clients);

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
