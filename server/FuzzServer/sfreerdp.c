/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Test Server
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Vic Lee
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
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

#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include <winpr/winpr.h>
#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/ssl.h>
#include <winpr/synch.h>
#include <winpr/file.h>
#include <winpr/string.h>
#include <winpr/path.h>
#include <winpr/winsock.h>

#include <winpr/tools/makecert.h>

#include <freerdp/streamdump.h>
#include <freerdp/transport_io.h>

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>
#include <freerdp/channels/drdynvc.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/server/rdpsnd.h>
#include <freerdp/settings.h>

#include "sfreerdp.h"

#include <freerdp/log.h>
#define TAG SERVER_TAG("fuzz")

#define SAMPLE_SERVER_USE_CLIENT_RESOLUTION 1
#define SAMPLE_SERVER_DEFAULT_WIDTH 1024
#define SAMPLE_SERVER_DEFAULT_HEIGHT 768

struct server_info
{
	char* file;
	char* path;
	char* cert;
	char* key;
	HANDLE event;
	int fd;
};

static void test_peer_context_free(freerdp_peer* client, rdpContext* ctx)
{
	testPeerContext* context = (testPeerContext*)ctx;

	WINPR_UNUSED(client);

	if (context)
	{

		Stream_Free(context->s, TRUE);
		rfx_context_free(context->rfx_context);
		nsc_context_free(context->nsc_context);

		WTSCloseServer((HANDLE)context->vcm);

		context->stopEvent = NULL;
		context->s = NULL;
		context->rfx_context = NULL;
		context->nsc_context = NULL;
		context->vcm = NULL;
	}
}

static BOOL test_peer_context_new(freerdp_peer* client, rdpContext* ctx)
{
	testPeerContext* context = (testPeerContext*)ctx;

	WINPR_ASSERT(client);
	WINPR_ASSERT(context);
	WINPR_ASSERT(ctx->settings);

	WINPR_ASSERT(!context->rfx_context);
	if (!(context->rfx_context = rfx_context_new_ex(TRUE, ctx->settings->ThreadingFlags)))
		goto fail;

	if (!rfx_context_reset(context->rfx_context, SAMPLE_SERVER_DEFAULT_WIDTH,
	                       SAMPLE_SERVER_DEFAULT_HEIGHT))
		goto fail;

	rfx_context_set_mode(context->rfx_context, RLGR3);
	rfx_context_set_pixel_format(context->rfx_context, PIXEL_FORMAT_RGB24);

	WINPR_ASSERT(!context->nsc_context);
	if (!(context->nsc_context = nsc_context_new()))
		goto fail;

	if (!nsc_context_set_parameters(context->nsc_context, NSC_COLOR_FORMAT, PIXEL_FORMAT_RGB24))
		goto fail;

	WINPR_ASSERT(!context->s);
	if (!(context->s = Stream_New(NULL, 65536)))
		goto fail;

	WINPR_ASSERT(!context->vcm);
	context->vcm = WTSOpenServerA((LPSTR)client->context);

	if (!context->vcm || context->vcm == INVALID_HANDLE_VALUE)
		goto fail;

	return TRUE;
fail:
	test_peer_context_free(client, ctx);
	return FALSE;
}

static BOOL test_peer_init(freerdp_peer* client)
{
	WINPR_ASSERT(client);

	client->ContextSize = sizeof(testPeerContext);
	client->ContextNew = test_peer_context_new;
	client->ContextFree = test_peer_context_free;
	return freerdp_peer_context_new(client);
}

static wStream* test_peer_stream_init(testPeerContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->s);

	Stream_Clear(context->s);
	Stream_SetPosition(context->s, 0);
	return context->s;
}

static void test_peer_begin_frame(freerdp_peer* client)
{
	rdpUpdate* update;
	SURFACE_FRAME_MARKER fm = { 0 };
	testPeerContext* context;

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);

	update = client->context->update;
	WINPR_ASSERT(update);

	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	fm.frameAction = SURFACECMD_FRAMEACTION_BEGIN;
	fm.frameId = context->frame_id;
	WINPR_ASSERT(update->SurfaceFrameMarker);
	update->SurfaceFrameMarker(update->context, &fm);
}

