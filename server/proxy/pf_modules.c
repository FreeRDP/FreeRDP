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

#include <winpr/wlog.h>
#include <winpr/library.h>
#include <freerdp/api.h>

#include "pf_log.h"
#include "pf_modules.h"
#include "pf_context.h"

#define TAG PROXY_TAG("modules")

#define MODULE_INIT_METHOD "module_init"
#define MODULE_EXIT_METHOD "module_exit"

static modules_list* proxy_modules = NULL;

/* module init/exit methods */
typedef BOOL (*moduleInitFn)(moduleOperations* ops);
typedef BOOL (*moduleExitFn)(moduleOperations* ops);

static const char* FILTER_TYPE_STRINGS[] = {
	"KEYBOARD_EVENT",
	"MOUSE_EVENT",
};

static const char* HOOK_TYPE_STRINGS[] = {
	"CLIENT_PRE_CONNECT",
	"SERVER_CHANNELS_INIT",
	"SERVER_CHANNELS_FREE",
};

static const char* pf_modules_get_filter_type_string(PF_FILTER_TYPE result)
{
	if (result >= FILTER_TYPE_KEYBOARD && result <= FILTER_TYPE_MOUSE)
		return FILTER_TYPE_STRINGS[result];
	else
		return "FILTER_UNKNOWN";
}

static const char* pf_modules_get_hook_type_string(PF_HOOK_TYPE result)
{
	if (result >= HOOK_TYPE_CLIENT_PRE_CONNECT && result <= HOOK_TYPE_SERVER_CHANNELS_FREE)
		return HOOK_TYPE_STRINGS[result];
	else
		return "HOOK_UNKNOWN";
}

