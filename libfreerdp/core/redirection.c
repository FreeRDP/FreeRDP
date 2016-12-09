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

#include "connection.h"

#include "redirection.h"

#define TAG FREERDP_TAG("core.redirection")

static void rdp_print_redirection_flags(UINT32 flags)
{
	WLog_DBG(TAG, "redirectionFlags = {");

	if (flags & LB_TARGET_NET_ADDRESS)
		WLog_DBG(TAG, "\tLB_TARGET_NET_ADDRESS");

	if (flags & LB_LOAD_BALANCE_INFO)
		WLog_DBG(TAG, "\tLB_LOAD_BALANCE_INFO");

	if (flags & LB_USERNAME)
		WLog_DBG(TAG, "\tLB_USERNAME");

	if (flags & LB_DOMAIN)
		WLog_DBG(TAG, "\tLB_DOMAIN");

	if (flags & LB_PASSWORD)
		WLog_DBG(TAG, "\tLB_PASSWORD");

	if (flags & LB_DONTSTOREUSERNAME)
		WLog_DBG(TAG, "\tLB_DONTSTOREUSERNAME");

	if (flags & LB_SMARTCARD_LOGON)
		WLog_DBG(TAG, "\tLB_SMARTCARD_LOGON");

	if (flags & LB_NOREDIRECT)
		WLog_DBG(TAG, "\tLB_NOREDIRECT");

	if (flags & LB_TARGET_FQDN)
		WLog_DBG(TAG, "\tLB_TARGET_FQDN");

	if (flags & LB_TARGET_NETBIOS_NAME)
		WLog_DBG(TAG, "\tLB_TARGET_NETBIOS_NAME");

	if (flags & LB_TARGET_NET_ADDRESSES)
		WLog_DBG(TAG, "\tLB_TARGET_NET_ADDRESSES");

	if (flags & LB_CLIENT_TSV_URL)
		WLog_DBG(TAG, "\tLB_CLIENT_TSV_URL");

	if (flags & LB_SERVER_TSV_CAPABLE)
		WLog_DBG(TAG, "\tLB_SERVER_TSV_CAPABLE");

	WLog_DBG(TAG, "}");
}

static BOOL rdp_redirection_read_unicode_string(wStream* s, char** str, size_t maxLength)
{
	UINT32 length;
	WCHAR* wstr = NULL;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG,  "rdp_redirection_read_string failure: cannot read length");
		return FALSE;
	}

	Stream_Read_UINT32(s, length);

	if ((length % 2) || length < 2 || length > maxLength)
	{
		WLog_ERR(TAG,  "rdp_redirection_read_string failure: invalid unicode string length: %lu", length);
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) < length)
	{
		WLog_ERR(TAG,  "rdp_redirection_read_string failure: insufficient stream length (%lu bytes required)", length);
		return FALSE;
	}

	wstr = (WCHAR*) Stream_Pointer(s);

	if (wstr[length / 2 - 1])
	{
		WLog_ERR(TAG,  "rdp_redirection_read_string failure: unterminated unicode string");
		return FALSE;
	}

	if (ConvertFromUnicode(CP_UTF8, 0, wstr, -1, str, 0, NULL, NULL) < 1)
	{
		WLog_ERR(TAG,  "rdp_redirection_read_string failure: string conversion failed");
		return FALSE;
	}
	Stream_Seek(s, length);
	return TRUE;
}

