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

#include <freerdp/config.h>

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
#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/gfx.h>

#define INVALID_CMDID 0x7FFF

#define TEST_CHECK(cond)                                                             \
	do                                                                               \
	{                                                                                \
		if (!(cond))                                                                 \
		{                                                                            \
			(void)fprintf(stderr, "%s:%d: failed: %s\n", __func__, __LINE__, #cond); \
			return FALSE;                                                            \
		}                                                                            \
	} while (0)

typedef struct
{
	IDRDYNVC_ENTRY_POINTS iface;
	rdpContext* context;
	IWTSPlugin* plugin;
} test_entry_points;

typedef struct
{
	IWTSVirtualChannelManager iface;
	IWTSListener listener;
	IWTSListenerCallback* listenerCallback;
} test_channel_manager;

static UINT test_register_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints,
                                 WINPR_ATTR_UNUSED const char* name, IWTSPlugin* pPlugin)
{
	test_entry_points* ep = (test_entry_points*)pEntryPoints;
	if (ep->plugin)
		return CHANNEL_RC_ALREADY_INITIALIZED;
	ep->plugin = pPlugin;
	return CHANNEL_RC_OK;
}

static IWTSPlugin* test_get_plugin(WINPR_ATTR_UNUSED IDRDYNVC_ENTRY_POINTS* pEntryPoints,
                                   WINPR_ATTR_UNUSED const char* name)
{
	return nullptr;
}

static const ADDIN_ARGV* test_get_plugin_data(WINPR_ATTR_UNUSED IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return nullptr;
}

static rdpSettings* test_get_rdp_settings(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return ((test_entry_points*)pEntryPoints)->context->settings;
}

static rdpContext* test_get_rdp_context(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return ((test_entry_points*)pEntryPoints)->context;
}

static UINT test_create_listener(IWTSVirtualChannelManager* pChannelMgr,
                                 WINPR_ATTR_UNUSED const char* pszChannelName,
                                 WINPR_ATTR_UNUSED ULONG ulFlags,
                                 IWTSListenerCallback* pListenerCallback, IWTSListener** ppListener)
{
	test_channel_manager* mgr = (test_channel_manager*)pChannelMgr;
	mgr->listenerCallback = pListenerCallback;
	if (ppListener)
		*ppListener = &mgr->listener;
	return CHANNEL_RC_OK;
}

static UINT test_destroy_listener(IWTSVirtualChannelManager* pChannelMgr,
                                  WINPR_ATTR_UNUSED IWTSListener* pListener)
{
	((test_channel_manager*)pChannelMgr)->listenerCallback = nullptr;
	return CHANNEL_RC_OK;
}

static UINT32 test_get_channel_id(WINPR_ATTR_UNUSED IWTSVirtualChannel* channel)
{
	return 1;
}

static const char* test_get_channel_name(WINPR_ATTR_UNUSED IWTSVirtualChannel* channel)
{
	return RDPGFX_DVC_CHANNEL_NAME;
}

static IWTSVirtualChannel* test_find_channel_by_id(WINPR_ATTR_UNUSED IWTSVirtualChannelManager* mgr,
                                                   WINPR_ATTR_UNUSED UINT32 ChannelId)
{
	return nullptr;
}

static UINT test_channel_write(WINPR_ATTR_UNUSED IWTSVirtualChannel* pChannel,
                               WINPR_ATTR_UNUSED ULONG cbSize,
                               WINPR_ATTR_UNUSED const BYTE* pBuffer,
                               WINPR_ATTR_UNUSED void* pReserved)
{
	return CHANNEL_RC_OK;
}

