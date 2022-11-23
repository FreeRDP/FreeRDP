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

#include <winpr/collections.h>

#include <freerdp/version.h>
#include <freerdp/freerdp.h>

#include <freerdp/server/proxy/proxy_server.h>
#include <freerdp/server/proxy/proxy_log.h>

#include <stdlib.h>
#include <signal.h>

#define TAG PROXY_TAG("server")

static proxyServer* server = NULL;

#if defined(_WIN32)
static const char* strsignal(int signum)
{
	switch (signum)
	{
		case SIGINT:
			return "SIGINT";
		case SIGTERM:
			return "SIGTERM";
		default:
			return "UNKNOWN";
	}
}
#endif

static void cleanup_handler(int signum)
{
	printf("\n");
	WLog_INFO(TAG, "[%s]: caught signal %s [%d], starting cleanup...", __FUNCTION__,
	          strsignal(signum), signum);

	WLog_INFO(TAG, "stopping all connections.");
	pf_server_stop(server);
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

static WINPR_NORETURN(void usage(const char* app))
{
	printf("Usage:\n");
	printf("%s -h                               Display this help text.\n", app);
	printf("%s --help                           Display this help text.\n", app);
	printf("%s --buildconfig                    Print the build configuration.\n", app);
	printf("%s <config ini file>                Start the proxy with <config.ini>\n", app);
	printf("%s --dump-config <config ini file>  Create a template <config.ini>\n", app);
	exit(0);
}

static WINPR_NORETURN(void buildconfig(const char* app))
{
	printf("This is FreeRDP version %s (%s)\n", FREERDP_VERSION_FULL, FREERDP_GIT_REVISION);
	printf("%s", freerdp_get_build_config());
	exit(0);
}

int main(int argc, char* argv[])
{
	proxyConfig* config = NULL;
	char* config_path = "config.ini";
	int status = -1;

	pf_server_register_signal_handlers();

	WLog_INFO(TAG, "freerdp-proxy version info:");
	WLog_INFO(TAG, "\tFreeRDP version: %s", FREERDP_VERSION_FULL);
	WLog_INFO(TAG, "\tGit commit: %s", FREERDP_GIT_REVISION);
	WLog_DBG(TAG, "\tBuild config: %s", freerdp_get_build_config());

	if (argc < 2)
		usage(argv[0]);

	{
		const char* arg = argv[1];

		if (_stricmp(arg, "-h") == 0)
			usage(argv[0]);
		else if (_stricmp(arg, "--help") == 0)
			usage(argv[0]);
		else if (_stricmp(arg, "--buildconfig") == 0)
			buildconfig(argv[0]);
		else if (_stricmp(arg, "--dump-config") == 0)
		{
			if (argc <= 2)
				usage(argv[0]);
			pf_server_config_dump(argv[2]);
			status = 0;
			goto fail;
		}
		config_path = argv[1];
	}

	config = pf_server_config_load_file(config_path);
	if (!config)
		goto fail;

	pf_server_config_print(config);

	server = pf_server_new(config);
	pf_server_config_free(config);

	if (!server)
		goto fail;

	if (!pf_server_start(server))
		goto fail;

	if (!pf_server_run(server))
		goto fail;

	status = 0;

fail:
	pf_server_free(server);

	return status;
}
