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

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <winpr/crt.h>
#include <winpr/winsock.h>

#include "info.h"
#include "certificate.h"

#include <freerdp/log.h>
#include <freerdp/streamdump.h>

#include "rdp.h"
#include "peer.h"

#define TAG FREERDP_TAG("core.peer")

static HANDLE freerdp_peer_virtual_channel_open(freerdp_peer* client, const char* name,
                                                UINT32 flags)
{
	int length;
	UINT32 index;
	BOOL joined = FALSE;
	rdpMcsChannel* mcsChannel = NULL;
	rdpPeerChannel* peerChannel = NULL;
	rdpMcs* mcs;

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	WINPR_ASSERT(client->context->rdp);
	WINPR_ASSERT(name);
	mcs = client->context->rdp->mcs;
	WINPR_ASSERT(mcs);

	if (flags & WTS_CHANNEL_OPTION_DYNAMIC)
		return NULL; /* not yet supported */

	length = strnlen(name, 9);

	if (length > 8)
		return NULL; /* SVC maximum name length is 8 */

	for (index = 0; index < mcs->channelCount; index++)
	{
		mcsChannel = &(mcs->channels[index]);

		if (!mcsChannel->joined)
			continue;

		if (_strnicmp(name, mcsChannel->Name, length) == 0)
		{
			joined = TRUE;
			break;
		}
	}

	if (!joined)
		return NULL; /* channel is not joined */

	peerChannel = (rdpPeerChannel*)mcsChannel->handle;

	if (peerChannel)
	{
		/* channel is already open */
		return (HANDLE)peerChannel;
	}

	peerChannel = (rdpPeerChannel*)calloc(1, sizeof(rdpPeerChannel));

	if (peerChannel)
	{
		peerChannel->index = index;
		peerChannel->client = client;
		peerChannel->channelFlags = flags;
		peerChannel->channelId = mcsChannel->ChannelId;
		peerChannel->mcsChannel = mcsChannel;
		mcsChannel->handle = (void*)peerChannel;
	}

	return (HANDLE)peerChannel;
}

static BOOL freerdp_peer_virtual_channel_close(freerdp_peer* client, HANDLE hChannel)
{
	rdpMcsChannel* mcsChannel = NULL;
	rdpPeerChannel* peerChannel = NULL;

	WINPR_ASSERT(client);

	if (!hChannel)
		return FALSE;

	peerChannel = (rdpPeerChannel*)hChannel;
	mcsChannel = peerChannel->mcsChannel;
	WINPR_ASSERT(mcsChannel);
	mcsChannel->handle = NULL;
	free(peerChannel);
	return TRUE;
}

static int freerdp_peer_virtual_channel_write(freerdp_peer* client, HANDLE hChannel,
                                              const BYTE* buffer, UINT32 length)
{
	wStream* s;
	UINT32 flags;
	UINT32 chunkSize;
	UINT32 maxChunkSize;
	UINT32 totalLength;
	rdpPeerChannel* peerChannel;
	rdpMcsChannel* mcsChannel;
	rdpRdp* rdp;

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);

	rdp = client->context->rdp;
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->settings);

	if (!hChannel)
		return -1;

	peerChannel = (rdpPeerChannel*)hChannel;
	mcsChannel = peerChannel->mcsChannel;
	WINPR_ASSERT(peerChannel);
	WINPR_ASSERT(mcsChannel);
	if (peerChannel->channelFlags & WTS_CHANNEL_OPTION_DYNAMIC)
		return -1; /* not yet supported */

	maxChunkSize = rdp->settings->VirtualChannelChunkSize;
	totalLength = length;
	flags = CHANNEL_FLAG_FIRST;

	while (length > 0)
	{
		s = rdp_send_stream_init(rdp);

		if (!s)
			return -1;

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

		if (!Stream_EnsureRemainingCapacity(s, chunkSize))
		{
			Stream_Release(s);
			return -1;
		}

		Stream_Write(s, buffer, chunkSize);

		if (!rdp_send(rdp, s, peerChannel->channelId))
			return -1;

		buffer += chunkSize;
		length -= chunkSize;
		flags = 0;
	}

	return 1;
}

static void* freerdp_peer_virtual_channel_get_data(freerdp_peer* client, HANDLE hChannel)
{
	rdpPeerChannel* peerChannel = (rdpPeerChannel*)hChannel;

	WINPR_ASSERT(client);
	if (!hChannel)
		return NULL;

	return peerChannel->extra;
}

