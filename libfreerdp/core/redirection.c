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
	printf("redirectionFlags = {\n");

	if (flags & LB_TARGET_NET_ADDRESS)
		printf("\tLB_TARGET_NET_ADDRESS\n");
	if (flags & LB_LOAD_BALANCE_INFO)
		printf("\tLB_LOAD_BALANCE_INFO\n");
	if (flags & LB_USERNAME)
		printf("\tLB_USERNAME\n");
	if (flags & LB_DOMAIN)
		printf("\tLB_DOMAIN\n");
	if (flags & LB_PASSWORD)
		printf("\tLB_PASSWORD\n");
	if (flags & LB_DONTSTOREUSERNAME)
		printf("\tLB_DONTSTOREUSERNAME\n");
	if (flags & LB_SMARTCARD_LOGON)
		printf("\tLB_SMARTCARD_LOGON\n");
	if (flags & LB_NOREDIRECT)
		printf("\tLB_NOREDIRECT\n");
	if (flags & LB_TARGET_FQDN)
		printf("\tLB_TARGET_FQDN\n");
	if (flags & LB_TARGET_NETBIOS_NAME)
		printf("\tLB_TARGET_NETBIOS_NAME\n");
	if (flags & LB_TARGET_NET_ADDRESSES)
		printf("\tLB_TARGET_NET_ADDRESSES\n");
	if (flags & LB_CLIENT_TSV_URL)
		printf("\tLB_CLIENT_TSV_URL\n");
	if (flags & LB_SERVER_TSV_CAPABLE)
		printf("\tLB_SERVER_TSV_CAPABLE\n");

	printf("}\n");
}

BOOL rdp_recv_server_redirection_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 flags;
	UINT16 length;
	rdpRedirection* redirection = rdp->redirection;

	if(stream_get_left(s) < 12)
		return FALSE;
	stream_read_UINT16(s, flags); /* flags (2 bytes) */
	stream_read_UINT16(s, length); /* length (2 bytes) */
	stream_read_UINT32(s, redirection->sessionID); /* sessionID (4 bytes) */
	stream_read_UINT32(s, redirection->flags); /* redirFlags (4 bytes) */

	DEBUG_REDIR("flags: 0x%04X, length:%d, sessionID:0x%08X", flags, length, redirection->sessionID);

#ifdef WITH_DEBUG_REDIR
	rdp_print_redirection_flags(redirection->flags);
#endif

	if (redirection->flags & LB_TARGET_NET_ADDRESS)
	{
		if(!freerdp_string_read_length32(s, &redirection->targetNetAddress))
			return FALSE;
		DEBUG_REDIR("targetNetAddress: %s", redirection->targetNetAddress.ascii);
	}

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
	{
		if(stream_get_left(s) < 4)
			return FALSE;
		stream_read_UINT32(s, redirection->LoadBalanceInfoLength);
		if(stream_get_left(s) < redirection->LoadBalanceInfoLength)
			return FALSE;

		redirection->LoadBalanceInfo = (BYTE*) malloc(redirection->LoadBalanceInfoLength);
		stream_read(s, redirection->LoadBalanceInfo, redirection->LoadBalanceInfoLength);
#ifdef WITH_DEBUG_REDIR
		DEBUG_REDIR("loadBalanceInfo:");
		winpr_HexDump(redirection->LoadBalanceInfo, redirection->LoadBalanceInfoLength);
#endif
	}

	if (redirection->flags & LB_USERNAME)
	{
		if(!freerdp_string_read_length32(s, &redirection->username))
			return FALSE;
		DEBUG_REDIR("username: %s", redirection->username.ascii);
	}

	if (redirection->flags & LB_DOMAIN)
	{
		if(!freerdp_string_read_length32(s, &redirection->domain))
			return FALSE;
		DEBUG_REDIR("domain: %s", redirection->domain.ascii);
	}

	if (redirection->flags & LB_PASSWORD)
	{
		/* Note: length (hopefully) includes double zero termination */
		if(stream_get_left(s) < 4)
			return FALSE;
		stream_read_UINT32(s, redirection->PasswordCookieLength);
		redirection->PasswordCookie = (BYTE*) malloc(redirection->PasswordCookieLength);
		stream_read(s, redirection->PasswordCookie, redirection->PasswordCookieLength);

#ifdef WITH_DEBUG_REDIR
		DEBUG_REDIR("password_cookie:");
		winpr_HexDump(redirection->PasswordCookie, redirection->PasswordCookieLength);
#endif
	}

	if (redirection->flags & LB_TARGET_FQDN)
	{
		if(!freerdp_string_read_length32(s, &redirection->targetFQDN))
			return FALSE;
		DEBUG_REDIR("targetFQDN: %s", redirection->targetFQDN.ascii);
	}

	if (redirection->flags & LB_TARGET_NETBIOS_NAME)
	{
		if(!freerdp_string_read_length32(s, &redirection->targetNetBiosName))
			return FALSE;
		DEBUG_REDIR("targetNetBiosName: %s", redirection->targetNetBiosName.ascii);
	}

	if (redirection->flags & LB_CLIENT_TSV_URL)
	{
		if(!freerdp_string_read_length32(s, &redirection->tsvUrl))
			return FALSE;
		DEBUG_REDIR("tsvUrl: %s", redirection->tsvUrl.ascii);
	}

	if (redirection->flags & LB_TARGET_NET_ADDRESSES)
	{
		int i;
		UINT32 count;
		UINT32 targetNetAddressesLength;

		if(stream_get_left(s) < 8)
			return FALSE;
		stream_read_UINT32(s, targetNetAddressesLength);

		stream_read_UINT32(s, redirection->targetNetAddressesCount);
		count = redirection->targetNetAddressesCount;

		redirection->targetNetAddresses = (rdpString*) malloc(count * sizeof(rdpString));
		ZeroMemory(redirection->targetNetAddresses, count * sizeof(rdpString));

		for (i = 0; i < (int) count; i++)
		{
			if(!freerdp_string_read_length32(s, &redirection->targetNetAddresses[i]))
				return FALSE;
			DEBUG_REDIR("targetNetAddresses: %s", (&redirection->targetNetAddresses[i])->ascii);
		}
	}

	if (!stream_skip(s, 8)) /* pad (8 bytes) */
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
	return stream_skip(s, 2) && 					/* pad2Octets (2 bytes) */
		rdp_recv_server_redirection_pdu(rdp, s) &&
		stream_skip(s, 1);							/* pad2Octets (1 byte) */
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
		freerdp_string_free(&redirection->tsvUrl);
		freerdp_string_free(&redirection->username);
		freerdp_string_free(&redirection->domain);
		freerdp_string_free(&redirection->targetFQDN);
		freerdp_string_free(&redirection->targetNetBiosName);
		freerdp_string_free(&redirection->targetNetAddress);

		if (redirection->LoadBalanceInfo)
			free(redirection->LoadBalanceInfo);

		if (redirection->PasswordCookie)
			free(redirection->PasswordCookie);

		if (redirection->targetNetAddresses != NULL)
		{
			int i;

			for (i = 0; i < (int) redirection->targetNetAddressesCount; i++)
				freerdp_string_free(&redirection->targetNetAddresses[i]);

			free(redirection->targetNetAddresses);
		}

		free(redirection);
	}
}


