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
#include <winpr/image.h>
#include <winpr/winsock.h>

#include <freerdp/streamdump.h>
#include <freerdp/transport_io.h>

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>
#include <freerdp/channels/drdynvc.h>

#include <freerdp/freerdp.h>
#include <freerdp/constants.h>
#include <freerdp/server/rdpsnd.h>
#include <freerdp/settings.h>

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
		winpr_image_free(context->image, TRUE);
		if (context->debug_channel_thread)
		{
			WINPR_ASSERT(context->stopEvent);
			(void)SetEvent(context->stopEvent);
			(void)WaitForSingleObject(context->debug_channel_thread, INFINITE);
			(void)CloseHandle(context->debug_channel_thread);
		}

		Stream_Free(context->s, TRUE);
		free(context->bg_data);
		rfx_context_free(context->rfx_context);
		nsc_context_free(context->nsc_context);

		if (context->debug_channel)
			(void)WTSVirtualChannelClose(context->debug_channel);

		sf_peer_audin_uninit(context);

#if defined(CHANNEL_AINPUT_SERVER)
		sf_peer_ainput_uninit(context);
#endif

		rdpsnd_server_context_free(context->rdpsnd);
		encomsp_server_context_free(context->encomsp);

		WTSCloseServer(context->vcm);
	}
}

