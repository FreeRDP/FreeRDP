/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server modules API
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#include <assert.h>

#include <winpr/file.h>
#include <winpr/wlog.h>
#include <winpr/library.h>
#include <freerdp/api.h>
#include <freerdp/build-config.h>

#include "pf_log.h"
#include "pf_modules.h"
#include "pf_context.h"

#define TAG PROXY_TAG("modules")

#define MODULE_ENTRY_POINT "proxy_module_entry_point"

static wArrayList* plugins_list = NULL; /* list of all loaded plugins */
static wArrayList* handles_list = NULL; /* list of module handles to free at shutdown */

typedef BOOL (*moduleEntryPoint)(proxyPluginsManager* plugins_manager);

static const char* FILTER_TYPE_STRINGS[] = {
	"KEYBOARD_EVENT",
	"MOUSE_EVENT",
	"CLIENT_CHANNEL_DATA",
	"SERVER_CHANNEL_DATA",
};

static const char* HOOK_TYPE_STRINGS[] = {
	"CLIENT_PRE_CONNECT",   "CLIENT_LOGIN_FAILURE", "SERVER_POST_CONNECT",
	"SERVER_CHANNELS_INIT", "SERVER_CHANNELS_FREE",
};

static const char* pf_modules_get_filter_type_string(PF_FILTER_TYPE result)
{
	if (result >= FILTER_TYPE_KEYBOARD && result < FILTER_LAST)
		return FILTER_TYPE_STRINGS[result];
	else
		return "FILTER_UNKNOWN";
}

static const char* pf_modules_get_hook_type_string(PF_HOOK_TYPE result)
{
	if (result >= HOOK_TYPE_CLIENT_PRE_CONNECT && result < HOOK_LAST)
		return HOOK_TYPE_STRINGS[result];
	else
		return "HOOK_UNKNOWN";
}

/*
 * runs all hooks of type `type`.
 *
 * @type: hook type to run.
 * @server: pointer of server's rdpContext struct of the current session.
 */
BOOL pf_modules_run_hook(PF_HOOK_TYPE type, proxyData* pdata)
{
	BOOL ok = TRUE;
	int index;
	proxyPlugin* plugin;

	ArrayList_ForEach(plugins_list, proxyPlugin*, index, plugin)
	{
		WLog_VRB(TAG, "running hook %s.%s", plugin->name, pf_modules_get_hook_type_string(type));

		switch (type)
		{
			case HOOK_TYPE_CLIENT_PRE_CONNECT:
				IFCALLRET(plugin->ClientPreConnect, ok, pdata);
				break;

			case HOOK_TYPE_CLIENT_LOGIN_FAILURE:
				IFCALLRET(plugin->ClientLoginFailure, ok, pdata);
				break;

			case HOOK_TYPE_SERVER_POST_CONNECT:
				IFCALLRET(plugin->ServerPostConnect, ok, pdata);
				break;

			case HOOK_TYPE_SERVER_CHANNELS_INIT:
				IFCALLRET(plugin->ServerChannelsInit, ok, pdata);
				break;

			case HOOK_TYPE_SERVER_CHANNELS_FREE:
				IFCALLRET(plugin->ServerChannelsFree, ok, pdata);
				break;

			default:
				WLog_ERR(TAG, "invalid hook called");
		}

		if (!ok)
		{
			WLog_INFO(TAG, "plugin %s, hook %s failed!", plugin->name,
			          pf_modules_get_hook_type_string(type));
			return FALSE;
		}
	}

	return TRUE;
}

/*
 * runs all filters of type `type`.
 *
 * @type: filter type to run.
 * @server: pointer of server's rdpContext struct of the current session.
 */
