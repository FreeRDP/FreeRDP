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

#include <fstream>
#include <iostream>
#include <regex>
#include <vector>
#include <sstream>
#include <string>
#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <filesystem>

#include <freerdp/server/proxy/proxy_modules_api.h>
#include <freerdp/server/proxy/proxy_context.h>

#include <freerdp/channels/drdynvc.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/utils/gfx.h>

#define TAG MODULE_TAG("dyn-channel-dump")

static constexpr char plugin_name[] = "dyn-channel-dump";
static constexpr char plugin_desc[] =
    "This plugin dumps configurable dynamic channel data to a file.";

static const std::vector<std::string> plugin_static_intercept = { DRDYNVC_SVC_CHANNEL_NAME };

static constexpr char key_path[] = "path";
static constexpr char key_channels[] = "channels";

class ChannelData
{
  public:
	ChannelData(const std::string& base, const std::vector<std::string>& list)
	    : _base(base), _channels_to_dump(list)
	{
	}

	bool add(const std::string& name, bool back)
	{
		auto id = idstr(name, back);
		if (_map.find(id) != _map.end())
		{
			WLog_ERR(TAG, "Duplicate stream dump entry '%s'", id.c_str());
			return false;
		}

		auto path = filepath(name, back);

		auto s = std::ofstream(path);
		if (!s.is_open() || !s.good())
		{
			WLog_ERR(TAG, "Failed to create stream dump entry '%s'", id.c_str());
			return false;
		}
		_map.insert({ id, std::move(s) });
		return true;
	}

	std::ofstream& stream(const std::string& name, bool back)
	{
		return _map[idstr(name, back)];
	}

	bool dump_enabled(const std::string& name) const
	{
		if (name.empty())
		{
			WLog_WARN(TAG, "empty dynamic channel name, skipping");
			return false;
		}

		return std::find(_channels_to_dump.begin(), _channels_to_dump.end(), name) !=
		       _channels_to_dump.end();
	}

	std::filesystem::path filepath(const std::string& channel, bool back) const
	{
		auto name = idstr(channel, back);
		auto path = _base / name;
		path += ".dump";
		return path;
	}

  private:
	std::string idstr(const std::string& name, bool back) const
	{
		std::stringstream ss;
		ss << name << ".";
		if (back)
			ss << "back";
		else
			ss << "front";
		return ss.str();
	}

  private:
	std::filesystem::path _base;
	std::vector<std::string> _channels_to_dump;
	std::map<std::string, std::ofstream> _map;
};

static ChannelData* dump_get_plugin_data(proxyPlugin* plugin, proxyData* pdata)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	auto mgr = static_cast<proxyPluginsManager*>(plugin->custom);
	WINPR_ASSERT(mgr);

	WINPR_ASSERT(mgr->GetPluginData);
	return static_cast<ChannelData*>(mgr->GetPluginData(mgr, plugin_name, pdata));
}

static BOOL dump_set_plugin_data(proxyPlugin* plugin, proxyData* pdata, ChannelData* data)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	auto mgr = static_cast<proxyPluginsManager*>(plugin->custom);
	WINPR_ASSERT(mgr);

	auto cdata = dump_get_plugin_data(plugin, pdata);
	delete cdata;

	WINPR_ASSERT(mgr->SetPluginData);
	return mgr->SetPluginData(mgr, plugin_name, pdata, data);
}

static bool dump_channel_enabled(proxyPlugin* plugin, proxyData* pdata, const std::string& name)
{
	auto config = dump_get_plugin_data(plugin, pdata);
	WINPR_ASSERT(config);
	return config->dump_enabled(name);
}

static BOOL dump_dyn_channel_intercept_list(proxyPlugin* plugin, proxyData* pdata, void* arg)
{
	auto data = static_cast<proxyChannelToInterceptData*>(arg);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(data);

	data->intercept = dump_channel_enabled(plugin, pdata, data->name);
	if (data->intercept)
	{
		auto cdata = dump_get_plugin_data(plugin, pdata);
		auto front = cdata->filepath(data->name, false);
		auto back = cdata->filepath(data->name, true);

		if (!cdata->add(data->name, false))
		{
			WLog_ERR(TAG, "failed to create file '%s'", front.c_str());
		}
		if (!cdata->add(data->name, true))
		{
			WLog_ERR(TAG, "failed to create file '%s'", back.c_str());
		}
		WLog_INFO(TAG, "Dumping channel '%s' to '%s' and '%s'", data->name, front.c_str(),
		          back.c_str());
	}
	return TRUE;
}

