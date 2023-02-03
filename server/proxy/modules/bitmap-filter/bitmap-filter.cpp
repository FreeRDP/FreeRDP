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

#define TAG MODULE_TAG("persist-bitmap-filter")

static constexpr char plugin_name[] = "bitmap-filter";
static constexpr char plugin_desc[] =
    "this plugin deactivates and filters persistent bitmap cache.";

static const std::vector<std::string> plugin_static_intercept = { DRDYNVC_SVC_CHANNEL_NAME };
static const std::vector<std::string> plugin_dyn_intercept = { RDPGFX_DVC_CHANNEL_NAME };

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

static UINT8 drdynvc_value_to_cblen(UINT32 value)
{
	if (value <= 0xFF)
		return 0;
	if (value <= 0xFFFF)
		return 1;
	return 2;
}

static BOOL drdynvc_write_variable_uint(wStream* s, UINT32 value, UINT8 cbLen)
{
	UINT32 val = 0;

	switch (cbLen)
	{
		case 0:
			Stream_Write_UINT8(s, (UINT8)val);
			break;

		case 1:
			Stream_Write_UINT16(s, (UINT16)val);
			break;

		default:
			Stream_Write_UINT32(s, val);
			break;
	}

	return TRUE;
}

static BOOL drdynvc_write_header(wStream* s, UINT32 channelId)
{
	const UINT8 cbChId = drdynvc_value_to_cblen(channelId);
	const UINT8 value = (DATA_PDU << 4) | cbChId;
	const size_t len = drdynvc_cblen_to_bytes(cbChId) + 1;

	if (!Stream_EnsureRemainingCapacity(s, len))
		return FALSE;

	Stream_Write_UINT8(s, value);
	return drdynvc_write_variable_uint(s, value, cbChId);
}

static BOOL filter_forward_empty_offer(const char* sessionID, proxyDynChannelInterceptData* data,
                                       size_t startPosition, UINT32 channelId)
{
	WINPR_ASSERT(data);

	Stream_SetPosition(data->data, startPosition);
	if (!drdynvc_write_header(data->data, channelId))
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(data->data, sizeof(UINT16)))
		return FALSE;
	Stream_Write_UINT16(data->data, 0);
	Stream_SealLength(data->data);

	WLog_INFO(TAG, "[SessionID=%s][%s] forwarding empty %s", sessionID, plugin_name,
	          rdpgfx_get_cmd_id_string(RDPGFX_CMDID_CACHEIMPORTOFFER));
	data->rewritten = TRUE;
	return TRUE;
}

static BOOL filter_dyn_channel_intercept(proxyPlugin* plugin, proxyData* pdata, void* arg)
{
	auto data = static_cast<proxyDynChannelInterceptData*>(arg);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(data);

	data->result = PF_CHANNEL_RESULT_PASS;
	if (!data->isBackData &&
	    (strncmp(data->name, RDPGFX_DVC_CHANNEL_NAME, ARRAYSIZE(RDPGFX_DVC_CHANNEL_NAME)) == 0))
	{
		auto state = filter_get_plugin_data(plugin, pdata);
		if (!state)
		{
			WLog_ERR(TAG, "[SessionID=%s][%s] missing custom data, aborting!", pdata->session_id,
			         plugin_name);
			return FALSE;
		}
		const size_t inputDataLength = Stream_Length(data->data);
		UINT16 cmdId = RDPGFX_CMDID_UNUSED_0000;

		const auto pos = Stream_GetPosition(data->data);
		if (!state->skip())
		{
			if (data->first)
			{
				size_t channelId = 0;
				size_t length = 0;
				if (drdynvc_try_read_header(data->data, channelId, length))
				{
					if (Stream_GetRemainingLength(data->data) >= 2)
					{
						Stream_Read_UINT16(data->data, cmdId);
						state->setSkipSize(length);
						state->setDrop(false);
					}
				}

				switch (cmdId)
				{
					case RDPGFX_CMDID_CACHEIMPORTOFFER:
						state->setDrop(true);
						state->setChannelId(channelId);
						break;
					default:
						break;
				}
				Stream_SetPosition(data->data, pos);
			}
		}

		if (state->skip())
		{
			state->skip(inputDataLength);
			if (state->drop())
			{
				WLog_WARN(TAG,
				          "[SessionID=%s][%s] dropping %s packet [total:%" PRIuz ", current:%" PRIuz
				          ", remaining: %" PRIuz "]",
				          pdata->session_id, plugin_name,
				          rdpgfx_get_cmd_id_string(RDPGFX_CMDID_CACHEIMPORTOFFER), state->total(),
				          inputDataLength, state->remaining());
				data->result = PF_CHANNEL_RESULT_DROP;

#if 0 // TODO: Sending this does screw up some windows RDP server versions :/
				if (state->remaining() == 0)
				{
					if (!filter_forward_empty_offer(pdata->session_id, data, pos,
					                                state->channelId()))
						return FALSE;
				}
#endif
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
