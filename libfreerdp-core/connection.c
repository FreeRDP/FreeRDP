/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Connection Sequence
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
#include "info.h"

/**
 *                                      Connection Sequence
 *     client                                                                    server
 *        |                                                                         |
 *        |-----------------------X.224 Connection Request PDU--------------------->|
 *        |<----------------------X.224 Connection Confirm PDU----------------------|
 *        |-------MCS Connect-Initial PDU with GCC Conference Create Request------->|
 *        |<-----MCS Connect-Response PDU with GCC Conference Create Response-------|
 *        |------------------------MCS Erect Domain Request PDU-------------------->|
 *        |------------------------MCS Attach User Request PDU--------------------->|
 *        |<-----------------------MCS Attach User Confirm PDU----------------------|
 *        |------------------------MCS Channel Join Request PDU-------------------->|
 *        |<-----------------------MCS Channel Join Confirm PDU---------------------|
 *        |----------------------------Security Exchange PDU----------------------->|
 *        |-------------------------------Client Info PDU-------------------------->|
 *        |<---------------------License Error PDU - Valid Client-------------------|
 *        |<-----------------------------Demand Active PDU--------------------------|
 *        |------------------------------Confirm Active PDU------------------------>|
 *        |-------------------------------Synchronize PDU-------------------------->|
 *        |---------------------------Control PDU - Cooperate---------------------->|
 *        |------------------------Control PDU - Request Control------------------->|
 *        |--------------------------Persistent Key List PDU(s)-------------------->|
 *        |--------------------------------Font List PDU--------------------------->|
 *        |<------------------------------Synchronize PDU---------------------------|
 *        |<--------------------------Control PDU - Cooperate-----------------------|
 *        |<-----------------------Control PDU - Granted Control--------------------|
 *        |<-------------------------------Font Map PDU-----------------------------|
 *
 */

/**
 * Establish RDP Connection.\n
 * @msdn{cc240452}
 * @param rdp RDP module
 */

boolean rdp_client_connect(rdpRdp* rdp)
{
	boolean ret;

	rdp->settings->autologon = 1;

	nego_init(rdp->nego);
	nego_set_target(rdp->nego, rdp->settings->hostname, rdp->settings->port);
	nego_set_cookie(rdp->nego, rdp->settings->username);
	nego_enable_rdp(rdp->nego, rdp->settings->rdp_security);
	nego_enable_nla(rdp->nego, rdp->settings->nla_security);
	nego_enable_tls(rdp->nego, rdp->settings->tls_security);

	if (nego_connect(rdp->nego) != True)
	{
		printf("Error: protocol security negotiation failure\n");
		return False;
	}

	ret = False;
	if (rdp->nego->selected_protocol & PROTOCOL_NLA)
		ret = transport_connect_nla(rdp->transport);
	else if (rdp->nego->selected_protocol & PROTOCOL_TLS)
		ret = transport_connect_tls(rdp->transport);
	else if (rdp->nego->selected_protocol & PROTOCOL_RDP)
		ret = transport_connect_rdp(rdp->transport);

	if (!ret)
		return False;

	if (mcs_connect(rdp->mcs) != True)
	{
		printf("Error: Multipoint Connection Service (MCS) connection failure\n");
		return False;
	}

	rdp_send_client_info(rdp);

	if (license_connect(rdp->license) != True)
	{
		printf("Error: license connection sequence failure\n");
		return False;
	}

	rdp->licensed = True;

	rdp_client_activate(rdp);
	rdp_set_blocking_mode(rdp, False);

	return True;
}

boolean rdp_server_accept_nego(rdpRdp* rdp, STREAM* s)
{
	if (!nego_recv_request(rdp->nego, s))
		return False;
	if (rdp->nego->requested_protocols == PROTOCOL_RDP)
	{
		printf("Standard RDP encryption is not supported.\n");
		return False;
	}

	printf("Requested protocols:");
	if ((rdp->nego->requested_protocols | PROTOCOL_TLS))
	{
		printf(" TLS");
		if (rdp->settings->tls_security)
		{
			printf("(Y)");
			rdp->nego->selected_protocol |= PROTOCOL_TLS;
		}
		else
			printf("(n)");
	}
	if ((rdp->nego->requested_protocols | PROTOCOL_NLA))
	{
		printf(" NLA");
		if (rdp->settings->nla_security)
		{
			printf("(Y)");
			rdp->nego->selected_protocol |= PROTOCOL_NLA;
		}
		else
			printf("(n)");
	}
	printf("\n");

	nego_send_negotiation_response(rdp->nego);

	rdp->state = CONNECTION_STATE_NEGO;

	return True;
}