static BOOL test_peer_context_new(freerdp_peer* client, rdpContext* ctx)
{
	testPeerContext* context = (testPeerContext*)ctx;

	WINPR_ASSERT(client);
	WINPR_ASSERT(context);
	WINPR_ASSERT(ctx->settings);

	context->image = winpr_image_new();
	if (!context->image)
		goto fail;
	if (!(context->rfx_context = rfx_context_new_ex(
	          TRUE, freerdp_settings_get_uint32(ctx->settings, FreeRDP_ThreadingFlags))))
		goto fail;

	if (!rfx_context_reset(context->rfx_context, SAMPLE_SERVER_DEFAULT_WIDTH,
	                       SAMPLE_SERVER_DEFAULT_HEIGHT))
		goto fail;

	const UINT32 rlgr = freerdp_settings_get_uint32(ctx->settings, FreeRDP_RemoteFxRlgrMode);
	rfx_context_set_mode(context->rfx_context, rlgr);

	if (!(context->nsc_context = nsc_context_new()))
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
	rdpUpdate* update = NULL;
	SURFACE_FRAME_MARKER fm = { 0 };
	testPeerContext* context = NULL;

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
	rdpUpdate* update = NULL;
	SURFACE_FRAME_MARKER fm = { 0 };
	testPeerContext* context = NULL;

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

static BOOL stream_surface_bits_supported(const rdpSettings* settings)
{
	const UINT32 supported =
	    freerdp_settings_get_uint32(settings, FreeRDP_SurfaceCommandsSupported);
	return ((supported & SURFCMDS_STREAM_SURFACE_BITS) != 0);
}

static BOOL test_peer_draw_background(freerdp_peer* client)
{
	size_t size = 0;
	wStream* s = NULL;
	RFX_RECT rect;
	BYTE* rgb_data = NULL;
	const rdpSettings* settings = NULL;
	rdpUpdate* update = NULL;
	SURFACE_BITS_COMMAND cmd = { 0 };
	testPeerContext* context = NULL;
	BOOL ret = FALSE;
	const UINT32 colorFormat = PIXEL_FORMAT_RGB24;
	const size_t bpp = FreeRDPGetBytesPerPixel(colorFormat);

	WINPR_ASSERT(client);
	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	update = client->context->update;
	WINPR_ASSERT(update);

	const BOOL RemoteFxCodec = freerdp_settings_get_bool(settings, FreeRDP_RemoteFxCodec);
	if (!RemoteFxCodec && !freerdp_settings_get_bool(settings, FreeRDP_NSCodec))
		return FALSE;

	WINPR_ASSERT(freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth) <= UINT16_MAX);
	WINPR_ASSERT(freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight) <= UINT16_MAX);

	s = test_peer_stream_init(context);
	rect.x = 0;
	rect.y = 0;
	rect.width = (UINT16)freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
	rect.height = (UINT16)freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
	size = bpp * rect.width * rect.height;

	if (!(rgb_data = malloc(size)))
	{
		WLog_ERR(TAG, "Problem allocating memory");
		return FALSE;
	}

	memset(rgb_data, 0xA0, size);

	if (RemoteFxCodec && stream_surface_bits_supported(settings))
	{
		WLog_DBG(TAG, "Using RemoteFX codec");
		rfx_context_set_pixel_format(context->rfx_context, colorFormat);
		if (!rfx_compose_message(context->rfx_context, s, &rect, 1, rgb_data, rect.width,
		                         rect.height, rect.width * bpp))
		{
			goto out;
		}

		const UINT32 RemoteFxCodecId =
		    freerdp_settings_get_uint32(settings, FreeRDP_RemoteFxCodecId);
		WINPR_ASSERT(RemoteFxCodecId <= UINT16_MAX);
		cmd.bmp.codecID = (UINT16)RemoteFxCodecId;
		cmd.cmdType = CMDTYPE_STREAM_SURFACE_BITS;
	}
	else
	{
		WLog_DBG(TAG, "Using NSCodec");
		nsc_context_set_parameters(context->nsc_context, NSC_COLOR_FORMAT, colorFormat);
		nsc_compose_message(context->nsc_context, s, rgb_data, rect.width, rect.height,
		                    rect.width * bpp);
		const UINT32 NSCodecId = freerdp_settings_get_uint32(settings, FreeRDP_NSCodecId);
		WINPR_ASSERT(NSCodecId <= UINT16_MAX);
		cmd.bmp.codecID = (UINT16)NSCodecId;
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

static int open_icon(wImage* img)
{
	char* paths[] = { SAMPLE_RESOURCE_ROOT, "." };
	const char* names[] = { "test_icon.webp", "test_icon.png", "test_icon.jpg", "test_icon.bmp" };

	for (size_t x = 0; x < ARRAYSIZE(paths); x++)
	{
		const char* path = paths[x];
		if (!winpr_PathFileExists(path))
			continue;

		for (size_t y = 0; y < ARRAYSIZE(names); y++)
		{
			const char* name = names[y];
			char* file = GetCombinedPath(path, name);
			int rc = winpr_image_read(img, file);
			free(file);
			if (rc > 0)
				return rc;
		}
	}
	WLog_ERR(TAG, "Unable to open test icon");
	return -1;
}

static BOOL test_peer_load_icon(freerdp_peer* client)
{
	testPeerContext* context = NULL;
	rdpSettings* settings = NULL;

	WINPR_ASSERT(client);

	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	if (!freerdp_settings_get_bool(settings, FreeRDP_RemoteFxCodec) &&
	    !freerdp_settings_get_bool(settings, FreeRDP_NSCodec))
	{
		WLog_ERR(TAG, "Client doesn't support RemoteFX or NSCodec");
		return FALSE;
	}

	int rc = open_icon(context->image);
	if (rc <= 0)
		goto out_fail;

	/* background with same size, which will be used to erase the icon from old position */
	if (!(context->bg_data = calloc(context->image->height, 3ULL * context->image->width)))
		goto out_fail;

	memset(context->bg_data, 0xA0, 3ULL * context->image->height * context->image->width);
	return TRUE;
out_fail:
	context->bg_data = NULL;
	return FALSE;
}

static void test_peer_draw_icon(freerdp_peer* client, UINT32 x, UINT32 y)
{
	wStream* s = NULL;
	RFX_RECT rect;
	rdpUpdate* update = NULL;
	rdpSettings* settings = NULL;
	SURFACE_BITS_COMMAND cmd = { 0 };
	testPeerContext* context = NULL;

	WINPR_ASSERT(client);

	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	update = client->context->update;
	WINPR_ASSERT(update);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	if (freerdp_settings_get_bool(settings, FreeRDP_DumpRemoteFx))
		return;

	if (context->image->width < 1 || !context->activated)
		return;

	rect.x = 0;
	rect.y = 0;
	rect.width = context->image->width;
	rect.height = context->image->height;

	const UINT32 w = freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth);
	const UINT32 h = freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight);
	if (context->icon_x + context->image->width > w)
		return;
	if (context->icon_y + context->image->height > h)
		return;
	if (x + context->image->width > w)
		return;
	if (y + context->image->height > h)
		return;

	test_peer_begin_frame(client);

	const BOOL RemoteFxCodec = freerdp_settings_get_bool(settings, FreeRDP_RemoteFxCodec);
	if (RemoteFxCodec && stream_surface_bits_supported(settings))
	{
		const UINT32 RemoteFxCodecId =
		    freerdp_settings_get_uint32(settings, FreeRDP_RemoteFxCodecId);
		WINPR_ASSERT(RemoteFxCodecId <= UINT16_MAX);
		cmd.bmp.codecID = (UINT16)RemoteFxCodecId;
		cmd.cmdType = CMDTYPE_STREAM_SURFACE_BITS;
	}
	else
	{
		const UINT32 NSCodecId = freerdp_settings_get_uint32(settings, FreeRDP_NSCodecId);
		WINPR_ASSERT(NSCodecId <= UINT16_MAX);
		cmd.bmp.codecID = (UINT16)NSCodecId;
		cmd.cmdType = CMDTYPE_SET_SURFACE_BITS;
	}

	if (context->icon_x != UINT32_MAX)
	{
		const UINT32 colorFormat = PIXEL_FORMAT_RGB24;
		const UINT32 bpp = FreeRDPGetBytesPerPixel(colorFormat);
		s = test_peer_stream_init(context);

		if (RemoteFxCodec)
		{
			rfx_context_set_pixel_format(context->rfx_context, colorFormat);
			rfx_compose_message(context->rfx_context, s, &rect, 1, context->bg_data, rect.width,
			                    rect.height, rect.width * bpp);
		}
		else
		{
			nsc_context_set_parameters(context->nsc_context, NSC_COLOR_FORMAT, colorFormat);
			nsc_compose_message(context->nsc_context, s, context->bg_data, rect.width, rect.height,
			                    rect.width * bpp);
		}

		cmd.destLeft = context->icon_x;
		cmd.destTop = context->icon_y;
		cmd.destRight = context->icon_x + rect.width;
		cmd.destBottom = context->icon_y + rect.height;
		cmd.bmp.bpp = 32;
		cmd.bmp.flags = 0;
		cmd.bmp.width = rect.width;
		cmd.bmp.height = rect.height;
		cmd.bmp.bitmapDataLength = (UINT32)Stream_GetPosition(s);
		cmd.bmp.bitmapData = Stream_Buffer(s);
		WINPR_ASSERT(update->SurfaceBits);
		update->SurfaceBits(update->context, &cmd);
	}

	s = test_peer_stream_init(context);

	{
		const UINT32 colorFormat =
		    context->image->bitsPerPixel > 24 ? PIXEL_FORMAT_BGRA32 : PIXEL_FORMAT_BGR24;

		if (RemoteFxCodec)
		{
			rfx_context_set_pixel_format(context->rfx_context, colorFormat);
			rfx_compose_message(context->rfx_context, s, &rect, 1, context->image->data, rect.width,
			                    rect.height, context->image->scanline);
		}
		else
		{
			nsc_context_set_parameters(context->nsc_context, NSC_COLOR_FORMAT, colorFormat);
			nsc_compose_message(context->nsc_context, s, context->image->data, rect.width,
			                    rect.height, context->image->scanline);
		}
	}

	cmd.destLeft = x;
	cmd.destTop = y;
	cmd.destRight = x + rect.width;
	cmd.destBottom = y + rect.height;
	cmd.bmp.bpp = 32;
	cmd.bmp.width = rect.width;
	cmd.bmp.height = rect.height;
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
	INT64 sec = 0;
	INT64 usec = 0;

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
	BOOL rc = FALSE;
	wStream* s = NULL;
	UINT32 prev_seconds = 0;
	UINT32 prev_useconds = 0;
	rdpUpdate* update = NULL;
	rdpPcap* pcap_rfx = NULL;
	pcap_record record = { 0 };
	struct server_info* info = NULL;

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);

	info = client->ContextExtra;
	WINPR_ASSERT(info);

	s = Stream_New(NULL, 512);

	if (!s)
		return FALSE;

	update = client->context->update;
	WINPR_ASSERT(update);

	pcap_rfx = pcap_open(info->test_pcap_file, FALSE);
	if (!pcap_rfx)
		goto fail;

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

	rc = TRUE;