static void test_peer_end_frame(freerdp_peer* client)
{
	rdpUpdate* update;
	SURFACE_FRAME_MARKER fm = { 0 };
	testPeerContext* context;

	WINPR_ASSERT(client);

	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	update = client->context->update;
	WINPR_ASSERT(update);

	fm.frameAction = SURFACECMD_FRAMEACTION_END;
	fm.frameId = context->frame_id;
	WINPR_ASSERT(update->SurfaceFrameMarker);
	update->SurfaceFrameMarker(update->context, &fm);
	context->frame_id++;
}

static BOOL test_peer_draw_background(freerdp_peer* client)
{
	size_t size;
	wStream* s;
	RFX_RECT rect;
	BYTE* rgb_data;
	const rdpSettings* settings;
	rdpUpdate* update;
	SURFACE_BITS_COMMAND cmd = { 0 };
	testPeerContext* context;
	BOOL ret = FALSE;

	WINPR_ASSERT(client);
	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	update = client->context->update;
	WINPR_ASSERT(update);

	if (!settings->RemoteFxCodec && !freerdp_settings_get_bool(settings, FreeRDP_NSCodec))
		return FALSE;

	WINPR_ASSERT(settings->DesktopWidth <= UINT16_MAX);
	WINPR_ASSERT(settings->DesktopHeight <= UINT16_MAX);

	s = test_peer_stream_init(context);
	rect.x = 0;
	rect.y = 0;
	rect.width = (UINT16)settings->DesktopWidth;
	rect.height = (UINT16)settings->DesktopHeight;
	size = rect.width * rect.height * 3ULL;

	if (!(rgb_data = malloc(size)))
	{
		WLog_ERR(TAG, "Problem allocating memory");
		return FALSE;
	}

	memset(rgb_data, 0xA0, size);

	if (settings->RemoteFxCodec)
	{
		WLog_DBG(TAG, "Using RemoteFX codec");
		if (!rfx_compose_message(context->rfx_context, s, &rect, 1, rgb_data, rect.width,
		                         rect.height, rect.width * 3))
		{
			goto out;
		}

		WINPR_ASSERT(settings->RemoteFxCodecId <= UINT16_MAX);
		cmd.bmp.codecID = (UINT16)settings->RemoteFxCodecId;
		cmd.cmdType = CMDTYPE_STREAM_SURFACE_BITS;
	}
	else
	{
		WLog_DBG(TAG, "Using NSCodec");
		nsc_compose_message(context->nsc_context, s, rgb_data, rect.width, rect.height,
		                    rect.width * 3ULL);
		WINPR_ASSERT(settings->NSCodecId <= UINT16_MAX);
		cmd.bmp.codecID = (UINT16)settings->NSCodecId;
		cmd.cmdType = CMDTYPE_SET_SURFACE_BITS;
	}

	cmd.destLeft = 0;
	cmd.destTop = 0;
	cmd.destRight = rect.width;
	cmd.destBottom = rect.height;
	cmd.bmp.bpp = 32;
	cmd.bmp.flags = 0;
	cmd.bmp.width = rect.width;
	cmd.bmp.height = rect.height;
	WINPR_ASSERT(Stream_GetPosition(s) <= UINT32_MAX);
	cmd.bmp.bitmapDataLength = (UINT32)Stream_GetPosition(s);
	cmd.bmp.bitmapData = Stream_Buffer(s);
	test_peer_begin_frame(client);
	update->SurfaceBits(update->context, &cmd);
	test_peer_end_frame(client);
	ret = TRUE;
out:
	free(rgb_data);
	return ret;
}

static BOOL test_sleep_tsdiff(UINT32* old_sec, UINT32* old_usec, UINT32 new_sec, UINT32 new_usec)
{
	INT64 sec, usec;

	WINPR_ASSERT(old_sec);
	WINPR_ASSERT(old_usec);

	if ((*old_sec == 0) && (*old_usec == 0))
	{
		*old_sec = new_sec;
		*old_usec = new_usec;
		return TRUE;
	}

	sec = new_sec - *old_sec;
	usec = new_usec - *old_usec;

	if ((sec < 0) || ((sec == 0) && (usec < 0)))
	{
		WLog_ERR(TAG, "Invalid time stamp detected.");
		return FALSE;
	}

	*old_sec = new_sec;
	*old_usec = new_usec;

	while (usec < 0)
	{
		usec += 1000000;
		sec--;
	}

	if (sec > 0)
		Sleep((DWORD)sec * 1000);

	if (usec > 0)
		USleep((DWORD)usec);

	return TRUE;
}

