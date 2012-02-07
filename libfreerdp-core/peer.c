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
	client->context->rdp->settings->server_mode = true;
	client->context->rdp->state = CONNECTION_STATE_INITIAL;
	if (client->context->rdp->settings->rdp_key_file != NULL) {
		client->context->rdp->settings->server_key =
		    key_new(client->context->rdp->settings->rdp_key_file);
	}

	return true;
}

static boolean freerdp_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	rfds[*rcount] = (void*)(long)(client->context->rdp->transport->tcp->sockfd);
	(*rcount)++;

	return true;
}

static boolean freerdp_peer_check_fds(freerdp_peer* client)
{
	rdpRdp* rdp;
	int status;

	rdp = client->context->rdp;

	status = rdp_check_fds(rdp);
	if (status < 0)
		return false;

	return true;
}

static boolean peer_recv_data_pdu(freerdp_peer* client, STREAM* s)
{
	uint8 type;
	uint16 length;
	uint32 share_id;
	uint8 compressed_type;
	uint16 compressed_len;


	if (!rdp_read_share_data_header(s, &length, &type, &share_id, &compressed_type, &compressed_len))
		return false;

	switch (type)
	{
		case DATA_PDU_TYPE_SYNCHRONIZE:
			if (!rdp_recv_client_synchronize_pdu(client->context->rdp, s))
				return false;
			break;

		case DATA_PDU_TYPE_CONTROL:
			if (!rdp_server_accept_client_control_pdu(client->context->rdp, s))
				return false;
			break;

		case DATA_PDU_TYPE_INPUT:
			if (!input_recv(client->context->rdp->input, s))
				return false;
			break;

		case DATA_PDU_TYPE_BITMAP_CACHE_PERSISTENT_LIST:
			/* TODO: notify server bitmap cache data */
			break;

		case DATA_PDU_TYPE_FONT_LIST:
			if (!rdp_server_accept_client_font_list_pdu(client->context->rdp, s))
				return false;
			if (client->PostConnect)
			{
				if (!client->PostConnect(client))
					return false;
				/**
				 * PostConnect should only be called once and should not be called
				 * after a reactivation sequence.
				 */
				client->PostConnect = NULL;
			}
			if (client->Activate)
			{
				/* Activate will be called everytime after the client is activated/reactivated. */
				if (!client->Activate(client))
					return false;
			}
			break;

		case DATA_PDU_TYPE_SHUTDOWN_REQUEST:
			mcs_send_disconnect_provider_ultimatum(client->context->rdp->mcs);
			return false;

		default:
			printf("Data PDU type %d\n", type);
			break;
	}

	return true;
}

static boolean peer_recv_tpkt_pdu(freerdp_peer* client, STREAM* s)
{
	rdpRdp *rdp;
	uint16 length;
	uint16 pduType;
	uint16 pduLength;
	uint16 pduSource;
	uint16 channelId;
	uint16 securityFlags;

	rdp = client->context->rdp;

	if (!rdp_read_header(rdp, s, &length, &channelId))
	{
		printf("Incorrect RDP header.\n");
		return false;
	}

	if (rdp->settings->encryption)
	{
		rdp_read_security_header(s, &securityFlags);
		if (securityFlags & SEC_ENCRYPT)
		{
			if (!rdp_decrypt(rdp, s, length - 4, securityFlags))
			{
				printf("rdp_decrypt failed\n");
				return false;
			}
		}
	}

	if (channelId != MCS_GLOBAL_CHANNEL_ID)
	{
		freerdp_channel_peer_process(client, s, channelId);
	}
	else
	{
		if (!rdp_read_share_control_header(s, &pduLength, &pduType, &pduSource))
			return false;

		client->settings->pdu_source = pduSource;

		switch (pduType)
		{
			case PDU_TYPE_DATA:
				if (!peer_recv_data_pdu(client, s))
					return false;
				break;

			default:
				printf("Client sent pduType %d\n", pduType);
				return false;
		}
	}

	return true;
}

static boolean peer_recv_fastpath_pdu(freerdp_peer* client, STREAM* s)
{
	uint16 length;
	rdpRdp* rdp;
	rdpFastPath* fastpath;

	rdp = client->context->rdp;
	fastpath = rdp->fastpath;
	length = fastpath_read_header_rdp(fastpath, s);

	if (length == 0 || length > stream_get_left(s))
	{
		printf("incorrect FastPath PDU header length %d\n", length);
		return false;
	}

	if (fastpath->encryptionFlags & FASTPATH_OUTPUT_ENCRYPTED)
	{
		rdp_decrypt(rdp, s, length, (fastpath->encryptionFlags & FASTPATH_OUTPUT_SECURE_CHECKSUM) ? SEC_SECURE_CHECKSUM : 0);
	}

	return fastpath_recv_inputs(fastpath, s);
}