BOOL pf_modules_run_filter(PF_FILTER_TYPE type, proxyData* pdata, void* param)
{
	BOOL result = TRUE;
	int index;
	proxyPlugin* plugin;

	ArrayList_ForEach(plugins_list, proxyPlugin*, index, plugin)
	{
		WLog_VRB(TAG, "[%s]: running filter: %s", __FUNCTION__, plugin->name);

		switch (type)
		{
			case FILTER_TYPE_KEYBOARD:
				IFCALLRET(plugin->KeyboardEvent, result, pdata, param);
				break;

			case FILTER_TYPE_MOUSE:
				IFCALLRET(plugin->MouseEvent, result, pdata, param);
				break;

			case FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_DATA:
				IFCALLRET(plugin->ClientChannelData, result, pdata, param);
				break;

			case FILTER_TYPE_SERVER_PASSTHROUGH_CHANNEL_DATA:
				IFCALLRET(plugin->ServerChannelData, result, pdata, param);
				break;
			default:
				WLog_ERR(TAG, "invalid filter called");
		}

		if (!result)
		{
			/* current filter return FALSE, no need to run other filters. */
			WLog_DBG(TAG, "plugin %s, filter type [%s] returned FALSE", plugin->name,
			         pf_modules_get_filter_type_string(type));
			return result;
		}
	}

	/* all filters returned TRUE */
	return TRUE;
}

/*
 * stores per-session data needed by a plugin.
 *
 * @context: current session server's rdpContext instance.
 * @info: pointer to per-session data.
 */
static BOOL pf_modules_set_plugin_data(const char* plugin_name, proxyData* pdata, void* data)
{
	union {
		const char* ccp;
		char* cp;
	} ccharconv;

	assert(plugin_name);

	ccharconv.ccp = plugin_name;
	if (data == NULL) /* no need to store anything */
		return FALSE;

	if (HashTable_Add(pdata->modules_info, ccharconv.cp, data) < 0)
	{
		WLog_ERR(TAG, "[%s]: HashTable_Add failed!");
		return FALSE;
	}

	return TRUE;
}

/*
 * returns per-session data needed a plugin.
 *
 * @context: current session server's rdpContext instance.
 * if there's no data related to `plugin_name` in `context` (current session), a NULL will be
 * returned.
 */
static void* pf_modules_get_plugin_data(const char* plugin_name, proxyData* pdata)
{
	union {
		const char* ccp;
		char* cp;
	} ccharconv;
	assert(plugin_name);
	assert(pdata);
	ccharconv.ccp = plugin_name;

	return HashTable_GetItemValue(pdata->modules_info, ccharconv.cp);
}

static void pf_modules_abort_connect(proxyData* pdata)
{
	assert(pdata);
	WLog_DBG(TAG, "%s is called!", __FUNCTION__);
	proxy_data_abort_connect(pdata);
}

static BOOL pf_modules_register_plugin(proxyPlugin* plugin_to_register)
{
	int index;
	proxyPlugin* plugin;

	if (!plugin_to_register)
		return FALSE;

	/* make sure there's no other loaded plugin with the same name of `plugin_to_register`. */
	ArrayList_ForEach(plugins_list, proxyPlugin*, index, plugin)
	{
		if (strcmp(plugin->name, plugin_to_register->name) == 0)
		{
			WLog_ERR(TAG, "can not register plugin '%s', it is already registered!", plugin->name);
			return FALSE;
		}
	}

	if (ArrayList_Add(plugins_list, plugin_to_register) < 0)
	{
		WLog_ERR(TAG, "[%s]: failed adding plugin to list: %s", __FUNCTION__,
		         plugin_to_register->name);
		return FALSE;
	}

	return TRUE;
}

BOOL pf_modules_is_plugin_loaded(const char* plugin_name)
{
	int i;
	proxyPlugin* plugin;

	if (plugins_list == NULL)
		return FALSE;

	ArrayList_ForEach(plugins_list, proxyPlugin*, i, plugin)
	{
		if (strcmp(plugin->name, plugin_name) == 0)
			return TRUE;
	}

	return FALSE;
}

