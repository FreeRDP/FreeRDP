/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2021 Armin Novak <armin.novak@thincast.com>
 * * Copyright 2021 Thincast Technologies GmbH
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

#include <winpr/assert.h>
#include <winpr/string.h>
#include <winpr/wtsapi.h>

#include <freerdp/server/proxy/proxy_log.h>
#include "pf_utils.h"

#define TAG PROXY_TAG("utils")

pf_utils_channel_mode pf_utils_get_channel_mode(const proxyConfig* config, const char* name)
{
	pf_utils_channel_mode rc = PF_UTILS_CHANNEL_NOT_HANDLED;
	size_t i;
	BOOL found = FALSE;

	WINPR_ASSERT(config);
	WINPR_ASSERT(name);

	for (i = 0; i < config->InterceptCount; i++)
	{
		const char* channel_name = config->Intercept[i];
		if (strcmp(name, channel_name) == 0)
		{
			rc = PF_UTILS_CHANNEL_INTERCEPT;
			goto end;
		}
	}

	for (i = 0; i < config->PassthroughCount; i++)
	{
		const char* channel_name = config->Passthrough[i];
		if (strcmp(name, channel_name) == 0)
		{
			found = TRUE;
			break;
		}
	}

	if (found)
	{
		if (config->PassthroughIsBlacklist)
			rc = PF_UTILS_CHANNEL_BLOCK;
		else
			rc = PF_UTILS_CHANNEL_PASSTHROUGH;
	}
	else if (config->PassthroughIsBlacklist)
		rc = PF_UTILS_CHANNEL_PASSTHROUGH;

end:
	WLog_DBG(TAG, "%s -> %s", name, pf_utils_channel_mode_string(rc));
	return rc;
}

BOOL pf_utils_is_passthrough(const proxyConfig* config)
{
	WINPR_ASSERT(config);

	/* TODO: For the time being only passthrough mode is supported. */
	return TRUE;
}

const char* pf_utils_channel_mode_string(pf_utils_channel_mode mode)
{
	switch (mode)
	{
		case PF_UTILS_CHANNEL_BLOCK:
			return "blocked";
		case PF_UTILS_CHANNEL_PASSTHROUGH:
			return "passthrough";
		case PF_UTILS_CHANNEL_INTERCEPT:
			return "intercepted";
		case PF_UTILS_CHANNEL_NOT_HANDLED:
		default:
			return "ignored";
	}
}
