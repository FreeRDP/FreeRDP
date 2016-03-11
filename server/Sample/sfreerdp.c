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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <signal.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/string.h>
#include <winpr/path.h>
#include <winpr/winsock.h>

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>

#include <freerdp/constants.h>
#include <freerdp/server/rdpsnd.h>

#include "sf_audin.h"
#include "sf_rdpsnd.h"
#include "sf_encomsp.h"

#include "sfreerdp.h"

#include <freerdp/log.h>
#define TAG SERVER_TAG("sample")

#define SAMPLE_SERVER_USE_CLIENT_RESOLUTION 1
#define SAMPLE_SERVER_DEFAULT_WIDTH 1024
#define SAMPLE_SERVER_DEFAULT_HEIGHT 768

static char* test_pcap_file = NULL;
static BOOL test_dump_rfx_realtime = TRUE;

BOOL test_peer_context_new(freerdp_peer* client, testPeerContext* context)
{
	if (!(context->rfx_context = rfx_context_new(TRUE)))
		goto fail_rfx_context;

	if (!rfx_context_reset(context->rfx_context, SAMPLE_SERVER_DEFAULT_WIDTH,
			       SAMPLE_SERVER_DEFAULT_HEIGHT))
		goto fail_rfx_context;

	context->rfx_context->mode = RLGR3;

	rfx_context_set_pixel_format(context->rfx_context, RDP_PIXEL_FORMAT_R8G8B8);

	if (!(context->nsc_context = nsc_context_new()))
		goto fail_nsc_context;

	nsc_context_set_pixel_format(context->nsc_context, RDP_PIXEL_FORMAT_R8G8B8);

	if (!(context->s = Stream_New(NULL, 65536)))
		goto fail_stream_new;

	context->icon_x = -1;
	context->icon_y = -1;

	context->vcm = WTSOpenServerA((LPSTR) client->context);

	if (!context->vcm || context->vcm == INVALID_HANDLE_VALUE)
		goto fail_open_server;

	return TRUE;

fail_open_server:
	context->vcm = NULL;
	Stream_Free(context->s, TRUE);
	context->s = NULL;
fail_stream_new:
	nsc_context_free(context->nsc_context);
	context->nsc_context = NULL;
fail_nsc_context:
	rfx_context_free(context->rfx_context);
	context->rfx_context = NULL;
fail_rfx_context:
	return FALSE;
}

