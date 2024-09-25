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
#include <limits>
#include <utility>
#include <vector>
#include <sstream>
#include <string>
#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#if __has_include(<filesystem>)
#include <filesystem>
namespace fs = std::filesystem;
#elif __has_include(<experimental/filesystem>)
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#else
#error Could not find system header "<filesystem>" or "<experimental/filesystem>"
#endif

#include <freerdp/server/proxy/proxy_modules_api.h>
#include <freerdp/server/proxy/proxy_context.h>

#include <freerdp/channels/drdynvc.h>
#include <freerdp/channels/rdpgfx.h>
#include <freerdp/utils/gfx.h>

#define TAG MODULE_TAG("dyn-channel-dump")

static constexpr char plugin_name[] = "dyn-channel-dump";
static constexpr char plugin_desc[] =
    "This plugin dumps configurable dynamic channel data to a file.";

static const std::vector<std::string>& plugin_static_intercept()
{
	static std::vector<std::string> vec;
	if (vec.empty())
		vec.emplace_back(DRDYNVC_SVC_CHANNEL_NAME);
	return vec;
}

static constexpr char key_path[] = "path";
static constexpr char key_channels[] = "channels";

class PluginData
{
  public:
	explicit PluginData(proxyPluginsManager* mgr) : _mgr(mgr)
	{
	}

	[[nodiscard]] proxyPluginsManager* mgr() const
	{
		return _mgr;
	}

	uint64_t session()
	{
		return _sessionid++;
	}

  private:
	proxyPluginsManager* _mgr;
	uint64_t _sessionid{ 0 };
};

class ChannelData
{
  public:
	ChannelData(const std::string& base, std::vector<std::string> list, uint64_t sessionid)
	    : _base(base), _channels_to_dump(std::move(list)), _session_id(sessionid)
	{
		char str[64] = {};
		(void)_snprintf(str, sizeof(str), "session-%016" PRIx64, _session_id);
		_base /= str;
	}

	bool add(const std::string& name, bool back)
	{
		std::lock_guard<std::mutex> guard(_mux);
		if (_map.find(name) == _map.end())
		{
			WLog_INFO(TAG, "adding '%s' to dump list", name.c_str());
			_map.insert({ name, 0 });
		}
		return true;
	}

	std::ofstream stream(const std::string& name, bool back)
	{
		std::lock_guard<std::mutex> guard(_mux);
		auto& atom = _map[name];
		auto count = atom++;
		auto path = filepath(name, back, count);
		WLog_DBG(TAG, "[%s] writing file '%s'", name.c_str(), path.c_str());
		return std::ofstream(path);
	}

	[[nodiscard]] bool dump_enabled(const std::string& name) const
	{
		if (name.empty())
		{
			WLog_WARN(TAG, "empty dynamic channel name, skipping");
			return false;
		}

		auto enabled = std::find(_channels_to_dump.begin(), _channels_to_dump.end(), name) !=
		               _channels_to_dump.end();
		WLog_DBG(TAG, "channel '%s' dumping %s", name.c_str(), enabled ? "enabled" : "disabled");
		return enabled;
	}

	bool ensure_path_exists()
	{
		if (!fs::exists(_base))
		{
			if (!fs::create_directories(_base))
			{
				WLog_ERR(TAG, "Failed to create dump directory %s", _base.c_str());
				return false;
			}
		}
		else if (!fs::is_directory(_base))
		{
			WLog_ERR(TAG, "dump path %s is not a directory", _base.c_str());
			return false;
		}
		return true;
	}

	bool create()
	{
		if (!ensure_path_exists())
			return false;

		if (_channels_to_dump.empty())
		{
			WLog_ERR(TAG, "Empty configuration entry [%s/%s], can not continue", plugin_name,
			         key_channels);
			return false;
		}
		return true;
	}

	[[nodiscard]] uint64_t session() const
	{
		return _session_id;
	}

  private:
	[[nodiscard]] fs::path filepath(const std::string& channel, bool back, uint64_t count) const
	{
		auto name = idstr(channel, back);
		char cstr[32] = {};
		(void)_snprintf(cstr, sizeof(cstr), "%016" PRIx64 "-", count);
		auto path = _base / cstr;
		path += name;
		path += ".dump";
		return path;
	}

	[[nodiscard]] static std::string idstr(const std::string& name, bool back)
	{
		std::stringstream ss;
		ss << name << ".";
		if (back)
			ss << "back";
		else
			ss << "front";
		return ss.str();
	}

	fs::path _base;
	std::vector<std::string> _channels_to_dump;

	std::mutex _mux;
	std::map<std::string, uint64_t> _map;
	uint64_t _session_id;
};

static PluginData* dump_get_plugin_data(proxyPlugin* plugin)
{
	WINPR_ASSERT(plugin);

	auto plugindata = static_cast<PluginData*>(plugin->custom);
	WINPR_ASSERT(plugindata);
	return plugindata;
}

static ChannelData* dump_get_plugin_data(proxyPlugin* plugin, proxyData* pdata)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	auto plugindata = dump_get_plugin_data(plugin);
	WINPR_ASSERT(plugindata);

	auto mgr = plugindata->mgr();
	WINPR_ASSERT(mgr);

	WINPR_ASSERT(mgr->GetPluginData);
	return static_cast<ChannelData*>(mgr->GetPluginData(mgr, plugin_name, pdata));
}

