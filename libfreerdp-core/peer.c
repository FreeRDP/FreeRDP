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

	peer->rdp->settings->server_mode = True;
	peer->rdp->state = CONNECTION_STATE_INITIAL;

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

static boolean peer_recv_data_pdu(rdpPeer* peer, STREAM* s)
{
	uint8 type;
	uint16 length;
	uint32 share_id;

	if (!rdp_read_share_data_header(s, &length, &type, &share_id))
		return False;

	switch (type)
	{
		case DATA_PDU_TYPE_SYNCHRONIZE:
			if (!rdp_recv_client_synchronize_pdu(s))
				return False;
			break;

		case DATA_PDU_TYPE_CONTROL:
			if (!rdp_server_accept_client_control_pdu(peer->rdp, s))
				return False;
			break;

		case DATA_PDU_TYPE_BITMAP_CACHE_PERSISTENT_LIST:
			/* TODO: notify server bitmap cache data */
			break;

		case DATA_PDU_TYPE_FONT_LIST:
			if (!rdp_server_accept_client_font_list_pdu(peer->rdp, s))
				return False;
			if (peer->client->PostConnect)
			{
				if (!peer->client->PostConnect(peer->client))
					return False;
			}
			break;

		default:
			printf("Data PDU type %d\n", type);
			break;
	}

	return True;
}

static boolean peer_recv_tpkt_pdu(rdpPeer* peer, STREAM* s)
{
	uint16 length;
	uint16 pduType;
	uint16 pduLength;
	uint16 channelId;

	if (!rdp_read_header(peer->rdp, s, &length, &channelId))
	{
		printf("Incorrect RDP header.\n");
		return False;
	}

	if (channelId != MCS_GLOBAL_CHANNEL_ID)
	{
		/* TODO: process channel data from client */
	}
	else
	{
		if (!rdp_read_share_control_header(s, &pduLength, &pduType, &peer->rdp->settings->pdu_source))
			return False;

		switch (pduType)
		{
			case PDU_TYPE_DATA:
				if (!peer_recv_data_pdu(peer, s))
					return False;
				break;

			default:
				printf("Client sent pduType %d\n", pduType);
				return False;
		}
	}

	return True;
}

static boolean peer_recv_fastpath_pdu(rdpPeer* peer, STREAM* s)
{
	return True;
}

static boolean peer_recv_pdu(rdpPeer* peer, STREAM* s)
{
	if (tpkt_verify_header(s))
		return peer_recv_tpkt_pdu(peer, s);
	else
		return peer_recv_fastpath_pdu(peer, s);
}

static int peer_recv_callback(rdpTransport* transport, STREAM* s, void* extra)
{
	rdpPeer* peer = (rdpPeer*)extra;

	switch (peer->rdp->state)
	{
		case CONNECTION_STATE_INITIAL:
			if (!rdp_server_accept_nego(peer->rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_NEGO:
			if (!rdp_server_accept_mcs_connect_initial(peer->rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_MCS_CONNECT:
			if (!rdp_server_accept_mcs_erect_domain_request(peer->rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_MCS_ERECT_DOMAIN:
			if (!rdp_server_accept_mcs_attach_user_request(peer->rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_MCS_ATTACH_USER:
			if (!rdp_server_accept_mcs_channel_join_request(peer->rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_MCS_CHANNEL_JOIN:
			if (!rdp_server_accept_client_info(peer->rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_LICENSE:
			if (!rdp_server_accept_confirm_active(peer->rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_ACTIVE:
			if (!peer_recv_pdu(peer, s))
				return -1;
			break;

		default:
			printf("Invalid state %d\n", peer->rdp->state);
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
	client->input = peer->rdp->input;
	client->update = peer->rdp->update;

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