static int freerdp_peer_virtual_channel_set_data(freerdp_peer* client, HANDLE hChannel, void* data)
{
	rdpPeerChannel* peerChannel = (rdpPeerChannel*)hChannel;

	WINPR_ASSERT(client);
	if (!hChannel)
		return -1;

	peerChannel->extra = data;
	return 1;
}

static BOOL freerdp_peer_set_state(freerdp_peer* client, CONNECTION_STATE state)
{
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	return rdp_server_transition_to_state(client->context->rdp, state);
}

static BOOL freerdp_peer_initialize(freerdp_peer* client)
{
	rdpRdp* rdp;
	rdpSettings* settings;

	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);

	rdp = client->context->rdp;
	WINPR_ASSERT(rdp);

	settings = rdp->settings;
	WINPR_ASSERT(settings);

	settings->ServerMode = TRUE;
	settings->FrameAcknowledge = 0;
	settings->LocalConnection = client->local;
	rdp_server_transition_to_state(rdp, CONNECTION_STATE_INITIAL);

	if (settings->RdpKeyFile)
	{
		settings->RdpServerRsaKey = key_new(settings->RdpKeyFile);

		if (!settings->RdpServerRsaKey)
		{
			WLog_ERR(TAG, "invalid RDP key file %s", settings->RdpKeyFile);
			return FALSE;
		}
	}
	else if (settings->RdpKeyContent)
	{
		settings->RdpServerRsaKey = key_new_from_content(settings->RdpKeyContent, NULL);

		if (!settings->RdpServerRsaKey)
		{
			WLog_ERR(TAG, "invalid RDP key content");
			return FALSE;
		}
	}

	return TRUE;
}

#if defined(WITH_FREERDP_DEPRECATED)
static BOOL freerdp_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	rdpTransport* transport;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	WINPR_ASSERT(client->context->rdp);

	transport = client->context->rdp->transport;
	WINPR_ASSERT(transport);
	transport_get_fds(transport, rfds, rcount);
	return TRUE;
}
#endif

static HANDLE freerdp_peer_get_event_handle(freerdp_peer* client)
{
	HANDLE hEvent = NULL;
	rdpTransport* transport;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	WINPR_ASSERT(client->context->rdp);

	transport = client->context->rdp->transport;
	hEvent = transport_get_front_bio(transport);
	return hEvent;
}

static DWORD freerdp_peer_get_event_handles(freerdp_peer* client, HANDLE* events, DWORD count)
{
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	WINPR_ASSERT(client->context->rdp);
	return transport_get_event_handles(client->context->rdp->transport, events, count);
}

static BOOL freerdp_peer_check_fds(freerdp_peer* peer)
{
	int status;
	rdpRdp* rdp;

	WINPR_ASSERT(peer);
	WINPR_ASSERT(peer->context);

	rdp = peer->context->rdp;
	status = rdp_check_fds(rdp);

	if (status < 0)
		return FALSE;

	return TRUE;
}

static BOOL peer_recv_data_pdu(freerdp_peer* client, wStream* s, UINT16 totalLength)
{
	BYTE type;
	UINT16 length;
	UINT32 share_id;
	BYTE compressed_type;
	UINT16 compressed_len;
	rdpUpdate* update;

	WINPR_ASSERT(s);
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	WINPR_ASSERT(client->context->rdp);
	WINPR_ASSERT(client->context->rdp->mcs);

	update = client->context->update;
	WINPR_ASSERT(update);

	if (!rdp_read_share_data_header(s, &length, &type, &share_id, &compressed_type,
	                                &compressed_len))
		return FALSE;

#ifdef WITH_DEBUG_RDP
	WLog_DBG(TAG, "recv %s Data PDU (0x%02" PRIX8 "), length: %" PRIu16 "",
	         data_pdu_type_to_string(type), type, length);
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
			if (!rdp_server_accept_client_persistent_key_list_pdu(client->context->rdp, s))
				return FALSE;
			break;

		case DATA_PDU_TYPE_FONT_LIST:
			if (!rdp_server_accept_client_font_list_pdu(client->context->rdp, s))
				return FALSE;

			break;

		case DATA_PDU_TYPE_SHUTDOWN_REQUEST:
			mcs_send_disconnect_provider_ultimatum(client->context->rdp->mcs);
			return FALSE;

		case DATA_PDU_TYPE_FRAME_ACKNOWLEDGE:
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
				return FALSE;

			Stream_Read_UINT32(s, client->ack_frame_id);
			IFCALL(update->SurfaceFrameAcknowledge, update->context, client->ack_frame_id);
			break;

		case DATA_PDU_TYPE_REFRESH_RECT:
			if (!update_read_refresh_rect(update, s))
				return FALSE;

			break;

		case DATA_PDU_TYPE_SUPPRESS_OUTPUT:
			if (!update_read_suppress_output(update, s))
				return FALSE;

			break;

		default:
			WLog_ERR(TAG, "Data PDU type %" PRIu8 "", type);
			break;
	}

	return TRUE;
}

