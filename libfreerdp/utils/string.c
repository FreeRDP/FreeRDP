/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * String Utils - Helper functions converting something to string
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#include <freerdp/utils/string.h>
#include <freerdp/settings.h>

char* rdp_redirection_flags_to_string(UINT32 flags, char* buffer, size_t size)
{
	size_t x;
	struct map_t
	{
		UINT32 flag;
		const char* name;
	};
	const struct map_t map[] = {
		{ LB_TARGET_NET_ADDRESS, "LB_TARGET_NET_ADDRESS" },
		{ LB_LOAD_BALANCE_INFO, "LB_LOAD_BALANCE_INFO" },
		{ LB_USERNAME, "LB_USERNAME" },
		{ LB_DOMAIN, "LB_DOMAIN" },
		{ LB_PASSWORD, "LB_PASSWORD" },
		{ LB_DONTSTOREUSERNAME, "LB_DONTSTOREUSERNAME" },
		{ LB_SMARTCARD_LOGON, "LB_SMARTCARD_LOGON" },
		{ LB_NOREDIRECT, "LB_NOREDIRECT" },
		{ LB_TARGET_FQDN, "LB_TARGET_FQDN" },
		{ LB_TARGET_NETBIOS_NAME, "LB_TARGET_NETBIOS_NAME" },
		{ LB_TARGET_NET_ADDRESSES, "LB_TARGET_NET_ADDRESSES" },
		{ LB_CLIENT_TSV_URL, "LB_CLIENT_TSV_URL" },
		{ LB_SERVER_TSV_CAPABLE, "LB_SERVER_TSV_CAPABLE" },
	};

	for (x = 0; x < ARRAYSIZE(map); x++)
	{
		const struct map_t* cur = &map[x];
		if (flags & cur->flag)
		{
			if (!winpr_str_append(cur->name, buffer, size, "|"))
				return NULL;
		}
	}
	return buffer;
}
