/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server Session Capture Module
 *
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

#include <winpr/environment.h>
#include <freerdp/types.h>
#include <errno.h>

#include "cap_config.h"

BOOL capture_plugin_init_config(captureConfig* config)
{
	const char* name = "PROXY_CAPTURE_TARGET";
	char* tmp = NULL;
	DWORD nSize = GetEnvironmentVariableA(name, NULL, 0);

	if (nSize > 0)
	{
		char* colon;
		int addrLen;
		unsigned long port;

		tmp = (LPSTR)malloc(nSize);
		if (!tmp)
			return FALSE;

		if (GetEnvironmentVariableA(name, tmp, nSize) != nSize - 1)
		{
			free(tmp);
			return FALSE;
		}

		colon = strchr(tmp, ':');

		if (!colon)
		{
			free(tmp);
			return FALSE;
		}

		addrLen = (int)(colon - tmp);
		config->host = malloc(addrLen + 1);
		if (!config->host)
		{
			free(tmp);
			return FALSE;
		}

		strncpy(config->host, tmp, addrLen);
		config->host[addrLen] = '\0';

		port = strtoul(colon + 1, NULL, 0);

		if ((errno != 0) || (port > UINT16_MAX))
		{
			free(config->host);
			config->host = NULL;

			free(tmp);
			return FALSE;
		}

		config->port = (UINT16)port;
		free(tmp);
	}
	else
	{
		config->host = _strdup("127.0.0.1");
		if (!config->host)
			return FALSE;

		config->port = 8889;
	}

	return TRUE;
}

void capture_plugin_config_free_internal(captureConfig* config)
{
	free(config->host);
	config->host = NULL;
}
