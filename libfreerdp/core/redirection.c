/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Server Redirection
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <freerdp/log.h>
#include <freerdp/utils/string.h>

#include "connection.h"
#include "redirection.h"
#include "utils.h"

#define TAG FREERDP_TAG("core.redirection")

struct rdp_redirection
{
	UINT32 flags;
	UINT32 sessionID;
	BYTE* TsvUrl;
	UINT32 TsvUrlLength;
	char* Username;
	char* Domain;
	BYTE* Password;
	UINT32 PasswordLength;
	char* TargetFQDN;
	BYTE* LoadBalanceInfo;
	UINT32 LoadBalanceInfoLength;
	char* TargetNetBiosName;
	char* TargetNetAddress;
	UINT32 TargetNetAddressesCount;
	char** TargetNetAddresses;
	UINT32 RedirectionGuidLength;
	BYTE* RedirectionGuid;
	UINT32 TargetCertificateLength;
	BYTE* TargetCertificate;
};

static void redirection_free_array(char*** what, UINT32* count)
{
	WINPR_ASSERT(what);
	WINPR_ASSERT(count);

	if (*what)
	{
		for (UINT32 x = 0; x < *count; x++)
			free((*what)[x]);
		free(*what);
	}

	*what = NULL;
	*count = 0;
}

static void redirection_free_string(char** str)
{
	WINPR_ASSERT(str);
	free(*str);
	*str = NULL;
}

static void redirection_free_data(BYTE** str, UINT32* length)
{
	WINPR_ASSERT(str);
	free(*str);
	if (length)
		*length = 0;
	*str = NULL;
}

static BOOL redirection_copy_data(BYTE** dst, UINT32* plen, const BYTE* str, UINT32 len)
{
	redirection_free_data(dst, plen);

	if (!str || (len == 0))
		return TRUE;

	*dst = malloc(len);
	if (!*dst)
		return FALSE;
	memcpy(*dst, str, len);
	*plen = len;
	return *dst != NULL;
}

static BOOL freerdp_settings_set_pointer_len(rdpSettings* settings, size_t id, const BYTE* data,
                                             size_t length)
{
	BYTE** pdata = NULL;
	UINT32* plen = NULL;
	switch (id)
	{
		case FreeRDP_TargetNetAddress:
			pdata = (BYTE**)&settings->TargetNetAddress;
			plen = &settings->TargetNetAddressCount;
			break;
		case FreeRDP_LoadBalanceInfo:
			pdata = &settings->LoadBalanceInfo;
			plen = &settings->LoadBalanceInfoLength;
			break;
		case FreeRDP_RedirectionPassword:
			pdata = &settings->RedirectionPassword;
			plen = &settings->RedirectionPasswordLength;
			break;
		case FreeRDP_RedirectionTsvUrl:
			pdata = &settings->RedirectionTsvUrl;
			plen = &settings->RedirectionTsvUrlLength;
			break;
		case FreeRDP_RedirectionGuid:
			pdata = &settings->RedirectionGuid;
			plen = &settings->RedirectionGuidLength;
			break;
		case FreeRDP_RedirectionTargetCertificate:
			pdata = &settings->RedirectionTargetCertificate;
			plen = &settings->RedirectionTargetCertificateLength;
			break;
		default:
			return FALSE;
	}

	return redirection_copy_data(pdata, plen, data, length);
}

static void rdp_print_redirection_flags(UINT32 flags)
{
	WLog_DBG(TAG, "redirectionFlags = {");

	for (UINT32 x = 0; x < 32; x++)
	{
		const UINT32 mask = 1 << x;
		if ((flags & mask) != 0)
		{
			char buffer[64] = { 0 };
			WLog_DBG(TAG, "\t%s", rdp_redirection_flags_to_string(mask, buffer, sizeof(buffer)));
		}
	}

	WLog_DBG(TAG, "}");
}

static BOOL rdp_redirection_read_unicode_string(wStream* s, char** str, size_t maxLength)
{
	UINT32 length = 0;
	const WCHAR* wstr = NULL;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, length);

	if ((length % 2) || length < 2 || length > maxLength)
	{
		WLog_ERR(TAG, "[%s] failure: invalid unicode string length: %" PRIu32 "", __FUNCTION__,
		         length);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
	{
		WLog_ERR(TAG, "[%s] failure: insufficient stream length (%" PRIu32 " bytes required)",
		         __FUNCTION__, length);
		return FALSE;
	}

	wstr = (const WCHAR*)Stream_Pointer(s);

	if (wstr[length / 2 - 1])
	{
		WLog_ERR(TAG, "[%s] failure: unterminated unicode string", __FUNCTION__);
		return FALSE;
	}

	redirection_free_string(str);
	{
		const int res =
		    ConvertFromUnicode(CP_UTF8, 0, wstr, length / sizeof(WCHAR), str, 0, NULL, NULL);
		if ((res < 0) || !(*str))
		{
			WLog_ERR(TAG, "[%s] failure: string conversion failed", __FUNCTION__);
			return FALSE;
		}
	}

	Stream_Seek(s, length);
	return TRUE;
}

