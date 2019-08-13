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

#include <stdio.h>
#include <string.h>
#include <winpr/crt.h>
#include <winpr/collections.h>

#include "pf_log.h"
#include "pf_server.h"
#include "pf_config.h"
#include "pf_modules.h"

#define TAG PROXY_TAG("config")

#define CONFIG_PRINT_SECTION(section) WLog_INFO(TAG, "\t%s:", section)
#define CONFIG_PRINT_STR(config, key) WLog_INFO(TAG, "\t\t%s: %s", #key, config->key)
#define CONFIG_PRINT_BOOL(config, key) WLog_INFO(TAG, "\t\t%s: %s", #key, config->key ? "TRUE" : "FALSE")
#define CONFIG_PRINT_UINT16(config, key) WLog_INFO(TAG, "\t\t%s: %"PRIu16"", #key, config->key);
#define CONFIG_PRINT_UINT32(config, key) WLog_INFO(TAG, "\t\t%s: %"PRIu32"", #key, config->key);

#define CONFIG_GET_STR(ini, section, key) IniFile_GetKeyValueString(ini, section, key)
#define CONFIG_GET_BOOL(ini, section, key) IniFile_GetKeyValueInt(ini, section, key)

static BOOL pf_config_get_uint16(wIniFile* ini, const char* section, const char* key, UINT16* result)
{
	int val;

	val = IniFile_GetKeyValueInt(ini, section, key);
	if ((val < 0) || (val > UINT16_MAX))
	{
		WLog_ERR(TAG, "pf_config_get_uint16(): invalid value %d for section '%s', key '%s'!", val, section, key);
		return FALSE;
	}

	*result = (UINT16) val;
	return TRUE;
}

static BOOL pf_config_get_uint32(wIniFile* ini, const char* section, const char* key, UINT32* result)
{
	int val;

	val = IniFile_GetKeyValueInt(ini, section, key);
	if ((val < 0) || (val > UINT32_MAX))
	{
		WLog_ERR(TAG, "pf_config_get_uint32(): invalid value %d for section '%s', key '%s'!", val, section, key);
		return FALSE;
	}

	*result = (UINT32) val;
	return TRUE;
}

static BOOL pf_config_load_server(wIniFile* ini, proxyConfig* config)
{
	config->Host = _strdup(CONFIG_GET_STR(ini, "Server", "Host"));
	config->LocalOnly = CONFIG_GET_BOOL(ini, "Server", "LocalOnly");
	
	if (!pf_config_get_uint16(ini, "Server", "Port", &config->Port))
		return FALSE;

	return TRUE;
}

static BOOL pf_config_load_target(wIniFile* ini, proxyConfig* config)
{
	config->TargetHost = _strdup(CONFIG_GET_STR(ini, "Target", "Host"));
	config->UseLoadBalanceInfo = CONFIG_GET_BOOL(ini, "Target", "UseLoadBalanceInfo");

	if (pf_config_get_uint16(ini, "Target", "Port", &config->TargetPort))
		return TRUE;

	return FALSE;
}

static BOOL pf_config_load_channels(wIniFile* ini, proxyConfig* config)
{
	config->GFX = CONFIG_GET_BOOL(ini, "Channels", "GFX");
	config->DisplayControl = CONFIG_GET_BOOL(ini, "Channels", "DisplayControl");
	config->Clipboard = CONFIG_GET_BOOL(ini, "Channels", "Clipboard");
	config->AudioOutput = CONFIG_GET_BOOL(ini, "Channels", "AudioOutput");
	return TRUE;
}

static BOOL pf_config_load_input(wIniFile* ini, proxyConfig* config)
{
	config->Keyboard = CONFIG_GET_BOOL(ini, "Input", "Keyboard");
	config->Mouse = CONFIG_GET_BOOL(ini, "Input", "Mouse");
	return TRUE;
}