int rdp_redirection_apply_settings(rdpRdp* rdp)
{
	rdpSettings* settings = rdp->settings;
	rdpRedirection* redirection = rdp->redirection;

	settings->RedirectionFlags = redirection->flags;
	settings->RedirectedSessionId = redirection->sessionID;

	if (settings->RedirectionFlags & LB_LOAD_BALANCE_INFO)
	{
		/* LoadBalanceInfo may not contain a null terminator */
		free(settings->LoadBalanceInfo);
		settings->LoadBalanceInfoLength = redirection->LoadBalanceInfoLength;
		settings->LoadBalanceInfo = (BYTE*) malloc(settings->LoadBalanceInfoLength);

		if (!settings->LoadBalanceInfo)
			return -1;

		CopyMemory(settings->LoadBalanceInfo, redirection->LoadBalanceInfo, settings->LoadBalanceInfoLength);
	}
	else
	{
		/**
		 * Free previous LoadBalanceInfo, if any, otherwise it may end up
		 * being reused for the redirected session, which is not what we want.
		 */
		free(settings->LoadBalanceInfo);
		settings->LoadBalanceInfo = NULL;
		settings->LoadBalanceInfoLength = 0;
	}

	if (settings->RedirectionFlags & LB_TARGET_FQDN)
	{
		free(settings->RedirectionTargetFQDN);
		settings->RedirectionTargetFQDN = _strdup(redirection->TargetFQDN);
		if (!settings->RedirectionTargetFQDN)
			return -1;
	}
	else if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESS)
	{
		free(settings->TargetNetAddress);
		settings->TargetNetAddress = _strdup(redirection->TargetNetAddress);
		if (!settings->TargetNetAddress)
			return -1;
	}
	else if (settings->RedirectionFlags & LB_TARGET_NETBIOS_NAME)
	{
		free(settings->RedirectionTargetNetBiosName);
		settings->RedirectionTargetNetBiosName = _strdup(redirection->TargetNetBiosName);
		if (!settings->RedirectionTargetNetBiosName)
			return -1;
	}

	if (settings->RedirectionFlags & LB_USERNAME)
	{
		free(settings->RedirectionUsername);
		settings->RedirectionUsername = _strdup(redirection->Username);
		if (!settings->RedirectionUsername)
			return -1;
	}

	if (settings->RedirectionFlags & LB_DOMAIN)
	{
		free(settings->RedirectionDomain);
		settings->RedirectionDomain = _strdup(redirection->Domain);
		if (!settings->RedirectionDomain)
			return -1;
	}

	if (settings->RedirectionFlags & LB_PASSWORD)
	{
		/* Password may be a cookie without a null terminator */
		free(settings->RedirectionPassword);
		settings->RedirectionPasswordLength = redirection->PasswordLength;
		/* For security reasons we'll allocate an additional zero WCHAR at the
		 * end of the buffer that is not included in RedirectionPasswordLength
		 */
		settings->RedirectionPassword = (BYTE*) calloc(1, settings->RedirectionPasswordLength + sizeof(WCHAR));
		if (!settings->RedirectionPassword)
			return -1;
		CopyMemory(settings->RedirectionPassword, redirection->Password, settings->RedirectionPasswordLength);
	}

	if (settings->RedirectionFlags & LB_CLIENT_TSV_URL)
	{
		/* TsvUrl may not contain a null terminator */
		free(settings->RedirectionTsvUrl);
		settings->RedirectionTsvUrlLength = redirection->TsvUrlLength;
		settings->RedirectionTsvUrl = (BYTE*) malloc(settings->RedirectionTsvUrlLength);
		if (!settings->RedirectionTsvUrl)
			return -1;
		CopyMemory(settings->RedirectionTsvUrl, redirection->TsvUrl, settings->RedirectionTsvUrlLength);
	}

	if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESSES)
	{
		UINT32 i;
		freerdp_target_net_addresses_free(settings);
		settings->TargetNetAddressCount = redirection->TargetNetAddressesCount;
		settings->TargetNetAddresses = (char**) malloc(sizeof(char*) * settings->TargetNetAddressCount);
		if (!settings->TargetNetAddresses)
		{
			settings->TargetNetAddressCount = 0;
			return -1;
		}

		for (i = 0; i < settings->TargetNetAddressCount; i++)
		{
			settings->TargetNetAddresses[i] = _strdup(redirection->TargetNetAddresses[i]);
			if (!settings->TargetNetAddresses[i])
			{
				UINT32 j;

				for (j=0; j < i; j++)
					free(settings->TargetNetAddresses[j]);
				return -1;
			}
		}
	}

	return 0;
}