fail:
	Stream_Free(s, TRUE);
	pcap_close(pcap_rfx);
	return rc;
}

static DWORD WINAPI tf_debug_channel_thread_func(LPVOID arg)
{
	void* fd = NULL;
	void* buffer = NULL;
	DWORD BytesReturned = 0;
	ULONG written = 0;
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

	wStream* s = Stream_New(NULL, 4096);
	if (!s)
		goto fail;

	if (!WTSVirtualChannelWrite(context->debug_channel, (PCHAR) "test1", 5, &written))
		goto fail;

	while (1)
	{
		DWORD status = 0;
		DWORD nCount = 0;
		HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };

		handles[nCount++] = context->event;
		handles[nCount++] = freerdp_abort_event(&context->_p);
		handles[nCount++] = context->stopEvent;
		status = WaitForMultipleObjects(nCount, handles, FALSE, INFINITE);
		switch (status)
		{
			case WAIT_OBJECT_0:
				break;
			default:
				goto fail;
		}

		Stream_SetPosition(s, 0);

		if (WTSVirtualChannelRead(context->debug_channel, 0, Stream_BufferAs(s, char),
		                          (ULONG)Stream_Capacity(s), &BytesReturned) == FALSE)
		{
			if (BytesReturned == 0)
				break;

			if (!Stream_EnsureRemainingCapacity(s, BytesReturned))
				break;

			if (WTSVirtualChannelRead(context->debug_channel, 0, Stream_BufferAs(s, char),
			                          (ULONG)Stream_Capacity(s), &BytesReturned) == FALSE)
			{
				/* should not happen */
				break;
			}
		}

		Stream_SetPosition(s, BytesReturned);
		WLog_DBG(TAG, "got %" PRIu32 " bytes", BytesReturned);
	}