int rdp_redirection_apply_settings(rdpRdp* rdp)
{
	rdpSettings* settings = NULL;
	rdpRedirection* redirection = NULL;

	WINPR_ASSERT(rdp);

	settings = rdp->settings;
	WINPR_ASSERT(settings);

	redirection = rdp->redirection;
	WINPR_ASSERT(redirection);

	{
		char buffer[2048] = { 0 };
		WLog_DBG(TAG, "RedirectionFlags=%s",
		         rdp_redirection_flags_to_string(redirection->flags, buffer, sizeof(buffer)));
	}

	settings->RedirectionFlags = redirection->flags;
	settings->RedirectedSessionId = redirection->sessionID;

	if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESS)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_TargetNetAddress,
		                                 redirection->TargetNetAddress))
			return -1;
	}

	if (settings->RedirectionFlags & LB_LOAD_BALANCE_INFO)
	{
		/* LoadBalanceInfo may not contain a null terminator */
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_LoadBalanceInfo,
		                                      redirection->LoadBalanceInfo,
		                                      redirection->LoadBalanceInfoLength))
			return -1;
	}
	else
	{
		/**
		 * Free previous LoadBalanceInfo, if any, otherwise it may end up
		 * being reused for the redirected session, which is not what we want.
		 */
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_LoadBalanceInfo, NULL, 0))
			return -1;
	}

	if (settings->RedirectionFlags & LB_USERNAME)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RedirectionUsername,
		                                 redirection->Username))
			return -1;
	}

	if (settings->RedirectionFlags & LB_DOMAIN)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RedirectionDomain, redirection->Domain))
			return -1;
	}

	if (settings->RedirectionFlags & LB_PASSWORD)
	{
		/* Password may be a cookie without a null terminator */
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RedirectionPassword,
		                                      redirection->Password, redirection->PasswordLength))
			return -1;
	}

	if (settings->RedirectionFlags & LB_DONTSTOREUSERNAME)
	{
		// TODO
	}

	if (settings->RedirectionFlags & LB_SMARTCARD_LOGON)
	{
		// TODO
	}

	if (settings->RedirectionFlags & LB_NOREDIRECT)
	{
		// TODO
	}

	if (settings->RedirectionFlags & LB_TARGET_FQDN)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RedirectionTargetFQDN,
		                                 redirection->TargetFQDN))
			return -1;
	}

	if (settings->RedirectionFlags & LB_TARGET_NETBIOS_NAME)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RedirectionTargetNetBiosName,
		                                 redirection->TargetNetBiosName))
			return -1;
	}

	if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESSES)
	{
		if (!freerdp_target_net_addresses_copy(settings, redirection->TargetNetAddresses,
		                                       redirection->TargetNetAddressesCount))
			return -1;
	}

	if (settings->RedirectionFlags & LB_CLIENT_TSV_URL)
	{
		/* TsvUrl may not contain a null terminator */
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RedirectionTsvUrl,
		                                      redirection->TsvUrl, redirection->TsvUrlLength))
			return -1;
	}

	if (settings->RedirectionFlags & LB_SERVER_TSV_CAPABLE)
	{
		// TODO
	}

	if (settings->RedirectionFlags & LB_PASSWORD_IS_PK_ENCRYPTED)
	{
		// TODO
	}

	if (settings->RedirectionFlags & LB_REDIRECTION_GUID)
	{
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RedirectionGuid,
		                                      redirection->RedirectionGuid,
		                                      redirection->RedirectionGuidLength))
			return -1;
	}

	if (settings->RedirectionFlags & LB_TARGET_CERTIFICATE)
	{
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RedirectionTargetCertificate,
		                                      redirection->TargetCertificate,
		                                      redirection->TargetCertificateLength))
			return -1;
	}

	return 0;
}

static BOOL rdp_redirection_read_data(UINT32 flag, wStream* s, UINT32* pLength, BYTE** pData)
{
	char buffer[64] = { 0 };

	WINPR_ASSERT(pLength);
	WINPR_ASSERT(pData);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, *pLength);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, *pLength))
		return FALSE;

	redirection_free_data(pData, NULL);
	*pData = (BYTE*)malloc(*pLength);

	if (!*pData)
		return FALSE;

	Stream_Read(s, *pData, *pLength);
	WLog_DBG(TAG, "%s:", rdp_redirection_flags_to_string(flag, buffer, sizeof(buffer)));
	winpr_HexDump(TAG, WLOG_DEBUG, *pData, *pLength);
	return TRUE;
}

