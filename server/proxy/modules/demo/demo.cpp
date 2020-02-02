/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server Demo C++ Module
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include "modules_api.h"

#define TAG MODULE_TAG("demo")

static constexpr char plugin_name[] = "demo";
static constexpr char plugin_desc[] = "this is a test plugin";

static proxyPluginsManager* g_plugins_manager = NULL;

static BOOL demo_filter_keyboard_event(proxyData* pdata, void* param)
{
	auto event_data = static_cast<proxyKeyboardEventInfo*>(param);
	if (event_data == NULL)
		return FALSE;

	if (event_data->rdp_scan_code == RDP_SCANCODE_KEY_B)
	{
		/* user typed 'B', that means bye :) */
		std::cout << "C++ demo plugin: aborting connection" << std::endl;
		g_plugins_manager->AbortConnect(pdata);
	}

	return TRUE;
}

static BOOL demo_plugin_unload()
{
	std::cout << "C++ demo plugin: unloading..." << std::endl;
	return TRUE;
}

static proxyPlugin demo_plugin = {
	plugin_name,                /* name */
	plugin_desc,                /* description */
	demo_plugin_unload,         /* PluginUnload */
	NULL,                       /* ClientPreConnect */
	NULL,                       /* ClientLoginFailure */
	NULL,                       /* ServerPostConnect */
	NULL,                       /* ServerChannelsInit */
	NULL,                       /* ServerChannelsFree */
	demo_filter_keyboard_event, /* KeyboardEvent */
	NULL,                       /* MouseEvent */
	NULL,                       /* ClientChannelData */
	NULL,                       /* ServerChannelData */
};

BOOL proxy_module_entry_point(proxyPluginsManager* plugins_manager)
{
	g_plugins_manager = plugins_manager;

	return plugins_manager->RegisterPlugin(&demo_plugin);
}
