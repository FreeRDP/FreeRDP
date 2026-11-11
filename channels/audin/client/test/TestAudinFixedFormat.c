/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Regression test: a /microphone format that matches none of the server formats must not leave
 * the client with zero formats, or the microphone never opens.
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
#include <freerdp/client/audin.h>
#include <freerdp/client/channels.h>
#include <freerdp/codec/audio.h>
#include <freerdp/dvc.h>
#include <freerdp/settings.h>

#include "dvc_test_host.h"

#define MSG_SNDIN_FORMATS 0x02
#define MAX_FORMATS 4

/* records the formats the client answers with */
typedef struct
{
	IWTSVirtualChannel iface;
	BOOL answered;
	UINT32 count;
	AUDIO_FORMAT formats[MAX_FORMATS];
} test_channel;

/* the test microphone: accepts PCM, never captures */
static UINT test_dev_open(WINPR_ATTR_UNUSED IAudinDevice* dev,
                          WINPR_ATTR_UNUSED AudinReceive receive, WINPR_ATTR_UNUSED void* userData)
{
	return CHANNEL_RC_OK;
}

static BOOL test_dev_format_supported(WINPR_ATTR_UNUSED IAudinDevice* dev,
                                      const AUDIO_FORMAT* format)
{
	return format->wFormatTag == WAVE_FORMAT_PCM;
}

static UINT test_dev_set_format(WINPR_ATTR_UNUSED IAudinDevice* dev,
                                WINPR_ATTR_UNUSED const AUDIO_FORMAT* format,
                                WINPR_ATTR_UNUSED UINT32 FramesPerPacket)
{
	return CHANNEL_RC_OK;
}

static UINT test_dev_close(WINPR_ATTR_UNUSED IAudinDevice* dev)
{
	return CHANNEL_RC_OK;
}

static IAudinDevice g_device = { test_dev_open, test_dev_format_supported, test_dev_set_format,
                                 test_dev_close, test_dev_close };

static UINT VCAPITYPE test_dev_entry(PFREERDP_AUDIN_DEVICE_ENTRY_POINTS pEntryPoints)
{
	return pEntryPoints->pRegisterAudinDevice(pEntryPoints->plugin, &g_device);
}

static FREERDP_LOAD_CHANNEL_ADDIN_ENTRY_FN g_provider = nullptr;

/* serves the test microphone as the audin "test" subsystem */
static PVIRTUALCHANNELENTRY VCAPITYPE test_provider(LPCSTR pszName, LPCSTR pszSubsystem,
                                                    LPCSTR pszType, DWORD dwFlags)
{
	if (pszName && pszSubsystem && (strcmp(pszName, AUDIN_CHANNEL_NAME) == 0) &&
	    (strcmp(pszSubsystem, "test") == 0))
	{
		PFREERDP_AUDIN_DEVICE_ENTRY entry = test_dev_entry;
		return WINPR_FUNC_PTR_CAST(entry, PVIRTUALCHANNELENTRY);
	}
	return g_provider(pszName, pszSubsystem, pszType, dwFlags);
}

static BOOL read_formats(test_channel* channel, wStream* s)
{
	TEST_CHECK(Stream_CheckAndLogRequiredLength("test", s, 8));
	Stream_Read_UINT32(s, channel->count);
	Stream_Seek_UINT32(s); /* cbSizeFormatsPacket */
	TEST_CHECK(channel->count <= MAX_FORMATS);
	for (UINT32 x = 0; x < channel->count; x++)
		TEST_CHECK(audio_format_read(s, &channel->formats[x]));
	channel->answered = TRUE;
	return TRUE;
}

static UINT test_channel_write(IWTSVirtualChannel* pChannel, ULONG cbSize, const BYTE* pBuffer,
                               WINPR_ATTR_UNUSED void* pReserved)
{
	wStream sbuffer = WINPR_C_ARRAY_INIT;
	wStream* s = Stream_StaticConstInit(&sbuffer, pBuffer, cbSize);
	BYTE messageId = 0;

	if (!Stream_CheckAndLogRequiredLength("test", s, 1))
		return ERROR_INVALID_DATA;
	Stream_Read_UINT8(s, messageId);
	if (messageId != MSG_SNDIN_FORMATS)
		return CHANNEL_RC_OK;
	return read_formats((test_channel*)pChannel, s) ? CHANNEL_RC_OK : ERROR_INVALID_DATA;
}

static BOOL write_pcm_format(wStream* s, UINT16 channels, UINT32 rate)
{
	const AUDIO_FORMAT format = { .wFormatTag = WAVE_FORMAT_PCM,
	                              .nChannels = channels,
	                              .nSamplesPerSec = rate,
	                              .nAvgBytesPerSec = rate * channels * 2,
	                              .nBlockAlign = (UINT16)(channels * 2),
	                              .wBitsPerSample = 16,
	                              .cbSize = 0,
	                              .data = nullptr };
	return audio_format_write(s, &format);
}

