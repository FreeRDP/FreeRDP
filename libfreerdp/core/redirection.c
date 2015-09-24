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

void rdp_print_redirection_flags(UINT32 flags)
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

BOOL rdp_redirection_read_string(wStream* s, char** str)
{
	UINT32 length;

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_ERR(TAG,  "rdp_redirection_read_string failure: cannot read length");
		return FALSE;
	}

	Stream_Read_UINT32(s, length);

	if (Stream_GetRemainingLength(s) < length)
	{
		WLog_ERR(TAG,  "rdp_redirection_read_string failure: incorrect length %d", length);
		return FALSE;
	}

	ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s), length / 2, str, 0, NULL, NULL);
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
		settings->RedirectionPassword = (BYTE*) malloc(settings->RedirectionPasswordLength);
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

BOOL rdp_recv_server_redirection_pdu(rdpRdp* rdp, wStream* s)
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

	if (redirection->flags & LB_TARGET_NET_ADDRESS)
	{
		if (!rdp_redirection_read_string(s, &(redirection->TargetNetAddress)))
			return -1;
	}

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
	{
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
		if (!rdp_redirection_read_string(s, &(redirection->Username)))
			return -1;

		WLog_DBG(TAG, "Username: %s", redirection->Username);
	}

	if (redirection->flags & LB_DOMAIN)
	{
		if (!rdp_redirection_read_string(s, &(redirection->Domain)))
			return FALSE;

		WLog_DBG(TAG, "Domain: %s", redirection->Domain);
	}

	if (redirection->flags & LB_PASSWORD)
	{
		/* Note: length (hopefully) includes double zero termination */
		if (Stream_GetRemainingLength(s) < 4)
			return -1;

		Stream_Read_UINT32(s, redirection->PasswordLength);
		redirection->Password = (BYTE*) malloc(redirection->PasswordLength);
		if (!redirection->Password)
			return -1;
		Stream_Read(s, redirection->Password, redirection->PasswordLength);

		WLog_DBG(TAG, "PasswordCookie:");
		winpr_HexDump(TAG, WLOG_DEBUG, redirection->Password, redirection->PasswordLength);
	}

	if (redirection->flags & LB_TARGET_FQDN)
	{
		if (!rdp_redirection_read_string(s, &(redirection->TargetFQDN)))
			return -1;

		WLog_DBG(TAG, "TargetFQDN: %s", redirection->TargetFQDN);
	}

	if (redirection->flags & LB_TARGET_NETBIOS_NAME)
	{
		if (!rdp_redirection_read_string(s, &(redirection->TargetNetBiosName)))
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
			if (!rdp_redirection_read_string(s, &(redirection->TargetNetAddresses[i])))
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

int rdp_recv_redirection_packet(rdpRdp* rdp, wStream* s)
{
	int status = 0;
	status = rdp_recv_server_redirection_pdu(rdp, s);
	return status;
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

