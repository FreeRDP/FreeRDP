/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Regression test for client-status initialization before RemoteApp execution.
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
#include <winpr/crt.h>
#include <freerdp/client.h>
#include <freerdp/client/channels.h>
#include <freerdp/client/rail.h>
#include <freerdp/svc.h>

static PCHANNEL_INIT_EVENT_EX_FN g_initEvent = nullptr;
static LPVOID g_userParam = nullptr;
static RailClientContext* g_rail = nullptr;
static int g_initHandle = 0;
static UINT g_events[2] = WINPR_C_ARRAY_INIT;
static size_t g_eventCount = 0;
static UINT32 g_flags = 0;
static UINT32 g_build = 0;
static BOOL g_failHandshake = FALSE;
static BOOL g_failStatus = FALSE;
static size_t g_execCount = 0;

static UINT VCAPITYPE test_init_ex(LPVOID userParam, LPVOID clientContext,
                                   WINPR_ATTR_UNUSED LPVOID initHandle,
                                   WINPR_ATTR_UNUSED PCHANNEL_DEF channel,
                                   WINPR_ATTR_UNUSED INT channelCount,
                                   WINPR_ATTR_UNUSED ULONG version,
                                   PCHANNEL_INIT_EVENT_EX_FN initEvent)
{
	g_userParam = userParam;
	g_rail = clientContext;
	g_initEvent = initEvent;
	return CHANNEL_RC_OK;
}

static UINT test_handshake(WINPR_ATTR_UNUSED RailClientContext* context,
                           const RAIL_HANDSHAKE_ORDER* handshake)
{
	if (g_eventCount >= ARRAYSIZE(g_events))
		return ERROR_INTERNAL_ERROR;
	g_events[g_eventCount++] = TS_RAIL_ORDER_HANDSHAKE;
	g_build = handshake->buildNumber;
	return g_failHandshake ? ERROR_INTERNAL_ERROR : CHANNEL_RC_OK;
}

static UINT test_status(WINPR_ATTR_UNUSED RailClientContext* context,
                        const RAIL_CLIENT_STATUS_ORDER* status)
{
	if (g_eventCount >= ARRAYSIZE(g_events))
		return ERROR_INTERNAL_ERROR;
	g_events[g_eventCount++] = TS_RAIL_ORDER_CLIENTSTATUS;
	g_flags = status->flags;
	return g_failStatus ? ERROR_INTERNAL_ERROR : CHANNEL_RC_OK;
}

static UINT test_sysparam(WINPR_ATTR_UNUSED RailClientContext* context,
                          WINPR_ATTR_UNUSED const RAIL_SYSPARAM_ORDER* sysparam)
{
	return CHANNEL_RC_OK;
}

static UINT test_execute(WINPR_ATTR_UNUSED RailClientContext* context,
                         WINPR_ATTR_UNUSED const RAIL_EXEC_ORDER* exec)
{
	g_execCount++;
	return CHANNEL_RC_OK;
}

static BOOL check_handshake(rdpSettings* settings, BOOL extended, BOOL reconnect, UINT32 flags,
                            BOOL failHandshake, BOOL failStatus)
{
	if (!freerdp_settings_set_uint32(settings, FreeRDP_RemoteAppFeatureFlags, flags) ||
	    !freerdp_settings_set_bool(settings, FreeRDP_AutoReconnectionEnabled, reconnect))
		return FALSE;
	g_eventCount = 0;
	g_execCount = 0;
	g_failHandshake = failHandshake;
	g_failStatus = failStatus;
	const RAIL_HANDSHAKE_ORDER handshake = { 0 };
	const RAIL_HANDSHAKE_EX_ORDER handshakeEx = WINPR_C_ARRAY_INIT;
	const UINT rc = extended ? g_rail->ServerHandshakeEx(g_rail, &handshakeEx)
	                         : g_rail->ServerHandshake(g_rail, &handshake);
	const UINT expected = (failHandshake || failStatus) ? ERROR_INTERNAL_ERROR : CHANNEL_RC_OK;
	const UINT32 expectedFlags = reconnect ? flags | TS_RAIL_CLIENTSTATUS_AUTORECONNECT
	                                       : flags & ~TS_RAIL_CLIENTSTATUS_AUTORECONNECT;
	if ((rc != expected) || (g_eventCount != (failHandshake ? 1 : 2)) ||
	    (g_events[0] != TS_RAIL_ORDER_HANDSHAKE) || (g_execCount != 0) ||
	    (g_build != freerdp_settings_get_uint32(settings, FreeRDP_ClientBuild)))
		return FALSE;
	return failHandshake ||
	       ((g_events[1] == TS_RAIL_ORDER_CLIENTSTATUS) && (g_flags == expectedFlags));
}