/* the PCM format Windows 11 offers, and one more to see the filter */
static BOOL send_server_formats(IWTSVirtualChannelCallback* callback)
{
	wStream* s = Stream_New(nullptr, 64);
	TEST_CHECK(s);
	Stream_Write_UINT8(s, MSG_SNDIN_FORMATS);
	Stream_Write_UINT32(s, 2); /* NumFormats */
	Stream_Write_UINT32(s, 0); /* cbSizeFormatsPacket */
	BOOL rc = write_pcm_format(s, 2, 44100) && write_pcm_format(s, 2, 22050);
	Stream_SealLength(s);
	Stream_ResetPosition(s);
	rc = rc && (callback->OnDataReceived(callback, s) == CHANNEL_RC_OK);
	Stream_Free(s, TRUE);
	return rc;
}

static BOOL check_channel(IWTSPlugin* plugin, test_channel* channel)
{
	dvc_test_channel_manager mgr = WINPR_C_ARRAY_INIT;
	IWTSVirtualChannelCallback* callback = nullptr;

	dvc_test_channel_manager_init(&mgr);
	TEST_CHECK(plugin->Initialize(plugin, &mgr.iface) == CHANNEL_RC_OK);
	TEST_CHECK(dvc_test_open_channel(&mgr, &channel->iface, &callback));

	const BOOL rc = send_server_formats(callback);
	/* frees the channel callback */
	(void)callback->OnClose(callback);
	TEST_CHECK(rc);
	TEST_CHECK(channel->answered);
	return TRUE;
}

/* Connects audin with the given /microphone options and records the formats it answers with. */
static BOOL negotiate(rdpContext* context, const ADDIN_ARGV* args, test_channel* channel)
{
	dvc_test_entry_points ep = WINPR_C_ARRAY_INIT;
	TEST_CHECK(dvc_test_load_plugin(&ep, context, AUDIN_CHANNEL_NAME, args));

	memset(channel, 0, sizeof(*channel));
	dvc_test_channel_init(&channel->iface);
	channel->iface.Write = test_channel_write;

	const BOOL rc = check_channel(ep.plugin, channel);
	/* frees the plugin */
	(void)ep.plugin->Terminated(ep.plugin);
	return rc;
}

static BOOL negotiate_with(rdpContext* context, const char* const* argv, size_t argc,
                           test_channel* channel)
{
	ADDIN_ARGV* args = freerdp_addin_argv_new(argc, argv);
	TEST_CHECK(args);
	const BOOL rc = negotiate(context, args, channel);
	freerdp_addin_argv_free(args);
	return rc;
}

static BOOL is_pcm(const AUDIO_FORMAT* format, UINT16 channels, UINT32 rate)
{
	return (format->wFormatTag == WAVE_FORMAT_PCM) && (format->nChannels == channels) &&
	       (format->nSamplesPerSec == rate);
}

/* a configured format the server offers is the only one answered */
static BOOL check_matching_format(rdpContext* context)
{
	const char* argv[] = { AUDIN_CHANNEL_NAME, "sys:test", "format:1", "rate:44100", "channel:2" };
	test_channel channel = WINPR_C_ARRAY_INIT;
	TEST_CHECK(negotiate_with(context, argv, ARRAYSIZE(argv), &channel));
	TEST_CHECK(channel.count == 1);
	TEST_CHECK(is_pcm(&channel.formats[0], 2, 44100));
	return TRUE;
}

/* a configured format the server does not offer: every supported one, not none */
static BOOL check_missing_format(rdpContext* context)
{
	const char* argv[] = { AUDIN_CHANNEL_NAME, "sys:test", "format:1", "rate:16000", "channel:1" };
	test_channel channel = WINPR_C_ARRAY_INIT;
	TEST_CHECK(negotiate_with(context, argv, ARRAYSIZE(argv), &channel));
	TEST_CHECK(channel.count == 2);
	TEST_CHECK(is_pcm(&channel.formats[0], 2, 44100));
	TEST_CHECK(is_pcm(&channel.formats[1], 2, 22050));
	return TRUE;
}

static BOOL check_no_format(rdpContext* context)
{
	const char* argv[] = { AUDIN_CHANNEL_NAME, "sys:test" };
	test_channel channel = WINPR_C_ARRAY_INIT;
	TEST_CHECK(negotiate_with(context, argv, ARRAYSIZE(argv), &channel));
	TEST_CHECK(channel.count == 2);
	return TRUE;
}

static BOOL check_formats(rdpContext* context)
{
	g_provider = freerdp_get_current_addin_provider();
	TEST_CHECK(g_provider);
	TEST_CHECK(freerdp_register_addin_provider(test_provider, 0) == 0);

	const BOOL rc =
	    check_matching_format(context) && check_missing_format(context) && check_no_format(context);
	(void)freerdp_register_addin_provider(g_provider, 0);
	return rc;
}

int TestAudinFixedFormat(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	entry.ContextSize = sizeof(rdpClientContext);

	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
		return -1;

	const BOOL rc = check_formats(context);
	freerdp_client_context_free(context);
	return rc ? 0 : -1;
}
