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

#ifndef FREERDP_SERVER_PROXY_PFCONFIG_H
#define FREERDP_SERVER_PROXY_PFCONFIG_H

#define CONFIG_PARSE_SUCCESS 0
#define CONFIG_PARSE_ERROR 	 1
#define CONFIG_INVALID 		 2

#include <winpr/ini.h>

#include "pf_filters.h"

struct proxy_config
{
	/* server */
	char* Host;
	UINT16 Port;
	BOOL  LocalOnly;

	/* target */
	BOOL UseLoadBalanceInfo;
	char* TargetHost;
	UINT16 TargetPort;

	/* graphics */
	BOOL GFX;
	BOOL BitmapUpdate;

	/* input */
	BOOL Keyboard;
	BOOL Mouse;

	/* security */
	BOOL NlaSecurity;
	BOOL TlsSecurity;
	BOOL RdpSecurity;

	/* channels */
	BOOL WhitelistMode;

	wArrayList* AllowedChannels;
	wArrayList* BlockedChannels;

	/* filters */
	filters_list* Filters;
};

typedef struct proxy_config proxyConfig;

DWORD pf_server_load_config(const char* path, proxyConfig* config);
void pf_server_config_free(proxyConfig* config);

#endif /* FREERDP_SERVER_PROXY_PFCONFIG_H */