void pf_modules_list_loaded_plugins(void)
{
	size_t count;
	int i;
	proxyPlugin* plugin;

	if (plugins_list == NULL)
		return;

	count = (size_t)ArrayList_Count(plugins_list);

	if (count > 0)
		WLog_INFO(TAG, "Loaded plugins:");

	ArrayList_ForEach(plugins_list, proxyPlugin*, i, plugin)
	{

		WLog_INFO(TAG, "\tName: %s", plugin->name);
		WLog_INFO(TAG, "\tDescription: %s", plugin->description);
	}
}

static proxyPluginsManager plugins_manager = { pf_modules_register_plugin,
	                                           pf_modules_set_plugin_data,
	                                           pf_modules_get_plugin_data,
	                                           pf_modules_abort_connect };

static BOOL pf_modules_load_module(const char* module_path)
{
	HMODULE handle = NULL;
	moduleEntryPoint pEntryPoint;
	handle = LoadLibraryA(module_path);

	if (handle == NULL)
	{
		WLog_ERR(TAG, "[%s]: failed loading external library: %s", __FUNCTION__, module_path);
		return FALSE;
	}

	if (!(pEntryPoint = (moduleEntryPoint)GetProcAddress(handle, MODULE_ENTRY_POINT)))
	{
		WLog_ERR(TAG, "[%s]: GetProcAddress failed while loading %s", __FUNCTION__, module_path);
		goto error;
	}

	if (!pEntryPoint(&plugins_manager))
	{
		WLog_ERR(TAG, "[%s]: module %s entry point failed!", __FUNCTION__, module_path);
		goto error;
	}

	/* save module handle for freeing the module later */
	if (ArrayList_Add(handles_list, handle) < 0)
	{
		WLog_ERR(TAG, "ArrayList_Add failed!");
		return FALSE;
	}

	return TRUE;

error:
	FreeLibrary(handle);
	return FALSE;
}

BOOL pf_modules_init(const char* root_dir, const char** modules, size_t count)
{
	size_t i;

	if (!PathFileExistsA(root_dir))
	{
		if (!CreateDirectoryA(root_dir, NULL))
		{
			WLog_ERR(TAG, "error occurred while creating modules directory: %s", root_dir);
			return FALSE;
		}

		return TRUE;
	}

	WLog_DBG(TAG, "modules root directory: %s", root_dir);

	plugins_list = ArrayList_New(FALSE);

	if (plugins_list == NULL)
	{
		WLog_ERR(TAG, "[%s]: ArrayList_New failed!", __FUNCTION__);
		goto error;
	}

	handles_list = ArrayList_New(FALSE);
	if (handles_list == NULL)
	{

		WLog_ERR(TAG, "[%s]: ArrayList_New failed!", __FUNCTION__);
		goto error;
	}

	for (i = 0; i < count; i++)
	{
		char* fullpath = GetCombinedPath(root_dir, modules[i]);
		pf_modules_load_module(fullpath);
		free(fullpath);
	}

	return TRUE;

error:
	ArrayList_Free(plugins_list);
	plugins_list = NULL;
	ArrayList_Free(handles_list);
	handles_list = NULL;
	return FALSE;
}

void pf_modules_free(void)
{
	int index;

	if (plugins_list)
	{
		proxyPlugin* plugin;

		ArrayList_ForEach(plugins_list, proxyPlugin*, index, plugin)
		{
			if (!IFCALLRESULT(TRUE, plugin->PluginUnload))
				WLog_WARN(TAG, "PluginUnload failed for plugin '%s'", plugin->name);
		}

		ArrayList_Free(plugins_list);
		plugins_list = NULL;
	}

	if (handles_list)
	{
		HANDLE handle;

		ArrayList_ForEach(handles_list, HANDLE, index, handle)
		{
			if (handle)
				FreeLibrary(handle);
		};

		ArrayList_Free(handles_list);
		handles_list = NULL;
	}
}
