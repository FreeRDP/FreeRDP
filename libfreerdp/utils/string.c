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
		{ LB_PASSWORD_IS_PK_ENCRYPTED, "LB_PASSWORD_IS_PK_ENCRYPTED" },
		{ LB_REDIRECTION_GUID, "LB_REDIRECTION_GUID" },
		{ LB_TARGET_CERTIFICATE, "LB_TARGET_CERTIFICATE" },
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

char* rdp_cluster_info_flags_to_string(UINT32 flags, char* buffer, size_t size)
{
	const UINT32 version = (flags & ServerSessionRedirectionVersionMask) >> 2;
	if (flags & REDIRECTION_SUPPORTED)
		winpr_str_append("REDIRECTION_SUPPORTED", buffer, size, "|");
	if (flags & REDIRECTED_SESSIONID_FIELD_VALID)
		winpr_str_append("REDIRECTED_SESSIONID_FIELD_VALID", buffer, size, "|");
	if (flags & REDIRECTED_SMARTCARD)
		winpr_str_append("REDIRECTED_SMARTCARD", buffer, size, "|");

	const char* str = NULL;
	switch (version)
	{
		case REDIRECTION_VERSION1:
			str = "REDIRECTION_VERSION1";
			break;
		case REDIRECTION_VERSION2:
			str = "REDIRECTION_VERSION2";
			break;
		case REDIRECTION_VERSION3:
			str = "REDIRECTION_VERSION3";
			break;
		case REDIRECTION_VERSION4:
			str = "REDIRECTION_VERSION4";
			break;
		case REDIRECTION_VERSION5:
			str = "REDIRECTION_VERSION5";
			break;
		case REDIRECTION_VERSION6:
			str = "REDIRECTION_VERSION6";
			break;
		default:
			str = "REDIRECTION_VERSION_UNKNOWN";
			break;
	}
	winpr_str_append(str, buffer, size, "|");
	{
		char msg[32] = { 0 };
		_snprintf(msg, sizeof(msg), "[0x%08" PRIx32 "]", flags);
		winpr_str_append(msg, buffer, size, "");
	}
	return buffer;
}
