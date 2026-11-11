/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Unit test for the statistics of the rdpgfx (graphics pipeline) client channel
 *
 * The built-in rdpgfx DVC plugin is loaded through its DVCPluginEntry with a
 * stub dynamic channel manager and server PDUs are delivered through the
 * channel's OnDataReceived callback.
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

#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/stream.h>

#include <freerdp/addin.h>
#include <freerdp/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/codec/zgfx.h>
#include <freerdp/dvc.h>

#include "dvc_test_host.h"

#define INVALID_CMDID 0x7FFF

static BOOL find_stat(const char* name, size_t* index)
{
	for (size_t x = 0; x < rdpgfx_stats_max_index(); x++)
	{
		if (strcmp(rdpgfx_stats_name_for_index(x), name) == 0)
		{
			*index = x;
			return TRUE;
		}
	}
	(void)fprintf(stderr, "no stat named %s\n", name);
	return FALSE;
}

static uint64_t stat_value(RdpgfxClientContext* gfx, const char* name)
{
	size_t index = 0;
	if (!find_stat(name, &index))
		return UINT64_MAX;
	return rdpgfx_stats_value_for_index(gfx, index);
}

static BOOL all_zero(RdpgfxClientContext* gfx)
{
	for (size_t x = 0; x < rdpgfx_stats_max_index(); x++)
		TEST_CHECK(rdpgfx_stats_value_for_index(gfx, x) == 0);
	return TRUE;
}

static BOOL check_names(void)
{
	static const char* const expected[] = { "RDPGFX_CODECID_UNCOMPRESSED_BYTES",
	                                        "RDPGFX_CODECID_AVC444v2_BYTES",
	                                        "RDPGFX_CMDID_WATERMARK",
	                                        "RDPGFX_CMDID_INVALID",
	                                        "RDPGFX_FRAMES_DECODED",
	                                        "RDPGFX_FRAMES_UNACKNOWLEDGED" };

	const size_t max = rdpgfx_stats_max_index();

	/* codec IDs, command IDs and the invalid command, bytes per codec ID, the two frame stats */
	TEST_CHECK(max == 2ull * RDPGFX_CODECID_MAX + RDPGFX_CMDID_MAX + 1ull + 2ull);

	for (size_t x = 0; x < max; x++)
	{
		const char* name = rdpgfx_stats_name_for_index(x);
		TEST_CHECK(name);
		TEST_CHECK(strlen(name) > 0);
		TEST_CHECK(strcmp(name, "RDPGFX_STATS_UNUSED") != 0);

		/* the codec IDs nothing uses share a name */
		if (strstr(name, "UNKNOWN"))
			continue;

		for (size_t y = x + 1; y < max; y++)
			TEST_CHECK(strcmp(name, rdpgfx_stats_name_for_index(y)) != 0);
	}

	TEST_CHECK(strcmp(rdpgfx_stats_name_for_index(max), "RDPGFX_STATS_UNUSED") == 0);

	/* the slots of the first version keep their index */
	TEST_CHECK(
	    strcmp(rdpgfx_stats_name_for_index(RDPGFX_CODECID_AVC444), "RDPGFX_CODECID_AVC444") == 0);
	TEST_CHECK(
	    strcmp(rdpgfx_stats_name_for_index(RDPGFX_CODECID_MAX + RDPGFX_CMDID_WIRETOSURFACE_1),
		       "RDPGFX_CMDID_WIRETOSURFACE_1") == 0);

	for (size_t x = 0; x < ARRAYSIZE(expected); x++)
	{
		size_t index = 0;
		TEST_CHECK(find_stat(expected[x], &index));
	}
	return TRUE;
}

static wStream* new_pdu(UINT16 cmdId, size_t payload)
{
	wStream* s = Stream_New(nullptr, 2 + RDPGFX_HEADER_SIZE + payload);
	if (!s)
		return nullptr;
	Stream_Write_UINT8(s, ZGFX_SEGMENTED_SINGLE);
	Stream_Write_UINT8(s, ZGFX_PACKET_COMPR_TYPE_RDP8);
	Stream_Write_UINT16(s, cmdId);
	Stream_Write_UINT16(s, 0); /* flags */
	Stream_Write_UINT32(s, (UINT32)(RDPGFX_HEADER_SIZE + payload));
	return s;
}

static UINT send_pdu(IWTSVirtualChannelCallback* callback, wStream* s)
{
	Stream_SealLength(s);
	Stream_ResetPosition(s);
	const UINT rc = callback->OnDataReceived(callback, s);
	Stream_Free(s, TRUE);
	return rc;
}

static UINT send_frame_pdu(IWTSVirtualChannelCallback* callback, UINT16 cmdId, UINT32 frameId)
{
	const size_t payload = (cmdId == RDPGFX_CMDID_STARTFRAME) ? RDPGFX_START_FRAME_PDU_SIZE
	                                                          : RDPGFX_END_FRAME_PDU_SIZE;
	wStream* s = new_pdu(cmdId, payload);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;
	if (cmdId == RDPGFX_CMDID_STARTFRAME)
		Stream_Write_UINT32(s, 0); /* timestamp */
	Stream_Write_UINT32(s, frameId);
	return send_pdu(callback, s);
}

