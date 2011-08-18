/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RDP Server Peer
 *
 * Copyright 2011 Vic Lee
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

#include "peer.h"

static boolean freerdp_peer_initialize(freerdp_peer* client)
{
	rdpPeer* peer = (rdpPeer*)client->peer;

	peer->state = CONNECTION_STATE_INITIAL;

	return True;
}

static boolean freerdp_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	rdpPeer* peer = (rdpPeer*)client->peer;

	rfds[*rcount] = (void*)(long)(peer->rdp->transport->tcp->sockfd);
	(*rcount)++;

	return True;
}

static boolean freerdp_peer_check_fds(freerdp_peer* client)
{
	rdpPeer* peer = (rdpPeer*)client->peer;
	rdpRdp* rdp;
	int status;

	rdp = (rdpRdp*) peer->rdp;

	status = rdp_check_fds(rdp);
	if (status < 0)
		return False;

	return True;
}

static int peer_process_connection_nego(rdpPeer* peer, STREAM* s)
{
	if (!nego_recv_request(peer->rdp->nego, s))
		return -1;
	if (peer->rdp->nego->requested_protocols == PROTOCOL_RDP)
	{
		printf("Standard RDP encryption is not supported.\n");
		return -1;
	}

	printf("Requested protocols:");
	if ((peer->rdp->nego->requested_protocols | PROTOCOL_TLS))
	{
		printf(" TLS");
		if (peer->rdp->settings->tls_security)
		{
			printf("(Y)");
			peer->rdp->nego->selected_protocol |= PROTOCOL_TLS;
		}
		else
			printf("(n)");
	}
	if ((peer->rdp->nego->requested_protocols | PROTOCOL_NLA))
	{
		printf(" NLA");
		if (peer->rdp->settings->nla_security)
		{
			printf("(Y)");
			peer->rdp->nego->selected_protocol |= PROTOCOL_NLA;
		}
		else
			printf("(n)");
	}
	printf("\n");

	nego_send_negotiation_response(peer->rdp->nego);

	peer->state = CONNECTION_STATE_NEGO;

	return 1;
}

static int peer_recv_callback(rdpTransport* transport, STREAM* s, void* extra)
{
	rdpPeer* peer = (rdpPeer*)extra;

	switch (peer->state)
	{
		case CONNECTION_STATE_INITIAL:
			return peer_process_connection_nego(peer, s);

		default:
			printf("Invalid state %d\n", peer->state);
			return -1;
	}

	return 1;
}

static void freerdp_peer_disconnect(freerdp_peer* client)
{
}

freerdp_peer* freerdp_peer_new(int sockfd)
{
	freerdp_peer* client;
	rdpPeer* peer;

	client = xnew(freerdp_peer);

	client->Initialize = freerdp_peer_initialize;
	client->GetFileDescriptor = freerdp_peer_get_fds;
	client->CheckFileDescriptor = freerdp_peer_check_fds;
	client->Disconnect = freerdp_peer_disconnect;

	peer = xnew(rdpPeer);
	peer->client = client;
	peer->rdp = rdp_new(NULL);

	client->peer = (void*)peer;
	client->settings = peer->rdp->settings;

	transport_attach(peer->rdp->transport, sockfd);

	peer->rdp->transport->recv_callback = peer_recv_callback;
	peer->rdp->transport->recv_extra = peer;
	transport_set_blocking_mode(peer->rdp->transport, False);

	return client;
}

void freerdp_peer_free(freerdp_peer* client)
{
	rdpPeer* peer = (rdpPeer*)client->peer;

	rdp_free(peer->rdp);
	xfree(peer);
	xfree(client);
}

