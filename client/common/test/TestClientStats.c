/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Unit test for the runtime statistics of a client session
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
#include <winpr/thread.h>

#include <freerdp/addin.h>
#include <freerdp/autodetect.h>
#include <freerdp/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/freerdp.h>

#if defined(CHANNEL_RDPGFX_CLIENT)
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/client/rdpgfx.h>
#include <freerdp/codec/zgfx.h>
#include <freerdp/dvc.h>
#include <freerdp/gdi/gdi.h>
#endif

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#define NETWORK_STATS 7

/* The default settings need rdpdr, which brings rdpsnd along: without it no connection starts */
#if !defined(_WIN32) && defined(CHANNEL_RDPSND_CLIENT)
#define TEST_NETWORK
#endif

#define TEST_CHECK(cond)                                                             \
	do                                                                               \
	{                                                                                \
		if (!(cond))                                                                 \
		{                                                                            \
			(void)fprintf(stderr, "%s:%d: failed: %s\n", __func__, __LINE__, #cond); \
			return FALSE;                                                            \
		}                                                                            \
	} while (0)

static BOOL find_stat(const char* name, size_t* index)
{
	for (size_t x = 0; x < freerdp_client_stats_max_index(); x++)
	{
		if (strcmp(freerdp_client_stats_name_for_index(x), name) == 0)
		{
			*index = x;
			return TRUE;
		}
	}
	(void)fprintf(stderr, "no stat named %s\n", name);
	return FALSE;
}

static uint64_t stat_value(const rdpContext* context, const char* name)
{
	size_t index = 0;
	if (!find_stat(name, &index))
		return UINT64_MAX;
	return freerdp_client_stats_value_for_index(context, index);
}

static BOOL check_names(void)
{
	static const char* const network[NETWORK_STATS] = {
		"NETWORK_BYTES_RECEIVED", "NETWORK_BYTES_SENT",     "NETWORK_PACKETS_RECEIVED",
		"NETWORK_PACKETS_SENT",   "NETWORK_BANDWIDTH_KBPS", "NETWORK_RTT_BASE_MS",
		"NETWORK_RTT_AVERAGE_MS"
	};

	const size_t max = freerdp_client_stats_max_index();
#if defined(CHANNEL_RDPGFX_CLIENT)
	TEST_CHECK(max == NETWORK_STATS + rdpgfx_stats_max_index());
#else
	TEST_CHECK(max == NETWORK_STATS);
#endif

	for (size_t x = 0; x < max; x++)
	{
		const char* name = freerdp_client_stats_name_for_index(x);
		TEST_CHECK(name);
		TEST_CHECK(strlen(name) > 0);
		TEST_CHECK(strcmp(name, "FREERDP_CLIENT_STATS_UNUSED") != 0);
	}
	TEST_CHECK(strcmp(freerdp_client_stats_name_for_index(max), "FREERDP_CLIENT_STATS_UNUSED") ==
	           0);

	/* each exactly once, in all the statistics */
	for (size_t x = 0; x < ARRAYSIZE(network); x++)
	{
		size_t found = 0;
		for (size_t y = 0; y < max; y++)
		{
			if (strcmp(freerdp_client_stats_name_for_index(y), network[x]) == 0)
				found++;
		}
		TEST_CHECK(found == 1);
	}
	return TRUE;
}

static BOOL check_all_zero(const rdpContext* context)
{
	for (size_t x = 0; x <= freerdp_client_stats_max_index(); x++)
		TEST_CHECK(freerdp_client_stats_value_for_index(context, x) == 0);
	return TRUE;
}

static BOOL check_autodetect(rdpContext* context)
{
	/* a fresh session, nothing received or sent, no gdi and no graphics channel */
	TEST_CHECK(!context->gdi);
	TEST_CHECK(check_all_zero(context));

	context->autodetect->netCharBandwidth = 1001;
	context->autodetect->netCharBaseRTT = 1002;
	context->autodetect->netCharAverageRTT = 1003;
	TEST_CHECK(stat_value(context, "NETWORK_BANDWIDTH_KBPS") == 1001);
	TEST_CHECK(stat_value(context, "NETWORK_RTT_BASE_MS") == 1002);
	TEST_CHECK(stat_value(context, "NETWORK_RTT_AVERAGE_MS") == 1003);

	context->autodetect->netCharBandwidth = 0;
	context->autodetect->netCharBaseRTT = 0;
	context->autodetect->netCharAverageRTT = 0;
	TEST_CHECK(check_all_zero(context));

	/* a context on its way down has neither */
	context->autodetect->netCharBandwidth = 1001;
	rdpAutoDetect* autodetect = context->autodetect;
	rdpRdp* rdp = context->rdp;
	context->autodetect = nullptr;
	context->rdp = nullptr;
	const BOOL rc = check_all_zero(context);
	context->autodetect = autodetect;
	context->rdp = rdp;
	context->autodetect->netCharBandwidth = 0;
	return rc;
}

#if defined(TEST_NETWORK)
#define SERVER_BYTES 16

typedef struct
{
	int listener;
	int rc;
} test_server;

/* Takes the client's first packet and answers with something that is no RDP at all: enough for
 * bytes to pass both ways through the real transport. */
static DWORD WINAPI test_server_thread(LPVOID arg)
{
	test_server* server = (test_server*)arg;
	BYTE buffer[256] = WINPR_C_ARRAY_INIT;

	const int client = accept(server->listener, nullptr, nullptr);

	/* the client tries again with the next protocol: refuse it, or it waits here for an answer */
	(void)close(server->listener);
	server->listener = -1;
	if (client < 0)
		return 1;

	if (recv(client, buffer, sizeof(buffer), 0) > 0)
	{
		memset(buffer, 0xff, SERVER_BYTES);
		if (send(client, buffer, SERVER_BYTES, 0) == SERVER_BYTES)
			server->rc = 0;
	}
	(void)close(client);
	return 0;
}

static BOOL check_network(rdpContext* context)
{
	BOOL rc = FALSE;
	test_server server = { .listener = -1, .rc = -1 };
	struct sockaddr_in addr = WINPR_C_ARRAY_INIT;
	socklen_t len = sizeof(addr);
	HANDLE thread = nullptr;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	/* accept() gives up as well, should the client never come */
	const struct timeval timeout = { .tv_sec = 30, .tv_usec = 0 };

	server.listener = socket(AF_INET, SOCK_STREAM, 0);
	if ((server.listener < 0) ||
	    (setsockopt(server.listener, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) ||
	    (bind(server.listener, (struct sockaddr*)&addr, sizeof(addr)) != 0) ||
	    (listen(server.listener, 1) != 0) ||
	    (getsockname(server.listener, (struct sockaddr*)&addr, &len) != 0))
		goto fail;

	if (!freerdp_settings_set_string(context->settings, FreeRDP_ServerHostname, "127.0.0.1") ||
	    !freerdp_settings_set_uint32(context->settings, FreeRDP_ServerPort, ntohs(addr.sin_port)) ||
	    !freerdp_settings_set_string(context->settings, FreeRDP_Username, "stats") ||
	    !freerdp_settings_set_bool(context->settings, FreeRDP_AutoReconnectionEnabled, FALSE))
		goto fail;

	thread = CreateThread(nullptr, 0, test_server_thread, &server, 0, nullptr);
	if (!thread)
		goto fail;

	/* fails: the other side is no RDP server, what passed until then is what counts */
	(void)freerdp_connect(context->instance);
	(void)WaitForSingleObject(thread, INFINITE);
	(void)CloseHandle(thread);

	if (server.rc != 0)
		goto fail;
	/* the server sent 16 bytes, the client's connection request is longer than that */
	{
		const uint64_t received = stat_value(context, "NETWORK_BYTES_RECEIVED");
		const uint64_t sent = stat_value(context, "NETWORK_BYTES_SENT");
		(void)printf("network: received %" PRIu64 ", sent %" PRIu64 "\n", received, sent);
		TEST_CHECK((received > 0) && (received <= SERVER_BYTES));
		TEST_CHECK(sent > SERVER_BYTES);
	}
	rc = TRUE;
fail:
	if (server.listener >= 0)
		(void)close(server.listener);
	if (!rc)
		(void)fprintf(stderr, "[%s] failed\n", __func__);
	return rc;
}
#endif

#if defined(CHANNEL_RDPGFX_CLIENT)
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
	((test_entry_points*)pEntryPoints)->plugin = pPlugin;
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

static UINT send_start_frame(IWTSVirtualChannelCallback* callback)
{
	wStream* s = Stream_New(nullptr, 2 + RDPGFX_HEADER_SIZE + RDPGFX_START_FRAME_PDU_SIZE);
	if (!s)
		return CHANNEL_RC_NO_MEMORY;
	Stream_Write_UINT8(s, ZGFX_SEGMENTED_SINGLE);
	Stream_Write_UINT8(s, ZGFX_PACKET_COMPR_TYPE_RDP8);
	Stream_Write_UINT16(s, RDPGFX_CMDID_STARTFRAME);
	Stream_Write_UINT16(s, 0); /* flags */
	Stream_Write_UINT32(s, RDPGFX_HEADER_SIZE + RDPGFX_START_FRAME_PDU_SIZE);
	Stream_Write_UINT32(s, 0); /* timestamp */
	Stream_Write_UINT32(s, 1); /* frameId */
	Stream_SealLength(s);
	Stream_ResetPosition(s);
	const UINT rc = callback->OnDataReceived(callback, s);
	Stream_Free(s, TRUE);
	return rc;
}

/* a graphics channel, reached the way a session reaches it: context->gdi->gfx */
static BOOL check_gfx_channel(rdpContext* context, IWTSPlugin* plugin, test_channel_manager* mgr)
{
	IWTSVirtualChannel channel = WINPR_C_ARRAY_INIT;
	channel.Write = test_channel_write;
	channel.Close = test_channel_close;

	TEST_CHECK(plugin->Initialize(plugin, &mgr->iface) == CHANNEL_RC_OK);
	TEST_CHECK(mgr->listenerCallback);

	IWTSVirtualChannelCallback* callback = nullptr;
	BOOL accept = TRUE;
	TEST_CHECK(mgr->listenerCallback->OnNewChannelConnection(
	               mgr->listenerCallback, &channel, nullptr, &accept, &callback) == CHANNEL_RC_OK);
	TEST_CHECK(callback);
	TEST_CHECK(callback->OnOpen(callback) == CHANNEL_RC_OK);

	rdpGdi gdi = WINPR_C_ARRAY_INIT;
	context->gdi = &gdi;

	/* no gfx yet, as on a session whose graphics channel closed */
	BOOL rc = stat_value(context, "RDPGFX_CMDID_STARTFRAME") == 0;

	if (rc)
	{
		gdi.gfx = (RdpgfxClientContext*)plugin->pInterface;
		rc = (stat_value(context, "RDPGFX_CMDID_STARTFRAME") == 0) &&
		     (send_start_frame(callback) == CHANNEL_RC_OK) &&
		     (stat_value(context, "RDPGFX_CMDID_STARTFRAME") == 1) &&
		     (stat_value(context, "RDPGFX_CMDID_ENDFRAME") == 0);
		gdi.gfx = nullptr;
		rc = rc && (stat_value(context, "RDPGFX_CMDID_STARTFRAME") == 0);
	}

	context->gdi = nullptr;
	(void)callback->OnClose(callback);
	TEST_CHECK(rc);
	return TRUE;
}

static BOOL check_gfx(rdpContext* context)
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

	const BOOL rc = check_gfx_channel(context, ep.plugin, &mgr);

	/* frees the plugin */
	(void)ep.plugin->Terminated(ep.plugin);
	return rc;
}
#endif

static rdpContext* new_context(void)
{
	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	entry.ContextSize = sizeof(rdpClientContext);
	return freerdp_client_context_new(&entry);
}

int TestClientStats(WINPR_ATTR_UNUSED int argc, WINPR_ATTR_UNUSED char* argv[])
{
	if (!check_names())
		return -1;
	if (!check_all_zero(nullptr))
		return -1;

	rdpContext* context = new_context();
	if (!context)
		return -1;

	BOOL rc = check_autodetect(context);
#if defined(CHANNEL_RDPGFX_CLIENT)
	rc = rc && check_gfx(context);
#endif
#if defined(TEST_NETWORK)
	rc = rc && check_network(context);
#endif

	freerdp_client_context_free(context);
	return rc ? 0 : -1;
}