static BOOL tf_peer_post_connect(freerdp_peer* client)
{
	testPeerContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(client);

	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	/**
	 * This callback is called when the entire connection sequence is done, i.e. we've received the
	 * Font List PDU from the client and sent out the Font Map PDU.
	 * The server may start sending graphics output and receiving keyboard/mouse input after this
	 * callback returns.
	 */
	WLog_DBG(TAG, "Client %s is activated (osMajorType %" PRIu32 " osMinorType %" PRIu32 ")",
	         client->local ? "(local)" : client->hostname, settings->OsMajorType,
	         settings->OsMinorType);

	if (settings->AutoLogonEnabled)
	{
		WLog_DBG(TAG, " and wants to login automatically as %s\\%s",
		         settings->Domain ? settings->Domain : "", settings->Username);
		/* A real server may perform OS login here if NLA is not executed previously. */
	}

	WLog_DBG(TAG, "");
	WLog_DBG(TAG, "Client requested desktop: %" PRIu32 "x%" PRIu32 "x%" PRIu32 "",
	         settings->DesktopWidth, settings->DesktopHeight,
	         freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth));
#if (SAMPLE_SERVER_USE_CLIENT_RESOLUTION == 1)

	if (!rfx_context_reset(context->rfx_context, settings->DesktopWidth, settings->DesktopHeight))
		return FALSE;

	WLog_DBG(TAG, "Using resolution requested by client.");
#else
	client->settings->DesktopWidth = context->rfx_context->width;
	client->settings->DesktopHeight = context->rfx_context->height;
	WLog_DBG(TAG, "Resizing client to %" PRIu32 "x%" PRIu32 "", client->settings->DesktopWidth,
	         client->settings->DesktopHeight);
	client->update->DesktopResize(client->update->context);
#endif

	/* Return FALSE here would stop the execution of the peer main loop. */
	return TRUE;
}

static BOOL tf_peer_activate(freerdp_peer* client)
{
	testPeerContext* context;
	struct server_info* info;
	rdpSettings* settings;

	WINPR_ASSERT(client);

	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	info = client->ContextExtra;
	WINPR_ASSERT(info);

	context->activated = TRUE;
	settings->CompressionLevel = PACKET_COMPR_TYPE_RDP8;
	test_peer_draw_background(client);

	return TRUE;
}

static BOOL tf_peer_synchronize_event(rdpInput* input, UINT32 flags)
{
	WINPR_UNUSED(input);
	WINPR_ASSERT(input);
	WLog_DBG(TAG, "Client sent a synchronize event (flags:0x%" PRIX32 ")", flags);
	return TRUE;
}

static BOOL tf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT8 code)
{
	freerdp_peer* client;
	rdpUpdate* update;
	rdpContext* context;
	testPeerContext* tcontext;
	rdpSettings* settings;

	WINPR_ASSERT(input);

	context = input->context;
	WINPR_ASSERT(context);

	client = context->peer;
	WINPR_ASSERT(client);

	settings = context->settings;
	WINPR_ASSERT(settings);

	update = context->update;
	WINPR_ASSERT(update);

	tcontext = (testPeerContext*)context;
	WINPR_ASSERT(tcontext);

	WLog_DBG(TAG, "Client sent a keyboard event (flags:0x%04" PRIX16 " code:0x%04" PRIX8 ")", flags,
	         code);

	if (((flags & KBD_FLAGS_RELEASE) == 0) && (code == RDP_SCANCODE_KEY_G)) /* 'g' key */
	{
		if (settings->DesktopWidth != 800)
		{
			settings->DesktopWidth = 800;
			settings->DesktopHeight = 600;
		}
		else
		{
			settings->DesktopWidth = SAMPLE_SERVER_DEFAULT_WIDTH;
			settings->DesktopHeight = SAMPLE_SERVER_DEFAULT_HEIGHT;
		}

		if (!rfx_context_reset(tcontext->rfx_context, settings->DesktopWidth,
		                       settings->DesktopHeight))
			return FALSE;

		WINPR_ASSERT(update->DesktopResize);
		update->DesktopResize(update->context);
		tcontext->activated = FALSE;
	}
	else if (((flags & KBD_FLAGS_RELEASE) == 0) && code == RDP_SCANCODE_KEY_X) /* 'x' key */
	{
		WINPR_ASSERT(client->Close);
		client->Close(client);
	}

	else if (((flags & KBD_FLAGS_RELEASE) == 0) && code == RDP_SCANCODE_KEY_S) /* 's' key */
	{
	}

	return TRUE;
}

