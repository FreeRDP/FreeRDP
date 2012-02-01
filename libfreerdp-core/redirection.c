/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include "connection.h"

#include "redirection.h"

void rdp_print_redirection_flags(uint32 flags)
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

boolean rdp_recv_server_redirection_pdu(rdpRdp* rdp, STREAM* s)
{
	uint16 flags;
	uint16 length;
	rdpRedirection* redirection = rdp->redirection;

	stream_read_uint16(s, flags); /* flags (2 bytes) */
	stream_read_uint16(s, length); /* length (2 bytes) */
	stream_read_uint32(s, redirection->sessionID); /* sessionID (4 bytes) */
	stream_read_uint32(s, redirection->flags); /* redirFlags (4 bytes) */

	DEBUG_REDIR("flags: 0x%04X, length:%d, sessionID:0x%08X", flags, length, redirection->sessionID);

#ifdef WITH_DEBUG_REDIR
	rdp_print_redirection_flags(redirection->flags);
#endif

	if (redirection->flags & LB_TARGET_NET_ADDRESS)
	{
		freerdp_string_read_length32(s, &redirection->targetNetAddress, rdp->settings->uniconv);
		DEBUG_REDIR("targetNetAddress: %s", redirection->targetNetAddress.ascii);
	}

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
	{
		uint32 loadBalanceInfoLength;
		stream_read_uint32(s, loadBalanceInfoLength);
		freerdp_blob_alloc(&redirection->loadBalanceInfo, loadBalanceInfoLength);
		stream_read(s, redirection->loadBalanceInfo.data, loadBalanceInfoLength);
#ifdef WITH_DEBUG_REDIR
		DEBUG_REDIR("loadBalanceInfo:");
		freerdp_hexdump(redirection->loadBalanceInfo.data, redirection->loadBalanceInfo.length);
#endif
	}

	if (redirection->flags & LB_USERNAME)
	{
		freerdp_string_read_length32(s, &redirection->username, rdp->settings->uniconv);
		DEBUG_REDIR("username: %s", redirection->username.ascii);
	}

	if (redirection->flags & LB_DOMAIN)
	{
		freerdp_string_read_length32(s, &redirection->domain, rdp->settings->uniconv);
		DEBUG_REDIR("domain: %s", redirection->domain.ascii);
	}

	if (redirection->flags & LB_PASSWORD)
	{
		uint32 passwordLength;
		stream_read_uint32(s, passwordLength);
		freerdp_blob_alloc(&redirection->password_cookie, passwordLength);
		stream_read(s, redirection->password_cookie.data, passwordLength);

#ifdef WITH_DEBUG_REDIR
		DEBUG_REDIR("password_cookie:");
		freerdp_hexdump(redirection->password_cookie.data, redirection->password_cookie.length);
#endif
	}

	if (redirection->flags & LB_TARGET_FQDN)
	{
		freerdp_string_read_length32(s, &redirection->targetFQDN, rdp->settings->uniconv);
		DEBUG_REDIR("targetFQDN: %s", redirection->targetFQDN.ascii);
	}

	if (redirection->flags & LB_TARGET_NETBIOS_NAME)
	{
		freerdp_string_read_length32(s, &redirection->targetNetBiosName, rdp->settings->uniconv);
		DEBUG_REDIR("targetNetBiosName: %s", redirection->targetNetBiosName.ascii);
	}

	if (redirection->flags & LB_CLIENT_TSV_URL)
	{
		freerdp_string_read_length32(s, &redirection->tsvUrl, rdp->settings->uniconv);
		DEBUG_REDIR("tsvUrl: %s", redirection->tsvUrl.ascii);
	}

	if (redirection->flags & LB_TARGET_NET_ADDRESSES)
	{
		int i;
		uint32 count;
		uint32 targetNetAddressesLength;

		stream_read_uint32(s, targetNetAddressesLength);

		stream_read_uint32(s, redirection->targetNetAddressesCount);
		count = redirection->targetNetAddressesCount;

		redirection->targetNetAddresses = (rdpString*) xzalloc(count * sizeof(rdpString));

		for (i = 0; i < (int) count; i++)
		{
			freerdp_string_read_length32(s, &redirection->targetNetAddresses[i], rdp->settings->uniconv);
			DEBUG_REDIR("targetNetAddresses: %s", (&redirection->targetNetAddresses[i])->ascii);
		}
	}

	stream_seek(s, 8); /* pad (8 bytes) */

	if (redirection->flags & LB_NOREDIRECT)
		return true;
	else
		return rdp_client_redirect(rdp);
}

boolean rdp_recv_redirection_packet(rdpRdp* rdp, STREAM* s)
{
	rdp_recv_server_redirection_pdu(rdp, s);
	return true;
}

boolean rdp_recv_enhanced_security_redirection_packet(rdpRdp* rdp, STREAM* s)
{
	stream_seek_uint16(s); /* pad2Octets (2 bytes) */
	rdp_recv_server_redirection_pdu(rdp, s);
	stream_seek_uint8(s); /* pad2Octets (1 byte) */
	return true;
}

rdpRedirection* redirection_new()
{
	rdpRedirection* redirection;

	redirection = (rdpRedirection*) xzalloc(sizeof(rdpRedirection));

	if (redirection != NULL)
	{

	}

	return redirection;
}

void redirection_free(rdpRedirection* redirection)
{
	if (redirection != NULL)
	{
		//these four have already been freed in settings_free() and freerdp_string_free() checks for NULL
		redirection->username.ascii = NULL;
		redirection->domain.ascii = NULL;
		redirection->targetNetAddress.ascii = NULL;
		redirection->targetNetBiosName.ascii = NULL;

		freerdp_string_free(&redirection->tsvUrl);
		freerdp_string_free(&redirection->username);
		freerdp_string_free(&redirection->domain);
		freerdp_blob_free(&redirection->password_cookie);
		freerdp_string_free(&redirection->targetFQDN);
		freerdp_string_free(&redirection->targetNetBiosName);
		freerdp_string_free(&redirection->targetNetAddress);
		freerdp_blob_free(&redirection->loadBalanceInfo);

		if (redirection->targetNetAddresses != NULL)
		{
			int i;

			for (i = 0; i < (int) redirection->targetNetAddressesCount; i++)
				freerdp_string_free(&redirection->targetNetAddresses[i]);

			xfree(redirection->targetNetAddresses);
		}

		xfree(redirection);
	}
}


