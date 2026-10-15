/**
 * Regression test: RDPEWA must accept multiple dynamic channel instances while
 * the generic DVC guard continues to reject duplicate connections by default.
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

#include <freerdp/client/channels.h>
#include <freerdp/dvc.h>

typedef struct
{
	IDRDYNVC_ENTRY_POINTS iface;
	IWTSPlugin* registeredPlugin;
} TEST_ENTRY_POINTS;

typedef struct
{
	IWTSVirtualChannelManager iface;
	IWTSListener listener;
	IWTSListenerCallback* listenerCallback;
} TEST_CHANNEL_MANAGER;

static IWTSPlugin* test_get_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints,
                                   WINPR_ATTR_UNUSED const char* name)
{
	TEST_ENTRY_POINTS* entry = (TEST_ENTRY_POINTS*)pEntryPoints;
	return entry->registeredPlugin;
}

static UINT test_register_plugin(IDRDYNVC_ENTRY_POINTS* pEntryPoints,
                                 WINPR_ATTR_UNUSED const char* name, IWTSPlugin* pPlugin)
{
	TEST_ENTRY_POINTS* entry = (TEST_ENTRY_POINTS*)pEntryPoints;
	entry->registeredPlugin = pPlugin;
	return CHANNEL_RC_OK;
}

static rdpContext* test_get_context(WINPR_ATTR_UNUSED IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return nullptr;
}

static rdpSettings* test_get_settings(WINPR_ATTR_UNUSED IDRDYNVC_ENTRY_POINTS* pEntryPoints)
{
	return nullptr;
}

static UINT test_create_listener(IWTSVirtualChannelManager* pChannelMgr,
                                 WINPR_ATTR_UNUSED const char* pszChannelName,
                                 WINPR_ATTR_UNUSED ULONG ulFlags,
                                 IWTSListenerCallback* pListenerCallback, IWTSListener** ppListener)
{
	TEST_CHANNEL_MANAGER* manager = (TEST_CHANNEL_MANAGER*)pChannelMgr;
	manager->listenerCallback = pListenerCallback;
	*ppListener = &manager->listener;
	return CHANNEL_RC_OK;
}

static UINT test_destroy_listener(WINPR_ATTR_UNUSED IWTSVirtualChannelManager* pChannelMgr,
                                  WINPR_ATTR_UNUSED IWTSListener* pListener)
{
	return CHANNEL_RC_OK;
}

static UINT test_callback_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	free(pChannelCallback);
	return CHANNEL_RC_OK;
}

static const IWTSVirtualChannelCallback test_callbacks = { nullptr, nullptr, test_callback_on_close,
                                                           nullptr };

static UINT test_init_plugin(GENERIC_DYNVC_PLUGIN* plugin, WINPR_ATTR_UNUSED rdpContext* context,
                             WINPR_ATTR_UNUSED rdpSettings* settings)
{
	plugin->allowMultipleInstances = FALSE;
	return CHANNEL_RC_OK;
}

static void test_entry_points_init(TEST_ENTRY_POINTS* entry)
{
	*entry = (TEST_ENTRY_POINTS)WINPR_C_ARRAY_INIT;
	entry->iface.RegisterPlugin = test_register_plugin;
	entry->iface.GetPlugin = test_get_plugin;
	entry->iface.GetRdpContext = test_get_context;
	entry->iface.GetRdpSettings = test_get_settings;
}

static void test_channel_manager_init(TEST_CHANNEL_MANAGER* manager)
{
	*manager = (TEST_CHANNEL_MANAGER)WINPR_C_ARRAY_INIT;
	manager->iface.CreateListener = test_create_listener;
	manager->iface.DestroyListener = test_destroy_listener;
}

static BOOL test_open_channel(IWTSListenerCallback* listenerCallback,
                              IWTSVirtualChannelCallback** callback)
{
	BOOL accepted = TRUE;
	const UINT rc = listenerCallback->OnNewChannelConnection(listenerCallback, nullptr, nullptr,
	                                                         &accepted, callback);
	return rc == CHANNEL_RC_OK && accepted && *callback != nullptr;
}

static BOOL test_close_channel(IWTSVirtualChannelCallback** callback)
{
	if (!callback || !*callback)
		return TRUE;

	const UINT rc = (*callback)->OnClose(*callback);
	*callback = nullptr;
	return rc == CHANNEL_RC_OK;
}

static int test_connection_behavior(IWTSPlugin* plugin, TEST_CHANNEL_MANAGER* manager,
                                    BOOL allowMultipleInstances)
{
	IWTSVirtualChannelCallback* first = nullptr;
	IWTSVirtualChannelCallback* second = nullptr;
	IWTSListenerCallback* listenerCallback = nullptr;
	int rc = -1;

	if (!plugin || !manager || plugin->Initialize(plugin, &manager->iface) != CHANNEL_RC_OK)
		goto out;

	listenerCallback = manager->listenerCallback;
	if (!listenerCallback || !test_open_channel(listenerCallback, &first))
		goto out;

	if (allowMultipleInstances)
	{
		if (!test_open_channel(listenerCallback, &second))
			goto out;
	}
	else
	{
		BOOL accepted = TRUE;
		const UINT status = listenerCallback->OnNewChannelConnection(listenerCallback, nullptr,
		                                                             nullptr, &accepted, &second);
		if ((status != ERROR_DEVICE_ALREADY_ATTACHED) || (second != nullptr))
			goto out;

		if (!test_close_channel(&first) || !test_open_channel(listenerCallback, &second))
			goto out;
	}

	rc = 0;

out:
	if (!test_close_channel(&first) || !test_close_channel(&second))
		rc = -1;
	if (plugin)
		plugin->Terminated(plugin);
	return rc;
}

static int test_generic_guard(void)
{
	TEST_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	TEST_CHANNEL_MANAGER manager = WINPR_C_ARRAY_INIT;

	test_entry_points_init(&entry);
	test_channel_manager_init(&manager);
	if (freerdp_generic_DVCPluginEntry(&entry.iface, "test.generic", "test.generic",
	                                   sizeof(GENERIC_DYNVC_PLUGIN),
	                                   sizeof(GENERIC_CHANNEL_CALLBACK), &test_callbacks,
	                                   test_init_plugin, nullptr) != CHANNEL_RC_OK)
		return -1;

	return test_connection_behavior(entry.registeredPlugin, &manager, FALSE);
}

extern UINT VCAPITYPE rdpewa_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints);

static int test_rdpewa_multiple_connections(void)
{
	TEST_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;
	TEST_CHANNEL_MANAGER manager = WINPR_C_ARRAY_INIT;

	test_entry_points_init(&entry);
	test_channel_manager_init(&manager);
	if (rdpewa_DVCPluginEntry(&entry.iface) != CHANNEL_RC_OK)
		return -1;

	return test_connection_behavior(entry.registeredPlugin, &manager, TRUE);
}

int TestRdpewaMultipleConnections(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (test_generic_guard() < 0)
	{
		(void)fprintf(stderr, "generic dynamic channel guard regression\n");
		return -1;
	}

	if (test_rdpewa_multiple_connections() < 0)
	{
		(void)fprintf(stderr, "RDPEWA multiple dynamic channel regression\n");
		return -1;
	}

	return 0;
}