static BOOL tf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	WINPR_UNUSED(input);
	WINPR_ASSERT(input);

	WLog_DBG(TAG,
	         "Client sent a unicode keyboard event (flags:0x%04" PRIX16 " code:0x%04" PRIX16 ")",
	         flags, code);
	return TRUE;
}

static BOOL tf_peer_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	WINPR_UNUSED(flags);
	WINPR_ASSERT(input);
	WINPR_ASSERT(input->context);

	return TRUE;
}

static BOOL tf_peer_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	WINPR_UNUSED(flags);
	WINPR_ASSERT(input);
	WINPR_ASSERT(input->context);

	return TRUE;
}

static BOOL tf_peer_refresh_rect(rdpContext* context, BYTE count, const RECTANGLE_16* areas)
{
	BYTE i;
	WINPR_UNUSED(context);
	WINPR_ASSERT(context);
	WINPR_ASSERT(areas || (count == 0));

	WLog_DBG(TAG, "Client requested to refresh:");

	for (i = 0; i < count; i++)
	{
		WLog_DBG(TAG, "  (%" PRIu16 ", %" PRIu16 ") (%" PRIu16 ", %" PRIu16 ")", areas[i].left,
		         areas[i].top, areas[i].right, areas[i].bottom);
	}

	return TRUE;
}

static BOOL tf_peer_suppress_output(rdpContext* context, BYTE allow, const RECTANGLE_16* area)
{
	WINPR_UNUSED(context);

	if (allow > 0)
	{
		WINPR_ASSERT(area);
		WLog_DBG(TAG,
		         "Client restore output (%" PRIu16 ", %" PRIu16 ") (%" PRIu16 ", %" PRIu16 ").",
		         area->left, area->top, area->right, area->bottom);
	}
	else
	{
		WLog_DBG(TAG, "Client minimized and suppress output.");
	}

	return TRUE;
}