static BOOL dump_static_channel_intercept_list(proxyPlugin* plugin, proxyData* pdata, void* arg)
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

static BOOL dump_dyn_channel_intercept(proxyPlugin* plugin, proxyData* pdata, void* arg)
{
	auto data = static_cast<proxyDynChannelInterceptData*>(arg);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(data);

	data->result = PF_CHANNEL_RESULT_PASS;
	if (dump_channel_enabled(plugin, pdata, data->name))
	{
		auto cdata = dump_get_plugin_data(plugin, pdata);
		auto& stream = cdata->stream(data->name, data->isBackData);
		auto buffer = reinterpret_cast<const char*>(Stream_ConstBuffer(data->data));
		if (!stream.is_open() || !stream.good())
		{
			WLog_ERR(TAG, "Could not write to stream");
			return FALSE;
		}
		stream.write(buffer, Stream_Length(data->data));
		if (stream.fail())
		{
			WLog_ERR(TAG, "Could not write to stream");
			return FALSE;
		}
	}

	return TRUE;
}

static std::vector<std::string> split(const std::string& input, const std::string& regex)
{
	// passing -1 as the submatch index parameter performs splitting
	std::regex re(regex);
	std::sregex_token_iterator first{ input.begin(), input.end(), re, -1 }, last;
	return { first, last };
}

static BOOL dump_session_started(proxyPlugin* plugin, proxyData* pdata, void*)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	auto config = pdata->config;
	WINPR_ASSERT(config);

	auto cpath = pf_config_get(config, plugin_name, key_path);
	if (!cpath)
	{
		WLog_ERR(TAG, "Missing configuration entry [%s/%s], can not continue", plugin_name,
		         key_path);
		return FALSE;
	}
	auto cchannels = pf_config_get(config, plugin_name, key_channels);
	if (!cchannels)
	{
		WLog_ERR(TAG, "Missing configuration entry [%s/%s], can not continue", plugin_name,
		         key_channels);
		return FALSE;
	}

	std::string path(cpath);
	std::string channels(cchannels);
	std::vector<std::string> list = split(channels, "[;,]");

	if (!std::filesystem::exists(path))
	{
		if (!std::filesystem::create_directories(path))
		{
			WLog_ERR(TAG, "Failed to create dump directory %s", path.c_str());
			return FALSE;
		}
	}
	else if (!std::filesystem::is_directory(path))
	{
		WLog_ERR(TAG, "dump path %s is not a directory", path.c_str());
		return FALSE;
	}

	if (list.empty())
	{
		WLog_ERR(TAG, "Empty configuration entry [%s/%s], can not continue", plugin_name,
		         key_channels);
		return FALSE;
	}
	dump_set_plugin_data(plugin, pdata, new ChannelData(path, list));

	return TRUE;
}

static BOOL dump_session_end(proxyPlugin* plugin, proxyData* pdata, void*)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	dump_set_plugin_data(plugin, pdata, nullptr);
	return TRUE;
}

extern "C" FREERDP_API BOOL proxy_module_entry_point(proxyPluginsManager* plugins_manager,
                                                     void* userdata);

BOOL proxy_module_entry_point(proxyPluginsManager* plugins_manager, void* userdata)
{
	proxyPlugin plugin = {};

	plugin.name = plugin_name;
	plugin.description = plugin_desc;

	plugin.ServerSessionStarted = dump_session_started;
	plugin.ServerSessionEnd = dump_session_end;

	plugin.StaticChannelToIntercept = dump_static_channel_intercept_list;
	plugin.DynChannelToIntercept = dump_dyn_channel_intercept_list;
	plugin.DynChannelIntercept = dump_dyn_channel_intercept;

	plugin.custom = plugins_manager;
	if (!plugin.custom)
		return FALSE;
	plugin.userdata = userdata;

	return plugins_manager->RegisterPlugin(plugins_manager, &plugin);
}
