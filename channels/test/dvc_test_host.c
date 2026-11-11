/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Test host for dynamic virtual channel plugins
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

#include <winpr/crt.h>

#include <freerdp/client.h>
#include <freerdp/client/channels.h>

#include "dvc_test_host.h"

static UINT test_register_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints,
                                 WINPR_ATTR_UNUSED const char* name, IWTSPlugin* pPlugin)
{
	dvc_test_entry_points* ep = (dvc_test_entry_points*)pEntryPoints;
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

static const ADDIN_ARGV* test_get_plugin_data(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return ((dvc_test_entry_points*)pEntryPoints)->args;
}

static rdpSettings* test_get_rdp_settings(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return ((dvc_test_entry_points*)pEntryPoints)->context->settings;
}

static rdpContext* test_get_rdp_context(IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return ((dvc_test_entry_points*)pEntryPoints)->context;
}

BOOL dvc_test_load_plugin(dvc_test_entry_points* ep, rdpContext* context, const char* name,
                          const ADDIN_ARGV* args)
{
	PDVC_PLUGIN_ENTRY pluginEntry = WINPR_FUNC_PTR_CAST(
	    freerdp_load_channel_addin_entry(name, nullptr, nullptr, FREERDP_ADDIN_CHANNEL_DYNAMIC),
	    PDVC_PLUGIN_ENTRY);
	TEST_CHECK(pluginEntry);

	memset(ep, 0, sizeof(*ep));
	ep->iface.RegisterPlugin = test_register_plugin;
	ep->iface.GetPlugin = test_get_plugin;
	ep->iface.GetPluginData = test_get_plugin_data;
	ep->iface.GetRdpSettings = test_get_rdp_settings;
	ep->iface.GetRdpContext = test_get_rdp_context;
	ep->context = context;
	ep->args = args;

	TEST_CHECK(pluginEntry(&ep->iface) == CHANNEL_RC_OK);
	TEST_CHECK(ep->plugin);
	return TRUE;
}

static UINT test_create_listener(IWTSVirtualChannelManager* pChannelMgr,
                                 WINPR_ATTR_UNUSED const char* pszChannelName,
                                 WINPR_ATTR_UNUSED ULONG ulFlags,
                                 IWTSListenerCallback* pListenerCallback, IWTSListener** ppListener)
{
	dvc_test_channel_manager* mgr = (dvc_test_channel_manager*)pChannelMgr;
	mgr->listenerCallback = pListenerCallback;
	if (ppListener)
		*ppListener = &mgr->listener;
	return CHANNEL_RC_OK;
}

static UINT test_destroy_listener(IWTSVirtualChannelManager* pChannelMgr,
                                  WINPR_ATTR_UNUSED IWTSListener* pListener)
{
	((dvc_test_channel_manager*)pChannelMgr)->listenerCallback = nullptr;
	return CHANNEL_RC_OK;
}

static UINT32 test_get_channel_id(WINPR_ATTR_UNUSED IWTSVirtualChannel* channel)
{
	return 1;
}

static const char* test_get_channel_name(WINPR_ATTR_UNUSED IWTSVirtualChannel* channel)
{
	return "test";
}

static IWTSVirtualChannel* test_find_channel_by_id(WINPR_ATTR_UNUSED IWTSVirtualChannelManager* mgr,
                                                   WINPR_ATTR_UNUSED UINT32 ChannelId)
{
	return nullptr;
}

void dvc_test_channel_manager_init(dvc_test_channel_manager* mgr)
{
	memset(mgr, 0, sizeof(*mgr));
	mgr->iface.CreateListener = test_create_listener;
	mgr->iface.DestroyListener = test_destroy_listener;
	mgr->iface.GetChannelId = test_get_channel_id;
	mgr->iface.GetChannelName = test_get_channel_name;
	mgr->iface.FindChannelById = test_find_channel_by_id;
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

void dvc_test_channel_init(IWTSVirtualChannel* channel)
{
	channel->Write = test_channel_write;
	channel->Close = test_channel_close;
}

BOOL dvc_test_open_channel(dvc_test_channel_manager* mgr, IWTSVirtualChannel* channel,
                           IWTSVirtualChannelCallback** callback)
{
	BOOL accept = TRUE;
	*callback = nullptr;

	TEST_CHECK(mgr->listenerCallback);
	TEST_CHECK(mgr->listenerCallback->OnNewChannelConnection(
	               mgr->listenerCallback, channel, nullptr, &accept, callback) == CHANNEL_RC_OK);
	TEST_CHECK(*callback);
	if ((*callback)->OnOpen)
		TEST_CHECK((*callback)->OnOpen(*callback) == CHANNEL_RC_OK);
	return TRUE;
}