static int peer_recv_tpkt_pdu(freerdp_peer* client, wStream* s)
{
	rdpRdp* rdp;
	UINT16 length;
	UINT16 pduType;
	UINT16 pduSource;
	UINT16 channelId;
	UINT16 securityFlags = 0;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);

	rdp = client->context->rdp;
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->mcs);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	if (!rdp_read_header(rdp, s, &length, &channelId))
		return -1;

	rdp->inPackets++;
	if (freerdp_shall_disconnect_context(rdp->context))
		return 0;

	if (settings->UseRdpSecurityLayer)
	{
		if (!rdp_read_security_header(s, &securityFlags, &length))
			return -1;

		if (securityFlags & SEC_ENCRYPT)
		{
			if (!rdp_decrypt(rdp, s, &length, securityFlags))
				return -1;
		}
	}

	if (channelId == MCS_GLOBAL_CHANNEL_ID)
	{
		UINT16 pduLength, remain;
		if (!rdp_read_share_control_header(s, &pduLength, &remain, &pduType, &pduSource))
			return -1;

		settings->PduSource = pduSource;

		WLog_DBG(TAG, "Received %s", pdu_type_to_str(pduType));
		switch (pduType)
		{
			case PDU_TYPE_DATA:
				if (!peer_recv_data_pdu(client, s, pduLength))
					return -1;

				break;

			case PDU_TYPE_CONFIRM_ACTIVE:
				if (!rdp_server_accept_confirm_active(rdp, s, pduLength))
					return -1;

				break;

			case PDU_TYPE_FLOW_RESPONSE:
			case PDU_TYPE_FLOW_STOP:
			case PDU_TYPE_FLOW_TEST:
				if (!Stream_SafeSeek(s, remain))
				{
					WLog_WARN(TAG, "Short PDU, need %" PRIuz " bytes, got %" PRIuz, remain,
					          Stream_GetRemainingLength(s));
					return -1;
				}
				break;

			default:
				WLog_ERR(TAG, "Client sent unknown pduType %" PRIu16 "", pduType);
				return -1;
		}
	}
	else if ((rdp->mcs->messageChannelId > 0) && (channelId == rdp->mcs->messageChannelId))
	{
		if (!settings->UseRdpSecurityLayer)
			if (!rdp_read_security_header(s, &securityFlags, NULL))
				return -1;

		return rdp_recv_message_channel_pdu(rdp, s, securityFlags);
	}
	else
	{
		if (!freerdp_channel_peer_process(client, s, channelId))
			return -1;
	}
	if (!tpkt_ensure_stream_consumed(s, length))
		return -1;

	return 0;
}

static int peer_recv_fastpath_pdu(freerdp_peer* client, wStream* s)
{
	rdpRdp* rdp;
	UINT16 length;
	BOOL rc;
	rdpFastPath* fastpath;

	WINPR_ASSERT(s);
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);

	rdp = client->context->rdp;
	WINPR_ASSERT(rdp);

	fastpath = rdp->fastpath;
	WINPR_ASSERT(fastpath);

	rc = fastpath_read_header_rdp(fastpath, s, &length);

	if (!rc || (length == 0))
	{
		WLog_ERR(TAG, "incorrect FastPath PDU header length %" PRIu16 "", length);
		return -1;
	}
	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return -1;

	if (fastpath_get_encryption_flags(fastpath) & FASTPATH_OUTPUT_ENCRYPTED)
	{
		if (!rdp_decrypt(rdp, s, &length,
		                 (fastpath_get_encryption_flags(fastpath) & FASTPATH_OUTPUT_SECURE_CHECKSUM)
		                     ? SEC_SECURE_CHECKSUM
		                     : 0))
			return -1;
	}

	rdp->inPackets++;

	return fastpath_recv_inputs(fastpath, s);
}