int TestRailHandshake(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	int rc = -1;
	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.Size = sizeof(RDP_CLIENT_ENTRY_POINTS_V1);
	entry.ContextSize = sizeof(rdpClientContext);
	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
		return -1;
	PVIRTUALCHANNELENTRYEX entryEx =
	    WINPR_FUNC_PTR_CAST(freerdp_channels_load_static_addin_entry(
	                            RAIL_SVC_CHANNEL_NAME, nullptr, nullptr,
	                            FREERDP_ADDIN_CHANNEL_STATIC | FREERDP_ADDIN_CHANNEL_ENTRYEX),
	                        PVIRTUALCHANNELENTRYEX);
	if (!entryEx)
		goto fail;
	CHANNEL_ENTRY_POINTS_FREERDP_EX channelEntry = WINPR_C_ARRAY_INIT;
	channelEntry.cbSize = sizeof(channelEntry);
	channelEntry.protocolVersion = VIRTUAL_CHANNEL_VERSION_WIN2000;
	channelEntry.pVirtualChannelInitEx = test_init_ex;
	channelEntry.MagicNumber = FREERDP_CHANNEL_MAGIC_NUMBER;
	channelEntry.context = context;
	if (!entryEx((PCHANNEL_ENTRY_POINTS_EX)&channelEntry, &g_initHandle))
		goto fail;
	if (!g_rail || !g_initEvent)
		goto terminate;
	g_rail->ClientHandshake = test_handshake;
	g_rail->ClientInformation = test_status;
	g_rail->ClientSystemParam = test_sysparam;
	g_rail->ClientExecute = test_execute;
	const UINT32 flags[] = { 0, TS_RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE |
		                            TS_RAIL_CLIENTSTATUS_AUTORECONNECT |
		                            TS_RAIL_CLIENTSTATUS_ZORDER_SYNC };
	for (size_t extended = 0; extended < 2; extended++)
	{
		for (size_t reconnect = 0; reconnect < 2; reconnect++)
		{
			for (size_t i = 0; i < ARRAYSIZE(flags); i++)
			{
				if (!check_handshake(context->settings, extended != 0, reconnect != 0, flags[i],
				                     FALSE, FALSE) ||
				    !check_handshake(context->settings, extended != 0, reconnect != 0, flags[i],
				                     TRUE, FALSE) ||
				    !check_handshake(context->settings, extended != 0, reconnect != 0, flags[i],
				                     FALSE, TRUE))
				{
					(void)fprintf(stderr,
					              "RAIL handshake failed: extended=%zu reconnect=%zu flags=%zu\n",
					              extended, reconnect, i);
					goto terminate;
				}
			}
		}
	}
	if (!freerdp_settings_set_bool(context->settings, FreeRDP_RemoteAppLanguageBarSupported,
	                               FALSE) ||
	    !freerdp_settings_set_string(context->settings, FreeRDP_RemoteApplicationProgram, "||test"))
		goto terminate;
	/* Callback-override clients still initialize status through the launch helper. */
	for (size_t failure = 0; failure < 2; failure++)
	{
		g_eventCount = 0;
		g_execCount = 0;
		g_failStatus = failure != 0;
		const UINT result = client_rail_server_start_cmd(g_rail);
		if ((result != (g_failStatus ? ERROR_INTERNAL_ERROR : CHANNEL_RC_OK)) ||
		    (g_eventCount != 1) || (g_events[0] != TS_RAIL_ORDER_CLIENTSTATUS) ||
		    (g_execCount != (g_failStatus ? 0 : 1)))
			goto terminate;
	}
	rc = 0;
terminate:
	if (g_initEvent)
		g_initEvent(g_userParam, &g_initHandle, CHANNEL_EVENT_TERMINATED, nullptr, 0);
fail:
	freerdp_client_context_free(context);
	return rc;
}
