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

#define TAG PROXY_TAG("server")

int main(int argc, char* argv[])
{
	const char* cfg = "config.ini";
	int status = 0;
	DWORD ld;
	UINT32 i;
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

		for (i = 0; i < config->AllowedChannelsCount; i++)
			WLog_INFO(TAG, "Allowing %s", config->AllowedChannels[i]);
	}
	else
	{
		WLog_INFO(TAG, "Channels mode: BLACKLIST");

		for (i = 0; i < config->BlockedChannelsCount; i++)
			WLog_INFO(TAG, "Blocking %s", config->BlockedChannels[i]);
	}

	status = pf_server_start(config);
fail:
	pf_server_config_free(config);
	return status;
}