static int peer_recv_pdu(freerdp_peer* client, wStream* s)
{
	if (tpkt_verify_header(s))
		return peer_recv_tpkt_pdu(client, s);
	else
		return peer_recv_fastpath_pdu(client, s);
}

static int peer_recv_callback_internal(rdpTransport* transport, wStream* s, void* extra)
{
	UINT32 SelectedProtocol;
	freerdp_peer* client = (freerdp_peer*)extra;
	rdpRdp* rdp;
	int ret = -1;
	rdpSettings* settings;

	WINPR_ASSERT(transport);
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);

	rdp = client->context->rdp;
	WINPR_ASSERT(rdp);

	settings = client->context->settings;
	WINPR_ASSERT(settings);

	IFCALL(client->ReachedState, client, rdp_get_state(rdp));
	switch (rdp_get_state(rdp))
	{
		case CONNECTION_STATE_INITIAL:
			if (!rdp_server_accept_nego(rdp, s))
			{
				WLog_ERR(TAG, "%s: %s - rdp_server_accept_nego() fail", __FUNCTION__,
				         rdp_get_state_string(rdp));
			}
			else
			{
				SelectedProtocol = nego_get_selected_protocol(rdp->nego);
				settings->NlaSecurity = (SelectedProtocol & PROTOCOL_HYBRID) ? TRUE : FALSE;
				settings->TlsSecurity = (SelectedProtocol & PROTOCOL_SSL) ? TRUE : FALSE;
				settings->RdpSecurity = (SelectedProtocol == PROTOCOL_RDP) ? TRUE : FALSE;

				if (SelectedProtocol & PROTOCOL_HYBRID)
				{
					SEC_WINNT_AUTH_IDENTITY* identity = nego_get_identity(rdp->nego);
					sspi_CopyAuthIdentity(&client->identity, identity);
					IFCALLRET(client->Logon, client->authenticated, client, &client->identity,
					          TRUE);
					nego_free_nla(rdp->nego);
				}
				else
				{
					IFCALLRET(client->Logon, client->authenticated, client, &client->identity,
					          FALSE);
				}
				ret = 0;
			}
			break;

		case CONNECTION_STATE_NEGO:
			if (!rdp_server_accept_mcs_connect_initial(rdp, s))
			{
				WLog_ERR(TAG,
				         "%s: %s - "
				         "rdp_server_accept_mcs_connect_initial() fail",
				         __FUNCTION__, rdp_get_state_string(rdp));
			}
			else
				ret = 0;

			break;

		case CONNECTION_STATE_MCS_CONNECT:
			if (!rdp_server_accept_mcs_erect_domain_request(rdp, s))
			{
				WLog_ERR(TAG,
				         "%s: %s - "
				         "rdp_server_accept_mcs_erect_domain_request() fail",
				         __FUNCTION__, rdp_get_state_string(rdp));
			}
			else
				ret = 0;

			break;

		case CONNECTION_STATE_MCS_ERECT_DOMAIN:
			if (!rdp_server_accept_mcs_attach_user_request(rdp, s))
			{
				WLog_ERR(TAG,
				         "%s: %s - "
				         "rdp_server_accept_mcs_attach_user_request() fail",
				         __FUNCTION__, rdp_get_state_string(rdp));
			}
			else
				ret = 0;

			break;

		case CONNECTION_STATE_MCS_ATTACH_USER:
			if (!rdp_server_accept_mcs_channel_join_request(rdp, s))
			{
				WLog_ERR(TAG,
				         "%s: %s - "
				         "rdp_server_accept_mcs_channel_join_request() fail",
				         __FUNCTION__, rdp_get_state_string(rdp));
			}
			else
				ret = 0;
			break;

		case CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT:
			ret = 0;
			if (rdp->settings->UseRdpSecurityLayer)
			{
				if (!rdp_server_establish_keys(rdp, s))
				{
					WLog_ERR(TAG,
					         "%s: %s - "
					         "rdp_server_establish_keys() fail",
					         __FUNCTION__, rdp_get_state_string(rdp));
					ret = -1;
				}
			}
			if (ret >= 0)
			{
				rdp_server_transition_to_state(rdp, CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE);

				if (Stream_GetRemainingLength(s) > 0)
					ret = 1; /* Rerun function */
			}
			break;

		case CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE:
			if (!rdp_recv_client_info(rdp, s))
			{
				WLog_ERR(TAG,
				         "%s: %s - "
				         "rdp_recv_client_info() fail",
				         __FUNCTION__, rdp_get_state_string(rdp));
			}
			else
			{
				rdp_server_transition_to_state(rdp, CONNECTION_STATE_LICENSING);
				ret = 2; /* Rerun, NULL stream */
			}
			break;

		case CONNECTION_STATE_LICENSING:
		{
			LicenseCallbackResult res;

			if (!client->LicenseCallback)
			{
				WLog_ERR(TAG,
				         "%s: LicenseCallback has been removed, assuming "
				         "licensing is ok (please fix your app)",
				         __FUNCTION__);
				res = LICENSE_CB_COMPLETED;
			}
			else
				res = client->LicenseCallback(client, s);

			switch (res)
			{
				case LICENSE_CB_INTERNAL_ERROR:
					WLog_ERR(TAG,
					         "%s: %s - callback internal "
					         "error, aborting",
					         __FUNCTION__, rdp_get_state_string(rdp));
					break;

				case LICENSE_CB_ABORT:
					break;

				case LICENSE_CB_IN_PROGRESS:
					ret = 0;
					break;

				case LICENSE_CB_COMPLETED:
					rdp_server_transition_to_state(rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE);
					ret = 2; /* Rerun, NULL stream */
					break;

				default:
					WLog_ERR(TAG,
					         "%s: CONNECTION_STATE_LICENSING - unknown license callback "
					         "result %d",
					         __FUNCTION__, res);
					ret = 0;
					break;
			}

			break;
		}

		case CONNECTION_STATE_CAPABILITIES_EXCHANGE:
			if (!rdp->AwaitCapabilities)
			{
				if (client->Capabilities && !client->Capabilities(client))
				{
				}
				else if (!rdp_send_demand_active(rdp))
				{
					WLog_ERR(TAG,
					         "%s: %s - "
					         "rdp_send_demand_active() fail",
					         __FUNCTION__, rdp_get_state_string(rdp));
				}
				else
				{
					rdp->AwaitCapabilities = TRUE;

					if (s)
					{
						ret = peer_recv_pdu(client, s);
						if (ret < 0)
						{
							WLog_ERR(TAG,
							         "%s: %s - "
							         "peer_recv_pdu() fail",
							         __FUNCTION__, rdp_get_state_string(rdp));
						}
					}
					else
						ret = 0;
				}
			}
			else
			{
				/**
				 * During reactivation sequence the client might sent some input or channel data
				 * before receiving the Deactivate All PDU. We need to process them as usual.
				 */
				ret = peer_recv_pdu(client, s);
				if (ret < 0)
				{
					WLog_ERR(TAG,
					         "%s: %s - "
					         "peer_recv_pdu() fail",
					         __FUNCTION__, rdp_get_state_string(rdp));
				}
			}

			break;

		case CONNECTION_STATE_FINALIZATION:
			ret = peer_recv_pdu(client, s);
			if (ret < 0)
			{
				WLog_ERR(TAG, "%s: %s - peer_recv_pdu() fail", __FUNCTION__,
				         rdp_get_state_string(rdp));
			}
			break;

		case CONNECTION_STATE_ACTIVE:
			ret = peer_recv_pdu(client, s);
			if (ret < 0)
			{
				WLog_ERR(TAG, "%s: %s - peer_recv_pdu() fail", __FUNCTION__,
				         rdp_get_state_string(rdp));
			}

			break;

		default:
			WLog_ERR(TAG, "%s state %d", rdp_get_state_string(rdp), rdp_get_state(rdp));
			break;
	}

	return ret;
}

