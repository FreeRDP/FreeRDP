/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server modules API
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
 * Copyright 2021 Armin Novak <anovak@thincast.com>
 * Copyright 2021 Thincast Technologies GmbH
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

#include <winpr/assert.h>

#include <winpr/file.h>
#include <winpr/wlog.h>
#include <winpr/path.h>
#include <winpr/library.h>
#include <freerdp/api.h>
#include <freerdp/build-config.h>

#include <freerdp/server/proxy/proxy_log.h>
#include <freerdp/server/proxy/proxy_modules_api.h>

#include <freerdp/server/proxy/proxy_context.h>
#include "proxy_modules.h"

#define TAG PROXY_TAG("modules")

#define MODULE_ENTRY_POINT "proxy_module_entry_point"

struct proxy_module
{
	proxyPluginsManager mgr;
	wArrayList* plugins;
	wArrayList* handles;
};

static const char* pf_modules_get_filter_type_string(PF_FILTER_TYPE result)
{
	switch (result)
	{
		case FILTER_TYPE_KEYBOARD:
			return "FILTER_TYPE_KEYBOARD";
		case FILTER_TYPE_UNICODE:
			return "FILTER_TYPE_UNICODE";
		case FILTER_TYPE_MOUSE:
			return "FILTER_TYPE_MOUSE";
		case FILTER_TYPE_MOUSE_EX:
			return "FILTER_TYPE_MOUSE_EX";
		case FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_DATA:
			return "FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_DATA";
		case FILTER_TYPE_SERVER_PASSTHROUGH_CHANNEL_DATA:
			return "FILTER_TYPE_SERVER_PASSTHROUGH_CHANNEL_DATA";
		case FILTER_TYPE_CLIENT_PASSTHROUGH_DYN_CHANNEL_CREATE:
			return "FILTER_TYPE_CLIENT_PASSTHROUGH_DYN_CHANNEL_CREATE";
		case FILTER_TYPE_SERVER_FETCH_TARGET_ADDR:
			return "FILTER_TYPE_SERVER_FETCH_TARGET_ADDR";
		case FILTER_TYPE_SERVER_PEER_LOGON:
			return "FILTER_TYPE_SERVER_PEER_LOGON";
		case FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_CREATE:
			return "FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_CREATE";
		case FILTER_LAST:
			return "FILTER_LAST";
		default:
			return "FILTER_UNKNOWN";
	}
}

static const char* pf_modules_get_hook_type_string(PF_HOOK_TYPE result)
{
	switch (result)
	{
		case HOOK_TYPE_CLIENT_INIT_CONNECT:
			return "HOOK_TYPE_CLIENT_INIT_CONNECT";
		case HOOK_TYPE_CLIENT_UNINIT_CONNECT:
			return "HOOK_TYPE_CLIENT_UNINIT_CONNECT";
		case HOOK_TYPE_CLIENT_PRE_CONNECT:
			return "HOOK_TYPE_CLIENT_PRE_CONNECT";
		case HOOK_TYPE_CLIENT_POST_CONNECT:
			return "HOOK_TYPE_CLIENT_POST_CONNECT";
		case HOOK_TYPE_CLIENT_POST_DISCONNECT:
			return "HOOK_TYPE_CLIENT_POST_DISCONNECT";
		case HOOK_TYPE_CLIENT_REDIRECT:
			return "HOOK_TYPE_CLIENT_REDIRECT";
		case HOOK_TYPE_CLIENT_VERIFY_X509:
			return "HOOK_TYPE_CLIENT_VERIFY_X509";
		case HOOK_TYPE_CLIENT_LOGIN_FAILURE:
			return "HOOK_TYPE_CLIENT_LOGIN_FAILURE";
		case HOOK_TYPE_CLIENT_END_PAINT:
			return "HOOK_TYPE_CLIENT_END_PAINT";
		case HOOK_TYPE_SERVER_POST_CONNECT:
			return "HOOK_TYPE_SERVER_POST_CONNECT";
		case HOOK_TYPE_SERVER_ACTIVATE:
			return "HOOK_TYPE_SERVER_ACTIVATE";
		case HOOK_TYPE_SERVER_CHANNELS_INIT:
			return "HOOK_TYPE_SERVER_CHANNELS_INIT";
		case HOOK_TYPE_SERVER_CHANNELS_FREE:
			return "HOOK_TYPE_SERVER_CHANNELS_FREE";
		case HOOK_TYPE_SERVER_SESSION_END:
			return "HOOK_TYPE_SERVER_SESSION_END";
		case HOOK_TYPE_CLIENT_LOAD_CHANNELS:
			return "HOOK_TYPE_CLIENT_LOAD_CHANNELS";
		case HOOK_TYPE_SERVER_SESSION_INITIALIZE:
			return "HOOK_TYPE_SERVER_SESSION_INITIALIZE";
		case HOOK_TYPE_SERVER_SESSION_STARTED:
			return "HOOK_TYPE_SERVER_SESSION_STARTED";
		case HOOK_LAST:
			return "HOOK_LAST";
		default:
			return "HOOK_TYPE_UNKNOWN";
	}
}

