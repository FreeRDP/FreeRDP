/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Server Peer
 *
 * Copyright 2011 Vic Lee
 * Copyright 2014 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include <winpr/winsock.h>

#include "info.h"
#include "certificate.h"

#include <freerdp/log.h>

#include "peer.h"

#define TAG FREERDP_TAG("core.peer")

#ifdef WITH_DEBUG_RDP
extern const char* DATA_PDU_TYPE_STRINGS[80];
#endif

static HANDLE freerdp_peer_virtual_channel_open(freerdp_peer* client, const char* name, UINT32 flags)
{
	int length;
	UINT32 index;
	BOOL joined = FALSE;
	rdpMcsChannel* mcsChannel = NULL;
	rdpPeerChannel* peerChannel = NULL;
	rdpMcs* mcs = client->context->rdp->mcs;

	if (flags & WTS_CHANNEL_OPTION_DYNAMIC)
		return NULL; /* not yet supported */

	length = strlen(name);

	if (length > 8)
		return NULL; /* SVC maximum name length is 8 */

	for (index = 0; index < mcs->channelCount; index++)
	{
		mcsChannel = &(mcs->channels[index]);

		if (!mcsChannel->joined)
			continue;

		if (strncmp(name, mcsChannel->Name, length) == 0)
		{
			joined = TRUE;
			break;
		}
	}

	if (!joined)
		return NULL; /* channel is not joined */

	peerChannel = (rdpPeerChannel*) mcsChannel->handle;

	if (peerChannel)
	{
		/* channel is already open */
		return (HANDLE) peerChannel;
	}

	peerChannel = (rdpPeerChannel*) calloc(1, sizeof(rdpPeerChannel));

	if (peerChannel)
	{
		peerChannel->index = index;
		peerChannel->client = client;
		peerChannel->channelFlags = flags;
		peerChannel->channelId = mcsChannel->ChannelId;
		peerChannel->mcsChannel = mcsChannel;
		mcsChannel->handle = (void*) peerChannel;
	}

	return (HANDLE) peerChannel;
}

static BOOL freerdp_peer_virtual_channel_close(freerdp_peer* client, HANDLE hChannel)
{
	rdpMcsChannel* mcsChannel = NULL;
	rdpPeerChannel* peerChannel = NULL;

	if (!hChannel)
		return FALSE;

	peerChannel = (rdpPeerChannel*) hChannel;
	mcsChannel = peerChannel->mcsChannel;

	mcsChannel->handle = NULL;
	free(peerChannel);

	return TRUE;
}

int freerdp_peer_virtual_channel_read(freerdp_peer* client, HANDLE hChannel, BYTE* buffer, UINT32 length)
{
	return 0; /* this needs to be implemented by the server application */
}

static int freerdp_peer_virtual_channel_write(freerdp_peer* client, HANDLE hChannel, BYTE* buffer, UINT32 length)
{
	wStream* s;
	UINT32 flags;
	UINT32 chunkSize;
	UINT32 maxChunkSize;
	UINT32 totalLength;
	rdpPeerChannel* peerChannel;
	rdpMcsChannel* mcsChannel;
	rdpRdp* rdp = client->context->rdp;

	if (!hChannel)
		return -1;

	peerChannel = (rdpPeerChannel*) hChannel;
	mcsChannel = peerChannel->mcsChannel;

	if (peerChannel->channelFlags & WTS_CHANNEL_OPTION_DYNAMIC)
		return -1; /* not yet supported */

	maxChunkSize = rdp->settings->VirtualChannelChunkSize;

	totalLength = length;
	flags = CHANNEL_FLAG_FIRST;

	while (length > 0)
	{
		s = rdp_send_stream_init(rdp);

		if (length > maxChunkSize)
		{
			chunkSize = rdp->settings->VirtualChannelChunkSize;
		}
		else
		{
			chunkSize = length;
			flags |= CHANNEL_FLAG_LAST;
		}

		if (mcsChannel->options & CHANNEL_OPTION_SHOW_PROTOCOL)
			flags |= CHANNEL_FLAG_SHOW_PROTOCOL;

		Stream_Write_UINT32(s, totalLength);
		Stream_Write_UINT32(s, flags);
		Stream_EnsureRemainingCapacity(s, chunkSize);
		Stream_Write(s, buffer, chunkSize);

		rdp_send(rdp, s, peerChannel->channelId);

		buffer += chunkSize;
		length -= chunkSize;
		flags = 0;
	}

	return 1;
}