void test_peer_context_free(freerdp_peer* client, testPeerContext* context)
{
	if (context)
	{
		if (context->debug_channel_thread)
		{
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

		if (context->audin)
			audin_server_context_free(context->audin);

		if (context->rdpsnd)
			rdpsnd_server_context_free(context->rdpsnd);

		if (context->encomsp)
			encomsp_server_context_free(context->encomsp);

		WTSCloseServer((HANDLE) context->vcm);
	}
}

static BOOL test_peer_init(freerdp_peer* client)
{
	client->ContextSize = sizeof(testPeerContext);
	client->ContextNew = (psPeerContextNew) test_peer_context_new;
	client->ContextFree = (psPeerContextFree) test_peer_context_free;
	return freerdp_peer_context_new(client);
}

static wStream* test_peer_stream_init(testPeerContext* context)
{
	Stream_Clear(context->s);
	Stream_SetPosition(context->s, 0);
	return context->s;
}

static void test_peer_begin_frame(freerdp_peer* client)
{
	rdpUpdate* update = client->update;
	SURFACE_FRAME_MARKER* fm = &update->surface_frame_marker;
	testPeerContext* context = (testPeerContext*) client->context;

	fm->frameAction = SURFACECMD_FRAMEACTION_BEGIN;
	fm->frameId = context->frame_id;
	update->SurfaceFrameMarker(update->context, fm);
}

static void test_peer_end_frame(freerdp_peer* client)
{
	rdpUpdate* update = client->update;
	SURFACE_FRAME_MARKER* fm = &update->surface_frame_marker;
	testPeerContext* context = (testPeerContext*) client->context;

	fm->frameAction = SURFACECMD_FRAMEACTION_END;
	fm->frameId = context->frame_id;
	update->SurfaceFrameMarker(update->context, fm);

	context->frame_id++;
}

static BOOL test_peer_draw_background(freerdp_peer* client)
{
	int size;
	wStream* s;
	RFX_RECT rect;
	BYTE* rgb_data;
	rdpUpdate* update = client->update;
	SURFACE_BITS_COMMAND* cmd = &update->surface_bits_command;
	testPeerContext* context = (testPeerContext*) client->context;
	BOOL ret= FALSE;

	if (!client->settings->RemoteFxCodec && !client->settings->NSCodec)
		return FALSE;

	s = test_peer_stream_init(context);

	rect.x = 0;
	rect.y = 0;
	rect.width = client->settings->DesktopWidth;
	rect.height = client->settings->DesktopHeight;

	size = rect.width * rect.height * 3;
	if (!(rgb_data = malloc(size)))
	{
		WLog_ERR(TAG, "Problem allocating memory");
		return FALSE;
	}

	memset(rgb_data, 0xA0, size);

	if (client->settings->RemoteFxCodec)
	{
		if (!rfx_compose_message(context->rfx_context, s,
			&rect, 1, rgb_data, rect.width, rect.height, rect.width * 3))
		{
			goto out;
		}
		cmd->codecID = client->settings->RemoteFxCodecId;
	}
	else
	{
		nsc_compose_message(context->nsc_context, s,
			rgb_data, rect.width, rect.height, rect.width * 3);
		cmd->codecID = client->settings->NSCodecId;
	}

	cmd->destLeft = 0;
	cmd->destTop = 0;
	cmd->destRight = rect.width;
	cmd->destBottom = rect.height;
	cmd->bpp = 32;
	cmd->width = rect.width;
	cmd->height = rect.height;
	cmd->bitmapDataLength = Stream_GetPosition(s);
	cmd->bitmapData = Stream_Buffer(s);

	test_peer_begin_frame(client);
	update->SurfaceBits(update->context, cmd);
	test_peer_end_frame(client);

	ret = TRUE;
out:
	free(rgb_data);
	return ret;
}

static BOOL test_peer_load_icon(freerdp_peer* client)
{
	testPeerContext* context = (testPeerContext*) client->context;
	FILE* fp;
	int i;
	char line[50];
	BYTE* rgb_data = NULL;
	int c;

	if (!client->settings->RemoteFxCodec && !client->settings->NSCodec)
	{
		WLog_ERR(TAG, "Client doesn't support RemoteFX or NSCodec");
		return FALSE;
	}

	if ((fp = fopen("test_icon.ppm", "r")) == NULL)
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
	if (sscanf(line, "%d %d", &context->icon_width, &context->icon_height) < 2)
	{
		WLog_ERR(TAG, "Problem while extracting width/height from the icon file");
		goto out_fail;
	}
	/* Max */
	fgets(line, sizeof(line), fp);

	if (!(rgb_data = malloc(context->icon_width * context->icon_height * 3)))
		goto out_fail;

	for (i = 0; i < context->icon_width * context->icon_height * 3; i++)
	{
		if (!fgets(line, sizeof(line), fp) || (sscanf(line, "%d", &c) != 1))
			goto out_fail;

		rgb_data[i] = (BYTE)c;
	}


	/* background with same size, which will be used to erase the icon from old position */
	if (!(context->bg_data = malloc(context->icon_width * context->icon_height * 3)))
		goto out_fail;
	memset(context->bg_data, 0xA0, context->icon_width * context->icon_height * 3);
	context->icon_data = rgb_data;

	fclose(fp);
	return TRUE;

out_fail:
	free(rgb_data);
	context->bg_data =  NULL;
	fclose(fp);
	return FALSE;
}

static void test_peer_draw_icon(freerdp_peer* client, int x, int y)
{
	wStream* s;
	RFX_RECT rect;
	rdpUpdate* update = client->update;
	SURFACE_BITS_COMMAND* cmd = &update->surface_bits_command;
	testPeerContext* context = (testPeerContext*) client->context;

	if (client->update->dump_rfx)
		return;

	if (!context)
		return;

	if (context->icon_width < 1 || !context->activated)
		return;

	test_peer_begin_frame(client);

	rect.x = 0;
	rect.y = 0;
	rect.width = context->icon_width;
	rect.height = context->icon_height;

	if (context->icon_x >= 0)
	{
		s = test_peer_stream_init(context);
		if (client->settings->RemoteFxCodec)
		{
			rfx_compose_message(context->rfx_context, s,
				&rect, 1, context->bg_data, rect.width, rect.height, rect.width * 3);
			cmd->codecID = client->settings->RemoteFxCodecId;
		}
		else
		{
			nsc_compose_message(context->nsc_context, s,
				context->bg_data, rect.width, rect.height, rect.width * 3);
			cmd->codecID = client->settings->NSCodecId;
		}

		cmd->destLeft = context->icon_x;
		cmd->destTop = context->icon_y;
		cmd->destRight = context->icon_x + context->icon_width;
		cmd->destBottom = context->icon_y + context->icon_height;
		cmd->bpp = 32;
		cmd->width = context->icon_width;
		cmd->height = context->icon_height;
		cmd->bitmapDataLength = Stream_GetPosition(s);
		cmd->bitmapData = Stream_Buffer(s);
		update->SurfaceBits(update->context, cmd);
	}

	s = test_peer_stream_init(context);

	if (client->settings->RemoteFxCodec)
	{
		rfx_compose_message(context->rfx_context, s,
			&rect, 1, context->icon_data, rect.width, rect.height, rect.width * 3);
		cmd->codecID = client->settings->RemoteFxCodecId;
	}
	else
	{
		nsc_compose_message(context->nsc_context, s,
			context->icon_data, rect.width, rect.height, rect.width * 3);
		cmd->codecID = client->settings->NSCodecId;
	}

	cmd->destLeft = x;
	cmd->destTop = y;
	cmd->destRight = x + context->icon_width;
	cmd->destBottom = y + context->icon_height;
	cmd->bpp = 32;
	cmd->width = context->icon_width;
	cmd->height = context->icon_height;
	cmd->bitmapDataLength = Stream_GetPosition(s);
	cmd->bitmapData = Stream_Buffer(s);
	update->SurfaceBits(update->context, cmd);

	context->icon_x = x;
	context->icon_y = y;

	test_peer_end_frame(client);
}

static BOOL test_sleep_tsdiff(UINT32 *old_sec, UINT32 *old_usec, UINT32 new_sec, UINT32 new_usec)
{
	INT32 sec, usec;

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
		Sleep(sec * 1000);

	if (usec > 0)
		USleep(usec);

	return TRUE;
}

BOOL tf_peer_dump_rfx(freerdp_peer* client)
{
	wStream* s;
	UINT32 prev_seconds;
	UINT32 prev_useconds;
	rdpUpdate* update;
	rdpPcap* pcap_rfx;
	pcap_record record;

	s = Stream_New(NULL, 512);
	if (!s)
		return FALSE;

	update = client->update;
	if (!(pcap_rfx = pcap_open(test_pcap_file, FALSE)))
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
		Stream_SetPointer(s, Stream_Buffer(s) + Stream_Capacity(s));

		if (test_dump_rfx_realtime && test_sleep_tsdiff(&prev_seconds, &prev_useconds, record.header.ts_sec, record.header.ts_usec) == FALSE)
			break;

		update->SurfaceCommand(update->context, s);

		if (client->CheckFileDescriptor(client) != TRUE)
			break;
	}


	Stream_Free(s, TRUE);
	pcap_close(pcap_rfx);
	return TRUE;
}