static DWORD WINAPI test_peer_mainloop(LPVOID arg)
{
	BOOL rc;
	DWORD error = CHANNEL_RC_OK;
	HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };
	DWORD count = 0;
	DWORD status = 0;
	testPeerContext* context = NULL;
	struct server_info* info;
	rdpSettings* settings;
	rdpInput* input;
	rdpUpdate* update;
	freerdp_peer* client = (freerdp_peer*)arg;

	WINPR_ASSERT(client);

	info = client->ContextExtra;
	WINPR_ASSERT(info);

	if (!test_peer_init(client))
	{
		freerdp_peer_free(client);
		return 0;
	}

	/* Initialize the real server settings here */
	WINPR_ASSERT(client->context);
	settings = client->context->settings;
	WINPR_ASSERT(settings);

	rdpPrivateKey* key = freerdp_key_new_from_file(info->key);
	if (!key)
		goto fail;
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerRsaKey, key, 1))
		goto fail;
	rdpCertificate* cert = freerdp_certificate_new_from_file(info->cert);
	if (!cert)
		goto fail;
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RdpServerCertificate, cert, 1))
		goto fail;

	settings->RdpSecurity = TRUE;
	settings->TlsSecurity = TRUE;
	settings->NlaSecurity = FALSE;
	settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
	/* settings->EncryptionLevel = ENCRYPTION_LEVEL_HIGH; */
	/* settings->EncryptionLevel = ENCRYPTION_LEVEL_LOW; */
	/* settings->EncryptionLevel = ENCRYPTION_LEVEL_FIPS; */
	settings->RemoteFxCodec = TRUE;
	if (!freerdp_settings_set_bool(settings, FreeRDP_NSCodec, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32))
		goto fail;

	settings->SuppressOutput = TRUE;
	settings->RefreshRect = TRUE;

	client->PostConnect = tf_peer_post_connect;
	client->Activate = tf_peer_activate;

	WINPR_ASSERT(client->context);
	input = client->context->input;
	WINPR_ASSERT(input);

	input->SynchronizeEvent = tf_peer_synchronize_event;
	input->KeyboardEvent = tf_peer_keyboard_event;
	input->UnicodeKeyboardEvent = tf_peer_unicode_keyboard_event;
	input->MouseEvent = tf_peer_mouse_event;
	input->ExtendedMouseEvent = tf_peer_extended_mouse_event;

	update = client->context->update;
	WINPR_ASSERT(update);

	update->RefreshRect = tf_peer_refresh_rect;
	update->SuppressOutput = tf_peer_suppress_output;
	settings->MultifragMaxRequestSize = 0xFFFFFF; /* FIXME */

	WINPR_ASSERT(client->Initialize);
	rc = client->Initialize(client);
	if (!rc)
		goto fail;

	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	WLog_INFO(TAG, "We've got a client %s", client->local ? "(local)" : client->hostname);

	while (error == CHANNEL_RC_OK)
	{
		count = 0;
		{
			WINPR_ASSERT(client->GetEventHandles);
			DWORD tmp =
			    client->GetEventHandles(client, &handles[count], ARRAYSIZE(handles) - count);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "Failed to get FreeRDP transport event handles");
				break;
			}

			count += tmp;
		}
		handles[count++] = WTSVirtualChannelManagerGetEventHandle(context->vcm);
		status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForMultipleObjects failed (errno: %d)", errno);
			break;
		}

		WINPR_ASSERT(client->CheckFileDescriptor);
		if (client->CheckFileDescriptor(client) != TRUE)
			break;

		if (WTSVirtualChannelManagerCheckFileDescriptor(context->vcm) != TRUE)
			break;

		/* Handle dynamic virtual channel intializations */
		if (WTSVirtualChannelManagerIsChannelJoined(context->vcm, DRDYNVC_SVC_CHANNEL_NAME))
		{
			switch (WTSVirtualChannelManagerGetDrdynvcState(context->vcm))
			{
				case DRDYNVC_STATE_NONE:
					break;

				case DRDYNVC_STATE_INITIALIZED:
					break;

				case DRDYNVC_STATE_READY:
					break;

				case DRDYNVC_STATE_FAILED:
				default:
					break;
			}
		}
	}

	WLog_INFO(TAG, "Client %s disconnected.", client->local ? "(local)" : client->hostname);

	WINPR_ASSERT(client->Disconnect);
	client->Disconnect(client);
fail:
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	return error;
}

static BOOL test_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	HANDLE hThread;
	struct server_info* info;

	WINPR_UNUSED(instance);

	WINPR_ASSERT(instance);
	WINPR_ASSERT(client);

	info = instance->info;
	client->ContextExtra = info;

	if (!(hThread = CreateThread(NULL, 0, test_peer_mainloop, (void*)client, 0, NULL)))
		return FALSE;

	CloseHandle(hThread);
	return TRUE;
}

static DWORD WINAPI test_server_mainloop(void* arg)
{
	freerdp_listener* instance = arg;

	WINPR_ASSERT(instance);

	struct server_info* info = instance->info;
	WINPR_ASSERT(info);

	while (WaitForSingleObject(info->event, 0) != WAIT_OBJECT_0)
	{
		HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };
		WINPR_ASSERT(instance->GetEventHandles);
		DWORD count = instance->GetEventHandles(instance, handles, ARRAYSIZE(handles) - 1);

		if (0 == count)
		{
			WLog_ERR(TAG, "Failed to get FreeRDP event handles");
			break;
		}
		handles[count++] = info->event;

		const DWORD status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);

		if (WAIT_FAILED == status)
		{
			WLog_ERR(TAG, "select failed");
			break;
		}

		WINPR_ASSERT(instance->CheckFileDescriptor);
		if (instance->CheckFileDescriptor(instance) != TRUE)
		{
			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			break;
		}
	}

	WINPR_ASSERT(instance->Close);
	instance->Close(instance);
	return 0;
}

