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

#include <freerdp/config.h>

#include <winpr/crt.h>
#include <freerdp/log.h>

#include "connection.h"

#include "redirection.h"

#define TAG FREERDP_TAG("core.redirection")

struct rdp_redirection
{
	UINT32 flags;
	UINT32 sessionID;
	BYTE* TsvUrl;
	DWORD TsvUrlLength;
	char* Username;
	char* Domain;
	BYTE* Password;
	DWORD PasswordLength;
	char* TargetFQDN;
	BYTE* LoadBalanceInfo;
	DWORD LoadBalanceInfoLength;
	char* TargetNetBiosName;
	char* TargetNetAddress;
	UINT32 TargetNetAddressesCount;
	char** TargetNetAddresses;
};

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

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, length);

	if ((length % 2) || length < 2 || length > maxLength)
	{
		WLog_ERR(TAG,
		         "rdp_redirection_read_string failure: invalid unicode string length: %" PRIu32 "",
		         length);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
	{
		WLog_ERR(TAG,
		         "rdp_redirection_read_string failure: insufficient stream length (%" PRIu32
		         " bytes required)",
		         length);
		return FALSE;
	}

	wstr = (WCHAR*)Stream_Pointer(s);

	if (wstr[length / 2 - 1])
	{
		WLog_ERR(TAG, "rdp_redirection_read_string failure: unterminated unicode string");
		return FALSE;
	}

	*str = ConvertWCharNToUtf8Alloc(wstr, length / sizeof(WCHAR), NULL);
	if (!*str)
	{
		WLog_ERR(TAG, "rdp_redirection_read_string failure: string conversion failed");
		return FALSE;
	}

	Stream_Seek(s, length);
	return TRUE;
}

int rdp_redirection_apply_settings(rdpRdp* rdp)
{
	rdpSettings* settings;
	rdpRedirection* redirection;

	WINPR_ASSERT(rdp);

	freerdp_settings_free(rdp->settings);
	rdp->context->settings = rdp->settings = freerdp_settings_clone(rdp->originalSettings);

	settings = rdp->settings;
	WINPR_ASSERT(settings);

	redirection = rdp->redirection;
	WINPR_ASSERT(redirection);

	settings->RedirectionFlags = redirection->flags;
	settings->RedirectedSessionId = redirection->sessionID;

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

	if (settings->RedirectionFlags & LB_TARGET_FQDN)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RedirectionTargetFQDN,
		                                 redirection->TargetFQDN))
			return -1;
	}

	if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESS)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_TargetNetAddress,
		                                 redirection->TargetNetAddress))
			return -1;
	}

	if (settings->RedirectionFlags & LB_TARGET_NETBIOS_NAME)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_RedirectionTargetNetBiosName,
		                                 redirection->TargetNetBiosName))
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

	if (settings->RedirectionFlags & LB_CLIENT_TSV_URL)
	{
		/* TsvUrl may not contain a null terminator */
		if (!freerdp_settings_set_pointer_len(settings, FreeRDP_RedirectionTsvUrl,
		                                      redirection->TsvUrl, redirection->TsvUrlLength))
			return -1;
	}

	if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESSES)
	{
		if (!freerdp_target_net_addresses_copy(settings, redirection->TargetNetAddresses,
		                                       redirection->TargetNetAddressesCount))
			return -1;
	}

	return 0;
}