static void* tf_debug_channel_thread_func(void* arg)
{
	void* fd;
	wStream* s;
	void* buffer;
	DWORD BytesReturned = 0;
	ULONG written;
	testPeerContext* context = (testPeerContext*) arg;

	if (WTSVirtualChannelQuery(context->debug_channel, WTSVirtualFileHandle, &buffer, &BytesReturned) == TRUE)
	{
		fd = *((void**) buffer);
		WTSFreeMemory(buffer);

		if (!(context->event = CreateWaitObjectEvent(NULL, TRUE, FALSE, fd)))
			return NULL;
	}

	s = Stream_New(NULL, 4096);

	WTSVirtualChannelWrite(context->debug_channel, (PCHAR) "test1", 5, &written);

	while (1)
	{
		WaitForSingleObject(context->event, INFINITE);

		if (WaitForSingleObject(context->stopEvent, 0) == WAIT_OBJECT_0)
			break;

		Stream_SetPosition(s, 0);

		if (WTSVirtualChannelRead(context->debug_channel, 0, (PCHAR) Stream_Buffer(s),
			Stream_Capacity(s), &BytesReturned) == FALSE)
		{
			if (BytesReturned == 0)
				break;

			Stream_EnsureRemainingCapacity(s, BytesReturned);

			if (WTSVirtualChannelRead(context->debug_channel, 0, (PCHAR) Stream_Buffer(s),
				Stream_Capacity(s), &BytesReturned) == FALSE)
			{
				/* should not happen */
				break;
			}
		}

		Stream_SetPosition(s, BytesReturned);
		WLog_DBG(TAG, "got %lu bytes", BytesReturned);
	}

	Stream_Free(s, TRUE);

	return 0;
}

