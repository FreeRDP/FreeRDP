/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Test Server
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Vic Lee
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
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

#include <winpr/winpr.h>
#include <winpr/crt.h>
#include <winpr/assert.h>
#include <winpr/ssl.h>
#include <winpr/synch.h>
#include <winpr/file.h>
#include <winpr/string.h>
#include <winpr/path.h>
#include <winpr/winsock.h>

#include <freerdp/streamdump.h>
#include <freerdp/transport_io.h>

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>
#include <freerdp/channels/drdynvc.h>

#include <freerdp/constants.h>
#include <freerdp/server/rdpsnd.h>

#include "sf_ainput.h"
#include "sf_audin.h"
#include "sf_rdpsnd.h"
#include "sf_encomsp.h"

#include "sfreerdp.h"

#include <freerdp/log.h>
#define TAG SERVER_TAG("sample")

#define SAMPLE_SERVER_USE_CLIENT_RESOLUTION 1
#define SAMPLE_SERVER_DEFAULT_WIDTH 1024
#define SAMPLE_SERVER_DEFAULT_HEIGHT 768

struct server_info
{
	BOOL test_dump_rfx_realtime;
	const char* test_pcap_file;
	const char* replay_dump;
	const char* cert;
	const char* key;
};

static void test_peer_context_free(freerdp_peer* client, rdpContext* ctx)
{
	testPeerContext* context = (testPeerContext*)ctx;

	WINPR_UNUSED(client);

	if (context)
	{
		if (context->debug_channel_thread)
		{
			WINPR_ASSERT(context->stopEvent);
			SetEvent(context->stopEvent);
			WaitForSingleObject(context->debug_channel_thread, INFINITE);
			CloseHandle(context->debug_channel_thread);
		}

		Stream_Free(context->s, TRUE);
		free(context->icon_data);
		free(context->bg_data);
		rfx_context_free(context->rfx_context);
		nsc_context_free(context->nsc_context);

		if (context->debug_channel)
			WTSVirtualChannelClose(context->debug_channel);

		sf_peer_audin_uninit(context);

#if defined(CHANNEL_AINPUT_SERVER)
		sf_peer_ainput_uninit(context);
#endif

		rdpsnd_server_context_free(context->rdpsnd);
		encomsp_server_context_free(context->encomsp);

		WTSCloseServer((HANDLE)context->vcm);
	}
}

