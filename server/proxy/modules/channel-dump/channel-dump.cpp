/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server persist-bitmap-filter Module
 *
 * this module is designed to deactivate all persistent bitmap cache settings a
 * client might send.
 *
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <map>
#include <memory>
#include <mutex>

#include <freerdp/server/proxy/proxy_modules_api.h>
#include <freerdp/server/proxy/proxy_context.h>

#include <freerdp/channels/drdynvc.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/utils/gfx.h>

#define TAG MODULE_TAG("channel-dump")

static constexpr char plugin_name[] = "channel-dump";
static constexpr char plugin_desc[] =
    "this plugin allows filtering and dumping dynamic channel data";

static const std::vector<std::string> plugin_static_intercept = { DRDYNVC_SVC_CHANNEL_NAME };
static const std::vector<std::string> plugin_dyn_intercept = { "WebAuthN_Channel" };

class DynChannelState
{

  public:
	bool skip() const
	{
		return _toSkip != 0;
	}

	bool skip(size_t s)
	{
		if (s > _toSkip)
			_toSkip = 0;
		else
			_toSkip -= s;
		return skip();
	}

	size_t remaining() const
	{
		return _toSkip;
	}

	size_t total() const
	{
		return _totalSkipSize;
	}

	void setSkipSize(size_t len)
	{
		_toSkip = _totalSkipSize = len;
	}

	bool drop() const
	{
		return _drop;
	}

	void setDrop(bool d)
	{
		_drop = d;
	}

	uint32_t channelId() const
	{
		return _channelId;
	}

	void setChannelId(uint32_t id)
	{
		_channelId = id;
	}

  private:
	size_t _toSkip = 0;
	size_t _totalSkipSize = 0;
	bool _drop = false;
	uint32_t _channelId = 0;
};

static BOOL filter_client_pre_connect(proxyPlugin* plugin, proxyData* pdata, void* custom)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(pdata->pc);
	WINPR_ASSERT(custom);

	auto settings = pdata->pc->context.settings;

	/* We do not want persistent bitmap cache to be used with proxy */
	return freerdp_settings_set_bool(settings, FreeRDP_BitmapCachePersistEnabled, FALSE);
}

static BOOL filter_dyn_channel_intercept_list(proxyPlugin* plugin, proxyData* pdata, void* arg)
{
	auto data = static_cast<proxyChannelToInterceptData*>(arg);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(data);

	auto intercept = std::find(plugin_dyn_intercept.begin(), plugin_dyn_intercept.end(),
	                           data->name) != plugin_dyn_intercept.end();
	if (intercept)
		data->intercept = TRUE;
	return TRUE;
}

static BOOL filter_static_channel_intercept_list(proxyPlugin* plugin, proxyData* pdata, void* arg)
{
	auto data = static_cast<proxyChannelToInterceptData*>(arg);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(data);

	auto intercept = std::find(plugin_static_intercept.begin(), plugin_static_intercept.end(),
	                           data->name) != plugin_static_intercept.end();
	if (intercept)
		data->intercept = TRUE;
	return TRUE;
}

static size_t drdynvc_cblen_to_bytes(UINT8 cbLen)
{
	switch (cbLen)
	{
		case 0:
			return 1;

		case 1:
			return 2;

		default:
			return 4;
	}
}

static UINT32 drdynvc_read_variable_uint(wStream* s, UINT8 cbLen)
{
	UINT32 val = 0;

	switch (cbLen)
	{
		case 0:
			Stream_Read_UINT8(s, val);
			break;

		case 1:
			Stream_Read_UINT16(s, val);
			break;

		default:
			Stream_Read_UINT32(s, val);
			break;
	}

	return val;
}