static state_run_t rdp_recv_server_redirection_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 flags;
	UINT16 length;
	rdpRedirection* redirection = rdp->redirection;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return STATE_RUN_FAILED;

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
			return STATE_RUN_FAILED;
	}

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
	{
		/* See [MSFT-SDLBTS] (a.k.a. TS_Session_Directory.doc)
		 * load balance info example data:
		 * 0000  43 6f 6f 6b 69 65 3a 20 6d 73 74 73 3d 32 31 33  Cookie: msts=213
		 * 0010  34 30 32 36 34 33 32 2e 31 35 36 32 39 2e 30 30  4026432.15629.00
		 * 0020  30 30 0d 0a                                      00..
		 */
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			return STATE_RUN_FAILED;

		Stream_Read_UINT32(s, redirection->LoadBalanceInfoLength);

		if (!Stream_CheckAndLogRequiredLength(TAG, s, redirection->LoadBalanceInfoLength))
			return STATE_RUN_FAILED;

		redirection->LoadBalanceInfo = (BYTE*)malloc(redirection->LoadBalanceInfoLength);

		if (!redirection->LoadBalanceInfo)
			return STATE_RUN_FAILED;

		Stream_Read(s, redirection->LoadBalanceInfo, redirection->LoadBalanceInfoLength);
		WLog_DBG(TAG, "loadBalanceInfo:");
		winpr_HexDump(TAG, WLOG_DEBUG, redirection->LoadBalanceInfo,
		              redirection->LoadBalanceInfoLength);
	}

	if (redirection->flags & LB_USERNAME)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->Username), 512))
			return STATE_RUN_FAILED;

		WLog_DBG(TAG, "Username: %s", redirection->Username);
	}

	if (redirection->flags & LB_DOMAIN)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->Domain), 52))
			return STATE_RUN_FAILED;

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
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			return STATE_RUN_FAILED;

		Stream_Read_UINT32(s, redirection->PasswordLength);

		/* [MS-RDPBCGR] specifies 512 bytes as the upper limit for the password length
		 * including the null terminatior(s). This should also be enough for the unknown
		 * password cookie format (see previous comment).
		 */

		if (!Stream_CheckAndLogRequiredLength(TAG, s, redirection->PasswordLength))
			return STATE_RUN_FAILED;

		if (redirection->PasswordLength > LB_PASSWORD_MAX_LENGTH)
			return STATE_RUN_FAILED;

		redirection->Password = (BYTE*)calloc(1, redirection->PasswordLength + sizeof(WCHAR));

		if (!redirection->Password)
			return STATE_RUN_FAILED;

		Stream_Read(s, redirection->Password, redirection->PasswordLength);
		WLog_DBG(TAG, "PasswordCookie:");
#if defined(WITH_DEBUG_REDIR)
		winpr_HexDump(TAG, WLOG_DEBUG, redirection->Password, redirection->PasswordLength);
#endif
	}

	if (redirection->flags & LB_TARGET_FQDN)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetFQDN), 512))
			return STATE_RUN_FAILED;

		WLog_DBG(TAG, "TargetFQDN: %s", redirection->TargetFQDN);
	}

	if (redirection->flags & LB_TARGET_NETBIOS_NAME)
	{
		if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetNetBiosName), 32))
			return STATE_RUN_FAILED;

		WLog_DBG(TAG, "TargetNetBiosName: %s", redirection->TargetNetBiosName);
	}

	if (redirection->flags & LB_CLIENT_TSV_URL)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
			return STATE_RUN_FAILED;

		Stream_Read_UINT32(s, redirection->TsvUrlLength);

		if (!Stream_CheckAndLogRequiredLength(TAG, s, redirection->TsvUrlLength))
			return STATE_RUN_FAILED;

		redirection->TsvUrl = (BYTE*)malloc(redirection->TsvUrlLength);

		if (!redirection->TsvUrl)
			return STATE_RUN_FAILED;

		Stream_Read(s, redirection->TsvUrl, redirection->TsvUrlLength);
		WLog_DBG(TAG, "TsvUrl:");
		winpr_HexDump(TAG, WLOG_DEBUG, redirection->TsvUrl, redirection->TsvUrlLength);
	}

	if (redirection->flags & LB_TARGET_NET_ADDRESSES)
	{
		size_t i;
		UINT32 count;
		UINT32 targetNetAddressesLength;

		if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
			return STATE_RUN_FAILED;

		Stream_Read_UINT32(s, targetNetAddressesLength);
		Stream_Read_UINT32(s, redirection->TargetNetAddressesCount);
		count = redirection->TargetNetAddressesCount;
		redirection->TargetNetAddresses = (char**)calloc(count, sizeof(char*));

		if (!redirection->TargetNetAddresses)
			return STATE_RUN_FAILED;

		WLog_DBG(TAG, "TargetNetAddressesCount: %" PRIu32 "", redirection->TargetNetAddressesCount);

		for (i = 0; i < count; i++)
		{
			if (!rdp_redirection_read_unicode_string(s, &(redirection->TargetNetAddresses[i]), 80))
				return STATE_RUN_FAILED;

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
		return STATE_RUN_SUCCESS;

	return STATE_RUN_REDIRECT;
}

state_run_t rdp_recv_enhanced_security_redirection_packet(rdpRdp* rdp, wStream* s)
{
	state_run_t status = STATE_RUN_SUCCESS;

	if (!Stream_SafeSeek(s, 2)) /* pad2Octets (2 bytes) */
		return STATE_RUN_FAILED;

	status = rdp_recv_server_redirection_pdu(rdp, s);

	if (state_run_failed(status))
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

			for (i = 0; i < (int)redirection->TargetNetAddressesCount; i++)
			{
				free(redirection->TargetNetAddresses[i]);
			}

			free(redirection->TargetNetAddresses);
		}

		free(redirection);
	}
}
