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

BOOL rdp_string_read_length32(wStream* s, rdpString* string)
{
	if(Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, string->length);

	if(Stream_GetRemainingLength(s) < string->length)
		return FALSE;

	string->unicode = (char*) malloc(string->length);
	Stream_Read(s, string->unicode, string->length);

	ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) string->unicode, string->length / 2, &string->ascii, 0, NULL, NULL);

	return TRUE;
}

void rdp_string_free(rdpString* string)
{
	if (string->unicode != NULL)
		free(string->unicode);

	if (string->ascii != NULL)
		free(string->ascii);
}

BOOL rdp_recv_server_redirection_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 flags;
	UINT16 length;
	rdpRedirection* redirection = rdp->redirection;

	if (Stream_GetRemainingLength(s) < 12)
		return FALSE;
	Stream_Read_UINT16(s, flags); /* flags (2 bytes) */
	Stream_Read_UINT16(s, length); /* length (2 bytes) */
	Stream_Read_UINT32(s, redirection->sessionID); /* sessionID (4 bytes) */
	Stream_Read_UINT32(s, redirection->flags); /* redirFlags (4 bytes) */

	DEBUG_REDIR("flags: 0x%04X, length:%d, sessionID:0x%08X", flags, length, redirection->sessionID);

#ifdef WITH_DEBUG_REDIR
	rdp_print_redirection_flags(redirection->flags);
#endif

	if (redirection->flags & LB_TARGET_NET_ADDRESS)
	{
		if (!rdp_string_read_length32(s, &redirection->targetNetAddress))
			return FALSE;
		DEBUG_REDIR("targetNetAddress: %s", redirection->targetNetAddress.ascii);
	}

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
	{
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;
		Stream_Read_UINT32(s, redirection->LoadBalanceInfoLength);
		if (Stream_GetRemainingLength(s) < redirection->LoadBalanceInfoLength)
			return FALSE;

		redirection->LoadBalanceInfo = (BYTE*) malloc(redirection->LoadBalanceInfoLength);
		Stream_Read(s, redirection->LoadBalanceInfo, redirection->LoadBalanceInfoLength);
#ifdef WITH_DEBUG_REDIR
		DEBUG_REDIR("loadBalanceInfo:");
		winpr_HexDump(redirection->LoadBalanceInfo, redirection->LoadBalanceInfoLength);
#endif
	}

	if (redirection->flags & LB_USERNAME)
	{
		if (!rdp_string_read_length32(s, &redirection->username))
			return FALSE;
		DEBUG_REDIR("username: %s", redirection->username.ascii);
	}

	if (redirection->flags & LB_DOMAIN)
	{
		if (!rdp_string_read_length32(s, &redirection->domain))
			return FALSE;
		DEBUG_REDIR("domain: %s", redirection->domain.ascii);
	}

	if (redirection->flags & LB_PASSWORD)
	{
		/* Note: length (hopefully) includes double zero termination */
		if (Stream_GetRemainingLength(s) < 4)
			return FALSE;
		Stream_Read_UINT32(s, redirection->PasswordCookieLength);
		redirection->PasswordCookie = (BYTE*) malloc(redirection->PasswordCookieLength);
		Stream_Read(s, redirection->PasswordCookie, redirection->PasswordCookieLength);

#ifdef WITH_DEBUG_REDIR
		DEBUG_REDIR("password_cookie:");
		winpr_HexDump(redirection->PasswordCookie, redirection->PasswordCookieLength);
#endif
	}

	if (redirection->flags & LB_TARGET_FQDN)
	{
		if (!rdp_string_read_length32(s, &redirection->targetFQDN))
			return FALSE;
		DEBUG_REDIR("targetFQDN: %s", redirection->targetFQDN.ascii);
	}

	if (redirection->flags & LB_TARGET_NETBIOS_NAME)
	{
		if (!rdp_string_read_length32(s, &redirection->targetNetBiosName))
			return FALSE;
		DEBUG_REDIR("targetNetBiosName: %s", redirection->targetNetBiosName.ascii);
	}

	if (redirection->flags & LB_CLIENT_TSV_URL)
	{
		if (!rdp_string_read_length32(s, &redirection->tsvUrl))
			return FALSE;
		DEBUG_REDIR("tsvUrl: %s", redirection->tsvUrl.ascii);
	}

	if (redirection->flags & LB_TARGET_NET_ADDRESSES)
	{
		int i;
		UINT32 count;
		UINT32 targetNetAddressesLength;

		if (Stream_GetRemainingLength(s) < 8)
			return FALSE;
		Stream_Read_UINT32(s, targetNetAddressesLength);

		Stream_Read_UINT32(s, redirection->targetNetAddressesCount);
		count = redirection->targetNetAddressesCount;

		redirection->targetNetAddresses = (rdpString*) malloc(count * sizeof(rdpString));
		ZeroMemory(redirection->targetNetAddresses, count * sizeof(rdpString));

		for (i = 0; i < (int) count; i++)
		{
			if (!rdp_string_read_length32(s, &redirection->targetNetAddresses[i]))
				return FALSE;
			DEBUG_REDIR("targetNetAddresses: %s", (&redirection->targetNetAddresses[i])->ascii);
		}
	}

	if (!Stream_SafeSeek(s, 8)) /* pad (8 bytes) */
		return FALSE;

	if (redirection->flags & LB_NOREDIRECT)
		return TRUE;
	else
		return rdp_client_redirect(rdp);
}

BOOL rdp_recv_redirection_packet(rdpRdp* rdp, wStream* s)
{
	return rdp_recv_server_redirection_pdu(rdp, s);
}

BOOL rdp_recv_enhanced_security_redirection_packet(rdpRdp* rdp, wStream* s)
{
	return Stream_SafeSeek(s, 2) && 					/* pad2Octets (2 bytes) */
		rdp_recv_server_redirection_pdu(rdp, s) &&
		Stream_SafeSeek(s, 1);							/* pad2Octets (1 byte) */
}

rdpRedirection* redirection_new()
{
	rdpRedirection* redirection;

	redirection = (rdpRedirection*) malloc(sizeof(rdpRedirection));

	if (redirection != NULL)
	{
		ZeroMemory(redirection, sizeof(rdpRedirection));
	}

	return redirection;
}

void redirection_free(rdpRedirection* redirection)
{
	if (redirection != NULL)
	{
		rdp_string_free(&redirection->tsvUrl);
		rdp_string_free(&redirection->username);
		rdp_string_free(&redirection->domain);
		rdp_string_free(&redirection->targetFQDN);
		rdp_string_free(&redirection->targetNetBiosName);
		rdp_string_free(&redirection->targetNetAddress);

		if (redirection->LoadBalanceInfo)
			free(redirection->LoadBalanceInfo);

		if (redirection->PasswordCookie)
			free(redirection->PasswordCookie);

		if (redirection->targetNetAddresses != NULL)
		{
			int i;

			for (i = 0; i < (int) redirection->targetNetAddressesCount; i++)
				rdp_string_free(&redirection->targetNetAddresses[i]);

			free(redirection->targetNetAddresses);
		}

		free(redirection);
	}
}


