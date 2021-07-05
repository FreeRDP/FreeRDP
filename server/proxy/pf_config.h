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

#include <freerdp/api.h>
#include <winpr/ini.h>
#include <winpr/path.h>

typedef struct proxy_config proxyConfig;

struct proxy_config
{
	/* server */
	char* Host;
	UINT16 Port;

	/* target */
	BOOL FixedTarget;
	char* TargetHost;
	UINT16 TargetPort;

	/* input */
	BOOL Keyboard;
	BOOL Mouse;

	/* server security */
	BOOL ServerTlsSecurity;
	BOOL ServerRdpSecurity;

	/* client security */
	BOOL ClientNlaSecurity;
	BOOL ClientTlsSecurity;
	BOOL ClientRdpSecurity;
	BOOL ClientAllowFallbackToTls;

	/* channels */
	BOOL GFX;
	BOOL DisplayControl;
	BOOL Clipboard;
	BOOL AudioOutput;
	BOOL RemoteApp;
	char** Passthrough;
	size_t PassthroughCount;

	/* clipboard specific settings */
	BOOL TextOnly;
	UINT32 MaxTextLength;

	/* gfx settings */
	BOOL DecodeGFX;

	/* modules */
	char** Modules; /* module file names to load */
	size_t ModulesCount;

	char** RequiredPlugins; /* required plugin names */
	size_t RequiredPluginsCount;
};

typedef struct proxy_config proxyConfig;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API BOOL pf_config_get_uint16(wIniFile* ini, const char* section, const char* key,
	                                      UINT16* result);
	FREERDP_API BOOL pf_config_get_uint32(wIniFile* ini, const char* section, const char* key,
	                                      UINT32* result);
	FREERDP_API BOOL pf_config_get_bool(wIniFile* ini, const char* section, const char* key);
	FREERDP_API const char* pf_config_get_str(wIniFile* ini, const char* section, const char* key);

#ifdef __cplusplus
};
#endif

proxyConfig* pf_server_config_load_file(const char* path);
proxyConfig* pf_server_config_load_buffer(const char* buffer);
void pf_server_config_print(proxyConfig* config);
void pf_server_config_free(proxyConfig* config);

#endif /* FREERDP_SERVER_PROXY_PFCONFIG_H */