void* freerdp_peer_virtual_channel_get_data(freerdp_peer* client, HANDLE hChannel)
{
	rdpPeerChannel* peerChannel = (rdpPeerChannel*) hChannel;

	if (!hChannel)
		return NULL;

	return peerChannel->extra;
}

int freerdp_peer_virtual_channel_set_data(freerdp_peer* client, HANDLE hChannel, void* data)
{
	rdpPeerChannel* peerChannel = (rdpPeerChannel*) hChannel;

	if (!hChannel)
		return -1;

	peerChannel->extra = data;

	return 1;
}

static BOOL freerdp_peer_initialize(freerdp_peer* client)
{
	rdpRdp* rdp = client->context->rdp;
	rdpSettings* settings = rdp->settings;

	settings->ServerMode = TRUE;
	settings->FrameAcknowledge = 0;
	settings->LocalConnection = client->local;
	rdp->state = CONNECTION_STATE_INITIAL;

	if (settings->RdpKeyFile)
	{
		settings->RdpServerRsaKey = key_new(settings->RdpKeyFile);

		if (!settings->RdpServerRsaKey)
		{
			WLog_ERR(TAG, "inavlid RDP key file %s", settings->RdpKeyFile);
			return FALSE;
		}

		if (settings->RdpServerRsaKey->ModulusLength > 256)
		{
			WLog_ERR(TAG, "Key sizes > 2048 are currently not supported for RDP security.");
			WLog_ERR(TAG, "Set a different key file than %s", settings->RdpKeyFile);
			exit(1);
		}
	}

	return TRUE;
}

static BOOL freerdp_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	rfds[*rcount] = (void*)(long)(client->context->rdp->transport->TcpIn->sockfd);
	(*rcount)++;

	return TRUE;
}

static HANDLE freerdp_peer_get_event_handle(freerdp_peer* client)
{
	return client->context->rdp->transport->TcpIn->event;
}

static BOOL freerdp_peer_check_fds(freerdp_peer* peer)
{
	int status;
	rdpRdp* rdp;

	rdp = peer->context->rdp;

	status = rdp_check_fds(rdp);

	if (status < 0)
		return FALSE;

	return TRUE;
}

static BOOL peer_recv_data_pdu(freerdp_peer* client, wStream* s)
{
	BYTE type;
	UINT16 length;
	UINT32 share_id;
	BYTE compressed_type;
	UINT16 compressed_len;

	if (!rdp_read_share_data_header(s, &length, &type, &share_id, &compressed_type, &compressed_len))
		return FALSE;

#ifdef WITH_DEBUG_RDP
	WLog_DBG(TAG, "recv %s Data PDU (0x%02X), length: %d",
			 type < ARRAYSIZE(DATA_PDU_TYPE_STRINGS) ? DATA_PDU_TYPE_STRINGS[type] : "???", type, length);
#endif

	switch (type)
	{
		case DATA_PDU_TYPE_SYNCHRONIZE:
			if (!rdp_recv_client_synchronize_pdu(client->context->rdp, s))
				return FALSE;
			break;

		case DATA_PDU_TYPE_CONTROL:
			if (!rdp_server_accept_client_control_pdu(client->context->rdp, s))
				return FALSE;
			break;

		case DATA_PDU_TYPE_INPUT:
			if (!input_recv(client->context->rdp->input, s))
				return FALSE;
			break;

		case DATA_PDU_TYPE_BITMAP_CACHE_PERSISTENT_LIST:
			/* TODO: notify server bitmap cache data */
			break;

		case DATA_PDU_TYPE_FONT_LIST:

			if (!rdp_server_accept_client_font_list_pdu(client->context->rdp, s))
				return FALSE;

			break;

		case DATA_PDU_TYPE_SHUTDOWN_REQUEST:
			mcs_send_disconnect_provider_ultimatum(client->context->rdp->mcs);
			return FALSE;

		case DATA_PDU_TYPE_FRAME_ACKNOWLEDGE:
			if (Stream_GetRemainingLength(s) < 4)
				return FALSE;
			Stream_Read_UINT32(s, client->ack_frame_id);
			IFCALL(client->update->SurfaceFrameAcknowledge, client->update->context, client->ack_frame_id);
			break;

		case DATA_PDU_TYPE_REFRESH_RECT:
			if (!update_read_refresh_rect(client->update, s))
				return FALSE;
			break;

		case DATA_PDU_TYPE_SUPPRESS_OUTPUT:
			if (!update_read_suppress_output(client->update, s))
				return FALSE;
			break;

		default:
			WLog_ERR(TAG,  "Data PDU type %d", type);
			break;
	}

	return TRUE;
}

