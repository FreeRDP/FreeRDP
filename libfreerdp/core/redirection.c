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

#include "connection.h"

#include "redirection.h"

void rdp_print_redirection_flags(UINT32 flags)
{
	fprintf(stderr, "redirectionFlags = {\n");

	if (flags & LB_TARGET_NET_ADDRESS)
		fprintf(stderr, "\tLB_TARGET_NET_ADDRESS\n");
	if (flags & LB_LOAD_BALANCE_INFO)
		fprintf(stderr, "\tLB_LOAD_BALANCE_INFO\n");
	if (flags & LB_USERNAME)
		fprintf(stderr, "\tLB_USERNAME\n");
	if (flags & LB_DOMAIN)
		fprintf(stderr, "\tLB_DOMAIN\n");
	if (flags & LB_PASSWORD)
		fprintf(stderr, "\tLB_PASSWORD\n");
	if (flags & LB_DONTSTOREUSERNAME)
		fprintf(stderr, "\tLB_DONTSTOREUSERNAME\n");
	if (flags & LB_SMARTCARD_LOGON)
		fprintf(stderr, "\tLB_SMARTCARD_LOGON\n");
	if (flags & LB_NOREDIRECT)
		fprintf(stderr, "\tLB_NOREDIRECT\n");
	if (flags & LB_TARGET_FQDN)
		fprintf(stderr, "\tLB_TARGET_FQDN\n");
	if (flags & LB_TARGET_NETBIOS_NAME)
		fprintf(stderr, "\tLB_TARGET_NETBIOS_NAME\n");
	if (flags & LB_TARGET_NET_ADDRESSES)
		fprintf(stderr, "\tLB_TARGET_NET_ADDRESSES\n");
	if (flags & LB_CLIENT_TSV_URL)
		fprintf(stderr, "\tLB_CLIENT_TSV_URL\n");
	if (flags & LB_SERVER_TSV_CAPABLE)
		fprintf(stderr, "\tLB_SERVER_TSV_CAPABLE\n");

	fprintf(stderr, "}\n");
}

