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

#include <freerdp/build-config.h>
#include <winpr/collections.h>
#include <stdlib.h>
#include <signal.h>

#define TAG PROXY_TAG("server")

static proxyConfig config = { 0 };

static void cleanup_handler(int signum)
{
	printf("\n");
	WLog_INFO(TAG, "[%s]: caught signal %d, starting cleanup...", __FUNCTION__, signum);

	WLog_INFO(TAG, "freeing loaded modules and plugins.");
	pf_modules_free();

	WLog_INFO(TAG, "freeing config.");
	pf_server_config_free_internal(&config);

	WLog_INFO(TAG, "exiting.");
	exit(signum);
}

int main(int argc, char* argv[])
{
	const char* cfg = "config.ini";
	int status = 0;

	if (argc > 1)
		cfg = argv[1];

	/* Register cleanup handler for graceful termination */
	signal(SIGINT, cleanup_handler);
	signal(SIGTERM, cleanup_handler);
#ifndef _WIN32
	signal(SIGQUIT, cleanup_handler);
	signal(SIGKILL, cleanup_handler);
#endif

	if (!pf_modules_init(FREERDP_PROXY_PLUGINDIR))
	{
		WLog_ERR(TAG, "failed to initialize proxy plugins!");
		goto fail;
	}

	pf_modules_list_loaded_plugins();

	if (!pf_server_config_load(cfg, &config))
		goto fail;

	pf_server_config_print(&config);
	status = pf_server_start(&config);
fail:
	pf_modules_free();
	pf_server_config_free_internal(&config);
	return status;
}