static BOOL pf_config_load_security(wIniFile* ini, proxyConfig* config)
{
	config->TlsSecurity = CONFIG_GET_BOOL(ini, "Security", "TlsSecurity");
	config->NlaSecurity = CONFIG_GET_BOOL(ini, "Security", "NlaSecurity");
	config->RdpSecurity = CONFIG_GET_BOOL(ini, "Security", "RdpSecurity");
	return TRUE;
}

static BOOL pf_config_load_clipboard(wIniFile* ini, proxyConfig* config)
{
	config->TextOnly = CONFIG_GET_BOOL(ini, "Clipboard", "TextOnly");

	if (!pf_config_get_uint32(ini, "Clipboard", "MaxTextLength", &config->MaxTextLength))
		return FALSE;

	return TRUE;
}

static BOOL pf_config_load_modules(wIniFile* ini, proxyConfig* config)
{
	UINT32 index;
	int modules_count = 0;
	char** module_names;

	module_names = IniFile_GetSectionKeyNames(ini, "Modules", &modules_count);

	for (index = 0; index < modules_count; index++)
	{
		char* module_name = module_names[index];
		const char* path = CONFIG_GET_STR(ini, "Modules", module_name);

		if (!pf_modules_register_new(path, module_name))
		{
			WLog_DBG(TAG, "pf_config_load_modules(): failed to register %s (%s)", module_name, path);
			continue;
		}

		WLog_INFO(TAG, "module '%s' is loaded!", module_name);
	}

	return TRUE;
}

BOOL pf_server_config_load(const char* path, proxyConfig* config)
{
	BOOL ok = FALSE;
	wIniFile* ini = IniFile_New();

	if (!ini)
	{
		WLog_ERR(TAG, "pf_server_load_config(): IniFile_New() failed!");
		return FALSE;
	}

	if (IniFile_ReadFile(ini, path) < 0)
	{
		WLog_ERR(TAG, "pf_server_load_config(): IniFile_ReadFile() failed!");
		goto out;
	}

	if (!pf_config_load_server(ini, config))
		goto out;

	if (!pf_config_load_target(ini, config))
		goto out;

	if (!pf_config_load_channels(ini, config))
		goto out;

	if (!pf_config_load_input(ini, config))
		goto out;

	if (!pf_config_load_security(ini, config))
		goto out;

	if (!pf_config_load_modules(ini, config))
		goto out;

	if (!pf_config_load_clipboard(ini, config))
		goto out;

	ok = TRUE;
out:
	IniFile_Free(ini);
	return ok;
}

void pf_server_config_print(proxyConfig* config)
{
	WLog_INFO(TAG, "Proxy configuration:");

	CONFIG_PRINT_SECTION("Server");
	CONFIG_PRINT_STR(config, Host);
	CONFIG_PRINT_UINT16(config, Port);

	if (!config->UseLoadBalanceInfo)
	{
		CONFIG_PRINT_SECTION("Target");
		CONFIG_PRINT_STR(config, TargetHost);
		CONFIG_PRINT_UINT16(config, TargetPort);
	}

	CONFIG_PRINT_SECTION("Input");
	CONFIG_PRINT_BOOL(config, Keyboard);
	CONFIG_PRINT_BOOL(config, Mouse);

	CONFIG_PRINT_SECTION("Security");
	CONFIG_PRINT_BOOL(config, NlaSecurity);
	CONFIG_PRINT_BOOL(config, TlsSecurity);
	CONFIG_PRINT_BOOL(config, RdpSecurity);

	CONFIG_PRINT_SECTION("Channels");
	CONFIG_PRINT_BOOL(config, GFX);
	CONFIG_PRINT_BOOL(config, DisplayControl);
	CONFIG_PRINT_BOOL(config, Clipboard);
	CONFIG_PRINT_BOOL(config, AudioOutput);

	CONFIG_PRINT_SECTION("Clipboard");
	CONFIG_PRINT_BOOL(config, TextOnly);
	if (config->MaxTextLength > 0)
		CONFIG_PRINT_UINT32(config, MaxTextLength);
}

void pf_server_config_free(proxyConfig* config)
{
	free(config->TargetHost);
	free(config->Host);
	free(config);
}