BOOL tf_peer_post_connect(freerdp_peer* client)
{
	testPeerContext* context = (testPeerContext*) client->context;

	/**
	 * This callback is called when the entire connection sequence is done, i.e. we've received the
	 * Font List PDU from the client and sent out the Font Map PDU.
	 * The server may start sending graphics output and receiving keyboard/mouse input after this
	 * callback returns.
	 */
	WLog_DBG(TAG, "Client %s is activated (osMajorType %d osMinorType %d)", client->local ? "(local)" : client->hostname,
			 client->settings->OsMajorType, client->settings->OsMinorType);

	if (client->settings->AutoLogonEnabled)
	{
		WLog_DBG(TAG, " and wants to login automatically as %s\\%s",
				 client->settings->Domain ? client->settings->Domain : "",
				 client->settings->Username);
		/* A real server may perform OS login here if NLA is not executed previously. */
	}

	WLog_DBG(TAG, "");
	WLog_DBG(TAG, "Client requested desktop: %dx%dx%d",
			 client->settings->DesktopWidth, client->settings->DesktopHeight, client->settings->ColorDepth);

#if (SAMPLE_SERVER_USE_CLIENT_RESOLUTION == 1)
	if (!rfx_context_reset(context->rfx_context, client->settings->DesktopWidth,
		       client->settings->DesktopHeight))
		return FALSE;

	WLog_DBG(TAG, "Using resolution requested by client.");
#else
	client->settings->DesktopWidth = context->rfx_context->width;
	client->settings->DesktopHeight = context->rfx_context->height;
	WLog_DBG(TAG, "Resizing client to %dx%d", client->settings->DesktopWidth, client->settings->DesktopHeight);
	client->update->DesktopResize(client->update->context);
#endif

	/* A real server should tag the peer as activated here and start sending updates in main loop. */
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

			if (!(context->debug_channel_thread = CreateThread(NULL, 0,
					(LPTHREAD_START_ROUTINE) tf_debug_channel_thread_func, (void*) context, 0, NULL)))
			{
				WLog_ERR(TAG, "Failed to create debug channel thread");
				CloseHandle(context->stopEvent);
				context->stopEvent = NULL;
				return FALSE;
			}
		}
	}

	if (WTSVirtualChannelManagerIsChannelJoined(context->vcm, "rdpsnd"))
	{
		sf_peer_rdpsnd_init(context); /* Audio Output */
	}

	if (WTSVirtualChannelManagerIsChannelJoined(context->vcm, "encomsp"))
	{
		sf_peer_encomsp_init(context); /* Lync Multiparty */
	}

	/* Dynamic Virtual Channels */

	sf_peer_audin_init(context); /* Audio Input */

	/* Return FALSE here would stop the execution of the peer main loop. */

	return TRUE;
}

BOOL tf_peer_activate(freerdp_peer* client)
{
	testPeerContext* context = (testPeerContext*) client->context;

	context->activated = TRUE;

	//client->settings->CompressionLevel = PACKET_COMPR_TYPE_8K;
	//client->settings->CompressionLevel = PACKET_COMPR_TYPE_64K;
	//client->settings->CompressionLevel = PACKET_COMPR_TYPE_RDP6;
	client->settings->CompressionLevel = PACKET_COMPR_TYPE_RDP61;

	if (test_pcap_file != NULL)
	{
		client->update->dump_rfx = TRUE;
		if (!tf_peer_dump_rfx(client))
			return FALSE;
	}
	else
		test_peer_draw_background(client);

	return TRUE;
}

