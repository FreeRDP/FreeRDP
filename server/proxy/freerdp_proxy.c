/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
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

#include "pf_server.h"
#include "pf_config.h"
#include "pf_log.h"
#include "pf_modules.h"

#include <freerdp/version.h>
#include <freerdp/build-config.h>
#include <winpr/collections.h>
#include <stdlib.h>
#include <signal.h>

#define TAG PROXY_TAG("server")

static proxyServer* server = NULL;

static WINPR_NORETURN(void cleanup_handler(int signum))
{
	printf("\n");
	WLog_INFO(TAG, "[%s]: caught signal %d, starting cleanup...", __FUNCTION__, signum);

	WLog_INFO(TAG, "stopping all connections.");
	pf_server_stop(server);

	WLog_INFO(TAG, "freeing loaded modules and plugins.");
	pf_modules_free();

	pf_server_config_free(server->config);
	pf_server_free(server);

	WLog_INFO(TAG, "exiting.");
	exit(0);
}

static void pf_server_register_signal_handlers(void)
{
	signal(SIGINT, cleanup_handler);
	signal(SIGTERM, cleanup_handler);
#ifndef _WIN32
	signal(SIGQUIT, cleanup_handler);
	signal(SIGKILL, cleanup_handler);
#endif
}

static BOOL is_all_required_modules_loaded(proxyConfig* config)
{
	size_t i;

	for (i = 0; i < config->RequiredPluginsCount; i++)
	{
		const char* plugin_name = config->RequiredPlugins[i];

		if (!pf_modules_is_plugin_loaded(plugin_name))
		{
			WLog_ERR(TAG, "Required plugin '%s' is not loaded. stopping.", plugin_name);
			return FALSE;
		}
	}

	return TRUE;
}

int main(int argc, char* argv[])
{
	proxyConfig* config = NULL;
	char* config_path = "config.ini";
	int status = -1;

	WLog_INFO(TAG, "freerdp-proxy version info:");
	WLog_INFO(TAG, "\tFreeRDP version: %s", FREERDP_VERSION_FULL);
	WLog_INFO(TAG, "\tGit commit: %s", FREERDP_GIT_REVISION);
	WLog_DBG(TAG, "\tBuild config: %s", freerdp_get_build_config());

	if (argc >= 2)
		config_path = argv[1];

	config = pf_server_config_load_file(config_path);
	if (!config)
		goto fail;

	pf_server_config_print(config);

	if (!pf_modules_init(FREERDP_PROXY_PLUGINDIR, (const char**)config->Modules,
	                     config->ModulesCount))
	{
		WLog_ERR(TAG, "failed to initialize proxy modules!");
		goto fail;
	}

	pf_modules_list_loaded_plugins();
	if (!is_all_required_modules_loaded(config))
		goto fail;

	pf_server_register_signal_handlers();

	server = pf_server_new(config);
	if (!server)
		goto fail;

	if (!pf_server_start(server))
		goto fail;

	if (WaitForSingleObject(server->thread, INFINITE) != WAIT_OBJECT_0)
		goto fail;

	status = 0;
fail:
	pf_server_free(server);
	pf_modules_free();
	pf_server_config_free(config);
	return status;
}
