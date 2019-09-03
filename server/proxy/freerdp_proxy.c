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

#include <winpr/collections.h>

#define TAG PROXY_TAG("server")


int main(int argc, char* argv[])
{
	const char* cfg = "config.ini";
	int status = 0;
	proxyConfig* config = calloc(1, sizeof(proxyConfig));

	if (!config)
		return -1;

	if (argc > 1)
		cfg = argv[1];

	if (!pf_modules_init())
		goto fail;

	if (!pf_server_config_load(cfg, config))
		goto fail;

	pf_server_config_print(config);
	status = pf_server_start(config);
fail:
	pf_modules_free();
	pf_server_config_free(config);
	return status;
}