BOOL pf_modules_init(void)
{
	proxy_modules = ArrayList_New(FALSE);

	if (proxy_modules == NULL)
	{
		WLog_ERR(TAG, "pf_modules_init(): ArrayList_New failed!");
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
BOOL pf_modules_run_hook(PF_HOOK_TYPE type, rdpContext* context)
{

	proxyModule* module;
	moduleOperations* ops;
	BOOL ok = TRUE;
	const size_t count = (size_t)ArrayList_Count(proxy_modules);
	size_t index;

	for (index = 0; index < count; index++)
	{
		module = (proxyModule*)ArrayList_GetItem(proxy_modules, index);
		ops = module->ops;
		WLog_VRB(TAG, "[%s]: Running module %s, hook %s", __FUNCTION__, module->name,
		         pf_modules_get_hook_type_string(type));

		switch (type)
		{
		case HOOK_TYPE_CLIENT_PRE_CONNECT:
			IFCALLRET(ops->ClientPreConnect, ok, ops, context);
			break;

		case HOOK_TYPE_SERVER_CHANNELS_INIT:
			IFCALLRET(ops->ServerChannelsInit, ok, ops, context);
			break;

		case HOOK_TYPE_SERVER_CHANNELS_FREE:
			IFCALLRET(ops->ServerChannelsFree, ok, ops, context);
			break;
		}

		if (!ok)
		{
			WLog_INFO(TAG, "Module %s, hook %s failed!", module->name,
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
BOOL pf_modules_run_filter(PF_FILTER_TYPE type, rdpContext* server, void* param)
{
	proxyModule* module;
	moduleOperations* ops;
	BOOL result = TRUE;
	const size_t count = (size_t)ArrayList_Count(proxy_modules);
	size_t index;

	for (index = 0; index < count; index++)
	{
		module = (proxyModule*)ArrayList_GetItem(proxy_modules, index);
		ops = module->ops;
		WLog_VRB(TAG, "[%s]: running filter: %s", __FUNCTION__, module->name);

		switch (type)
		{
		case FILTER_TYPE_KEYBOARD:
			IFCALLRET(ops->KeyboardEvent, result, ops, server, param);
			break;

		case FILTER_TYPE_MOUSE:
			IFCALLRET(ops->MouseEvent, result, ops, server, param);
			break;
		}

		if (!result)
		{
			/* current filter return FALSE, no need to run other filters. */
			WLog_INFO(TAG, "module %s, filter type [%s] returned FALSE", module->name,
			          pf_modules_get_filter_type_string(type));
			return result;
		}
	}

	/* all filters returned TRUE */
	return TRUE;
}

static void pf_modules_module_free(proxyModule* module)
{
	moduleExitFn exitFn;

	assert(module);
	assert(module->handle);

	exitFn = (moduleExitFn)GetProcAddress(module->handle, MODULE_EXIT_METHOD);

	if (!exitFn)
	{
		WLog_ERR(TAG, "[%s]: GetProcAddress module_exit for %s failed!", __FUNCTION__,
		         module->name);
	}
	else
	{
		if (!exitFn(module->ops))
		{
			WLog_ERR(TAG, "[%s]: module_exit failed for %s!", __FUNCTION__, module->name);
		}
	}

	FreeLibrary(module->handle);
	module->handle = NULL;

	free(module->name);
	free(module->ops);
	free(module);
}

void pf_modules_free(void)
{
	size_t index, count;

	if (proxy_modules == NULL)
		return;

	count = (size_t)ArrayList_Count(proxy_modules);

	for (index = 0; index < count; index++)
	{
		proxyModule* module = (proxyModule*)ArrayList_GetItem(proxy_modules, index);
		WLog_INFO(TAG, "[%s]: freeing module: %s", __FUNCTION__, module->name);
		pf_modules_module_free(module);
	}

	ArrayList_Free(proxy_modules);
}

/*
 * stores per-session data needed by module.
 *
 * @context: current session server's rdpContext instance.
 * @info: pointer to per-session data.
 */
static BOOL pf_modules_set_session_data(moduleOperations* module, rdpContext* context, void* data)
{
	pServerContext* ps;

	assert(module);
	assert(context);

	if (data == NULL) /* no need to store anything */
		return FALSE;

	ps = (pServerContext*) context;
	if (HashTable_Add(ps->modules_info, (void*)module, data) < 0)
	{
		WLog_ERR(TAG, "[%s]: HashTable_Add failed!");
		return FALSE;
	}

	return TRUE;
}

/*
 * returns per-session data needed by module.
 *
 * @context: current session server's rdpContext instance.
 * if there's no data related to `module` in `context` (current session), a NULL will be returned.
 */
static void* pf_modules_get_session_data(moduleOperations* module, rdpContext* context)
{
	pServerContext* ps;

	assert(module);
	assert(context);

	ps = (pServerContext*)context;
	return HashTable_GetItemValue(ps->modules_info, module);
}

static void pf_modules_abort_connect(moduleOperations* module, rdpContext* context)
{
	pServerContext* ps;

	assert(module);
	assert(context);

	WLog_INFO(TAG, "%s is called!", __FUNCTION__);

	ps = (pServerContext*)context;
	proxy_data_abort_connect(ps->pdata);
}

BOOL pf_modules_register_new(const char* module_path, const char* module_name)
{
	moduleOperations* ops = NULL;
	proxyModule* module = NULL;
	HMODULE handle = NULL;
	moduleInitFn fn;

	assert(proxy_modules != NULL);
	handle = LoadLibraryA(module_path);

	if (handle == NULL)
	{
		WLog_ERR(TAG, "pf_modules_register_new(): failed loading external module: %s", module_path);
		return FALSE;
	}

	if (!(fn = (moduleInitFn)GetProcAddress(handle, MODULE_INIT_METHOD)))
	{
		WLog_ERR(TAG, "pf_modules_register_new(): GetProcAddress failed while loading %s",
		         module_path);
		goto error;
	}

	module = (proxyModule*)calloc(1, sizeof(proxyModule));

	if (module == NULL)
	{
		WLog_ERR(TAG, "pf_modules_register_new(): malloc failed");
		goto error;
	}

	ops = calloc(1, sizeof(moduleOperations));

	if (ops == NULL)
	{
		WLog_ERR(TAG, "pf_modules_register_new(): calloc moduleOperations failed");
		goto error;
	}

	ops->AbortConnect = pf_modules_abort_connect;
	ops->SetSessionData = pf_modules_set_session_data;
	ops->GetSessionData = pf_modules_get_session_data;

	if (!fn(ops))
	{
		WLog_ERR(TAG, "pf_modules_register_new(): failed to initialize module %s", module_path);
		goto error;
	}

	module->name = _strdup(module_name);
	if (!module->name)
	{
		WLog_ERR(TAG, "pf_modules_register_new(): _strdup failed while loading %s", module_path);
		goto error;
	}

	module->handle = handle;
	module->ops = ops;
	module->enabled = TRUE;

	if (ArrayList_Add(proxy_modules, module) < 0)
	{
		WLog_ERR(TAG, "pf_modules_register_new(): failed adding module to list: %s", module_path);
		goto error;
	}

	return TRUE;

error:
	pf_modules_module_free(module);
	return FALSE;
}