static HANDLE run_server(struct server_info* info)
{
	WINPR_ASSERT(info);

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
	freerdp_listener* instance = freerdp_listener_new();

	if (!instance)
		return NULL;

	instance->info = (void*)info;
	instance->PeerAccepted = test_peer_accepted;

	WSADATA wsaData = { 0 };
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		goto fail;

	/* Open the server socket and start listening. */
	char name[MAX_PATH] = { 0 };
	UINT16 port = 0;
	winpr_RAND(&port, sizeof(port));

	sprintf_s(name, sizeof(name), "fuzzsrv.%04" PRIx16, port);
	info->file = GetKnownSubPath(KNOWN_PATH_TEMP, name);

	if (!info->file)
		goto fail;

	WINPR_ASSERT(instance->OpenLocal);
	const BOOL started = instance->OpenLocal(instance, info->file);

	if (!started)
		goto fail;

	return CreateThread(NULL, 0, test_server_mainloop, instance, 0, NULL);

fail:
	freerdp_listener_free(instance);
	WSACleanup();
	return NULL;
}

static char* subdir(const char* path, const char* name)
{
	char* str = NULL;
	size_t len = 0;
	winpr_asprintf(&str, &len, "%s/%s", path, name);
	return str;
}

static BOOL generate_certs(struct server_info* info)
{
	MAKECERT_CONTEXT* ctx = NULL;
	BOOL res = FALSE;
	WINPR_ASSERT(info);

	const char randstr[] = "oss-fuzz-server-cert";
	info->path = GetKnownSubPath(KNOWN_PATH_TEMP, randstr);
	if (!info->path)
		goto fail;
	ctx = makecert_context_new();
	if (!ctx)
		goto fail;

	char* makecert_argv[] = { "makecert", "-rdp",  "-silent",  "-y",
		                      "5",        "-path", info->path, "temporary" };

	const int rc = makecert_context_process(ctx, ARRAYSIZE(makecert_argv), makecert_argv);
	if (rc != 0)
		goto fail;
	info->cert = subdir(info->path, "temporary.crt");
	info->key = subdir(info->path, "temporary.key");

	res = info->key && info->cert;
fail:
	makecert_context_free(ctx);
	return res;
}

static void info_free(struct server_info* info)
{
	if (!info)
		return;

	if (info->cert)
		winpr_DeleteFile(info->cert);
	free(info->cert);

	if (info->key)
		winpr_DeleteFile(info->key);
	free(info->key);

	if (info->path && winpr_PathFileExists(info->path))
		winpr_RemoveDirectory(info->path);
	free(info->path);
	free(info->file);

	if (info->fd >= 0)
		close(info->fd);
	CloseHandle(info->event);
}

static struct server_info* info_new(void)
{
	struct server_info* info = calloc(1, sizeof(struct server_info));
	if (!info)
		return NULL;
	info->event = CreateEventA(NULL, TRUE, FALSE, NULL);
	return info;
fail:
	info_free(info);
	return NULL;
}

static BOOL write_data(int fd, const void* data, size_t size)
{
	const ssize_t r = write(fd, data, size);
	if (r < 0)
		return FALSE;
	return (size_t)r == size;
}

static BOOL open_socket(struct server_info* info)
{
	WINPR_ASSERT(info);

	struct sockaddr_un saddr = { AF_UNIX, {} };
	strncpy(saddr.sun_path, info->file, sizeof(saddr.sun_path));
	socklen_t saddrlen = sizeof(saddr);

	const int sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (sock < 0)
		return FALSE;
	const int conn = connect(sock, &saddr, saddrlen);
	if (conn < 0)
		return FALSE;
	if (info->fd < 0)
	{
		fprintf(stderr, "failed to open file '%s' for writing. (%s [%d])\n", info->file,
		        strerror(errno), errno);
	}
	info->fd = sock;
	return info->fd >= 0;
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	int rc = -1;
	HANDLE thread = NULL;
	struct server_info* info = info_new();
	if (!info)
		goto fail;
	if (!generate_certs(info))
		goto fail;

	thread = run_server(info);
	if (!thread)
		goto fail;

	if (!open_socket(info))
		goto fail;

	if (!write_data(info->fd, Data, Size))
		goto fail;

	rc = 0;
fail:
	if (thread)
	{
		SetEvent(info->event);
		WaitForSingleObject(thread, INFINITE);
	}
	info_free(info);
	return rc;
}
