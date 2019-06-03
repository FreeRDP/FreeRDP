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

#include <stdio.h>
#include <string.h>
#include <winpr/crt.h>
#include <winpr/collections.h>

#include "pf_log.h"
#include "pf_server.h"
#include "pf_config.h"

#define TAG PROXY_TAG("config")

#define CHANNELS_SEPERATOR ","

wArrayList* parse_string_array_from_str(const char* str)
{
	wArrayList* list = ArrayList_New(FALSE);
	char* s;
	char* temp;
	char* token;

	if (list == NULL)
	{
		WLog_ERR(TAG, "parse_string_array_from_str(): ArrayList_New failed!");
		return NULL;
	}

	
 	temp = s = _strdup(str);
	if (!s)
	{
		WLog_ERR(TAG, "parse_string_array_from_str(): strdup failed!");
		return NULL;
	}

	if (s == NULL)
	{
		WLog_ERR(TAG, "parse_string_array_from_str(): strdup failed!");
		goto error;
	}

	while ((token = StrSep(&temp, CHANNELS_SEPERATOR)) != NULL)
	{
		char* current_token = _strdup(token);

		if (current_token == NULL)
		{
			WLog_ERR(TAG, "parse_string_array_from_str(): strdup failed!");
			goto error;
		}

		if (ArrayList_Add(list, current_token) < 0)
		{
			free(current_token);
			goto error;
		}
	}

	free(s);
	return list;
error:
	free(s);
	ArrayList_Free(list);
	return NULL;
}

static BOOL pf_server_is_config_valid(proxyConfig* config)
{
	if (config->Host == NULL)
	{
		WLog_ERR(TAG, "Configuration value for `Server.Host` is not valid");
		return FALSE;
	}

	if (config->Port <= 0)
	{
		WLog_ERR(TAG, "Configuration value for `Server.Port` is not valid");
		return FALSE;
	}

	if (!config->UseLoadBalanceInfo)
	{
		if (config->TargetHost == NULL)
		{
			WLog_ERR(TAG, "Configuration value for `Target.Host` is not valid");
			return FALSE;
		}

		if (config->TargetPort <= 0)
		{
			WLog_ERR(TAG, "Configuration value for `Target.Port` is not valid");
			return FALSE;
		}
	}

	return TRUE;
}

DWORD pf_server_load_config(const char* path, proxyConfig* config)
{
	const char* input;
	char** filters_names;
	int rc;
	int filters_count = 0;
	UINT32 index;
	DWORD result = CONFIG_PARSE_ERROR;
	wIniFile* ini = IniFile_New();

	if (!ini)
		return CONFIG_PARSE_ERROR;

	if (IniFile_ReadFile(ini, path) < 0)
		goto out;

	/* server */
	config->Host = _strdup(IniFile_GetKeyValueString(ini, "Server", "Host"));
	config->LocalOnly = IniFile_GetKeyValueInt(ini, "Server", "LocalOnly");
	rc = IniFile_GetKeyValueInt(ini, "Server", "Port");

	if ((rc < 0) || (rc > UINT16_MAX))
		goto out;

	config->Port = (UINT16)rc;
	/* target */
	config->UseLoadBalanceInfo = IniFile_GetKeyValueInt(ini, "Target", "UseLoadBalanceInfo");
	config->TargetHost = _strdup(IniFile_GetKeyValueString(ini, "Target", "Host"));
	rc = IniFile_GetKeyValueInt(ini, "Target", "Port");

	if ((rc < 0) || (rc > UINT16_MAX))
		goto out;

	config->TargetPort = (UINT16)rc;
	/* graphics */
	config->GFX = IniFile_GetKeyValueInt(ini, "Graphics", "GFX");
	config->BitmapUpdate = IniFile_GetKeyValueInt(ini, "Graphics", "BitmapUpdate");
	/* input */
	config->Keyboard = IniFile_GetKeyValueInt(ini, "Input", "Keyboard");
	config->Mouse = IniFile_GetKeyValueInt(ini, "Input", "Mouse");
	/* security */
	config->TlsSecurity = IniFile_GetKeyValueInt(ini, "Security", "TlsSecurity");
	config->NlaSecurity = IniFile_GetKeyValueInt(ini, "Security", "NlaSecurity");
	config->RdpSecurity = IniFile_GetKeyValueInt(ini, "Security", "RdpSecurity");
	/* channels filtering */
	config->WhitelistMode = IniFile_GetKeyValueInt(ini, "Channels", "WhitelistMode");
	input = IniFile_GetKeyValueString(ini, "Channels", "AllowedChannels");
	/* filters api */

	if (input)
	{
		config->AllowedChannels = parse_string_array_from_str(input);

		if (config->AllowedChannels == NULL)
			goto out;
	}

	input = IniFile_GetKeyValueString(ini, "Channels", "DeniedChannels");

	if (input)
	{
		config->BlockedChannels = parse_string_array_from_str(input);

		if (config->BlockedChannels == NULL)
			goto out;
	}

	result = CONFIG_PARSE_SUCCESS;

	if (!pf_filters_init(&config->Filters))
		goto out;
		
	filters_names = IniFile_GetSectionKeyNames(ini, "Filters", &filters_count);

	for (index = 0; index < filters_count; index++)
	{
		char* filter_name = filters_names[index];
		const char* path = IniFile_GetKeyValueString(ini, "Filters", filter_name);

		if (!pf_filters_register_new(config->Filters, path, filter_name))
		{
			WLog_DBG(TAG, "pf_server_load_config(): failed to register %s (%s)", filter_name, path);
		}
		else
		{
			WLog_DBG(TAG, "pf_server_load_config(): registered filter %s (%s) successfully", filter_name, path);
		}
	}

out:
	IniFile_Free(ini);

	if (!pf_server_is_config_valid(config))
		return CONFIG_INVALID;

	return result;
}

void pf_server_config_free(proxyConfig* config)
{
	pf_filters_unregister_all(config->Filters);
	ArrayList_Free(config->AllowedChannels);
	ArrayList_Free(config->BlockedChannels);
	free(config->TargetHost);
	free(config->Host);
	free(config);
}