static BOOL rdp_recv_server_redirection_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 flags;
	UINT16 length;
	rdpRedirection* redirection = rdp->redirection;

	if (Stream_GetRemainingLength(s) < 12)
		return -1;

	Stream_Read_UINT16(s, flags); /* flags (2 bytes) */
	Stream_Read_UINT16(s, length); /* length (2 bytes) */
	Stream_Read_UINT32(s, redirection->sessionID); /* sessionID (4 bytes) */
	Stream_Read_UINT32(s, redirection->flags); /* redirFlags (4 bytes) */

	WLog_DBG(TAG, "flags: 0x%04X, redirFlags: 0x%04X length: %d, sessionID: 0x%08X",
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

		if (Stream_GetRemainingLength(s) < 4)
			return -1;

		Stream_Read_UINT32(s, redirection->LoadBalanceInfoLength);

		if (Stream_GetRemainingLength(s) < redirection->LoadBalanceInfoLength)
			return -1;

		redirection->LoadBalanceInfo = (BYTE*) malloc(redirection->LoadBalanceInfoLength);
		if (!redirection->LoadBalanceInfo)
			return -1;
		Stream_Read(s, redirection->LoadBalanceInfo, redirection->LoadBalanceInfoLength);

		WLog_DBG(TAG, "loadBalanceInfo:");
		winpr_HexDump(TAG, WLOG_DEBUG, redirection->LoadBalanceInfo, redirection->LoadBalanceInfoLength);
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
			return FALSE;

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

		if (Stream_GetRemainingLength(s) < 4)
			return -1;

		Stream_Read_UINT32(s, redirection->PasswordLength);

		/* [MS-RDPBCGR] specifies 512 bytes as the upper limit for the password length
		 * including the null terminatior(s). This should also be enough for the unknown
		 * password cookie format (see previous comment).
		 */

		if (Stream_GetRemainingLength(s) < redirection->PasswordLength)
			return -1;

		if (redirection->PasswordLength > 512)
			return -1;

		redirection->Password = (BYTE*) calloc(1, redirection->PasswordLength + sizeof(WCHAR));
		if (!redirection->Password)
			return -1;
		Stream_Read(s, redirection->Password, redirection->PasswordLength);

		WLog_DBG(TAG, "PasswordCookie:");
		winpr_HexDump(TAG, WLOG_DEBUG, redirection->Password, redirection->PasswordLength);
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
		if (Stream_GetRemainingLength(s) < 4)
			return -1;

		Stream_Read_UINT32(s, redirection->TsvUrlLength);

		if (Stream_GetRemainingLength(s) < redirection->TsvUrlLength)
			return -1;

		redirection->TsvUrl = (BYTE*) malloc(redirection->TsvUrlLength);
		if (!redirection->TsvUrl)
			return -1;
		Stream_Read(s, redirection->TsvUrl, redirection->TsvUrlLength);

		WLog_DBG(TAG, "TsvUrl:");
		winpr_HexDump(TAG, WLOG_DEBUG, redirection->TsvUrl, redirection->TsvUrlLength);
	}

	if (redirection->flags & LB_TARGET_NET_ADDRESSES)
	{
		int i;
		UINT32 count;
		UINT32 targetNetAddressesLength;

		if (Stream_GetRemainingLength(s) < 8)
			return -1;

		Stream_Read_UINT32(s, targetNetAddressesLength);
		Stream_Read_UINT32(s, redirection->TargetNetAddressesCount);
		count = redirection->TargetNetAddressesCount;

		redirection->TargetNetAddresses = (char**) calloc(count, sizeof(char*));

		if (!redirection->TargetNetAddresses)
			return FALSE;

		WLog_DBG(TAG, "TargetNetAddressesCount: %d", redirection->TargetNetAddressesCount);

		for (i = 0; i < (int) count; i++)
		{
			if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetNetAddresses[i]), 80))
				return FALSE;

			WLog_DBG(TAG, "TargetNetAddresses[%d]: %s", i, redirection->TargetNetAddresses[i]);
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

rdpRedirection* redirection_new()
{
	rdpRedirection* redirection;
	redirection = (rdpRedirection*) calloc(1, sizeof(rdpRedirection));

	if (redirection)
	{

	}

	return redirection;
}

void redirection_free(rdpRedirection* redirection)
{
	if (redirection)
	{
		free(redirection->TsvUrl);
		free(redirection->Username);
		free(redirection->Domain);
		free(redirection->TargetFQDN);
		free(redirection->TargetNetBiosName);
		free(redirection->TargetNetAddress);
		free(redirection->LoadBalanceInfo);
		free(redirection->Password);

		if (redirection->TargetNetAddresses)
		{
			int i;

			for (i = 0; i < (int) redirection->TargetNetAddressesCount; i++)
			{
				free(redirection->TargetNetAddresses[i]);
			}

			free(redirection->TargetNetAddresses);
		}

		free(redirection);
	}
}