static BOOL pf_modules_proxy_ArrayList_ForEachFkt(void* data, size_t index, va_list ap)
{
	proxyPlugin* plugin = (proxyPlugin*)data;
	PF_HOOK_TYPE type;
	proxyData* pdata;
	void* custom;
	BOOL ok = FALSE;

	WINPR_UNUSED(index);

	type = va_arg(ap, PF_HOOK_TYPE);
	pdata = va_arg(ap, proxyData*);
	custom = va_arg(ap, void*);

	WLog_VRB(TAG, "running hook %s.%s", plugin->name, pf_modules_get_hook_type_string(type));

	switch (type)
	{
		case HOOK_TYPE_CLIENT_INIT_CONNECT:
			ok = IFCALLRESULT(TRUE, plugin->ClientInitConnect, plugin, pdata, custom);
			break;
		case HOOK_TYPE_CLIENT_UNINIT_CONNECT:
			ok = IFCALLRESULT(TRUE, plugin->ClientUninitConnect, plugin, pdata, custom);
			break;
		case HOOK_TYPE_CLIENT_PRE_CONNECT:
			ok = IFCALLRESULT(TRUE, plugin->ClientPreConnect, plugin, pdata, custom);
			break;

		case HOOK_TYPE_CLIENT_POST_CONNECT:
			ok = IFCALLRESULT(TRUE, plugin->ClientPostConnect, plugin, pdata, custom);
			break;

		case HOOK_TYPE_CLIENT_REDIRECT:
			ok = IFCALLRESULT(TRUE, plugin->ClientRedirect, plugin, pdata, custom);
			break;

		case HOOK_TYPE_CLIENT_POST_DISCONNECT:
			ok = IFCALLRESULT(TRUE, plugin->ClientPostDisconnect, plugin, pdata, custom);
			break;

		case HOOK_TYPE_CLIENT_VERIFY_X509:
			ok = IFCALLRESULT(TRUE, plugin->ClientX509Certificate, plugin, pdata, custom);
			break;

		case HOOK_TYPE_CLIENT_LOGIN_FAILURE:
			ok = IFCALLRESULT(TRUE, plugin->ClientLoginFailure, plugin, pdata, custom);
			break;

		case HOOK_TYPE_CLIENT_END_PAINT:
			ok = IFCALLRESULT(TRUE, plugin->ClientEndPaint, plugin, pdata, custom);
			break;

		case HOOK_TYPE_CLIENT_LOAD_CHANNELS:
			ok = IFCALLRESULT(TRUE, plugin->ClientLoadChannels, plugin, pdata, custom);
			break;

		case HOOK_TYPE_SERVER_POST_CONNECT:
			ok = IFCALLRESULT(TRUE, plugin->ServerPostConnect, plugin, pdata, custom);
			break;

		case HOOK_TYPE_SERVER_ACTIVATE:
			ok = IFCALLRESULT(TRUE, plugin->ServerPeerActivate, plugin, pdata, custom);
			break;

		case HOOK_TYPE_SERVER_CHANNELS_INIT:
			ok = IFCALLRESULT(TRUE, plugin->ServerChannelsInit, plugin, pdata, custom);
			break;

		case HOOK_TYPE_SERVER_CHANNELS_FREE:
			ok = IFCALLRESULT(TRUE, plugin->ServerChannelsFree, plugin, pdata, custom);
			break;

		case HOOK_TYPE_SERVER_SESSION_END:
			ok = IFCALLRESULT(TRUE, plugin->ServerSessionEnd, plugin, pdata, custom);
			break;

		case HOOK_TYPE_SERVER_SESSION_INITIALIZE:
			ok = IFCALLRESULT(TRUE, plugin->ServerSessionInitialize, plugin, pdata, custom);
			break;

		case HOOK_TYPE_SERVER_SESSION_STARTED:
			ok = IFCALLRESULT(TRUE, plugin->ServerSessionStarted, plugin, pdata, custom);
			break;

		case HOOK_LAST:
		default:
			WLog_ERR(TAG, "invalid hook called");
	}

	if (!ok)
	{
		WLog_INFO(TAG, "plugin %s, hook %s failed!", plugin->name,
		          pf_modules_get_hook_type_string(type));
		return FALSE;
	}
	return TRUE;
}