BOOL tf_peer_synchronize_event(rdpInput* input, UINT32 flags)
{
	WLog_DBG(TAG, "Client sent a synchronize event (flags:0x%X)", flags);
	return TRUE;
}

BOOL tf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	freerdp_peer* client = input->context->peer;
	rdpUpdate* update = client->update;
	testPeerContext* context = (testPeerContext*) input->context;
	WLog_DBG(TAG, "Client sent a keyboard event (flags:0x%X code:0x%X)", flags, code);

	if ((flags & 0x4000) && code == 0x22) /* 'g' key */
	{
		if (client->settings->DesktopWidth != 800)
		{
			client->settings->DesktopWidth = 800;
			client->settings->DesktopHeight = 600;
		}
		else
		{
			client->settings->DesktopWidth = SAMPLE_SERVER_DEFAULT_WIDTH;
			client->settings->DesktopHeight = SAMPLE_SERVER_DEFAULT_HEIGHT;
		}
		if (!rfx_context_reset(context->rfx_context, client->settings->DesktopWidth,
			       client->settings->DesktopHeight))
			return FALSE;

		update->DesktopResize(update->context);
		context->activated = FALSE;
	}
	else if ((flags & 0x4000) && code == 0x2E) /* 'c' key */
	{
		if (context->debug_channel)
		{
			ULONG written;
			WTSVirtualChannelWrite(context->debug_channel, (PCHAR) "test2", 5, &written);
		}
	}
	else if ((flags & 0x4000) && code == 0x2D) /* 'x' key */
	{
		client->Close(client);
	}
	else if ((flags & 0x4000) && code == 0x13) /* 'r' key */
	{
		if (!context->audin_open)
		{
			context->audin->Open(context->audin);
			context->audin_open = TRUE;
		}
		else
		{
			context->audin->Close(context->audin);
			context->audin_open = FALSE;
		}
	}
	else if ((flags & 0x4000) && code == 0x1F) /* 's' key */
	{

	}
	return TRUE;
}

BOOL tf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	WLog_DBG(TAG, "Client sent a unicode keyboard event (flags:0x%X code:0x%X)", flags, code);
	return TRUE;
}

BOOL tf_peer_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	//WLog_DBG(TAG, "Client sent a mouse event (flags:0x%X pos:%d,%d)", flags, x, y);
	test_peer_draw_icon(input->context->peer, x + 10, y);
	return TRUE;
}

BOOL tf_peer_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	//WLog_DBG(TAG, "Client sent an extended mouse event (flags:0x%X pos:%d,%d)", flags, x, y);
	return TRUE;
}

static BOOL tf_peer_refresh_rect(rdpContext* context, BYTE count, RECTANGLE_16* areas)
{
	BYTE i;
	WLog_DBG(TAG, "Client requested to refresh:");

	for (i = 0; i < count; i++)
	{
		WLog_DBG(TAG, "  (%d, %d) (%d, %d)", areas[i].left, areas[i].top, areas[i].right, areas[i].bottom);
	}
	return TRUE;
}

static BOOL tf_peer_suppress_output(rdpContext* context, BYTE allow, RECTANGLE_16* area)
{
	if (allow > 0)
	{
		WLog_DBG(TAG, "Client restore output (%d, %d) (%d, %d).", area->left, area->top, area->right, area->bottom);
	}
	else
	{
		WLog_DBG(TAG, "Client minimized and suppress output.");
	}
	return TRUE;
}