static BOOL dump_set_plugin_data(proxyPlugin* plugin, proxyData* pdata, ChannelData* data)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	auto plugindata = dump_get_plugin_data(plugin);
	WINPR_ASSERT(plugindata);

	auto mgr = plugindata->mgr();
	WINPR_ASSERT(mgr);

	auto cdata = dump_get_plugin_data(plugin, pdata);
	delete cdata;

	WINPR_ASSERT(mgr->SetPluginData);
	return mgr->SetPluginData(mgr, plugin_name, pdata, data);
}

static bool dump_channel_enabled(proxyPlugin* plugin, proxyData* pdata, const std::string& name)
{
	auto config = dump_get_plugin_data(plugin, pdata);
	if (!config)
	{
		WLog_ERR(TAG, "Missing channel data");
		return false;
	}
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
		if (!cdata)
			return FALSE;

		if (!cdata->add(data->name, false))
		{
			WLog_ERR(TAG, "failed to create files for '%s'", data->name);
		}
		if (!cdata->add(data->name, true))
		{
			WLog_ERR(TAG, "failed to create files for '%s'", data->name);
		}
		WLog_INFO(TAG, "Dumping channel '%s'", data->name);
	}
	return TRUE;
}

static BOOL dump_static_channel_intercept_list(proxyPlugin* plugin, proxyData* pdata, void* arg)
{
	auto data = static_cast<proxyChannelToInterceptData*>(arg);

	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);
	WINPR_ASSERT(data);

	auto intercept = std::find(plugin_static_intercept().begin(), plugin_static_intercept().end(),
	                           data->name) != plugin_static_intercept().end();
	if (intercept)
	{
		WLog_INFO(TAG, "intercepting channel '%s'", data->name);
		data->intercept = TRUE;
	}

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
		WLog_DBG(TAG, "intercepting channel '%s'", data->name);
		auto cdata = dump_get_plugin_data(plugin, pdata);
		if (!cdata)
		{
			WLog_ERR(TAG, "Missing channel data");
			return FALSE;
		}

		if (!cdata->ensure_path_exists())
			return FALSE;

		auto stream = cdata->stream(data->name, data->isBackData);
		auto buffer = reinterpret_cast<const char*>(Stream_ConstBuffer(data->data));
		if (!stream.is_open() || !stream.good())
		{
			WLog_ERR(TAG, "Could not write to stream");
			return FALSE;
		}
		const auto s = Stream_Length(data->data);
		if (s > std::numeric_limits<std::streamsize>::max())
		{
			WLog_ERR(TAG, "Stream length %" PRIuz " exceeds std::streamsize::max", s);
			return FALSE;
		}
		stream.write(buffer, static_cast<std::streamsize>(s));
		if (stream.fail())
		{
			WLog_ERR(TAG, "Could not write to stream");
			return FALSE;
		}
		stream.flush();
	}

	return TRUE;
}

static std::vector<std::string> split(const std::string& input, const std::string& regex)
{
	// passing -1 as the submatch index parameter performs splitting
	std::regex re(regex);
	std::sregex_token_iterator first{ input.begin(), input.end(), re, -1 };
	std::sregex_token_iterator last;
	return { first, last };
}

static BOOL dump_session_started(proxyPlugin* plugin, proxyData* pdata, void* /*unused*/)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	auto custom = dump_get_plugin_data(plugin);
	WINPR_ASSERT(custom);

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
	auto cfg = new ChannelData(path, std::move(list), custom->session());
	if (!cfg || !cfg->create())
	{
		delete cfg;
		return FALSE;
	}

	dump_set_plugin_data(plugin, pdata, cfg);

	WLog_DBG(TAG, "starting session dump %" PRIu64, cfg->session());
	return TRUE;
}

static BOOL dump_session_end(proxyPlugin* plugin, proxyData* pdata, void* /*unused*/)
{
	WINPR_ASSERT(plugin);
	WINPR_ASSERT(pdata);

	auto cfg = dump_get_plugin_data(plugin, pdata);
	if (cfg)
		WLog_DBG(TAG, "ending session dump %" PRIu64, cfg->session());
	dump_set_plugin_data(plugin, pdata, nullptr);
	return TRUE;
}

static BOOL dump_unload(proxyPlugin* plugin)
{
	if (!plugin)
		return TRUE;
	delete static_cast<PluginData*>(plugin->custom);
	return TRUE;
}

extern "C" FREERDP_API BOOL proxy_module_entry_point(proxyPluginsManager* plugins_manager,
                                                     void* userdata);

BOOL proxy_module_entry_point(proxyPluginsManager* plugins_manager, void* userdata)
{
	proxyPlugin plugin = {};

	plugin.name = plugin_name;
	plugin.description = plugin_desc;

	plugin.PluginUnload = dump_unload;
	plugin.ServerSessionStarted = dump_session_started;
	plugin.ServerSessionEnd = dump_session_end;

	plugin.StaticChannelToIntercept = dump_static_channel_intercept_list;
	plugin.DynChannelToIntercept = dump_dyn_channel_intercept_list;
	plugin.DynChannelIntercept = dump_dyn_channel_intercept;

	plugin.custom = new PluginData(plugins_manager);
	if (!plugin.custom)
		return FALSE;
	plugin.userdata = userdata;

	return plugins_manager->RegisterPlugin(plugins_manager, &plugin);
}
