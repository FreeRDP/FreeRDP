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
#include "pf_log.h"
#include "pf_server.h"
#include "pf_config.h"

#define TAG PROXY_TAG("config")

#define CHANNELS_SEPERATOR ","

static char** parse_channels_from_str(const char* str, UINT32* length)
{
	char* s = strdup(str);
	size_t tokens_alloc = 1;
	size_t tokens_count = 0;
	char** tokens = calloc(tokens_alloc, sizeof(char*));
	char* token;

	while ((token = StrSep(&s, CHANNELS_SEPERATOR)) != NULL)
	{
		if (tokens_count == tokens_alloc)
		{
			tokens_alloc *= 2;
			tokens = realloc(tokens, tokens_alloc * sizeof(char*));
		}

		tokens[tokens_count++] = strdup(token);
	}

	if ((tokens_count == 0) || (tokens_count > UINT32_MAX))
	{
		free(tokens);
		tokens = NULL;
		tokens_count = 0;
	}
	else
	{
		tokens = realloc(tokens, tokens_count * sizeof(char*));
	}

	*length = (DWORD)tokens_count;
	free(s);
	return tokens;
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
	int rc;
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

	if (input)
	{
		config->AllowedChannels = parse_channels_from_str(input, &config->AllowedChannelsCount);

		if (config->AllowedChannels == NULL)
			goto out;
	}

	input = IniFile_GetKeyValueString(ini, "Channels", "DeniedChannels");

	if (input)
	{
		config->BlockedChannels = parse_channels_from_str(input, &config->BlockedChannelsCount);

		if (config->BlockedChannels == NULL)
			goto out;
	}

	result = CONFIG_PARSE_SUCCESS;
out:
	IniFile_Free(ini);

	if (!pf_server_is_config_valid(config))
		return CONFIG_INVALID;

	return result;
}

void pf_server_config_free(proxyConfig* config)
{
	UINT32 i;

	for (i = 0; i < config->AllowedChannelsCount; i++)
		free(config->AllowedChannels[i]);

	for (i = 0; i < config->BlockedChannelsCount; i++)
		free(config->BlockedChannels[i]);

	free(config->AllowedChannels);
	free(config->BlockedChannels);
	free(config->TargetHost);
	free(config->Host);
	free(config);
}