fail:
	Stream_Free(s, TRUE);
	return 0;
}

static BOOL tf_peer_post_connect(freerdp_peer* client)
{
	testPeerContext* context = NULL;
	rdpSettings* settings = NULL;

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
	         client->local ? "(local)" : client->hostname,
	         freerdp_settings_get_uint32(settings, FreeRDP_OsMajorType),
	         freerdp_settings_get_uint32(settings, FreeRDP_OsMinorType));

	if (freerdp_settings_get_bool(settings, FreeRDP_AutoLogonEnabled))
	{
		const char* Username = freerdp_settings_get_string(settings, FreeRDP_Username);
		const char* Domain = freerdp_settings_get_string(settings, FreeRDP_Domain);
		WLog_DBG(TAG, " and wants to login automatically as %s\\%s", Domain ? Domain : "",
		         Username);
		/* A real server may perform OS login here if NLA is not executed previously. */
	}

	WLog_DBG(TAG, "");
	WLog_DBG(TAG, "Client requested desktop: %" PRIu32 "x%" PRIu32 "x%" PRIu32 "",
	         freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth),
	         freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight),
	         freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth));
#if (SAMPLE_SERVER_USE_CLIENT_RESOLUTION == 1)

	if (!rfx_context_reset(context->rfx_context,
	                       freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth),
	                       freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight)))
		return FALSE;

	WLog_DBG(TAG, "Using resolution requested by client.");