static int peer_recv_tpkt_pdu(freerdp_peer* client, wStream* s)
{
	rdpRdp* rdp;
	UINT16 length;
	UINT16 pduType;
	UINT16 pduLength;
	UINT16 pduSource;
	UINT16 channelId;
	UINT16 securityFlags;

	rdp = client->context->rdp;

	if (!rdp_read_header(rdp, s, &length, &channelId))
	{
		WLog_ERR(TAG,  "Incorrect RDP header.");
		return -1;
	}

	if (rdp->disconnect)
		return 0;
 
	if (rdp->settings->UseRdpSecurityLayer)
	{
		if (!rdp_read_security_header(s, &securityFlags))
			return -1;

		if (securityFlags & SEC_ENCRYPT)
		{
			if (!rdp_decrypt(rdp, s, length - 4, securityFlags))
			{
				WLog_ERR(TAG,  "rdp_decrypt failed");
				return -1;
			}
		}
	}

	if (channelId == MCS_GLOBAL_CHANNEL_ID)
	{
		if (!rdp_read_share_control_header(s, &pduLength, &pduType, &pduSource))
			return -1;

		client->settings->PduSource = pduSource;

		switch (pduType)
		{
			case PDU_TYPE_DATA:
				if (!peer_recv_data_pdu(client, s))
					return -1;
				break;

			case PDU_TYPE_CONFIRM_ACTIVE:
				if (!rdp_server_accept_confirm_active(rdp, s))
					return -1;
				break;

			case PDU_TYPE_FLOW_RESPONSE:
			case PDU_TYPE_FLOW_STOP:
			case PDU_TYPE_FLOW_TEST:
				break;

			default:
				WLog_ERR(TAG,  "Client sent pduType %d", pduType);
				return -1;
		}
	}
	else if (rdp->mcs->messageChannelId && channelId == rdp->mcs->messageChannelId)
	{
		return rdp_recv_message_channel_pdu(rdp, s);
	}
	else
	{
		if (!freerdp_channel_peer_process(client, s, channelId))
			return -1;
	}

	return 0;
}

static int peer_recv_fastpath_pdu(freerdp_peer* client, wStream* s)
{
	rdpRdp* rdp;
	UINT16 length;
	rdpFastPath* fastpath;

	rdp = client->context->rdp;
	fastpath = rdp->fastpath;

	fastpath_read_header_rdp(fastpath, s, &length);

	if ((length == 0) || (length > Stream_GetRemainingLength(s)))
	{
		WLog_ERR(TAG,  "incorrect FastPath PDU header length %d", length);
		return -1;
	}

	if (fastpath->encryptionFlags & FASTPATH_OUTPUT_ENCRYPTED)
	{
		if (!rdp_decrypt(rdp, s, length, (fastpath->encryptionFlags & FASTPATH_OUTPUT_SECURE_CHECKSUM) ? SEC_SECURE_CHECKSUM : 0))
			return -1;
	}

	return fastpath_recv_inputs(fastpath, s);
}

