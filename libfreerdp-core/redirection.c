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
	uint32 sessionID;
	uint32 redirFlags;
	uint32 targetNetAddressLength;
	uint32 loadBalanceInfoLength;
	uint32 userNameLength;
	uint32 domainLength;
	uint32 passwordLength;
	uint32 targetFQDNLength;
	uint32 targetNetBiosNameLength;
	uint32 tsvUrlLength;
	uint32 targetNetAddressesLength;

	stream_read_uint16(s, flags); /* flags (2 bytes) */
	stream_read_uint16(s, length); /* length (2 bytes) */
	stream_read_uint32(s, sessionID); /* sessionID (4 bytes) */
	stream_read_uint32(s, redirFlags); /* redirFlags (4 bytes) */

	DEBUG_REDIR("flags: 0x%04X, length:%d, sessionID:0x%08X", flags, length, sessionID);

#ifdef WITH_DEBUG_REDIR
	rdp_print_redirection_flags(redirFlags);
#endif

	if (redirFlags & LB_TARGET_NET_ADDRESS)
	{
		stream_read_uint32(s, targetNetAddressLength); /* targetNetAddressLength (4 bytes) */
		stream_seek(s, targetNetAddressLength);
	}

	if (redirFlags & LB_LOAD_BALANCE_INFO)
	{
		stream_read_uint32(s, loadBalanceInfoLength); /* loadBalanceInfoLength (4 bytes) */
		stream_seek(s, loadBalanceInfoLength);
	}

	if (redirFlags & LB_USERNAME)
	{
		stream_read_uint32(s, userNameLength); /* userNameLength (4 bytes) */
		stream_seek(s, userNameLength);
	}

	if (redirFlags & LB_DOMAIN)
	{
		stream_read_uint32(s, domainLength); /* domainLength (4 bytes) */
		stream_seek(s, domainLength);
	}

	if (redirFlags & LB_PASSWORD)
	{
		stream_read_uint32(s, passwordLength); /* passwordLength (4 bytes) */
		stream_seek(s, passwordLength);
	}

	if (redirFlags & LB_TARGET_FQDN)
	{
		stream_read_uint32(s, targetFQDNLength); /* targetFQDNLength (4 bytes) */
		stream_seek(s, targetFQDNLength);
	}

	if (redirFlags & LB_TARGET_NETBIOS_NAME)
	{
		stream_read_uint32(s, targetNetBiosNameLength); /* targetNetBiosNameLength (4 bytes) */
		stream_seek(s, targetNetBiosNameLength);
	}

	if (redirFlags & LB_CLIENT_TSV_URL)
	{
		stream_read_uint32(s, tsvUrlLength); /* tsvUrlLength (4 bytes) */
		stream_seek(s, tsvUrlLength);
	}

	if (redirFlags & LB_TARGET_NET_ADDRESSES)
	{
		stream_read_uint32(s, targetNetAddressesLength); /* targetNetAddressesLength (4 bytes) */
		stream_seek(s, targetNetAddressesLength);
	}

	stream_seek(s, 8); /* pad (8 bytes) */

	return True;
}

boolean rdp_recv_redirection_packet(rdpRdp* rdp, STREAM* s)
{
	return True;
}

boolean rdp_recv_enhanced_security_redirection_packet(rdpRdp* rdp, STREAM* s)
{
	stream_seek_uint16(s); /* pad2Octets (2 bytes) */
	rdp_recv_server_redirection_pdu(rdp, s);
	stream_seek_uint8(s); /* pad2Octets (1 byte) */
	return True;
}

