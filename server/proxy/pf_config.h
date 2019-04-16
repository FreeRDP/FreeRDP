/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2019 Mati Shabtay <matishabtay@gmail.com>
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include <winpr/ini.h>

struct proxy_config
{
	/* server */
	char* Host;
	UINT16 Port;
	BOOL  LocalOnly;

	/* graphics */
	BOOL GFX;
	BOOL BitmapUpdate;

	/* input */
	BOOL Keyboard;
	BOOL Mouse;

	/* security */
	BOOL NlaSupport;
	BOOL TlsSupport;
	BOOL RdpSupport;

	/* channels */
	BOOL WhitelistMode;

	char** AllowedChannels;
	UINT32 AllowedChannelsCount;

	char** BlockedChannels;
	UINT32 BlockedChannelsCount;
};

typedef struct proxy_config proxyConfig;

BOOL pf_server_load_config(char* path, proxyConfig* config);
void pf_server_config_free(proxyConfig* config);

#endif /* FREERDP_SERVER_PROXY_PFCONFIG_H */