static UINT test_channel_close(WINPR_ATTR_UNUSED IWTSVirtualChannel* pChannel)
{
	return CHANNEL_RC_OK;
}

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
		                                    "RDPGFX_FRAMES_UNACKNOWLEDGED",
		                                    "RDPGFX_DECODER_AV1_PICTURES",
		                                    "RDPGFX_DECODER_AVC420_FAILED",
		                                    "RDPGFX_DECODER_AVC444_NO_PICTURE",
		                                    "RDPGFX_DECODER_AVC444v2_FALLBACKS" };

	const size_t max = rdpgfx_stats_max_index();

	/* codec IDs, command IDs and the invalid command, bytes per codec ID, the two frame stats */
	/* and nine stats for each of the four decoders */
	TEST_CHECK(max == 2ull * RDPGFX_CODECID_MAX + RDPGFX_CMDID_MAX + 1ull + 2ull + 4ull * 9ull);

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

/* Everything goes to the slots of the codec it was reported for, and each stat to its own */
static BOOL check_decode_results(RdpgfxClientContext* gfx)
{
	TEST_CHECK(gfx->DecodeResult);
	TEST_CHECK(rdpgfx_stats_decoder_backend(gfx, RDPGFX_CODECID_AVC420) == nullptr);
	TEST_CHECK(rdpgfx_stats_decoder_backend(gfx, RDPGFX_CODECID_CLEARCODEC) == nullptr);
	TEST_CHECK(rdpgfx_stats_decoder_backend(nullptr, RDPGFX_CODECID_AVC420) == nullptr);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_LAST_PICTURE_AGE_NS") == 0);

	FREERDP_DECODE_RESULT result = { .status = FREERDP_DECODE_PICTURE,
		                             .keyFrame = TRUE,
		                             .fellBack = FALSE,
		                             .hardware = FREERDP_DECODE_HARDWARE_NO,
		                             .backend = "first" };
	gfx->DecodeResult(gfx, RDPGFX_CODECID_AVC420, &result, 300);

	result.status = FREERDP_DECODE_PICTURE;
	result.keyFrame = FALSE;
	gfx->DecodeResult(gfx, RDPGFX_CODECID_AVC420, &result, 200);

	result.status = FREERDP_DECODE_FAILED;
	result.fellBack = TRUE;
	result.hardware = FREERDP_DECODE_HARDWARE_YES;
	result.backend = "second";
	gfx->DecodeResult(gfx, RDPGFX_CODECID_AVC420, &result, 500);

	result.status = FREERDP_DECODE_NO_PICTURE;
	result.fellBack = FALSE;
	gfx->DecodeResult(gfx, RDPGFX_CODECID_AVC420, &result, 100);

	/* nothing happened, and nothing for a codec without decoder stats */
	result.status = FREERDP_DECODE_NONE;
	gfx->DecodeResult(gfx, RDPGFX_CODECID_AVC420, &result, 10000);
	result.status = FREERDP_DECODE_PICTURE;
	gfx->DecodeResult(gfx, RDPGFX_CODECID_CLEARCODEC, &result, 10000);

	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_PICTURES") == 2);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_FAILED") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_NO_PICTURE") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_KEY_FRAMES") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_DECODE_NS_SUM") == 1100);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_DECODE_NS_MAX") == 500);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_FALLBACKS") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_HARDWARE") == FREERDP_DECODE_HARDWARE_YES);

	const uint64_t age = stat_value(gfx, "RDPGFX_DECODER_AVC420_LAST_PICTURE_AGE_NS");
	TEST_CHECK(age > 0);
	TEST_CHECK(age < 60ull * 1000ull * 1000ull * 1000ull);

	const char* backend = rdpgfx_stats_decoder_backend(gfx, RDPGFX_CODECID_AVC420);
	TEST_CHECK(backend && (strcmp(backend, "second") == 0));

	/* the slots of the other codecs are their own */
	static const struct
	{
		UINT16 codecId;
		const char* pictures;
	} others[] = { { RDPGFX_CODECID_AVC444, "RDPGFX_DECODER_AVC444_PICTURES" },
		           { RDPGFX_CODECID_AVC444v2, "RDPGFX_DECODER_AVC444v2_PICTURES" } };
	for (size_t x = 0; x < ARRAYSIZE(others); x++)
	{
		TEST_CHECK(stat_value(gfx, others[x].pictures) == 0);
		gfx->DecodeResult(gfx, others[x].codecId, &result, 1);
		TEST_CHECK(stat_value(gfx, others[x].pictures) == 1);
	}
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_PICTURES") == 2);
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
	TEST_CHECK(check_decode_results(gfx));

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