static UINT send_wire_to_surface_1(IWTSVirtualChannelCallback* callback, UINT16 codecId,
                                   UINT32 length)
{
	wStream* s = new_pdu(RDPGFX_CMDID_WIRETOSURFACE_1, RDPGFX_WIRE_TO_SURFACE_PDU_1_SIZE + length);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;
	Stream_Write_UINT16(s, 1); /* surfaceId */
	Stream_Write_UINT16(s, codecId);
	Stream_Write_UINT8(s, GFX_PIXEL_FORMAT_XRGB_8888);
	Stream_Write_UINT16(s, 0);  /* destRect.left */
	Stream_Write_UINT16(s, 0);  /* destRect.top */
	Stream_Write_UINT16(s, 16); /* destRect.right */
	Stream_Write_UINT16(s, 16); /* destRect.bottom */
	Stream_Write_UINT32(s, length);
	Stream_Zero(s, length);
	return send_pdu(callback, s);
}

static BOOL check_session(RdpgfxClientContext* gfx, IWTSVirtualChannelCallback* callback)
{
	TEST_CHECK(all_zero(gfx));

	TEST_CHECK(send_frame_pdu(callback, RDPGFX_CMDID_STARTFRAME, 1) == CHANNEL_RC_OK);
	TEST_CHECK(stat_value(gfx, "RDPGFX_FRAMES_UNACKNOWLEDGED") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_FRAMES_DECODED") == 0);

	TEST_CHECK(send_wire_to_surface_1(callback, RDPGFX_CODECID_UNCOMPRESSED, 5) == CHANNEL_RC_OK);
	TEST_CHECK(send_wire_to_surface_1(callback, RDPGFX_CODECID_UNCOMPRESSED, 7) == CHANNEL_RC_OK);
	TEST_CHECK(send_wire_to_surface_1(callback, RDPGFX_CODECID_CLEARCODEC, 11) == CHANNEL_RC_OK);
	TEST_CHECK(stat_value(gfx, "RDPGFX_CODECID_UNCOMPRESSED") == 2);
	TEST_CHECK(stat_value(gfx, "RDPGFX_CODECID_UNCOMPRESSED_BYTES") == 12);
	TEST_CHECK(stat_value(gfx, "RDPGFX_CODECID_CLEARCODEC") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_CODECID_CLEARCODEC_BYTES") == 11);
	TEST_CHECK(stat_value(gfx, "RDPGFX_CODECID_PLANAR_BYTES") == 0);
	TEST_CHECK(stat_value(gfx, "RDPGFX_CMDID_WIRETOSURFACE_1") == 3);

	/* refused: a surface command nobody decoded is no traffic of that codec */
	TEST_CHECK(send_wire_to_surface_1(callback, 0x0002, 13) != CHANNEL_RC_OK);
	TEST_CHECK(rdpgfx_stats_value_for_index(gfx, 0x0002) == 0);
	TEST_CHECK(rdpgfx_stats_value_for_index(gfx, 1ull * RDPGFX_CODECID_MAX + RDPGFX_CMDID_MAX +
	                                                 1ull + 0x0002) == 0);

	TEST_CHECK(send_frame_pdu(callback, RDPGFX_CMDID_ENDFRAME, 1) == CHANNEL_RC_OK);
	TEST_CHECK(stat_value(gfx, "RDPGFX_FRAMES_DECODED") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_FRAMES_UNACKNOWLEDGED") == 0);

	/* the slot past the last command is readable, too */
	wStream* s = new_pdu(INVALID_CMDID, 0);
	TEST_CHECK(s);
	(void)send_pdu(callback, s);
	TEST_CHECK(stat_value(gfx, "RDPGFX_CMDID_INVALID") == 1);

	/* a frame that never ended is still in the stats after the channel closed */
	TEST_CHECK(send_frame_pdu(callback, RDPGFX_CMDID_STARTFRAME, 2) == CHANNEL_RC_OK);
	TEST_CHECK(stat_value(gfx, "RDPGFX_FRAMES_UNACKNOWLEDGED") == 1);
	return TRUE;
}

static BOOL check_channel(IWTSPlugin* plugin)
{
	dvc_test_channel_manager mgr = WINPR_C_ARRAY_INIT;
	IWTSVirtualChannel channel = WINPR_C_ARRAY_INIT;
	IWTSVirtualChannelCallback* callback = nullptr;

	dvc_test_channel_manager_init(&mgr);
	dvc_test_channel_init(&channel);
	TEST_CHECK(plugin->Initialize(plugin, &mgr.iface) == CHANNEL_RC_OK);

	RdpgfxClientContext* gfx = (RdpgfxClientContext*)plugin->pInterface;
	TEST_CHECK(gfx);
	TEST_CHECK(dvc_test_open_channel(&mgr, &channel, &callback));

	const BOOL rc = check_session(gfx, callback);

	/* frees the channel callback */
	(void)callback->OnClose(callback);
	TEST_CHECK(rc);

	TEST_CHECK(stat_value(gfx, "RDPGFX_FRAMES_UNACKNOWLEDGED") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_FRAMES_DECODED") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_CODECID_UNCOMPRESSED_BYTES") == 12);

	/* the next channel starts from zero */
	TEST_CHECK(dvc_test_open_channel(&mgr, &channel, &callback));
	const BOOL zero = all_zero(gfx);
	(void)callback->OnClose(callback);
	TEST_CHECK(zero);
	return TRUE;
}

static BOOL check_plugin(rdpContext* context)
{
	dvc_test_entry_points ep = WINPR_C_ARRAY_INIT;
	TEST_CHECK(dvc_test_load_plugin(&ep, context, RDPGFX_CHANNEL_NAME, nullptr));

	const BOOL rc = check_channel(ep.plugin);

	/* frees the plugin */
	(void)ep.plugin->Terminated(ep.plugin);
	return rc;
}

int TestRdpgfxStats(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	if (!check_names())
		return -1;

	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	entry.ContextSize = sizeof(rdpClientContext);

	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
		return -1;

	const BOOL rc = check_plugin(context);
	freerdp_client_context_free(context);
	return rc ? 0 : -1;
}