static int rdp_recv_server_redirection_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 flags = 0;
	UINT16 length = 0;
	rdpRedirection* redirection = rdp->redirection;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return -1;

	Stream_Read_UINT16(s, flags);                  /* flags (2 bytes) */
	Stream_Read_UINT16(s, length);                 /* length (2 bytes) */
	Stream_Read_UINT32(s, redirection->sessionID); /* sessionID (4 bytes) */
	Stream_Read_UINT32(s, redirection->flags);     /* redirFlags (4 bytes) */
	WLog_DBG(TAG,
	         "flags: 0x%04" PRIX16 ", redirFlags: 0x%08" PRIX32 " length: %" PRIu16
	         ", sessionID: 0x%08" PRIX32 "",
	         flags, redirection->flags, length, redirection->sessionID);
	rdp_print_redirection_flags(redirection->flags);

	/* Although MS-RDPBCGR does not mention any length constraints limits for the
	 * variable length null-terminated unicode strings in the RDP_SERVER_REDIRECTION_PACKET
	 * structure we will use the following limits in bytes including the null terminator:
	 *
	 * TargetNetAddress:     80 bytes
	 * UserName:            512 bytes
	 * Domain:               52 bytes
	 * Password(Cookie):    512 bytes
	 * TargetFQDN:          512 bytes
	 * TargetNetBiosName:    32 bytes
	 */

	if (redirection->flags & LB_TARGET_NET_ADDRESS)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetNetAddress), 80))
			return -1;
	}

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
	{
		/* See [MSFT-SDLBTS] (a.k.a. TS_Session_Directory.doc)
		 * load balance info example data:
		 * 0000  43 6f 6f 6b 69 65 3a 20 6d 73 74 73 3d 32 31 33  Cookie: msts=213
		 * 0010  34 30 32 36 34 33 32 2e 31 35 36 32 39 2e 30 30  4026432.15629.00
		 * 0020  30 30 0d 0a                                      00..
		 */
		if (!rdp_redirection_read_data(LB_LOAD_BALANCE_INFO, s, &redirection->LoadBalanceInfoLength,
		                               &redirection->LoadBalanceInfo))
			return -1;
	}

	if (redirection->flags & LB_USERNAME)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->Username), 512))
			return -1;

		WLog_DBG(TAG, "Username: %s", redirection->Username);
	}

	if (redirection->flags & LB_DOMAIN)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->Domain), 52))
			return -1;

		WLog_DBG(TAG, "Domain: %s", redirection->Domain);
	}

	if (redirection->flags & LB_PASSWORD)
	{
		/* Note: Password is a variable-length array of bytes containing the
		 * password used by the user in Unicode format, including a null-terminator
		 * or (!) or a cookie value that MUST be passed to the target server on
		 * successful connection.
		 * Since the format of the password cookie (probably some salted hash) is
		 * currently unknown we'll treat it as opaque data. All cookies seen so far
		 * are 120 bytes including \0\0 termination.
		 * Here is an observed example of a redirection password cookie:
		 *
		 * 0000  02 00 00 80 44 53 48 4c 60 ab 69 2f 07 d6 9e 2d  ....DSHL`.i/...-
		 * 0010  f0 3a 97 3b a9 c5 ec 7e 66 bd b3 84 6c b1 ef b9  .:.;...~f...l...
		 * 0020  b6 82 4e cc 3a df 64 b7 7b 25 04 54 c2 58 98 f8  ..N.:.d.{%.T.X..
		 * 0030  97 87 d4 93 c7 c1 e1 5b c2 85 f8 22 49 1f 81 88  .......[..."I...
		 * 0040  43 44 83 f6 9a 72 40 24 dc 4d 43 cb d9 92 3c 8f  CD...r@$.MC...<.
		 * 0050  3a 37 5c 77 13 a0 72 3c 72 08 64 2a 29 fb dc eb  :7\w..r<r.d*)...
		 * 0060  0d 2b 06 b4 c6 08 b4 73 34 16 93 62 6d 24 e9 93  .+.....s4..bm$..
		 * 0070  97 27 7b dd 9a 72 00 00                          .'{..r..
		 *
		 * Notwithstanding the above, we'll allocated an additional zero WCHAR at the
		 * end of the buffer which won't get counted in PasswordLength.
		 */
		if (!rdp_redirection_read_data(LB_PASSWORD, s, &redirection->PasswordLength,
		                               &redirection->Password))
			return -1;

		/* [MS-RDPBCGR] specifies 512 bytes as the upper limit for the password length
		 * including the null terminatior(s). This should also be enough for the unknown
		 * password cookie format (see previous comment).
		 */
		if ((redirection->flags & LB_PASSWORD_IS_PK_ENCRYPTED) == 0)
		{
			const size_t charLen = redirection->PasswordLength / sizeof(WCHAR);
			if (redirection->PasswordLength > LB_PASSWORD_MAX_LENGTH)
				return -1;

			/* Ensure the text password is '\0' terminated */
			if (_wcsnlen((const WCHAR*)redirection->Password, charLen) == charLen)
				return -1;
		}
	}

	if (redirection->flags & LB_TARGET_FQDN)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetFQDN), 512))
			return -1;

		WLog_DBG(TAG, "TargetFQDN: %s", redirection->TargetFQDN);
	}

	if (redirection->flags & LB_TARGET_NETBIOS_NAME)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetNetBiosName), 32))
			return -1;

		WLog_DBG(TAG, "TargetNetBiosName: %s", redirection->TargetNetBiosName);
	}

	if (redirection->flags & LB_CLIENT_TSV_URL)
	{
		if (!rdp_redirection_read_data(LB_CLIENT_TSV_URL, s, &redirection->TsvUrlLength,
		                               &redirection->TsvUrl))
			return -1;
	}

	if (redirection->flags & LB_REDIRECTION_GUID)
	{
		if (!rdp_redirection_read_data(LB_REDIRECTION_GUID, s, &redirection->RedirectionGuidLength,
		                               &redirection->RedirectionGuid))
			return -1;
	}

	if (redirection->flags & LB_TARGET_CERTIFICATE)
	{
		if (!rdp_redirection_read_data(LB_TARGET_CERTIFICATE, s,
		                               &redirection->TargetCertificateLength,
		                               &redirection->TargetCertificate))
			return -1;
	}

	if (redirection->flags & LB_TARGET_NET_ADDRESSES)
	{
		size_t i = 0;
		UINT32 count = 0;
		UINT32 targetNetAddressesLength = 0;

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return -1;

		Stream_Read_UINT32(s, targetNetAddressesLength);
		Stream_Read_UINT32(s, redirection->TargetNetAddressesCount);
		count = redirection->TargetNetAddressesCount;
		redirection->TargetNetAddresses = (char**)calloc(count, sizeof(char*));

		if (!redirection->TargetNetAddresses)
			return -1;

		WLog_DBG(TAG, "TargetNetAddressesCount: %" PRIu32 "", count);

		for (i = 0; i < count; i++)
		{
			if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetNetAddresses[i]), 80))
				return -1;

			WLog_DBG(TAG, "TargetNetAddresses[%" PRIuz "]: %s", i,
			         redirection->TargetNetAddresses[i]);
		}
	}

	if (Stream_GetRemainingLength(s) >= 8)
	{
		/* some versions of windows don't included this padding before closing the connection */
		Stream_Seek(s, 8); /* pad (8 bytes) */
	}

	if (redirection->flags & LB_NOREDIRECT)
		return 0;

	return 1;
}