static BOOL open_channel(test_channel_manager* mgr, IWTSVirtualChannel* channel,
                         IWTSVirtualChannelCallback** callback)
{
	BOOL accept = TRUE;
	*callback = nullptr;

	TEST_CHECK(mgr->listenerCallback->OnNewChannelConnection(
	               mgr->listenerCallback, channel, nullptr, &accept, callback) == CHANNEL_RC_OK);
	TEST_CHECK(*callback);
	TEST_CHECK((*callback)->OnOpen(*callback) == CHANNEL_RC_OK);
	return TRUE;
}

#if defined(WITH_GFX_AV1) || defined(WITH_GFX_H264)
#define TEST_SIZE 64
#define TEST_SURFACE 1
#define TEST_METABLOCK_SIZE (4ull + 8ull + 2ull)

static void write_metablock(wStream* s)
{
	Stream_Write_UINT32(s, 1); /* numRegionRects */
	Stream_Write_UINT16(s, 0);
	Stream_Write_UINT16(s, 0);
	Stream_Write_UINT16(s, TEST_SIZE);
	Stream_Write_UINT16(s, TEST_SIZE);
	Stream_Write_UINT8(s, 22);  /* qpVal */
	Stream_Write_UINT8(s, 100); /* qualityVal */
}

#if defined(WITH_GFX_AV1)
/* AV1 is only accepted on a connection that agreed on the capability version with it */
static UINT send_caps_confirm(IWTSVirtualChannelCallback* callback)
{
	wStream* s = new_pdu(RDPGFX_CMDID_CAPSCONFIRM, 12);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;
	Stream_Write_UINT32(s, RDPGFX_CAPVERSION_FRDP_1);
	Stream_Write_UINT32(s, 4); /* capsDataLength */
	Stream_Write_UINT32(s, 0); /* capsData */
	return send_pdu(callback, s);
}
#endif

static UINT send_create_surface(IWTSVirtualChannelCallback* callback)
{
	wStream* s = new_pdu(RDPGFX_CMDID_CREATESURFACE, 7);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;
	Stream_Write_UINT16(s, TEST_SURFACE);
	Stream_Write_UINT16(s, TEST_SIZE);
	Stream_Write_UINT16(s, TEST_SIZE);
	Stream_Write_UINT8(s, GFX_PIXEL_FORMAT_XRGB_8888);
	return send_pdu(callback, s);
}

/* A surface command with a real bitstream. avc444 is the prefix of an AVC444 command: the LC
 * value in the bits on top of the size of the first stream, the one stream this test sends. */
static UINT send_bitstream(IWTSVirtualChannelCallback* callback, UINT16 codecId, const BYTE* data,
                           size_t length, BOOL avc444)
{
	const size_t bitmap = (avc444 ? 4ull : 0ull) + TEST_METABLOCK_SIZE + length;
	wStream* s = new_pdu(RDPGFX_CMDID_WIRETOSURFACE_1, RDPGFX_WIRE_TO_SURFACE_PDU_1_SIZE + bitmap);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;
	Stream_Write_UINT16(s, TEST_SURFACE);
	Stream_Write_UINT16(s, codecId);
	Stream_Write_UINT8(s, GFX_PIXEL_FORMAT_XRGB_8888);
	Stream_Write_UINT16(s, 0);
	Stream_Write_UINT16(s, 0);
	Stream_Write_UINT16(s, TEST_SIZE);
	Stream_Write_UINT16(s, TEST_SIZE);
	Stream_Write_UINT32(s, (UINT32)bitmap);
	if (avc444)
		Stream_Write_UINT32(s, (UINT32)((1u << 30) | (TEST_METABLOCK_SIZE + length)));
	write_metablock(s);
	Stream_Write(s, data, length);
	return send_pdu(callback, s);
}