#else
	client->freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth) =
	    context->rfx_context->width;
	client->freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight) =
	    context->rfx_context->height;
	WLog_DBG(TAG, "Resizing client to %" PRIu32 "x%" PRIu32 "",
	         client->freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth),
	         client->freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight));
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
				(void)CloseHandle(context->stopEvent);
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
	testPeerContext* context = NULL;
	struct server_info* info = NULL;
	rdpSettings* settings = NULL;

	WINPR_ASSERT(client);

	context = (testPeerContext*)client->context;
	WINPR_ASSERT(context);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	info = client->ContextExtra;
	WINPR_ASSERT(info);

	context->activated = TRUE;
	// PACKET_COMPR_TYPE_8K;
	// PACKET_COMPR_TYPE_64K;
	// PACKET_COMPR_TYPE_RDP6;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_CompressionLevel, PACKET_COMPR_TYPE_RDP8))
		return FALSE;

	if (info->test_pcap_file != NULL)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_DumpRemoteFx, TRUE))
			return FALSE;

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
	freerdp_peer* client = NULL;
	rdpUpdate* update = NULL;
	rdpContext* context = NULL;
	testPeerContext* tcontext = NULL;
	rdpSettings* settings = NULL;

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
		if (freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth) != 800)
		{
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth, 800))
				return FALSE;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight, 600))
				return FALSE;
		}
		else
		{
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopWidth,
			                                 SAMPLE_SERVER_DEFAULT_WIDTH))
				return FALSE;
			if (!freerdp_settings_set_uint32(settings, FreeRDP_DesktopHeight,
			                                 SAMPLE_SERVER_DEFAULT_HEIGHT))
				return FALSE;
		}

		if (!rfx_context_reset(tcontext->rfx_context,
		                       freerdp_settings_get_uint32(settings, FreeRDP_DesktopWidth),
		                       freerdp_settings_get_uint32(settings, FreeRDP_DesktopHeight)))
			return FALSE;

		WINPR_ASSERT(update->DesktopResize);
		update->DesktopResize(update->context);
		tcontext->activated = FALSE;
	}
	else if (((flags & KBD_FLAGS_RELEASE) == 0) && code == RDP_SCANCODE_KEY_C) /* 'c' key */
	{
		if (tcontext->debug_channel)
		{
			ULONG written = 0;
			if (!WTSVirtualChannelWrite(tcontext->debug_channel, (PCHAR) "test2", 5, &written))
				return FALSE;
		}
	}
	else if (((flags & KBD_FLAGS_RELEASE) == 0) && code == RDP_SCANCODE_KEY_X) /* 'x' key */
	{
		WINPR_ASSERT(client->Close);
		client->Close(client);
	}
	else if (((flags & KBD_FLAGS_RELEASE) == 0) && code == RDP_SCANCODE_KEY_R) /* 'r' key */
	{
		tcontext->audin_open = !tcontext->audin_open;
	}
#if defined(CHANNEL_AINPUT_SERVER)
	else if (((flags & KBD_FLAGS_RELEASE) == 0) && code == RDP_SCANCODE_KEY_I) /* 'i' key */
	{
		tcontext->ainput_open = !tcontext->ainput_open;
	}
#endif
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
	WINPR_UNUSED(context);
	WINPR_ASSERT(context);
	WINPR_ASSERT(areas || (count == 0));

	WLog_DBG(TAG, "Client requested to refresh:");

	for (BYTE i = 0; i < count; i++)
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

static int hook_peer_write_pdu(rdpTransport* transport, wStream* s)
{
	UINT64 ts = 0;
	wStream* ls = NULL;
	UINT64 last_ts = 0;
	const struct server_info* info = NULL;
	freerdp_peer* client = NULL;
	testPeerContext* peerCtx = NULL;
	size_t offset = 0;
	UINT32 flags = 0;
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
	CONNECTION_STATE state = freerdp_get_state(context);
	if (state < CONNECTION_STATE_NEGO)
		return peerCtx->io.WritePdu(transport, s);

	ls = Stream_New(NULL, 4096);
	if (!ls)
		goto fail;

	while (stream_dump_get(context, &flags, ls, &offset, &ts) > 0)
	{
		int rc = 0;
		/* Skip messages from client. */
		if (flags & STREAM_MSG_SRV_TX)
		{
			if ((last_ts > 0) && (ts > last_ts))
			{
				UINT64 diff = ts - last_ts;
				Sleep(diff);
			}
			last_ts = ts;
			rc = peerCtx->io.WritePdu(transport, ls);
			if (rc < 0)
				goto fail;
		}
		Stream_SetPosition(ls, 0);
	}

fail:
	Stream_Free(ls, TRUE);
	return -1;
}