/*
 * runs all hooks of type `type`.
 *
 * @type: hook type to run.
 * @server: pointer of server's rdpContext struct of the current session.
 */
BOOL pf_modules_run_hook(proxyModule* module, PF_HOOK_TYPE type, proxyData* pdata, void* custom)
{
	WINPR_ASSERT(module);
	WINPR_ASSERT(module->plugins);
	return ArrayList_ForEach(module->plugins, pf_modules_proxy_ArrayList_ForEachFkt, type, pdata,
	                         custom);
}

static BOOL pf_modules_ArrayList_ForEachFkt(void* data, size_t index, va_list ap)
{
	proxyPlugin* plugin = (proxyPlugin*)data;
	PF_FILTER_TYPE type;
	proxyData* pdata;
	void* param;
	BOOL result = FALSE;

	WINPR_UNUSED(index);

	type = va_arg(ap, PF_FILTER_TYPE);
	pdata = va_arg(ap, proxyData*);
	param = va_arg(ap, void*);

	WLog_VRB(TAG, "[%s]: running filter: %s", __FUNCTION__, plugin->name);

	switch (type)
	{
		case FILTER_TYPE_KEYBOARD:
			result = IFCALLRESULT(TRUE, plugin->KeyboardEvent, plugin, pdata, param);
			break;

		case FILTER_TYPE_UNICODE:
			result = IFCALLRESULT(TRUE, plugin->UnicodeEvent, plugin, pdata, param);
			break;

		case FILTER_TYPE_MOUSE:
			result = IFCALLRESULT(TRUE, plugin->MouseEvent, plugin, pdata, param);
			break;

		case FILTER_TYPE_MOUSE_EX:
			result = IFCALLRESULT(TRUE, plugin->MouseExEvent, plugin, pdata, param);
			break;

		case FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_DATA:
			result = IFCALLRESULT(TRUE, plugin->ClientChannelData, plugin, pdata, param);
			break;

		case FILTER_TYPE_SERVER_PASSTHROUGH_CHANNEL_DATA:
			result = IFCALLRESULT(TRUE, plugin->ServerChannelData, plugin, pdata, param);
			break;

		case FILTER_TYPE_CLIENT_PASSTHROUGH_CHANNEL_CREATE:
			result = IFCALLRESULT(TRUE, plugin->ChannelCreate, plugin, pdata, param);
			break;

		case FILTER_TYPE_CLIENT_PASSTHROUGH_DYN_CHANNEL_CREATE:
			result = IFCALLRESULT(TRUE, plugin->DynamicChannelCreate, plugin, pdata, param);
			break;

		case FILTER_TYPE_SERVER_FETCH_TARGET_ADDR:
			result = IFCALLRESULT(TRUE, plugin->ServerFetchTargetAddr, plugin, pdata, param);
			break;

		case FILTER_TYPE_SERVER_PEER_LOGON:
			result = IFCALLRESULT(TRUE, plugin->ServerPeerLogon, plugin, pdata, param);
			break;

		case FILTER_LAST:
		default:
			WLog_ERR(TAG, "invalid filter called");
	}

	if (!result)
	{
		/* current filter return FALSE, no need to run other filters. */
		WLog_DBG(TAG, "plugin %s, filter type [%s] returned FALSE", plugin->name,
		         pf_modules_get_filter_type_string(type));
	}
	return result;
}

/*
 * runs all filters of type `type`.
 *
 * @type: filter type to run.
 * @server: pointer of server's rdpContext struct of the current session.
 */
BOOL pf_modules_run_filter(proxyModule* module, PF_FILTER_TYPE type, proxyData* pdata, void* param)
{
	WINPR_ASSERT(module);
	WINPR_ASSERT(module->plugins);

	return ArrayList_ForEach(module->plugins, pf_modules_ArrayList_ForEachFkt, type, pdata, param);
}

/*
 * stores per-session data needed by a plugin.
 *
 * @context: current session server's rdpContext instance.
 * @info: pointer to per-session data.
 */