static BOOL test_peer_context_new(freerdp_peer* client, rdpContext* ctx)
{
	testPeerContext* context = (testPeerContext*)ctx;

	WINPR_ASSERT(client);
	WINPR_ASSERT(context);
	WINPR_ASSERT(ctx->settings);

	if (!(context->rfx_context = rfx_context_new_ex(TRUE, ctx->settings->ThreadingFlags)))
		goto fail;

	if (!rfx_context_reset(context->rfx_context, SAMPLE_SERVER_DEFAULT_WIDTH,
	                       SAMPLE_SERVER_DEFAULT_HEIGHT))
		goto fail;

	context->rfx_context->mode = RLGR3;
	rfx_context_set_pixel_format(context->rfx_context, PIXEL_FORMAT_RGB24);

	if (!(context->nsc_context = nsc_context_new()))
		goto fail;

	if (!nsc_context_set_parameters(context->nsc_context, NSC_COLOR_FORMAT, PIXEL_FORMAT_RGB24))
		goto fail;

	if (!(context->s = Stream_New(NULL, 65536)))
		goto fail;

	context->icon_x = UINT32_MAX;
	context->icon_y = UINT32_MAX;
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

static BOOL test_peer_load_icon(freerdp_peer* client)
{
	testPeerContext* context;
	FILE* fp;
	int i;
	char line[50];
	BYTE* rgb_data = NULL;
	int c;
	rdpSettings* settings;

	WINPR_ASSERT(client);

	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	if (!settings->RemoteFxCodec && !freerdp_settings_get_bool(settings, FreeRDP_NSCodec))
	{
		WLog_ERR(TAG, "Client doesn't support RemoteFX or NSCodec");
		return FALSE;
	}

	if ((fp = winpr_fopen("test_icon.ppm", "r")) == NULL)
	{
		WLog_ERR(TAG, "Unable to open test icon");
		return FALSE;
	}

	/* P3 */
	fgets(line, sizeof(line), fp);
	/* Creater comment */
	fgets(line, sizeof(line), fp);
	/* width height */
	fgets(line, sizeof(line), fp);

	if (sscanf(line, "%hu %hu", &context->icon_width, &context->icon_height) < 2)
	{
		WLog_ERR(TAG, "Problem while extracting width/height from the icon file");
		goto out_fail;
	}

	/* Max */
	fgets(line, sizeof(line), fp);

	if (!(rgb_data = calloc(context->icon_height, context->icon_width * 3)))
		goto out_fail;

	for (i = 0; i < context->icon_width * context->icon_height * 3; i++)
	{
		if (!fgets(line, sizeof(line), fp) || (sscanf(line, "%d", &c) != 1))
			goto out_fail;

		rgb_data[i] = (BYTE)c;
	}

	/* background with same size, which will be used to erase the icon from old position */
	if (!(context->bg_data = calloc(context->icon_height, context->icon_width * 3)))
		goto out_fail;

	memset(context->bg_data, 0xA0, context->icon_width * context->icon_height * 3);
	context->icon_data = rgb_data;
	fclose(fp);
	return TRUE;
out_fail:
	free(rgb_data);
	context->bg_data = NULL;
	fclose(fp);
	return FALSE;
}

static void test_peer_draw_icon(freerdp_peer* client, UINT32 x, UINT32 y)
{
	wStream* s;
	RFX_RECT rect;
	rdpUpdate* update;
	rdpSettings* settings;
	SURFACE_BITS_COMMAND cmd = { 0 };
	testPeerContext* context;

	WINPR_ASSERT(client);

	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	update = client->context->update;
	WINPR_ASSERT(update);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	if (freerdp_settings_get_bool(settings, FreeRDP_DumpRemoteFx))
		return;

	if (context->icon_width < 1 || !context->activated)
		return;

	test_peer_begin_frame(client);
	rect.x = 0;
	rect.y = 0;
	rect.width = context->icon_width;
	rect.height = context->icon_height;

	if (settings->RemoteFxCodec)
	{
		WINPR_ASSERT(settings->RemoteFxCodecId <= UINT16_MAX);
		cmd.bmp.codecID = (UINT16)settings->RemoteFxCodecId;
		cmd.cmdType = CMDTYPE_STREAM_SURFACE_BITS;
	}
	else
	{
		WINPR_ASSERT(settings->NSCodecId <= UINT16_MAX);
		cmd.bmp.codecID = (UINT16)settings->NSCodecId;
		cmd.cmdType = CMDTYPE_SET_SURFACE_BITS;
	}

	if (context->icon_x != UINT32_MAX)
	{
		s = test_peer_stream_init(context);

		if (settings->RemoteFxCodec)
			rfx_compose_message(context->rfx_context, s, &rect, 1, context->bg_data, rect.width,
			                    rect.height, rect.width * 3);
		else
			nsc_compose_message(context->nsc_context, s, context->bg_data, rect.width, rect.height,
			                    rect.width * 3);

		cmd.destLeft = context->icon_x;
		cmd.destTop = context->icon_y;
		cmd.destRight = context->icon_x + context->icon_width;
		cmd.destBottom = context->icon_y + context->icon_height;
		cmd.bmp.bpp = 32;
		cmd.bmp.flags = 0;
		cmd.bmp.width = context->icon_width;
		cmd.bmp.height = context->icon_height;
		cmd.bmp.bitmapDataLength = (UINT32)Stream_GetPosition(s);
		cmd.bmp.bitmapData = Stream_Buffer(s);
		WINPR_ASSERT(update->SurfaceBits);
		update->SurfaceBits(update->context, &cmd);
	}

	s = test_peer_stream_init(context);

	if (settings->RemoteFxCodec)
		rfx_compose_message(context->rfx_context, s, &rect, 1, context->icon_data, rect.width,
		                    rect.height, rect.width * 3);
	else
		nsc_compose_message(context->nsc_context, s, context->icon_data, rect.width, rect.height,
		                    rect.width * 3);

	cmd.destLeft = x;
	cmd.destTop = y;
	cmd.destRight = x + context->icon_width;
	cmd.destBottom = y + context->icon_height;
	cmd.bmp.bpp = 32;
	cmd.bmp.width = context->icon_width;
	cmd.bmp.height = context->icon_height;
	cmd.bmp.bitmapDataLength = (UINT32)Stream_GetPosition(s);
	cmd.bmp.bitmapData = Stream_Buffer(s);
	WINPR_ASSERT(update->SurfaceBits);
	update->SurfaceBits(update->context, &cmd);
	context->icon_x = x;
	context->icon_y = y;
	test_peer_end_frame(client);
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

static BOOL tf_peer_dump_rfx(freerdp_peer* client)
{
	wStream* s;
	UINT32 prev_seconds;
	UINT32 prev_useconds;
	rdpUpdate* update;
	rdpPcap* pcap_rfx;
	pcap_record record = { 0 };
	struct server_info* info;

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);

	info = client->ContextExtra;
	WINPR_ASSERT(info);

	s = Stream_New(NULL, 512);

	if (!s)
		return FALSE;

	update = client->context->update;
	WINPR_ASSERT(update);

	if (!(pcap_rfx = pcap_open(info->test_pcap_file, FALSE)))
		return FALSE;

	prev_seconds = prev_useconds = 0;

	while (pcap_has_next_record(pcap_rfx))
	{
		if (!pcap_get_next_record_header(pcap_rfx, &record))
			break;

		if (!Stream_EnsureCapacity(s, record.length))
			break;

		record.data = Stream_Buffer(s);
		pcap_get_next_record_content(pcap_rfx, &record);
		Stream_SetPosition(s, Stream_Capacity(s));

		if (info->test_dump_rfx_realtime &&
		    test_sleep_tsdiff(&prev_seconds, &prev_useconds, record.header.ts_sec,
		                      record.header.ts_usec) == FALSE)
			break;

		WINPR_ASSERT(update->SurfaceCommand);
		update->SurfaceCommand(update->context, s);

		WINPR_ASSERT(client->CheckFileDescriptor);
		if (client->CheckFileDescriptor(client) != TRUE)
			break;
	}

	Stream_Free(s, TRUE);
	pcap_close(pcap_rfx);
	return TRUE;
}

static DWORD WINAPI tf_debug_channel_thread_func(LPVOID arg)
{
	void* fd;
	wStream* s;
	void* buffer;
	DWORD BytesReturned = 0;
	ULONG written;
	testPeerContext* context = (testPeerContext*)arg;

	WINPR_ASSERT(context);
	if (WTSVirtualChannelQuery(context->debug_channel, WTSVirtualFileHandle, &buffer,
	                           &BytesReturned) == TRUE)
	{
		fd = *((void**)buffer);
		WTSFreeMemory(buffer);

		if (!(context->event = CreateWaitObjectEvent(NULL, TRUE, FALSE, fd)))
			return 0;
	}

	s = Stream_New(NULL, 4096);
	WTSVirtualChannelWrite(context->debug_channel, (PCHAR) "test1", 5, &written);

	while (1)
	{
		WaitForSingleObject(context->event, INFINITE);

		if (WaitForSingleObject(context->stopEvent, 0) == WAIT_OBJECT_0)
			break;

		Stream_SetPosition(s, 0);

		if (WTSVirtualChannelRead(context->debug_channel, 0, (PCHAR)Stream_Buffer(s),
		                          (ULONG)Stream_Capacity(s), &BytesReturned) == FALSE)
		{
			if (BytesReturned == 0)
				break;

			Stream_EnsureRemainingCapacity(s, BytesReturned);

			if (WTSVirtualChannelRead(context->debug_channel, 0, (PCHAR)Stream_Buffer(s),
			                          (ULONG)Stream_Capacity(s), &BytesReturned) == FALSE)
			{
				/* should not happen */
				break;
			}
		}

		Stream_SetPosition(s, BytesReturned);
		WLog_DBG(TAG, "got %" PRIu32 " bytes", BytesReturned);
	}

	Stream_Free(s, TRUE);
	return 0;
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

	/* A real server should tag the peer as activated here and start sending updates in main loop.
	 */
	if (!test_peer_load_icon(client))
	{
		WLog_DBG(TAG, "Unable to load icon");
		return FALSE;
	}

	if (WTSVirtualChannelManagerIsChannelJoined(context->vcm, "rdpdbg"))
	{
		context->debug_channel = WTSVirtualChannelOpen(context->vcm, WTS_CURRENT_SESSION, "rdpdbg");

		if (context->debug_channel != NULL)
		{
			WLog_DBG(TAG, "Open channel rdpdbg.");

			if (!(context->stopEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
			{
				WLog_ERR(TAG, "Failed to create stop event");
				return FALSE;
			}

			if (!(context->debug_channel_thread =
			          CreateThread(NULL, 0, tf_debug_channel_thread_func, (void*)context, 0, NULL)))
			{
				WLog_ERR(TAG, "Failed to create debug channel thread");
				CloseHandle(context->stopEvent);
				context->stopEvent = NULL;
				return FALSE;
			}
		}
	}

	if (WTSVirtualChannelManagerIsChannelJoined(context->vcm, RDPSND_CHANNEL_NAME))
	{
		sf_peer_rdpsnd_init(context); /* Audio Output */
	}

	if (WTSVirtualChannelManagerIsChannelJoined(context->vcm, ENCOMSP_SVC_CHANNEL_NAME))
	{
		sf_peer_encomsp_init(context); /* Lync Multiparty */
	}

	/* Dynamic Virtual Channels */
	sf_peer_audin_init(context); /* Audio Input */

#if defined(CHANNEL_AINPUT_SERVER)
	sf_peer_ainput_init(context);
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
	// client->settings->CompressionLevel = PACKET_COMPR_TYPE_8K;
	// client->settings->CompressionLevel = PACKET_COMPR_TYPE_64K;
	// client->settings->CompressionLevel = PACKET_COMPR_TYPE_RDP6;
	settings->CompressionLevel = PACKET_COMPR_TYPE_RDP8;

	if (info->test_pcap_file != NULL)
	{
		freerdp_settings_set_bool(settings, FreeRDP_DumpRemoteFx, TRUE);

		if (!tf_peer_dump_rfx(client))
			return FALSE;
	}
	else
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

	if ((flags & KBD_FLAGS_DOWN) && (code == RDP_SCANCODE_KEY_G)) /* 'g' key */
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
	else if ((flags & KBD_FLAGS_DOWN) && code == RDP_SCANCODE_KEY_C) /* 'c' key */
	{
		if (tcontext->debug_channel)
		{
			ULONG written;
			WTSVirtualChannelWrite(tcontext->debug_channel, (PCHAR) "test2", 5, &written);
		}
	}
	else if ((flags & KBD_FLAGS_DOWN) && code == RDP_SCANCODE_KEY_X) /* 'x' key */
	{
		WINPR_ASSERT(client->Close);
		client->Close(client);
	}
	else if ((flags & KBD_FLAGS_DOWN) && code == RDP_SCANCODE_KEY_R) /* 'r' key */
	{
		tcontext->audin_open = !tcontext->audin_open;
	}
#if defined(CHANNEL_AINPUT_SERVER)
	else if ((flags & KBD_FLAGS_DOWN) && code == RDP_SCANCODE_KEY_I) /* 'i' key */
	{
		tcontext->ainput_open = !tcontext->ainput_open;
	}
#endif
	else if ((flags & KBD_FLAGS_DOWN) && code == RDP_SCANCODE_KEY_S) /* 's' key */
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

	// WLog_DBG(TAG, "Client sent a mouse event (flags:0x%04"PRIX16" pos:%"PRIu16",%"PRIu16")",
	// flags, x, y);
	test_peer_draw_icon(input->context->peer, x + 10, y);
	return TRUE;
}

static BOOL tf_peer_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	WINPR_UNUSED(flags);
	WINPR_ASSERT(input);
	WINPR_ASSERT(input->context);

	// WLog_DBG(TAG, "Client sent an extended mouse event (flags:0x%04"PRIX16"
	// pos:%"PRIu16",%"PRIu16")", flags, x, y);
	test_peer_draw_icon(input->context->peer, x + 10, y);
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
	WINPR_ASSERT(area);

	if (allow > 0)
	{
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

static int hook_peer_write_pdu(rdpTransport* transport, wStream* s)
{
	UINT64 ts = 0;
	wStream* ls = NULL;
	UINT64 last_ts = 0;
	const struct server_info* info;
	freerdp_peer* client;
	CONNECTION_STATE state;
	testPeerContext* peerCtx;
	size_t offset = 0;
	rdpContext* context = transport_get_context(transport);

	WINPR_ASSERT(context);
	WINPR_ASSERT(s);

	client = context->peer;
	WINPR_ASSERT(client);

	peerCtx = (testPeerContext*)client->context;
	WINPR_ASSERT(peerCtx);
	WINPR_ASSERT(peerCtx->io.WritePdu);

	info = client->ContextExtra;
	WINPR_ASSERT(info);

	/* Let the client authenticate.
	 * After that is done, we stop the normal operation and send
	 * a previously recorded session PDU by PDU to the client.
	 *
	 * This is fragile and the connecting client needs to use the same
	 * configuration as the one that recorded the session!
	 */
	WINPR_ASSERT(info);
	state = freerdp_get_state(context);
	if (state < CONNECTION_STATE_NEGO)
		return peerCtx->io.WritePdu(transport, s);

	ls = Stream_New(NULL, 4096);
	if (!ls)
		goto fail;

	while (stream_dump_get(context, NULL, ls, &offset, &ts) > 0)
	{
		int rc;
		if ((last_ts > 0) && (ts > last_ts))
		{
			UINT64 diff = ts - last_ts;
			Sleep(diff);
		}
		last_ts = ts;
		rc = peerCtx->io.WritePdu(transport, ls);
		if (rc < 0)
			goto fail;
		Stream_SetPosition(ls, 0);
	}

fail:
	Stream_Free(ls, TRUE);
	return -1;
}

static DWORD WINAPI test_peer_mainloop(LPVOID arg)
{
	BOOL rc;
	DWORD error = CHANNEL_RC_OK;
	HANDLE handles[32] = { 0 };
	DWORD count;
	DWORD status;
	testPeerContext* context;
	struct server_info* info;
	rdpSettings* settings;
	rdpInput* input;
	rdpUpdate* update;
	freerdp_peer* client = (freerdp_peer*)arg;

	const char* key = "server.key";
	const char* cert = "server.crt";

	WINPR_ASSERT(client);

	info = client->ContextExtra;
	WINPR_ASSERT(info);

	if (!test_peer_init(client))
	{
		freerdp_peer_free(client);
		return 0;
	}

	if (info->key)
		key = info->key;
	if (info->cert)
		cert = info->cert;

	/* Initialize the real server settings here */
	WINPR_ASSERT(client->context);
	settings = client->context->settings;
	WINPR_ASSERT(settings);
	if (info->replay_dump)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_TransportDumpReplay, TRUE) ||
		    !freerdp_settings_set_string(settings, FreeRDP_TransportDumpFile, info->replay_dump))
		{
			freerdp_peer_free(client);
			return 0;
		}
	}
	if (!freerdp_settings_set_string(settings, FreeRDP_CertificateFile, cert) ||
	    !freerdp_settings_set_string(settings, FreeRDP_PrivateKeyFile, key) ||
	    !freerdp_settings_set_string(settings, FreeRDP_RdpKeyFile, key))
	{
		WLog_ERR(TAG, "Memory allocation failed (strdup)");
		freerdp_peer_free(client);
		return 0;
	}

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
		return FALSE;
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
	WINPR_ASSERT(rc);

	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	if (info->replay_dump)
	{
		const rdpTransportIo* cb = freerdp_get_io_callbacks(client->context);
		rdpTransportIo replay;

		WINPR_ASSERT(cb);
		replay = *cb;
		context->io = *cb;
		replay.WritePdu = hook_peer_write_pdu;
		freerdp_set_io_callbacks(client->context, &replay);
	}

	WLog_INFO(TAG, "We've got a client %s", client->local ? "(local)" : client->hostname);

	while (error == CHANNEL_RC_OK)
	{
		count = 0;
		{
			DWORD tmp;

			WINPR_ASSERT(client->GetEventHandles);
			tmp = client->GetEventHandles(client, &handles[count], 32 - count);

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

					/* Here is the correct state to start dynamic virtual channels */
					if (sf_peer_audin_running(context) != context->audin_open)
					{
						if (!sf_peer_audin_running(context))
							sf_peer_audin_start(context);
						else
							sf_peer_audin_stop(context);
					}

#if defined(CHANNEL_AINPUT_SERVER)
					if (sf_peer_ainput_running(context) != context->ainput_open)
					{
						if (!sf_peer_ainput_running(context))
							sf_peer_ainput_start(context);
						else
							sf_peer_ainput_stop(context);
					}
#endif

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

static void test_server_mainloop(freerdp_listener* instance)
{
	HANDLE handles[32] = { 0 };
	DWORD count;
	DWORD status;

	WINPR_ASSERT(instance);
	while (1)
	{
		WINPR_ASSERT(instance->GetEventHandles);
		count = instance->GetEventHandles(instance, handles, 32);

		if (0 == count)
		{
			WLog_ERR(TAG, "Failed to get FreeRDP event handles");
			break;
		}

		status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);

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
}

static const struct
{
	const char spcap[7];
	const char sfast[7];
	const char sport[7];
	const char slocal_only[13];
	const char scert[7];
	const char skey[6];
} options = { "--pcap=", "--fast", "--port=", "--local-only", "--cert=", "--key=" };

static void print_entry(FILE* fp, const char* fmt, const char* what, size_t size)
{
	char buffer[32] = { 0 };
	strncpy(buffer, what, MIN(size, sizeof(buffer)));
	fprintf(fp, fmt, buffer);
}

static WINPR_NORETURN(void usage(const char* app, const char* invalid))
{
	FILE* fp = stdout;

	fprintf(fp, "Invalid argument '%s'\n", invalid);
	fprintf(fp, "Usage: %s <arg>[ <arg> ...]\n", app);
	fprintf(fp, "Arguments:\n");
	print_entry(fp, "\t%s<pcap file>\n", options.spcap, sizeof(options.spcap));
	print_entry(fp, "\t%s<cert file>\n", options.scert, sizeof(options.scert));
	print_entry(fp, "\t%s<key file>\n", options.skey, sizeof(options.skey));
	print_entry(fp, "\t%s\n", options.sfast, sizeof(options.sfast));
	print_entry(fp, "\t%s<port>\n", options.sport, sizeof(options.sport));
	print_entry(fp, "\t%s\n", options.slocal_only, sizeof(options.slocal_only));
	exit(-1);
}

int main(int argc, char* argv[])
{
	int rc = -1;
	BOOL started = FALSE;
	WSADATA wsaData;
	freerdp_listener* instance;
	char* file = NULL;
	char name[MAX_PATH];
	long port = 3389, i;
	BOOL localOnly = FALSE;
	struct server_info info = { 0 };
	const char* app = argv[0];

	info.test_dump_rfx_realtime = TRUE;

	errno = 0;

	for (i = 1; i < argc; i++)
	{
		char* arg = argv[i];

		if (strncmp(arg, options.sfast, sizeof(options.sfast)) == 0)
			info.test_dump_rfx_realtime = FALSE;
		else if (strncmp(arg, options.sport, sizeof(options.sport)) == 0)
		{
			const char* sport = &arg[sizeof(options.sport)];
			port = strtol(sport, NULL, 10);

			if ((port < 1) || (port > UINT16_MAX) || (errno != 0))
				usage(app, arg);
		}
		else if (strncmp(arg, options.slocal_only, sizeof(options.slocal_only)) == 0)
			localOnly = TRUE;
		else if (strncmp(arg, options.spcap, sizeof(options.spcap)) == 0)
		{
			info.test_pcap_file = &arg[sizeof(options.spcap)];
			if (!winpr_PathFileExists(info.test_pcap_file))
				usage(app, arg);
		}
		else if (strncmp(arg, options.scert, sizeof(options.scert)) == 0)
		{
			info.cert = &arg[sizeof(options.scert)];
			if (!winpr_PathFileExists(info.cert))
				usage(app, arg);
		}
		else if (strncmp(arg, options.skey, sizeof(options.skey)) == 0)
		{
			info.key = &arg[sizeof(options.skey)];
			if (!winpr_PathFileExists(info.key))
				usage(app, arg);
		}
		else
			usage(app, arg);
	}

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
	instance = freerdp_listener_new();

	if (!instance)
		return -1;

	instance->info = (void*)&info;
	instance->PeerAccepted = test_peer_accepted;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		goto fail;

	/* Open the server socket and start listening. */
	sprintf_s(name, sizeof(name), "tfreerdp-server.%ld", port);
	file = GetKnownSubPath(KNOWN_PATH_TEMP, name);

	if (!file)
		goto fail;

	if (localOnly)
	{
		WINPR_ASSERT(instance->OpenLocal);
		started = instance->OpenLocal(instance, file);
	}
	else
	{
		WINPR_ASSERT(instance->Open);
		started = instance->Open(instance, NULL, (UINT16)port);
	}

	if (started)
	{
		/* Entering the server main loop. In a real server the listener can be run in its own
		 * thread. */
		test_server_mainloop(instance);
	}

	rc = 0;
fail:
	free(file);
	freerdp_listener_free(instance);
	WSACleanup();
	return rc;
}
