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
#include "pf_filters.h"

#include <winpr/collections.h>

#define TAG PROXY_TAG("server")

int main(int argc, char* argv[])
{
	const char* cfg = "config.ini";
	int status = 0;
	DWORD ld;
	UINT32 i;
	UINT32 count;
	proxyConfig* config = calloc(1, sizeof(proxyConfig));

	if (!config)
		return -1;

	if (argc > 1)
		cfg = argv[1];

	ld = pf_server_load_config(cfg, config);

	switch (ld)
	{
		case CONFIG_PARSE_SUCCESS:
			WLog_DBG(TAG, "Configuration parsed successfully");
			break;

		case CONFIG_PARSE_ERROR:
			WLog_ERR(TAG, "An error occured while parsing configuration file, exiting...");
			goto fail;

		case CONFIG_INVALID:
			goto fail;
	}

	if (config->WhitelistMode)
	{
		WLog_INFO(TAG, "Channels mode: WHITELIST");
		count = ArrayList_Count(config->AllowedChannels);

		for (i = 0; i < count; i++)
			WLog_INFO(TAG, "Allowing %s", (char*) ArrayList_GetItem(config->AllowedChannels, i));
	}
	else
	{
		WLog_INFO(TAG, "Channels mode: BLACKLIST");
		count = ArrayList_Count(config->BlockedChannels);

		for (i = 0; i < count; i++)
			WLog_INFO(TAG, "Blocking %s", (char*) ArrayList_GetItem(config->BlockedChannels, i));
	}

	status = pf_server_start(config);
fail:
	pf_server_config_free(config);
	return status;
}