static BOOL pf_modules_set_plugin_data(proxyPluginsManager* mgr, const char* plugin_name,
                                       proxyData* pdata, void* data)
{
	union
	{
		const char* ccp;
		char* cp;
	} ccharconv;

	WINPR_ASSERT(plugin_name);

	ccharconv.ccp = plugin_name;
	if (data == NULL) /* no need to store anything */
		return FALSE;

	if (!HashTable_Insert(pdata->modules_info, ccharconv.cp, data))
	{
		WLog_ERR(TAG, "[%s]: HashTable_Insert failed!");
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
static void* pf_modules_get_plugin_data(proxyPluginsManager* mgr, const char* plugin_name,
                                        proxyData* pdata)
{
	union
	{
		const char* ccp;
		char* cp;
	} ccharconv;
	WINPR_ASSERT(plugin_name);
	WINPR_ASSERT(pdata);
	ccharconv.ccp = plugin_name;

	return HashTable_GetItemValue(pdata->modules_info, ccharconv.cp);
}

static void pf_modules_abort_connect(proxyPluginsManager* mgr, proxyData* pdata)
{
	WINPR_ASSERT(pdata);
	WLog_DBG(TAG, "%s is called!", __FUNCTION__);
	proxy_data_abort_connect(pdata);
}

static BOOL pf_modules_register_ArrayList_ForEachFkt(void* data, size_t index, va_list ap)
{
	proxyPlugin* plugin = (proxyPlugin*)data;
	proxyPlugin* plugin_to_register = va_arg(ap, proxyPlugin*);

	WINPR_UNUSED(index);

	if (strcmp(plugin->name, plugin_to_register->name) == 0)
	{
		WLog_ERR(TAG, "can not register plugin '%s', it is already registered!", plugin->name);
		return FALSE;
	}
	return TRUE;
}

static BOOL pf_modules_register_plugin(proxyPluginsManager* mgr,
                                       const proxyPlugin* plugin_to_register)
{
	proxyPlugin internal = { 0 };
	proxyModule* module = (proxyModule*)mgr;
	WINPR_ASSERT(module);

	if (!plugin_to_register)
		return FALSE;

	internal = *plugin_to_register;
	internal.mgr = mgr;

	/* make sure there's no other loaded plugin with the same name of `plugin_to_register`. */
	if (!ArrayList_ForEach(module->plugins, pf_modules_register_ArrayList_ForEachFkt, &internal))
		return FALSE;

	if (!ArrayList_Append(module->plugins, &internal))
	{
		WLog_ERR(TAG, "[%s]: failed adding plugin to list: %s", __FUNCTION__,
		         plugin_to_register->name);
		return FALSE;
	}

	return TRUE;
}

static BOOL pf_modules_load_ArrayList_ForEachFkt(void* data, size_t index, va_list ap)
{
	proxyPlugin* plugin = (proxyPlugin*)data;
	const char* plugin_name = va_arg(ap, const char*);

	WINPR_UNUSED(index);
	WINPR_UNUSED(ap);

	if (strcmp(plugin->name, plugin_name) == 0)
		return TRUE;
	return FALSE;
}

BOOL pf_modules_is_plugin_loaded(proxyModule* module, const char* plugin_name)
{
	WINPR_ASSERT(module);
	if (ArrayList_Count(module->plugins) < 1)
		return FALSE;
	return ArrayList_ForEach(module->plugins, pf_modules_load_ArrayList_ForEachFkt, plugin_name);
}

static BOOL pf_modules_print_ArrayList_ForEachFkt(void* data, size_t index, va_list ap)
{
	proxyPlugin* plugin = (proxyPlugin*)data;

	WINPR_UNUSED(index);
	WINPR_UNUSED(ap);

	WLog_INFO(TAG, "\tName: %s", plugin->name);
	WLog_INFO(TAG, "\tDescription: %s", plugin->description);
	return TRUE;
}

void pf_modules_list_loaded_plugins(proxyModule* module)
{
	size_t count;

	WINPR_ASSERT(module);
	WINPR_ASSERT(module->plugins);

	count = ArrayList_Count(module->plugins);

	if (count > 0)
		WLog_INFO(TAG, "Loaded plugins:");

	ArrayList_ForEach(module->plugins, pf_modules_print_ArrayList_ForEachFkt);
}

static BOOL pf_modules_load_module(const char* module_path, proxyModule* module, void* userdata)
{
	HMODULE handle = NULL;
	proxyModuleEntryPoint pEntryPoint;
	WINPR_ASSERT(module);

	handle = LoadLibraryX(module_path);

	if (handle == NULL)
	{
		WLog_ERR(TAG, "[%s]: failed loading external library: %s", __FUNCTION__, module_path);
		return FALSE;
	}

	pEntryPoint = (proxyModuleEntryPoint)GetProcAddress(handle, MODULE_ENTRY_POINT);
	if (!pEntryPoint)
	{
		WLog_ERR(TAG, "[%s]: GetProcAddress failed while loading %s", __FUNCTION__, module_path);
		goto error;
	}
	if (!ArrayList_Append(module->handles, handle))
	{
		WLog_ERR(TAG, "ArrayList_Append failed!");
		goto error;
	}
	return pf_modules_add(module, pEntryPoint, userdata);

error:
	FreeLibrary(handle);
	return FALSE;
}

static void free_handle(void* obj)
{
	HANDLE handle = (HANDLE)obj;
	if (handle)
		FreeLibrary(handle);
}

static void free_plugin(void* obj)
{
	proxyPlugin* plugin = (proxyPlugin*)obj;
	WINPR_ASSERT(plugin);

	if (!IFCALLRESULT(TRUE, plugin->PluginUnload, plugin))
		WLog_WARN(TAG, "PluginUnload failed for plugin '%s'", plugin->name);

	free(plugin);
}

static void* new_plugin(const void* obj)
{
	const proxyPlugin* src = obj;
	proxyPlugin* proxy = calloc(1, sizeof(proxyPlugin));
	if (!proxy)
		return NULL;
	*proxy = *src;
	return proxy;
}

proxyModule* pf_modules_new(const char* root_dir, const char** modules, size_t count)
{
	size_t i;
	wObject* obj;
	char* path = NULL;
	proxyModule* module = calloc(1, sizeof(proxyModule));
	if (!module)
		return NULL;

	module->mgr.RegisterPlugin = pf_modules_register_plugin;
	module->mgr.SetPluginData = pf_modules_set_plugin_data;
	module->mgr.GetPluginData = pf_modules_get_plugin_data;
	module->mgr.AbortConnect = pf_modules_abort_connect;
	module->plugins = ArrayList_New(FALSE);

	if (module->plugins == NULL)
	{
		WLog_ERR(TAG, "[%s]: ArrayList_New failed!", __FUNCTION__);
		goto error;
	}
	obj = ArrayList_Object(module->plugins);
	WINPR_ASSERT(obj);

	obj->fnObjectFree = free_plugin;
	obj->fnObjectNew = new_plugin;

	module->handles = ArrayList_New(FALSE);
	if (module->handles == NULL)
	{

		WLog_ERR(TAG, "[%s]: ArrayList_New failed!", __FUNCTION__);
		goto error;
	}
	ArrayList_Object(module->handles)->fnObjectFree = free_handle;

	if (count > 0)
	{
		WINPR_ASSERT(root_dir);
		if (!winpr_PathFileExists(root_dir))
			path = GetCombinedPath(FREERDP_INSTALL_PREFIX, root_dir);
		else
			path = _strdup(root_dir);

		if (!winpr_PathFileExists(path))
		{
			if (!winpr_PathMakePath(path, NULL))
			{
				WLog_ERR(TAG, "error occurred while creating modules directory: %s", root_dir);
				goto error;
			}
		}

		if (winpr_PathFileExists(path))
			WLog_DBG(TAG, "modules root directory: %s", path);

		for (i = 0; i < count; i++)
		{
			char name[8192] = { 0 };
			char* fullpath;
			_snprintf(name, sizeof(name), "proxy-%s-plugin%s", modules[i],
			          FREERDP_SHARED_LIBRARY_SUFFIX);
			fullpath = GetCombinedPath(path, name);
			pf_modules_load_module(fullpath, module, NULL);
			free(fullpath);
		}
	}

	free(path);
	return module;

error:
	free(path);
	pf_modules_free(module);
	return NULL;
}

void pf_modules_free(proxyModule* module)
{
	if (!module)
		return;

	ArrayList_Free(module->plugins);
	ArrayList_Free(module->handles);
	free(module);
}

BOOL pf_modules_add(proxyModule* module, proxyModuleEntryPoint ep, void* userdata)
{
	WINPR_ASSERT(module);
	WINPR_ASSERT(ep);

	return ep(&module->mgr, userdata);
}