/* What the decoders behind the renderer report reaches the stats of their codec */
static BOOL check_decoders(RdpgfxClientContext* gfx, IWTSVirtualChannelCallback* callback)
{
	TEST_CHECK(send_create_surface(callback) == CHANNEL_RC_OK);

#if defined(WITH_GFX_AV1)
	TEST_CHECK(send_caps_confirm(callback) == CHANNEL_RC_OK);

	/* A temporal delimiter alone: dav1d takes it, libaom refuses a temporal unit without a
	 * frame. Then an OBU cut off inside of its header, which no decoder takes. */
	static const BYTE av1NoFrame[] = { 0x12, 0x00 };
	static const BYTE av1Cut[] = { 0x12 };

	TEST_CHECK(send_bitstream(callback, RDPGFX_CODECID_AV1, av1NoFrame, sizeof(av1NoFrame),
	                          FALSE) == CHANNEL_RC_OK);
	const char* backend = rdpgfx_stats_decoder_backend(gfx, RDPGFX_CODECID_AV1);
	TEST_CHECK(backend);
	const uint64_t taken = (strcmp(backend, "dav1d") == 0) ? 1 : 0;
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AV1_NO_PICTURE") == taken);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AV1_FAILED") == 1 - taken);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AV1_PICTURES") == 0);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AV1_DECODE_NS_SUM") > 0);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AV1_HARDWARE") == FREERDP_DECODE_HARDWARE_NO);

	/* the channel carries on: a decoder's failure is no failure of the channel */
	TEST_CHECK(send_bitstream(callback, RDPGFX_CODECID_AV1, av1Cut, sizeof(av1Cut), FALSE) ==
	           CHANNEL_RC_OK);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AV1_FAILED") == 2 - taken);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AV1_PICTURES") == 0);
#endif

#if defined(WITH_GFX_H264)
	/* 64x64 solid colour, the key frame of TestFreeRDPCodecH264 */
	static const BYTE idr[] = { 0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0xc0, 0x0a, 0xd9, 0x04,
		                        0x26, 0xc0, 0x44, 0x00, 0x00, 0x03, 0x00, 0x04, 0x00, 0x00,
		                        0x03, 0x00, 0xc8, 0x3c, 0x48, 0x99, 0x20, 0x00, 0x00, 0x00,
		                        0x01, 0x68, 0xcb, 0x83, 0xcb, 0x20, 0x00, 0x00, 0x01, 0x65,
		                        0x88, 0x84, 0x0a, 0xf1, 0x18, 0xa0, 0x00, 0x22, 0x4b, 0x1c,
		                        0x00, 0x04, 0x53, 0xa3, 0x80, 0x00, 0x85, 0x8c, 0x9c, 0x9c,
		                        0x9d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5d, 0x75, 0xd7, 0x5e };

	TEST_CHECK(send_bitstream(callback, RDPGFX_CODECID_AVC420, idr, sizeof(idr), FALSE) ==
	           CHANNEL_RC_OK);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_PICTURES") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_KEY_FRAMES") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_FAILED") == 0);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_LAST_PICTURE_AGE_NS") > 0);
	TEST_CHECK(rdpgfx_stats_decoder_backend(gfx, RDPGFX_CODECID_AVC420));

	TEST_CHECK(send_bitstream(callback, RDPGFX_CODECID_AVC444, idr, sizeof(idr), TRUE) ==
	           CHANNEL_RC_OK);
	TEST_CHECK(send_bitstream(callback, RDPGFX_CODECID_AVC444v2, idr, sizeof(idr), TRUE) ==
	           CHANNEL_RC_OK);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC444_PICTURES") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC444v2_PICTURES") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_DECODER_AVC420_PICTURES") == 1);
#endif
	return TRUE;
}