static int peer_recv_pdu(freerdp_peer* client, wStream* s)
{
	if (tpkt_verify_header(s))
		return peer_recv_tpkt_pdu(client, s);
	else
		return peer_recv_fastpath_pdu(client, s);
}

static int peer_recv_callback(rdpTransport* transport, wStream* s, void* extra)
{
	freerdp_peer* client = (freerdp_peer*) extra;
	rdpRdp* rdp = client->context->rdp;

	switch (rdp->state)
	{
		case CONNECTION_STATE_INITIAL:
			if (!rdp_server_accept_nego(rdp, s))
				return -1;

			if (rdp->nego->SelectedProtocol & PROTOCOL_NLA)
			{
				sspi_CopyAuthIdentity(&client->identity, &(rdp->nego->transport->credssp->identity));
				IFCALLRET(client->Logon, client->authenticated, client, &client->identity, TRUE);
				credssp_free(rdp->nego->transport->credssp);
				rdp->nego->transport->credssp = NULL;
			}
			else
			{
				IFCALLRET(client->Logon, client->authenticated, client, &client->identity, FALSE);
			}

			break;

		case CONNECTION_STATE_NEGO:
			if (!rdp_server_accept_mcs_connect_initial(rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_MCS_CONNECT:
			if (!rdp_server_accept_mcs_erect_domain_request(rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_MCS_ERECT_DOMAIN:
			if (!rdp_server_accept_mcs_attach_user_request(rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_MCS_ATTACH_USER:
			if (!rdp_server_accept_mcs_channel_join_request(rdp, s))
				return -1;
			break;

		case CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT:
			if (rdp->settings->UseRdpSecurityLayer)
			{
				if (!rdp_server_establish_keys(rdp, s))
					return -1;
			}

			rdp_server_transition_to_state(rdp, CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE);
			if (Stream_GetRemainingLength(s) > 0)
				return peer_recv_callback(transport, s, extra);
			break;

		case CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE:

			if (!rdp_recv_client_info(rdp, s))
				return -1;

			rdp_server_transition_to_state(rdp, CONNECTION_STATE_LICENSING);
			return peer_recv_callback(transport, NULL, extra);

			break;

		case CONNECTION_STATE_LICENSING:

			if (!license_send_valid_client_error_packet(rdp->license))
				return FALSE;

			rdp_server_transition_to_state(rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE);
			return peer_recv_callback(transport, NULL, extra);

			break;

		case CONNECTION_STATE_CAPABILITIES_EXCHANGE:

			if (!rdp->AwaitCapabilities)
			{
				IFCALL(client->Capabilities, client);

				if (!rdp_send_demand_active(rdp))
					return -1;

				rdp->AwaitCapabilities = TRUE;

				if (s)
				{
					if (peer_recv_pdu(client, s) < 0)
						return -1;
				}
			}
			else
			{
				/**
				 * During reactivation sequence the client might sent some input or channel data
				 * before receiving the Deactivate All PDU. We need to process them as usual.
				 */

				if (peer_recv_pdu(client, s) < 0)
					return -1;
			}

			break;

		case CONNECTION_STATE_FINALIZATION:
			if (peer_recv_pdu(client, s) < 0)
				return -1;
			break;

		case CONNECTION_STATE_ACTIVE:
			if (peer_recv_pdu(client, s) < 0)
				return -1;
			break;

		default:
			WLog_ERR(TAG,  "Invalid state %d", rdp->state);
			return -1;
	}

	return 0;
}

static BOOL freerdp_peer_close(freerdp_peer* client)
{
	/**
	 * [MS-RDPBCGR] 1.3.1.4.2 User-Initiated Disconnection Sequence on Server
	 * The server first sends the client a Deactivate All PDU followed by an
	 * optional MCS Disconnect Provider Ultimatum PDU.
	 */
	if (!rdp_send_deactivate_all(client->context->rdp))
		return FALSE;

	if (freerdp_get_param_bool(client->settings, FreeRDP_SupportErrorInfoPdu) ) {
		rdp_send_error_info(client->context->rdp);
	}

	return mcs_send_disconnect_provider_ultimatum(client->context->rdp->mcs);
}

static void freerdp_peer_disconnect(freerdp_peer* client)
{
	transport_disconnect(client->context->rdp->transport);
}

static int freerdp_peer_send_channel_data(freerdp_peer* client, UINT16 channelId, BYTE* data, int size)
{
	return rdp_send_channel_data(client->context->rdp, channelId, data, size);
}

static BOOL freerdp_peer_is_write_blocked(freerdp_peer* peer)
{
	return tranport_is_write_blocked(peer->context->rdp->transport);
}

static int freerdp_peer_drain_output_buffer(freerdp_peer* peer)
{
	rdpTransport* transport = peer->context->rdp->transport;

	return tranport_drain_output_buffer(transport);
}

void freerdp_peer_context_new(freerdp_peer* client)
{
	rdpRdp* rdp;
	rdpContext* context;

	context = client->context = (rdpContext*) calloc(1, client->ContextSize);

	if (!context)
		return;

	context->ServerMode = TRUE;

	context->metrics = metrics_new(context);

	rdp = rdp_new(context);

	client->input = rdp->input;
	client->update = rdp->update;
	client->settings = rdp->settings;
	client->autodetect = rdp->autodetect;

	context->rdp = rdp;
	context->peer = client;
	context->input = client->input;
	context->update = client->update;
	context->settings = client->settings;
	context->autodetect = client->autodetect;

	client->update->context = context;
	client->input->context = context;
	client->autodetect->context = context;

	update_register_server_callbacks(client->update);
	autodetect_register_server_callbacks(client->autodetect);

	transport_attach(rdp->transport, client->sockfd);

	rdp->transport->ReceiveCallback = peer_recv_callback;
	rdp->transport->ReceiveExtra = client;
	transport_set_blocking_mode(rdp->transport, FALSE);

	client->IsWriteBlocked = freerdp_peer_is_write_blocked;
	client->DrainOutputBuffer = freerdp_peer_drain_output_buffer;

	IFCALL(client->ContextNew, client, client->context);
}

void freerdp_peer_context_free(freerdp_peer* client)
{
	IFCALL(client->ContextFree, client, client->context);

	metrics_free(client->context->metrics);
}

freerdp_peer* freerdp_peer_new(int sockfd)
{
	UINT32 option_value;
	socklen_t option_len;
	freerdp_peer* client;

	client = (freerdp_peer*) calloc(1, sizeof(freerdp_peer));

	if (!client)
		return NULL;

	option_value = TRUE;
	option_len = sizeof(option_value);

	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void*) &option_value, option_len);

	if (client)
	{
		client->sockfd = sockfd;
		client->ContextSize = sizeof(rdpContext);
		client->Initialize = freerdp_peer_initialize;
		client->GetFileDescriptor = freerdp_peer_get_fds;
		client->GetEventHandle = freerdp_peer_get_event_handle;
		client->CheckFileDescriptor = freerdp_peer_check_fds;
		client->Close = freerdp_peer_close;
		client->Disconnect = freerdp_peer_disconnect;
		client->SendChannelData = freerdp_peer_send_channel_data;
		client->IsWriteBlocked = freerdp_peer_is_write_blocked;
		client->DrainOutputBuffer = freerdp_peer_drain_output_buffer;
		client->VirtualChannelOpen = freerdp_peer_virtual_channel_open;
		client->VirtualChannelClose = freerdp_peer_virtual_channel_close;
		client->VirtualChannelWrite = freerdp_peer_virtual_channel_write;
		client->VirtualChannelRead = NULL; /* must be defined by server application */
		client->VirtualChannelGetData = freerdp_peer_virtual_channel_get_data;
		client->VirtualChannelSetData = freerdp_peer_virtual_channel_set_data;
	}

	return client;
}

void freerdp_peer_free(freerdp_peer* client)
{
	if (!client)
		return;

	rdp_free(client->context->rdp);
	free(client->context);
	free(client);
}