static void* test_peer_mainloop(void* arg)
{
	HANDLE handles[32];
	DWORD count;
	DWORD status;
	testPeerContext* context;
	freerdp_peer* client = (freerdp_peer*) arg;

	if (!test_peer_init(client))
	{
		freerdp_peer_free(client);
		return NULL;
	}

	/* Initialize the real server settings here */
	client->settings->CertificateFile = _strdup("server.crt");
	client->settings->PrivateKeyFile = _strdup("server.key");
	client->settings->RdpKeyFile = _strdup("server.key");
	if (!client->settings->CertificateFile || !client->settings->PrivateKeyFile || !client->settings->RdpKeyFile)
	{
		WLog_ERR(TAG, "Memory allocation failed (strdup)");
		freerdp_peer_free(client);
		return NULL;
	}
	client->settings->RdpSecurity = TRUE;
	client->settings->TlsSecurity = TRUE;
	client->settings->NlaSecurity = FALSE;
	client->settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
	/* client->settings->EncryptionLevel = ENCRYPTION_LEVEL_HIGH; */
	/* client->settings->EncryptionLevel = ENCRYPTION_LEVEL_LOW; */
	/* client->settings->EncryptionLevel = ENCRYPTION_LEVEL_FIPS; */

	client->settings->RemoteFxCodec = TRUE;
	client->settings->ColorDepth = 32;
	client->settings->SuppressOutput = TRUE;
	client->settings->RefreshRect = TRUE;

	client->PostConnect = tf_peer_post_connect;
	client->Activate = tf_peer_activate;

	client->input->SynchronizeEvent = tf_peer_synchronize_event;
	client->input->KeyboardEvent = tf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = tf_peer_unicode_keyboard_event;
	client->input->MouseEvent = tf_peer_mouse_event;
	client->input->ExtendedMouseEvent = tf_peer_extended_mouse_event;

	client->update->RefreshRect = tf_peer_refresh_rect;
	client->update->SuppressOutput = tf_peer_suppress_output;

	client->settings->MultifragMaxRequestSize = 0xFFFFFF; /* FIXME */

	client->Initialize(client);
	context = (testPeerContext*) client->context;
	WLog_INFO(TAG, "We've got a client %s", client->local ? "(local)" : client->hostname);

	while (1)
	{
		count = 0;
		handles[count++] = client->GetEventHandle(client);
		handles[count++] = WTSVirtualChannelManagerGetEventHandle(context->vcm);

		status = WaitForMultipleObjects(count, handles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForMultipleObjects failed (errno: %d)", errno);
			break;
		}

		if (client->CheckFileDescriptor(client) != TRUE)
			break;

		if (WTSVirtualChannelManagerCheckFileDescriptor(context->vcm) != TRUE)
			break;
	}

	WLog_INFO(TAG, "Client %s disconnected.", client->local ? "(local)" : client->hostname);
	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);

	return NULL;
}

static BOOL test_peer_accepted(freerdp_listener* instance, freerdp_peer* client)
{
	HANDLE hThread;

	if (!(hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE) test_peer_mainloop, (void*) client, 0, NULL)))
		return FALSE;

	CloseHandle(hThread);
	return TRUE;
}

static void test_server_mainloop(freerdp_listener* instance)
{
	HANDLE handles[32];
	DWORD count;
	DWORD status;

	while (1)
	{
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

		if (instance->CheckFileDescriptor(instance) != TRUE)
		{
			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			break;
		}
	}

	instance->Close(instance);
}

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	freerdp_listener* instance;
	char* file;
	char name[MAX_PATH];
	int port = 3389, i;

	for (i=1; i<argc; i++)
	{
		char* arg = argv[i];

		if (strncmp(arg, "--fast", 7) == 0)
			test_dump_rfx_realtime = FALSE;
		else if (strncmp(arg, "--port=", 7) == 0)
		{
			StrSep(&arg, "=");
			if (!arg)
				return -1;

			port = strtol(arg, NULL, 10);
			if ((port < 1) || (port > 0xFFFF))
				return -1;
		}
		else if (strncmp(arg, "--", 2))
			test_pcap_file = arg;
	}

	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	instance = freerdp_listener_new();
	if (!instance)
		return -1;

	instance->PeerAccepted = test_peer_accepted;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		freerdp_listener_free(instance);
		return -1;
	}

	/* Open the server socket and start listening. */
	sprintf_s(name, sizeof(name), "tfreerdp-server.%d", port);
	file = GetKnownSubPath(KNOWN_PATH_TEMP, name);
	if (!file)
	{
		freerdp_listener_free(instance);
		WSACleanup();
		return -1;
	}

	if (instance->Open(instance, NULL, port) &&
		instance->OpenLocal(instance, file))
	{
		/* Entering the server main loop. In a real server the listener can be run in its own thread. */
		test_server_mainloop(instance);
	}

	free (file);
	freerdp_listener_free(instance);

	WSACleanup();

	return 0;
}