static int peer_recv_callback(rdpTransport* transport, wStream* s, void* extra)
{
	int rc = 0;
	do
	{
		switch (rc)
		{
			default:
				rc = peer_recv_callback_internal(transport, s, extra);
				break;
			case 2:
				rc = peer_recv_callback_internal(transport, NULL, extra);
				break;
		}
	} while (rc > 0);

	return rc;
}

static BOOL freerdp_peer_close(freerdp_peer* client)
{
	UINT32 SelectedProtocol;
	rdpContext* context;

	WINPR_ASSERT(client);

	context = client->context;
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->settings);
	WINPR_ASSERT(context->rdp);

	/** if negotiation has failed, we're not MCS connected. So don't
	 * 	send anything else, or some mstsc will consider that as an error
	 */
	SelectedProtocol = nego_get_selected_protocol(context->rdp->nego);

	if (SelectedProtocol & PROTOCOL_FAILED_NEGO)
		return TRUE;

	/**
	 * [MS-RDPBCGR] 1.3.1.4.2 User-Initiated Disconnection Sequence on Server
	 * The server first sends the client a Deactivate All PDU followed by an
	 * optional MCS Disconnect Provider Ultimatum PDU.
	 */
	if (!rdp_send_deactivate_all(context->rdp))
		return FALSE;

	if (freerdp_settings_get_bool(context->settings, FreeRDP_SupportErrorInfoPdu))
	{
		rdp_send_error_info(context->rdp);
	}

	return mcs_send_disconnect_provider_ultimatum(context->rdp->mcs);
}

