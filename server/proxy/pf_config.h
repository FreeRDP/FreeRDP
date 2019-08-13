/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
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

#ifndef FREERDP_SERVER_PROXY_PFCONFIG_H
#define FREERDP_SERVER_PROXY_PFCONFIG_H

#include <winpr/ini.h>

#define CONFIG_GET_STR(ini, section, key) IniFile_GetKeyValueString(ini, section, key)
#define CONFIG_GET_BOOL(ini, section, key) IniFile_GetKeyValueInt(ini, section, key)

typedef struct proxy_config proxyConfig;

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

	/* input */
	BOOL Keyboard;
	BOOL Mouse;

	/* security */
	BOOL NlaSecurity;
	BOOL TlsSecurity;
	BOOL RdpSecurity;

	/* channels */
	BOOL GFX;
	BOOL DisplayControl;
	BOOL Clipboard;
	BOOL AudioOutput;

	/* clipboard specific settings*/
	BOOL TextOnly;
	UINT32 MaxTextLength;
};

typedef struct proxy_config proxyConfig;

BOOL pf_server_config_load(const char* path, proxyConfig* config);
void pf_server_config_print(proxyConfig* config);
void pf_server_config_free(proxyConfig* config);

#endif /* FREERDP_SERVER_PROXY_PFCONFIG_H */