static BOOL drdynvc_try_read_header(wStream* s, size_t& channelId, size_t& length)
{
	UINT8 value = 0;
	Stream_SetPosition(s, 0);
	if (Stream_GetRemainingLength(s) < 1)
		return FALSE;
	Stream_Read_UINT8(s, value);

	const UINT8 cmd = (value & 0xf0) >> 4;
	const UINT8 Sp = (value & 0x0c) >> 2;
	const UINT8 cbChId = (value & 0x03);

	switch (cmd)
	{
		case DATA_PDU:
		case DATA_FIRST_PDU:
			break;
		default:
			return FALSE;
	}

	const size_t channelIdLen = drdynvc_cblen_to_bytes(cbChId);
	if (Stream_GetRemainingLength(s) < channelIdLen)
		return FALSE;

	channelId = drdynvc_read_variable_uint(s, cbChId);
	length = Stream_Length(s);
	if (cmd == DATA_FIRST_PDU)
	{
		const size_t dataLen = drdynvc_cblen_to_bytes(Sp);
		if (Stream_GetRemainingLength(s) < dataLen)
			return FALSE;

		length = drdynvc_read_variable_uint(s, Sp);
	}

	return TRUE;
}

static DynChannelState* filter_get_plugin_data(proxyPlugin* plugin, proxyData* pdata)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	auto mgr = static_cast<proxyPluginsManager*>(plugin->custom);
	WINPR_ASSERT(mgr);

	WINPR_ASSERT(mgr->GetPluginData);
	return static_cast<DynChannelState*>(mgr->GetPluginData(mgr, plugin_name, pdata));
}

static BOOL filter_set_plugin_data(proxyPlugin* plugin, proxyData* pdata, DynChannelState* data)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	auto mgr = static_cast<proxyPluginsManager*>(plugin->custom);
	WINPR_ASSERT(mgr);

	WINPR_ASSERT(mgr->SetPluginData);
	return mgr->SetPluginData(mgr, plugin_name, pdata, data);
}

static BOOL filter_dyn_channel_intercept(proxyPlugin* plugin, proxyData* pdata, void* arg)
{
	auto data = static_cast<proxyDynChannelInterceptData*>(arg);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(data);

	data->result = PF_CHANNEL_RESULT_PASS;

	if (std::find(plugin_dyn_intercept.begin(), plugin_dyn_intercept.end(), data->name) !=
	    plugin_dyn_intercept.end())
	{
		char name[PATH_MAX] = { 0 };
		static size_t xxx = 0;
		if (data->isBackData)
			_snprintf(name, sizeof(name), "/tmp/rx_%s_%" PRIuz, data->name, xxx++);
		else
			_snprintf(name, sizeof(name), "/tmp/tx_%s_%" PRIuz, data->name, xxx++);

		if (strnlen(name, sizeof(name)) > 0)
		{
			FILE* fp = fopen(name, "a+");
			if (fp)
			{
				fwrite(Stream_Buffer(data->data), 1, Stream_Length(data->data), fp);
				fclose(fp);
			}
		}
	}

	return TRUE;
}

static BOOL filter_server_session_started(proxyPlugin* plugin, proxyData* pdata, void*)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	auto state = filter_get_plugin_data(plugin, pdata);
	delete state;

	auto newstate = new DynChannelState();
	if (!filter_set_plugin_data(plugin, pdata, newstate))
	{
		delete newstate;
		return FALSE;
	}

	return TRUE;
}

static BOOL filter_server_session_end(proxyPlugin* plugin, proxyData* pdata, void*)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	auto state = filter_get_plugin_data(plugin, pdata);
	delete state;
	filter_set_plugin_data(plugin, pdata, nullptr);
	return TRUE;
}

#ifdef __cplusplus
extern "C"
{
#endif
	FREERDP_API BOOL proxy_module_entry_point(proxyPluginsManager* plugins_manager, void* userdata);
#ifdef __cplusplus
}
#endif

BOOL proxy_module_entry_point(proxyPluginsManager* plugins_manager, void* userdata)
{
	proxyPlugin plugin = {};

	plugin.name = plugin_name;
	plugin.description = plugin_desc;

	plugin.ServerSessionStarted = filter_server_session_started;
	plugin.ServerSessionEnd = filter_server_session_end;

	plugin.ClientPreConnect = filter_client_pre_connect;

	plugin.StaticChannelToIntercept = filter_static_channel_intercept_list;
	plugin.DynChannelToIntercept = filter_dyn_channel_intercept_list;
	plugin.DynChannelIntercept = filter_dyn_channel_intercept;

	plugin.custom = plugins_manager;
	if (!plugin.custom)
		return FALSE;
	plugin.userdata = userdata;

	return plugins_manager->RegisterPlugin(plugins_manager, &plugin);
}