static DWORD WINAPI test_peer_mainloop(LPVOID arg)
{
	BOOL rc = 0;
	DWORD error = CHANNEL_RC_OK;
	HANDLE handles[MAXIMUM_WAIT_OBJECTS] = { 0 };
	DWORD count = 0;
	DWORD status = 0;
	testPeerContext* context = NULL;
	struct server_info* info = NULL;
	rdpSettings* settings = NULL;
	rdpInput* input = NULL;
	rdpUpdate* update = NULL;
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
	if (info->replay_dump)
	{
		if (!freerdp_settings_set_bool(settings, FreeRDP_TransportDumpReplay, TRUE) ||
		    !freerdp_settings_set_string(settings, FreeRDP_TransportDumpFile, info->replay_dump))
			goto fail;
	}

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

	if (!freerdp_settings_set_bool(settings, FreeRDP_RdpSecurity, TRUE))
		goto fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_TlsSecurity, TRUE))
		goto fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_NlaSecurity, FALSE))
		goto fail;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel,
	                                 ENCRYPTION_LEVEL_CLIENT_COMPATIBLE))
		goto fail;
	/*  ENCRYPTION_LEVEL_HIGH; */
	/*  ENCRYPTION_LEVEL_LOW; */
	/*  ENCRYPTION_LEVEL_FIPS; */
	if (!freerdp_settings_set_bool(settings, FreeRDP_RemoteFxCodec, TRUE))
		goto fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_NSCodec, TRUE) ||
	    !freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, 32))
		goto fail;

	if (!freerdp_settings_set_bool(settings, FreeRDP_SuppressOutput, TRUE))
		goto fail;
	if (!freerdp_settings_set_bool(settings, FreeRDP_RefreshRect, TRUE))
		goto fail;

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
	if (!freerdp_settings_set_uint32(settings, FreeRDP_MultifragMaxRequestSize,
	                                 0xFFFFFF /* FIXME */))
		goto fail;

	WINPR_ASSERT(client->Initialize);
	rc = client->Initialize(client);
	if (!rc)
		goto fail;

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
			WINPR_ASSERT(client->GetEventHandles);
			DWORD tmp = client->GetEventHandles(client, &handles[count], 32 - count);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "Failed to get FreeRDP transport event handles");
				break;
			}

			count += tmp;
		}

		HANDLE channelHandle = WTSVirtualChannelManagerGetEventHandle(context->vcm);
		handles[count++] = channelHandle;
		status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForMultipleObjects failed (errno: %d)", errno);
			break;
		}

		WINPR_ASSERT(client->CheckFileDescriptor);
		if (client->CheckFileDescriptor(client) != TRUE)
			break;

		if (WaitForSingleObject(channelHandle, 0) != WAIT_OBJECT_0)
			continue;

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
fail:
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	return error;
}

static BOOL test_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	HANDLE hThread = NULL;
	struct server_info* info = NULL;

	WINPR_UNUSED(instance);

	WINPR_ASSERT(instance);
	WINPR_ASSERT(client);

	info = instance->info;
	client->ContextExtra = info;

	if (!(hThread = CreateThread(NULL, 0, test_peer_mainloop, (void*)client, 0, NULL)))
		return FALSE;

	(void)CloseHandle(hThread);
	return TRUE;
}

static void test_server_mainloop(freerdp_listener* instance)
{
	HANDLE handles[32] = { 0 };
	DWORD count = 0;
	DWORD status = 0;

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

WINPR_PRAGMA_DIAG_PUSH
WINPR_PRAGMA_DIAG_IGNORED_FORMAT_NONLITERAL
WINPR_ATTR_FORMAT_ARG(2, 0)
static void print_entry(FILE* fp, WINPR_FORMAT_ARG const char* fmt, const char* what, size_t size)
{
	char buffer[32] = { 0 };
	strncpy(buffer, what, MIN(size, sizeof(buffer) - 1));
	(void)fprintf(fp, fmt, buffer);
}
WINPR_PRAGMA_DIAG_POP

static WINPR_NORETURN(void usage(const char* app, const char* invalid))
{
	FILE* fp = stdout;

	(void)fprintf(fp, "Invalid argument '%s'\n", invalid);
	(void)fprintf(fp, "Usage: %s <arg>[ <arg> ...]\n", app);
	(void)fprintf(fp, "Arguments:\n");
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
	WSADATA wsaData = { 0 };
	freerdp_listener* instance = NULL;
	char* file = NULL;
	char name[MAX_PATH] = { 0 };
	long port = 3389;
	BOOL localOnly = FALSE;
	struct server_info info = { 0 };
	const char* app = argv[0];

	info.test_dump_rfx_realtime = TRUE;

	errno = 0;

	for (int i = 1; i < argc; i++)
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

	if (!info.cert)
		info.cert = "server.crt";
	if (!info.key)
		info.key = "server.key";

	instance->info = (void*)&info;
	instance->PeerAccepted = test_peer_accepted;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
		goto fail;

	/* Open the server socket and start listening. */
	(void)sprintf_s(name, sizeof(name), "tfreerdp-server.%ld", port);
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