static boolean peer_recv_pdu(freerdp_peer* client, STREAM* s)
{
	if (tpkt_verify_header(s))
		return peer_recv_tpkt_pdu(client, s);
	else
		return peer_recv_fastpath_pdu(client, s);
}

static boolean peer_recv_callback(rdpTransport* transport, STREAM* s, void* extra)
{
	freerdp_peer* client = (freerdp_peer*) extra;

	switch (client->context->rdp->state)
	{
		case CONNECTION_STATE_INITIAL:
			if (!rdp_server_accept_nego(client->context->rdp, s))
				return false;
			break;

		case CONNECTION_STATE_NEGO:
			if (!rdp_server_accept_mcs_connect_initial(client->context->rdp, s))
				return false;
			break;

		case CONNECTION_STATE_MCS_CONNECT:
			if (!rdp_server_accept_mcs_erect_domain_request(client->context->rdp, s))
				return false;
			break;

		case CONNECTION_STATE_MCS_ERECT_DOMAIN:
			if (!rdp_server_accept_mcs_attach_user_request(client->context->rdp, s))
				return false;
			break;

		case CONNECTION_STATE_MCS_ATTACH_USER:
			if (!rdp_server_accept_mcs_channel_join_request(client->context->rdp, s))
				return false;
			break;

		case CONNECTION_STATE_MCS_CHANNEL_JOIN:
			if (client->context->rdp->settings->encryption) {
				if (!rdp_server_accept_client_keys(client->context->rdp, s))
					return false;
				break;
			}
			client->context->rdp->state = CONNECTION_STATE_ESTABLISH_KEYS;
			/* FALLTHROUGH */

		case CONNECTION_STATE_ESTABLISH_KEYS:
			if (!rdp_server_accept_client_info(client->context->rdp, s))
				return false;
			IFCALL(client->Capabilities, client);
			if (!rdp_send_demand_active(client->context->rdp))
				return false;
			break;

		case CONNECTION_STATE_LICENSE:
			if (!rdp_server_accept_confirm_active(client->context->rdp, s))
			{
				/**
				 * During reactivation sequence the client might sent some input or channel data
				 * before receiving the Deactivate All PDU. We need to process them as usual.
				 */
				stream_set_pos(s, 0);
				return peer_recv_pdu(client, s);
			}
			break;

		case CONNECTION_STATE_ACTIVE:
			if (!peer_recv_pdu(client, s))
				return false;
			break;

		default:
			printf("Invalid state %d\n", client->context->rdp->state);
			return false;
	}

	return true;
}

static void freerdp_peer_disconnect(freerdp_peer* client)
{
	transport_disconnect(client->context->rdp->transport);
}

static int freerdp_peer_send_channel_data(freerdp_peer* client, int channelId, uint8* data, int size)
{
	return rdp_send_channel_data(client->context->rdp, channelId, data, size);
}

void freerdp_peer_context_new(freerdp_peer* client)
{
	rdpRdp* rdp;

	rdp = rdp_new(NULL);
	client->input = rdp->input;
	client->update = rdp->update;
	client->settings = rdp->settings;

	client->context = (rdpContext*) xzalloc(client->context_size);
	client->context->rdp = rdp;
	client->context->peer = client;

	client->update->context = client->context;
	client->input->context = client->context;

	update_register_server_callbacks(client->update);

	transport_attach(rdp->transport, client->sockfd);

	rdp->transport->recv_callback = peer_recv_callback;
	rdp->transport->recv_extra = client;
	transport_set_blocking_mode(rdp->transport, false);

	IFCALL(client->ContextNew, client, client->context);
}

void freerdp_peer_context_free(freerdp_peer* client)
{
	IFCALL(client->ContextFree, client, client->context);
}

freerdp_peer* freerdp_peer_new(int sockfd)
{
	freerdp_peer* client;

	client = xnew(freerdp_peer);

	if (client != NULL)
	{
		client->sockfd = sockfd;
		client->context_size = sizeof(rdpContext);
		client->Initialize = freerdp_peer_initialize;
		client->GetFileDescriptor = freerdp_peer_get_fds;
		client->CheckFileDescriptor = freerdp_peer_check_fds;
		client->Disconnect = freerdp_peer_disconnect;
		client->SendChannelData = freerdp_peer_send_channel_data;
	}

	return client;
}

void freerdp_peer_free(freerdp_peer* client)
{
	if (client)
	{
		rdp_free(client->context->rdp);
		xfree(client);
	}
}