static void freerdp_peer_disconnect(freerdp_peer* client)
{
	rdpTransport* transport;
	WINPR_ASSERT(client);

	transport = freerdp_get_transport(client->context);
	transport_disconnect(transport);
}

static BOOL freerdp_peer_send_channel_data(freerdp_peer* client, UINT16 channelId, const BYTE* data,
                                           size_t size)
{
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	WINPR_ASSERT(client->context->rdp);
	return rdp_send_channel_data(client->context->rdp, channelId, data, size);
}

static BOOL freerdp_peer_send_channel_packet(freerdp_peer* client, UINT16 channelId,
                                             size_t totalSize, UINT32 flags, const BYTE* data,
                                             size_t chunkSize)
{
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->context);
	WINPR_ASSERT(client->context->rdp);
	return rdp_channel_send_packet(client->context->rdp, channelId, totalSize, flags, data,
	                               chunkSize);
}

static BOOL freerdp_peer_is_write_blocked(freerdp_peer* peer)
{
	rdpTransport* transport;
	WINPR_ASSERT(peer);
	WINPR_ASSERT(peer->context);
	WINPR_ASSERT(peer->context->rdp);
	WINPR_ASSERT(peer->context->rdp->transport);
	transport = peer->context->rdp->transport;
	return transport_is_write_blocked(transport);
}

static int freerdp_peer_drain_output_buffer(freerdp_peer* peer)
{
	rdpTransport* transport;
	WINPR_ASSERT(peer);
	WINPR_ASSERT(peer->context);
	WINPR_ASSERT(peer->context->rdp);
	WINPR_ASSERT(peer->context->rdp->transport);
	transport = peer->context->rdp->transport;
	return transport_drain_output_buffer(transport);
}

static BOOL freerdp_peer_has_more_to_read(freerdp_peer* peer)
{
	WINPR_ASSERT(peer);
	WINPR_ASSERT(peer->context);
	WINPR_ASSERT(peer->context->rdp);
	return transport_have_more_bytes_to_read(peer->context->rdp->transport);
}

static LicenseCallbackResult freerdp_peer_nolicense(freerdp_peer* peer, wStream* s)
{
	rdpRdp* rdp;

	WINPR_ASSERT(peer);
	WINPR_ASSERT(peer->context);

	rdp = peer->context->rdp;

	if (!license_send_valid_client_error_packet(rdp))
	{
		WLog_ERR(TAG, "freerdp_peer_nolicense: license_send_valid_client_error_packet() failed");
		return LICENSE_CB_ABORT;
	}

	return LICENSE_CB_COMPLETED;
}

BOOL freerdp_peer_context_new(freerdp_peer* client)
{
	return freerdp_peer_context_new_ex(client, NULL);
}

void freerdp_peer_context_free(freerdp_peer* client)
{
	if (!client)
		return;

	IFCALL(client->ContextFree, client, client->context);

	if (client->context)
	{
		rdpContext* ctx = client->context;

		free(ctx->errorDescription);
		ctx->errorDescription = NULL;
		rdp_free(ctx->rdp);
		ctx->rdp = NULL;
		metrics_free(ctx->metrics);
		ctx->metrics = NULL;
		stream_dump_free(ctx->dump);
		ctx->dump = NULL;
		free(ctx);
	}
	client->context = NULL;
}