BOOL rdp_redirection_read_string(wStream* s, char** str)
{
	UINT32 length;

	if (Stream_GetRemainingLength(s) < 4)
	{
		fprintf(stderr, "rdp_redirection_read_string failure: cannot read length\n");
		return FALSE;
	}

	Stream_Read_UINT32(s, length);

	if (Stream_GetRemainingLength(s) < length)
	{
		fprintf(stderr, "rdp_redirection_read_string failure: incorrect length %d\n", length);
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
	}
	else if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESS)
	{
		free(settings->TargetNetAddress);
		settings->TargetNetAddress = _strdup(redirection->TargetNetAddress);
	}
	else if (settings->RedirectionFlags & LB_TARGET_NETBIOS_NAME)
	{
		free(settings->RedirectionTargetNetBiosName);
		settings->RedirectionTargetNetBiosName = _strdup(redirection->TargetNetBiosName);
	}

	if (settings->RedirectionFlags & LB_USERNAME)
	{
		free(settings->RedirectionUsername);
		settings->RedirectionUsername = _strdup(redirection->Username);
	}

	if (settings->RedirectionFlags & LB_DOMAIN)
	{
		free(settings->RedirectionDomain);
		settings->RedirectionDomain = _strdup(redirection->Domain);
	}

	if (settings->RedirectionFlags & LB_PASSWORD)
	{
		/* Password may be a cookie without a null terminator */
		free(settings->RedirectionPassword);
		settings->RedirectionPasswordLength = redirection->PasswordLength;
		settings->RedirectionPassword = (BYTE*) malloc(settings->RedirectionPasswordLength);
		CopyMemory(settings->RedirectionPassword, redirection->Password, settings->RedirectionPasswordLength);
	}

	if (settings->RedirectionFlags & LB_CLIENT_TSV_URL)
	{
		/* TsvUrl may not contain a null terminator */
		free(settings->RedirectionTsvUrl);
		settings->RedirectionTsvUrlLength = redirection->TsvUrlLength;
		settings->RedirectionTsvUrl = (BYTE*) malloc(settings->RedirectionTsvUrlLength);
		CopyMemory(settings->RedirectionTsvUrl, redirection->TsvUrl, settings->RedirectionTsvUrlLength);
	}

	if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESSES)
	{
		UINT32 i;

		freerdp_target_net_addresses_free(settings);

		settings->TargetNetAddressCount = redirection->TargetNetAddressesCount;
		settings->TargetNetAddresses = (char**) malloc(sizeof(char*) * settings->TargetNetAddressCount);

		for (i = 0; i < settings->TargetNetAddressCount; i++)
		{
			settings->TargetNetAddresses[i] = _strdup(redirection->TargetNetAddresses[i]);
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

	WLog_Print(redirection->log, WLOG_DEBUG, "flags: 0x%04X, redirFlags: 0x%04X length: %d, sessionID: 0x%08X",
			flags, redirection->flags, length, redirection->sessionID);

#ifdef WITH_DEBUG_REDIR
	rdp_print_redirection_flags(redirection->flags);
#endif

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
		Stream_Read(s, redirection->LoadBalanceInfo, redirection->LoadBalanceInfoLength);
#ifdef WITH_DEBUG_REDIR
		DEBUG_REDIR("loadBalanceInfo:");
		winpr_HexDump(redirection->LoadBalanceInfo, redirection->LoadBalanceInfoLength);
#endif
	}

	if (redirection->flags & LB_USERNAME)
	{
		if (!rdp_redirection_read_string(s, &(redirection->Username)))
			return -1;

		WLog_Print(redirection->log, WLOG_DEBUG, "Username: %s", redirection->Username);
	}

	if (redirection->flags & LB_DOMAIN)
	{
		if (!rdp_redirection_read_string(s, &(redirection->Domain)))
			return FALSE;

		WLog_Print(redirection->log, WLOG_DEBUG, "Domain: %s", redirection->Domain);
	}

	if (redirection->flags & LB_PASSWORD)
	{
		/* Note: length (hopefully) includes double zero termination */
		if (Stream_GetRemainingLength(s) < 4)
			return -1;

		Stream_Read_UINT32(s, redirection->PasswordLength);
		redirection->Password = (BYTE*) malloc(redirection->PasswordLength);
		Stream_Read(s, redirection->Password, redirection->PasswordLength);

#ifdef WITH_DEBUG_REDIR
		DEBUG_REDIR("PasswordCookie:");
		winpr_HexDump(redirection->Password, redirection->PasswordLength);
#endif
	}

	if (redirection->flags & LB_TARGET_FQDN)
	{
		if (!rdp_redirection_read_string(s, &(redirection->TargetFQDN)))
			return -1;

		WLog_Print(redirection->log, WLOG_DEBUG, "TargetFQDN: %s", redirection->TargetFQDN);
	}

	if (redirection->flags & LB_TARGET_NETBIOS_NAME)
	{
		if (!rdp_redirection_read_string(s, &(redirection->TargetNetBiosName)))
			return -1;

		WLog_Print(redirection->log, WLOG_DEBUG, "TargetNetBiosName: %s", redirection->TargetNetBiosName);
	}

	if (redirection->flags & LB_CLIENT_TSV_URL)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return -1;

		Stream_Read_UINT32(s, redirection->TsvUrlLength);

		if (Stream_GetRemainingLength(s) < redirection->TsvUrlLength)
			return -1;

		redirection->TsvUrl = (BYTE*) malloc(redirection->TsvUrlLength);
		Stream_Read(s, redirection->TsvUrl, redirection->TsvUrlLength);

#ifdef WITH_DEBUG_REDIR
		DEBUG_REDIR("TsvUrl:");
		winpr_HexDump(redirection->TsvUrl, redirection->TsvUrlLength);
#endif
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

		redirection->TargetNetAddresses = (char**) malloc(count * sizeof(char*));
		ZeroMemory(redirection->TargetNetAddresses, count * sizeof(char*));

		WLog_Print(redirection->log, WLOG_DEBUG, "TargetNetAddressesCount: %d", redirection->TargetNetAddressesCount);

		for (i = 0; i < (int) count; i++)
		{
			if (!rdp_redirection_read_string(s, &(redirection->TargetNetAddresses[i])))
				return FALSE;

			WLog_Print(redirection->log, WLOG_DEBUG, "TargetNetAddresses[%d]: %s", i, redirection->TargetNetAddresses[i]);
		}
	}

	if (!Stream_SafeSeek(s, 8)) /* pad (8 bytes) */
		return -1;

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

	if (!Stream_SafeSeek(s, 1)) /* pad2Octets (1 byte) */
		return -1;

	return status;
}

rdpRedirection* redirection_new()
{
	rdpRedirection* redirection;

	redirection = (rdpRedirection*) calloc(1, sizeof(rdpRedirection));

	if (redirection)
	{
		WLog_Init();
		redirection->log = WLog_Get("com.freerdp.core.redirection");

#ifdef WITH_DEBUG_REDIR
		WLog_SetLogLevel(redirection->log, WLOG_TRACE);
#endif
	}

	return redirection;
}

void redirection_free(rdpRedirection* redirection)
{
	if (redirection)
	{
		if (redirection->TsvUrl)
			free(redirection->TsvUrl);

		if (redirection->Username)
			free(redirection->Username);

		if (redirection->Domain)
			free(redirection->Domain);

		if (redirection->TargetFQDN)
			free(redirection->TargetFQDN);

		if (redirection->TargetNetBiosName)
			free(redirection->TargetNetBiosName);

		if (redirection->TargetNetAddress)
			free(redirection->TargetNetAddress);

		if (redirection->LoadBalanceInfo)
			free(redirection->LoadBalanceInfo);

		if (redirection->Password)
			free(redirection->Password);

		if (redirection->TargetNetAddresses)
		{
			int i;

			for (i = 0; i < (int) redirection->TargetNetAddressesCount; i++)
			{
				if (redirection->TargetNetAddresses[i])
					free(redirection->TargetNetAddresses[i]);
			}

			free(redirection->TargetNetAddresses);
		}

		free(redirection);
	}
}

