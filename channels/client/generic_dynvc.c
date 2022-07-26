/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic channel
 *
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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
#include <freerdp/log.h>
#include <freerdp/client/channels.h>

#define TAG FREERDP_TAG("genericdynvc")

static UINT generic_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
                                              IWTSVirtualChannel* pChannel, BYTE* Data,
                                              BOOL* pbAccept,
                                              IWTSVirtualChannelCallback** ppCallback)
{
	GENERIC_CHANNEL_CALLBACK* callback;
	GENERIC_DYNVC_PLUGIN* plugin;
	GENERIC_LISTENER_CALLBACK* listener_callback = (GENERIC_LISTENER_CALLBACK*)pListenerCallback;

	if (!listener_callback || !listener_callback->plugin)
		return ERROR_INTERNAL_ERROR;

	plugin = (GENERIC_DYNVC_PLUGIN*)listener_callback->plugin;
	WLog_Print(plugin->log, WLOG_TRACE, "...");

	callback = (GENERIC_CHANNEL_CALLBACK*)calloc(1, plugin->channelCallbackSize);
	if (!callback)
	{
		WLog_Print(plugin->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	/* implant configured channel callbacks */
	callback->iface = *plugin->channel_callbacks;

	callback->plugin = listener_callback->plugin;
	callback->channel_mgr = listener_callback->channel_mgr;
	callback->channel = pChannel;

	listener_callback->channel_callback = callback;
	*ppCallback = (IWTSVirtualChannelCallback*)callback;
	return CHANNEL_RC_OK;
}

static UINT generic_dynvc_plugin_initialize(IWTSPlugin* pPlugin,
                                            IWTSVirtualChannelManager* pChannelMgr)
{
	UINT rc;
	GENERIC_LISTENER_CALLBACK* listener_callback;
	GENERIC_DYNVC_PLUGIN* plugin = (GENERIC_DYNVC_PLUGIN*)pPlugin;

	if (!plugin)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	if (!pChannelMgr)
		return ERROR_INVALID_PARAMETER;

	if (plugin->initialized)
	{
		WLog_ERR(TAG, "[%s] channel initialized twice, aborting", plugin->dynvc_name);
		return ERROR_INVALID_DATA;
	}

	WLog_Print(plugin->log, WLOG_TRACE, "...");
	listener_callback = (GENERIC_LISTENER_CALLBACK*)calloc(1, sizeof(GENERIC_LISTENER_CALLBACK));
	if (!listener_callback)
	{
		WLog_Print(plugin->log, WLOG_ERROR, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	plugin->listener_callback = listener_callback;
	listener_callback->iface.OnNewChannelConnection = generic_on_new_channel_connection;
	listener_callback->plugin = pPlugin;
	listener_callback->channel_mgr = pChannelMgr;
	rc = pChannelMgr->CreateListener(pChannelMgr, plugin->dynvc_name, 0, &listener_callback->iface,
	                                 &plugin->listener);

	plugin->listener->pInterface = plugin->iface.pInterface;
	plugin->initialized = (rc == CHANNEL_RC_OK);
	return rc;
}

static UINT generic_plugin_terminated(IWTSPlugin* pPlugin)
{
	GENERIC_DYNVC_PLUGIN* plugin = (GENERIC_DYNVC_PLUGIN*)pPlugin;
	UINT error = CHANNEL_RC_OK;

	if (!plugin)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	WLog_Print(plugin->log, WLOG_TRACE, "...");

	/* some channels (namely rdpei), look at initialized to see if they should continue to run */
	plugin->initialized = FALSE;

	if (plugin->terminatePluginFn)
		plugin->terminatePluginFn(plugin);

	if (plugin->listener_callback)
	{
		IWTSVirtualChannelManager* mgr = plugin->listener_callback->channel_mgr;
		if (mgr)
			IFCALL(mgr->DestroyListener, mgr, plugin->listener);
	}

	free(plugin->listener_callback);
	free(plugin->dynvc_name);
	free(plugin);
	return error;
}

static UINT generic_dynvc_plugin_attached(IWTSPlugin* pPlugin)
{
	GENERIC_DYNVC_PLUGIN* pluginn = (GENERIC_DYNVC_PLUGIN*)pPlugin;
	UINT error = CHANNEL_RC_OK;

	if (!pluginn)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	pluginn->attached = TRUE;
	return error;
}

static UINT generic_dynvc_plugin_detached(IWTSPlugin* pPlugin)
{
	GENERIC_DYNVC_PLUGIN* plugin = (GENERIC_DYNVC_PLUGIN*)pPlugin;
	UINT error = CHANNEL_RC_OK;

	if (!plugin)
		return CHANNEL_RC_BAD_CHANNEL_HANDLE;

	plugin->attached = FALSE;
	return error;
}

UINT freerdp_generic_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints, const char* logTag,
                                    const char* name, size_t pluginSize, size_t channelCallbackSize,
                                    const IWTSVirtualChannelCallback* channel_callbacks,
                                    DYNVC_PLUGIN_INIT_FN initPluginFn,
                                    DYNVC_PLUGIN_TERMINATE_FN terminatePluginFn)
{
	GENERIC_DYNVC_PLUGIN* plugin;
	UINT error = CHANNEL_RC_INITIALIZATION_ERROR;

	WINPR_ASSERT(pEntryPoints);
	WINPR_ASSERT(pEntryPoints->GetPlugin);
	WINPR_ASSERT(logTag);
	WINPR_ASSERT(name);
	WINPR_ASSERT(pluginSize >= sizeof(*plugin));
	WINPR_ASSERT(channelCallbackSize >= sizeof(GENERIC_CHANNEL_CALLBACK));

	plugin = (GENERIC_DYNVC_PLUGIN*)pEntryPoints->GetPlugin(pEntryPoints, name);
	if (plugin != NULL)
		return CHANNEL_RC_ALREADY_INITIALIZED;

	plugin = (GENERIC_DYNVC_PLUGIN*)calloc(1, pluginSize);
	if (!plugin)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	plugin->log = WLog_Get(logTag);
	plugin->attached = TRUE;
	plugin->channel_callbacks = channel_callbacks;
	plugin->channelCallbackSize = channelCallbackSize;
	plugin->iface.Initialize = generic_dynvc_plugin_initialize;
	plugin->iface.Connected = NULL;
	plugin->iface.Disconnected = NULL;
	plugin->iface.Terminated = generic_plugin_terminated;
	plugin->iface.Attached = generic_dynvc_plugin_attached;
	plugin->iface.Detached = generic_dynvc_plugin_detached;
	plugin->terminatePluginFn = terminatePluginFn;

	if (initPluginFn)
	{
		rdpSettings* settings = pEntryPoints->GetRdpSettings(pEntryPoints);
		rdpContext* context = pEntryPoints->GetRdpContext(pEntryPoints);

		error = initPluginFn(plugin, context, settings);
		if (error != CHANNEL_RC_OK)
			goto error;
	}

	plugin->dynvc_name = _strdup(name);
	if (!plugin->dynvc_name)
		goto error;

	error = pEntryPoints->RegisterPlugin(pEntryPoints, name, &plugin->iface);
	if (error == CHANNEL_RC_OK)
		return error;

error:
	generic_plugin_terminated(&plugin->iface);
	return error;
}
