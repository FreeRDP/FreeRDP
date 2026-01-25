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

#include <errno.h>

#include <freerdp/utils/string.h>
#include <freerdp/settings.h>

#if defined(CHANNEL_RDPEI)
#include <freerdp/channels/rdpei.h>
#endif

const char* rdp_redirection_flags_to_string(UINT32 flags, char* buffer, size_t size)
{
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

	for (size_t x = 0; x < ARRAYSIZE(map); x++)
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

const char* rdp_cluster_info_flags_to_string(UINT32 flags, char* buffer, size_t size)
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
		(void)_snprintf(msg, sizeof(msg), "[0x%08" PRIx32 "]", flags);
		winpr_str_append(msg, buffer, size, "");
	}
	return buffer;
}

BOOL freerdp_extract_key_value(const char* str, UINT32* pkey, UINT32* pvalue)
{
	if (!str || !pkey || !pvalue)
		return FALSE;

	char* end1 = NULL;
	errno = 0;
	unsigned long key = strtoul(str, &end1, 0);
	if ((errno != 0) || !end1 || (*end1 != '=') || (key > UINT32_MAX))
		return FALSE;

	errno = 0;
	unsigned long val = strtoul(&end1[1], NULL, 0);
	if ((errno != 0) || (val > UINT32_MAX))
		return FALSE;

	*pkey = (UINT32)key;
	*pvalue = (UINT32)val;
	return TRUE;
}

const char* freerdp_desktop_rotation_flags_to_string(UINT32 flags)
{
#define ENTRY(x) \
	case x:      \
		return #x

	switch (flags)
	{
		ENTRY(ORIENTATION_LANDSCAPE);
		ENTRY(ORIENTATION_PORTRAIT);
		ENTRY(ORIENTATION_LANDSCAPE_FLIPPED);
		ENTRY(ORIENTATION_PORTRAIT_FLIPPED);
		default:
			return "ORIENTATION_UNKNOWN";
	}
#undef ENTRY
}

const char* freerdp_input_touch_state_string(DWORD flags)
{
#if defined(CHANNEL_RDPEI)
	if (flags & RDPINPUT_CONTACT_FLAG_DOWN)
		return "RDPINPUT_CONTACT_FLAG_DOWN";
	else if (flags & RDPINPUT_CONTACT_FLAG_UPDATE)
		return "RDPINPUT_CONTACT_FLAG_UPDATE";
	else if (flags & RDPINPUT_CONTACT_FLAG_UP)
		return "RDPINPUT_CONTACT_FLAG_UP";
	else if (flags & RDPINPUT_CONTACT_FLAG_INRANGE)
		return "RDPINPUT_CONTACT_FLAG_INRANGE";
	else if (flags & RDPINPUT_CONTACT_FLAG_INCONTACT)
		return "RDPINPUT_CONTACT_FLAG_INCONTACT";
	else if (flags & RDPINPUT_CONTACT_FLAG_CANCELED)
		return "RDPINPUT_CONTACT_FLAG_CANCELED";
	else
		return "RDPINPUT_CONTACT_FLAG_UNKNOWN";
#else
	return "CHANNEL_RDPEI not supported";
#endif
}

const char* freerdp_order_support_flags_string(UINT8 type)
{
#define ENTRY(x) \
	case x:      \
		return #x

	switch (type)
	{
		ENTRY(NEG_DSTBLT_INDEX);
		ENTRY(NEG_PATBLT_INDEX);
		ENTRY(NEG_SCRBLT_INDEX);
		ENTRY(NEG_MEMBLT_INDEX);
		ENTRY(NEG_MEM3BLT_INDEX);
		ENTRY(NEG_ATEXTOUT_INDEX);
		ENTRY(NEG_AEXTTEXTOUT_INDEX);
		ENTRY(NEG_DRAWNINEGRID_INDEX);
		ENTRY(NEG_LINETO_INDEX);
		ENTRY(NEG_MULTI_DRAWNINEGRID_INDEX);
		ENTRY(NEG_OPAQUE_RECT_INDEX);
		ENTRY(NEG_SAVEBITMAP_INDEX);
		ENTRY(NEG_WTEXTOUT_INDEX);
		ENTRY(NEG_MEMBLT_V2_INDEX);
		ENTRY(NEG_MEM3BLT_V2_INDEX);
		ENTRY(NEG_MULTIDSTBLT_INDEX);
		ENTRY(NEG_MULTIPATBLT_INDEX);
		ENTRY(NEG_MULTISCRBLT_INDEX);
		ENTRY(NEG_MULTIOPAQUERECT_INDEX);
		ENTRY(NEG_FAST_INDEX_INDEX);
		ENTRY(NEG_POLYGON_SC_INDEX);
		ENTRY(NEG_POLYGON_CB_INDEX);
		ENTRY(NEG_POLYLINE_INDEX);
		ENTRY(NEG_UNUSED23_INDEX);
		ENTRY(NEG_FAST_GLYPH_INDEX);
		ENTRY(NEG_ELLIPSE_SC_INDEX);
		ENTRY(NEG_ELLIPSE_CB_INDEX);
		ENTRY(NEG_GLYPH_INDEX_INDEX);
		ENTRY(NEG_GLYPH_WEXTTEXTOUT_INDEX);
		ENTRY(NEG_GLYPH_WLONGTEXTOUT_INDEX);
		ENTRY(NEG_GLYPH_WLONGEXTTEXTOUT_INDEX);
		ENTRY(NEG_UNUSED31_INDEX);
		default:
			return "UNKNOWN";
	}
#undef ENTRY
}