int rdp_recv_enhanced_security_redirection_packet(rdpRdp* rdp, wStream* s)
{
	int status = 0;

	if (!Stream_SafeSeek(s, 2)) /* pad2Octets (2 bytes) */
		return -1;

	status = rdp_recv_server_redirection_pdu(rdp, s);

	if (status < 0)
		return status;

	if (Stream_GetRemainingLength(s) >= 1)
	{
		/* this field is optional, and its absence is not an error */
		Stream_Seek(s, 1); /* pad2Octets (1 byte) */
	}

	return status;
}

rdpRedirection* redirection_new(void)
{
	rdpRedirection* redirection;
	redirection = (rdpRedirection*)calloc(1, sizeof(rdpRedirection));

	if (redirection)
	{
	}

	return redirection;
}

void redirection_free(rdpRedirection* redirection)
{
	if (redirection)
	{
		redirection_free_data(&redirection->TsvUrl, &redirection->TsvUrlLength);
		redirection_free_string(&redirection->Username);
		redirection_free_string(&redirection->Domain);
		redirection_free_string(&redirection->TargetFQDN);
		redirection_free_string(&redirection->TargetNetBiosName);
		redirection_free_string(&redirection->TargetNetAddress);
		redirection_free_data(&redirection->LoadBalanceInfo, &redirection->LoadBalanceInfoLength);
		redirection_free_data(&redirection->Password, &redirection->PasswordLength);
		redirection_free_data(&redirection->RedirectionGuid, &redirection->RedirectionGuidLength);
		redirection_free_data(&redirection->TargetCertificate,
		                      &redirection->TargetCertificateLength);
		redirection_free_array(&redirection->TargetNetAddresses,
		                       &redirection->TargetNetAddressesCount);

		free(redirection);
	}
}