static BOOL check_renderer(rdpContext* context, IWTSPlugin* plugin, test_channel_manager* mgr,
                           IWTSVirtualChannel* channel)
{
	RdpgfxClientContext* gfx = (RdpgfxClientContext*)plugin->pInterface;
	TEST_CHECK(gfx);

	/* Decoding needs nothing of the desktop: the surface stays unmapped */
	rdpGdi gdi = WINPR_C_ARRAY_INIT;
	gdi.context = context;
	gdi.log = WLog_Get("com.freerdp.test.rdpgfx");
	TEST_CHECK(freerdp_settings_set_uint32(context->settings, FreeRDP_DesktopWidth, TEST_SIZE));
	TEST_CHECK(freerdp_settings_set_uint32(context->settings, FreeRDP_DesktopHeight, TEST_SIZE));
	BOOL rc = gdi_graphics_pipeline_init(&gdi, gfx);

	IWTSVirtualChannelCallback* callback = nullptr;
	rc = rc && open_channel(mgr, channel, &callback);
	if (rc)
	{
		rc = check_decoders(gfx, callback);
		(void)callback->OnClose(callback);
	}

	gdi_graphics_pipeline_uninit(&gdi, gfx);
	return rc;
}
#endif

static BOOL check_channel(rdpContext* context, IWTSPlugin* plugin, test_channel_manager* mgr)
{
	IWTSVirtualChannel channel = WINPR_C_ARRAY_INIT;
	channel.Write = test_channel_write;
	channel.Close = test_channel_close;

	TEST_CHECK(plugin->Initialize(plugin, &mgr->iface) == CHANNEL_RC_OK);
	TEST_CHECK(mgr->listenerCallback);

	RdpgfxClientContext* gfx = (RdpgfxClientContext*)plugin->pInterface;
	TEST_CHECK(gfx);

	IWTSVirtualChannelCallback* callback = nullptr;
	TEST_CHECK(open_channel(mgr, &channel, &callback));

	const BOOL rc = check_session(gfx, callback);

	/* frees the channel callback */
	(void)callback->OnClose(callback);
	TEST_CHECK(rc);

	TEST_CHECK(stat_value(gfx, "RDPGFX_FRAMES_UNACKNOWLEDGED") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_FRAMES_DECODED") == 1);
	TEST_CHECK(stat_value(gfx, "RDPGFX_CODECID_UNCOMPRESSED_BYTES") == 12);

	/* the next channel starts from zero */
	TEST_CHECK(open_channel(mgr, &channel, &callback));
	const BOOL zero =
	    all_zero(gfx) && (rdpgfx_stats_decoder_backend(gfx, RDPGFX_CODECID_AVC420) == nullptr);
	(void)callback->OnClose(callback);
	TEST_CHECK(zero);

#if defined(WITH_GFX_AV1) || defined(WITH_GFX_H264)
	TEST_CHECK(check_renderer(context, plugin, mgr, &channel));
#else
	WINPR_UNUSED(context);
#endif
	return TRUE;
}

static BOOL check_plugin(rdpContext* context)
{
	PDVC_PLUGIN_ENTRY pluginEntry = WINPR_FUNC_PTR_CAST(
	    freerdp_channels_load_static_addin_entry(RDPGFX_CHANNEL_NAME, nullptr, nullptr,
	                                             FREERDP_ADDIN_CHANNEL_DYNAMIC),
	    PDVC_PLUGIN_ENTRY);
	TEST_CHECK(pluginEntry);

	test_entry_points ep = WINPR_C_ARRAY_INIT;
	ep.iface.RegisterPlugin = test_register_plugin;
	ep.iface.GetPlugin = test_get_plugin;
	ep.iface.GetPluginData = test_get_plugin_data;
	ep.iface.GetRdpSettings = test_get_rdp_settings;
	ep.iface.GetRdpContext = test_get_rdp_context;
	ep.context = context;

	TEST_CHECK(pluginEntry(&ep.iface) == CHANNEL_RC_OK);
	TEST_CHECK(ep.plugin);

	test_channel_manager mgr = WINPR_C_ARRAY_INIT;
	mgr.iface.CreateListener = test_create_listener;
	mgr.iface.DestroyListener = test_destroy_listener;
	mgr.iface.GetChannelId = test_get_channel_id;
	mgr.iface.GetChannelName = test_get_channel_name;
	mgr.iface.FindChannelById = test_find_channel_by_id;

	const BOOL rc = check_channel(context, ep.plugin, &mgr);

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