freerdp_peer* freerdp_peer_new(int sockfd)
{
	UINT32 option_value;
	socklen_t option_len;
	freerdp_peer* client;
	client = (freerdp_peer*)calloc(1, sizeof(freerdp_peer));

	if (!client)
		return NULL;

	option_value = TRUE;
	option_len = sizeof(option_value);
	setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void*)&option_value, option_len);

	if (client)
	{
		client->sockfd = sockfd;
		client->ContextSize = sizeof(rdpContext);
		client->Initialize = freerdp_peer_initialize;
#if defined(WITH_FREERDP_DEPRECATED)
		client->GetFileDescriptor = freerdp_peer_get_fds;
#endif
		client->GetEventHandle = freerdp_peer_get_event_handle;
		client->GetEventHandles = freerdp_peer_get_event_handles;
		client->CheckFileDescriptor = freerdp_peer_check_fds;
		client->Close = freerdp_peer_close;
		client->Disconnect = freerdp_peer_disconnect;
		client->SendChannelData = freerdp_peer_send_channel_data;
		client->SendChannelPacket = freerdp_peer_send_channel_packet;
		client->IsWriteBlocked = freerdp_peer_is_write_blocked;
		client->DrainOutputBuffer = freerdp_peer_drain_output_buffer;
		client->HasMoreToRead = freerdp_peer_has_more_to_read;
		client->VirtualChannelOpen = freerdp_peer_virtual_channel_open;
		client->VirtualChannelClose = freerdp_peer_virtual_channel_close;
		client->VirtualChannelWrite = freerdp_peer_virtual_channel_write;
		client->VirtualChannelRead = NULL; /* must be defined by server application */
		client->VirtualChannelGetData = freerdp_peer_virtual_channel_get_data;
		client->VirtualChannelSetData = freerdp_peer_virtual_channel_set_data;
		client->SetState = freerdp_peer_set_state;
	}

	return client;
}

void freerdp_peer_free(freerdp_peer* client)
{
	if (!client)
		return;

	sspi_FreeAuthIdentity(&client->identity);
	closesocket((SOCKET)client->sockfd);
	free(client);
}

BOOL freerdp_peer_context_new_ex(freerdp_peer* client, const rdpSettings* settings)
{
	rdpRdp* rdp;
	rdpContext* context;
	BOOL ret = TRUE;

	if (!client)
		return FALSE;

	if (!(context = (rdpContext*)calloc(1, client->ContextSize)))
		goto fail;

	client->context = context;
	context->peer = client;
	context->ServerMode = TRUE;

	if (settings)
	{
		context->settings = freerdp_settings_clone(settings);
		if (!context->settings)
			goto fail;
	}

	context->dump = stream_dump_new();
	if (!context->dump)
		goto fail;
	if (!(context->metrics = metrics_new(context)))
		goto fail;

	if (!(rdp = rdp_new(context)))
		goto fail;

#if defined(WITH_FREERDP_DEPRECATED)
	client->update = rdp->update;
	client->settings = rdp->settings;
	client->autodetect = rdp->autodetect;
#endif
	context->rdp = rdp;
	context->input = rdp->input;
	context->update = rdp->update;
	context->settings = rdp->settings;
	context->autodetect = rdp->autodetect;
	update_register_server_callbacks(rdp->update);
	autodetect_register_server_callbacks(rdp->autodetect);

	if (!(context->errorDescription = calloc(1, 500)))
	{
		WLog_ERR(TAG, "calloc failed!");
		goto fail;
	}

	if (!transport_attach(rdp->transport, client->sockfd))
		goto fail;

	transport_set_recv_callbacks(rdp->transport, peer_recv_callback, client);
	transport_set_blocking_mode(rdp->transport, FALSE);
	client->IsWriteBlocked = freerdp_peer_is_write_blocked;
	client->DrainOutputBuffer = freerdp_peer_drain_output_buffer;
	client->HasMoreToRead = freerdp_peer_has_more_to_read;
	client->LicenseCallback = freerdp_peer_nolicense;
	IFCALLRET(client->ContextNew, ret, client, client->context);

	if (!ret)
		goto fail;
	return TRUE;

fail:
	WLog_ERR(TAG, "ContextNew callback failed");
	freerdp_peer_context_free(client);
	return FALSE;
}
