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
#include "per.h"

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
	else if (rdp->nego->selected_protocol == PROTOCOL_RDP) /* 0 */
		ret = transport_connect_rdp(rdp->transport);

	if (!ret)
		return False;

	rdp_set_blocking_mode(rdp, False);
	rdp->state = CONNECTION_STATE_NEGO;

	if (!mcs_send_connect_initial(rdp->mcs))
	{
		printf("Error: unable to send MCS Connect Initial\n");
		return False;
	}

	while (rdp->state != CONNECTION_STATE_ACTIVE)
	{
		if (rdp_check_fds(rdp) < 0)
			return False;
	}

	return True;
}

boolean rdp_client_disconnect(rdpRdp* rdp)
{
	return transport_disconnect(rdp->transport);
}

boolean rdp_client_redirect(rdpRdp* rdp)
{
	rdpSettings* settings = rdp->settings;
	rdpRedirection* redirection = rdp->redirection;

	rdp_client_disconnect(rdp);

	mcs_free(rdp->mcs);
	nego_free(rdp->nego);
	license_free(rdp->license);
	transport_free(rdp->transport);
	rdp->transport = transport_new(settings);
	rdp->license = license_new(rdp);
	rdp->nego = nego_new(rdp->transport);
	rdp->mcs = mcs_new(rdp->transport);

	rdp->transport->layer = TRANSPORT_LAYER_TCP;
	settings->redirected_session_id = redirection->sessionID;

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
		nego_set_routing_token(rdp->nego, &redirection->loadBalanceInfo);

	if (redirection->flags & LB_TARGET_NET_ADDRESS)
		settings->hostname = redirection->targetNetAddress.ascii;
	else if (redirection->flags & LB_TARGET_NETBIOS_NAME)
		settings->hostname = redirection->targetNetBiosName.ascii;

	if (redirection->flags & LB_USERNAME)
		settings->username = redirection->username.ascii;

	if (redirection->flags & LB_DOMAIN)
		settings->domain = redirection->domain.ascii;

	return rdp_client_connect(rdp);
}

static boolean rdp_establish_keys(rdpRdp* rdp)
{
	uint8 client_random[32];
	uint8 crypt_client_random[256 + 8];
	uint32 key_len;
	uint8* mod;
	uint8* exp;
	uint32 length;
	STREAM* s;

	if (rdp->settings->encryption == False)
	{
		/* no RDP encryption */
		return True;
	}

	/* encrypt client random */
	memset(crypt_client_random, 0, sizeof(crypt_client_random));
	memset(client_random, 0x5e, 32);
	crypto_nonce(client_random, 32);
	key_len = rdp->settings->server_cert->cert_info.modulus.length;
	mod = rdp->settings->server_cert->cert_info.modulus.data;
	exp = rdp->settings->server_cert->cert_info.exponent;
	crypto_rsa_encrypt(client_random, 32, key_len, mod, exp, crypt_client_random);

	/* send crypt client random to server */
	length = 7 + 8 + 4 + 4 + key_len + 8;
	s = transport_send_stream_init(rdp->mcs->transport, length);
	tpkt_write_header(s, length);
	tpdu_write_header(s, 2, 0xf0);
	per_write_choice(s, DomainMCSPDU_SendDataRequest << 2);
	per_write_integer16(s, rdp->mcs->user_id, MCS_BASE_CHANNEL_ID);
	per_write_integer16(s, MCS_GLOBAL_CHANNEL_ID, 0);
	stream_write_uint8(s, 0x70);
	length = (4 + 4 + key_len + 8) | 0x8000;
	stream_write_uint16_be(s, length);
	stream_write_uint32(s, 1); /* SEC_CLIENT_RANDOM */
	length = key_len + 8;
	stream_write_uint32(s, length);
	memcpy(s->p, crypt_client_random, length);
	stream_seek(s, length);
	if (transport_write(rdp->mcs->transport, s) < 0)
	{
		return False;
	}

	/* now calculate encrypt / decrypt and upate keys */
	if (!security_establish_keys(client_random, rdp->settings))
	{
		return False;
	}

	rdp->do_crypt = True;

	if (rdp->settings->encryption_method == ENCRYPTION_METHOD_FIPS)
	{
		uint8 fips_ivec[8] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };
		rdp->fips_encrypt = crypto_des3_encrypt_init(rdp->settings->fips_encrypt_key, fips_ivec);
		rdp->fips_decrypt = crypto_des3_decrypt_init(rdp->settings->fips_decrypt_key, fips_ivec);

		rdp->fips_hmac = crypto_hmac_new();
		return True;
	}

	rdp->rc4_decrypt_key = crypto_rc4_init(rdp->settings->decrypt_key, rdp->settings->rc4_key_len);
	rdp->rc4_encrypt_key = crypto_rc4_init(rdp->settings->encrypt_key, rdp->settings->rc4_key_len);

	return True;
}

boolean rdp_client_connect_mcs_connect_response(rdpRdp* rdp, STREAM* s)
{
	if (!mcs_recv_connect_response(rdp->mcs, s))
	{
		printf("rdp_client_connect_mcs_connect_response: mcs_recv_connect_response failed\n");
		return False;
	}
	if (!mcs_send_erect_domain_request(rdp->mcs))
		return False;
	if (!mcs_send_attach_user_request(rdp->mcs))
		return False;

	rdp->state = CONNECTION_STATE_MCS_ATTACH_USER;

	return True;
}

boolean rdp_client_connect_mcs_attach_user_confirm(rdpRdp* rdp, STREAM* s)
{
	if (!mcs_recv_attach_user_confirm(rdp->mcs, s))
		return False;

	if (!mcs_send_channel_join_request(rdp->mcs, rdp->mcs->user_id))
		return False;

	rdp->state = CONNECTION_STATE_MCS_CHANNEL_JOIN;

	return True;
}

boolean rdp_client_connect_mcs_channel_join_confirm(rdpRdp* rdp, STREAM* s)
{
	int i;
	uint16 channel_id;
	boolean all_joined = True;

	if (!mcs_recv_channel_join_confirm(rdp->mcs, s, &channel_id))
		return False;

	if (!rdp->mcs->user_channel_joined)
	{
		if (channel_id != rdp->mcs->user_id)
			return False;
		rdp->mcs->user_channel_joined = True;

		if (!mcs_send_channel_join_request(rdp->mcs, MCS_GLOBAL_CHANNEL_ID))
			return False;
	}
	else if (!rdp->mcs->global_channel_joined)
	{
		if (channel_id != MCS_GLOBAL_CHANNEL_ID)
			return False;
		rdp->mcs->global_channel_joined = True;

		if (rdp->settings->num_channels > 0)
		{
			if (!mcs_send_channel_join_request(rdp->mcs, rdp->settings->channels[0].chan_id))
				return False;

			all_joined = False;
		}
	}
	else
	{
		for (i = 0; i < rdp->settings->num_channels; i++)
		{
			if (rdp->settings->channels[i].joined)
				continue;

			if (rdp->settings->channels[i].chan_id != channel_id)
				return False;

			rdp->settings->channels[i].joined = True;
			break;
		}
		if (i + 1 < rdp->settings->num_channels)
		{
			if (!mcs_send_channel_join_request(rdp->mcs, rdp->settings->channels[i + 1].chan_id))
				return False;

			all_joined = False;
		}
	}

	if (rdp->mcs->user_channel_joined && rdp->mcs->global_channel_joined && all_joined)
	{
		if (!rdp_establish_keys(rdp))
			return False;
		if (!rdp_send_client_info(rdp))
			return False;
		rdp->state = CONNECTION_STATE_LICENSE;
	}

	return True;
}

boolean rdp_client_connect_license(rdpRdp* rdp, STREAM* s)
{
	if (!license_recv(rdp->license, s))
		return False;

	if (rdp->license->state == LICENSE_STATE_ABORTED)
	{
		printf("license connection sequence aborted.\n");
		return False;
	}

	if (rdp->license->state == LICENSE_STATE_COMPLETED)
	{
		rdp->state = CONNECTION_STATE_CAPABILITY;
	}

	return True;
}

boolean rdp_client_connect_demand_active(rdpRdp* rdp, STREAM* s)
{
	uint8* mark;
	uint16 width;
	uint16 height;

	width = rdp->settings->width;
	height = rdp->settings->height;

	stream_get_mark(s, mark);

	if (!rdp_recv_demand_active(rdp, s))
	{
		stream_set_mark(s, mark);
		stream_seek(s, RDP_PACKET_HEADER_LENGTH);

		if (rdp_recv_out_of_sequence_pdu(rdp, s) != True)
			return False;

		return True;
	}

	if (!rdp_send_confirm_active(rdp))
		return False;

	/**
	 * The server may request a different desktop size during Deactivation-Reactivation sequence.
	 * In this case, the UI should be informed and do actual window resizing at this point.
	 */
	if (width != rdp->settings->width || height != rdp->settings->height)
	{
		IFCALL(rdp->update->DesktopResize, rdp->update);
	}

	/**
	 * [MS-RDPBCGR] 1.3.1.1 - 8.
	 * The client-to-server PDUs sent during this phase have no dependencies on any of the server-to-
	 * client PDUs; they may be sent as a single batch, provided that sequencing is maintained.
	 */

	if (!rdp_send_client_synchronize_pdu(rdp))
		return False;
	if (!rdp_send_client_control_pdu(rdp, CTRLACTION_COOPERATE))
		return False;
	if (!rdp_send_client_control_pdu(rdp, CTRLACTION_REQUEST_CONTROL))
		return False;
	if (!rdp_send_client_persistent_key_list_pdu(rdp))
		return False;
	if (!rdp_send_client_font_list_pdu(rdp, FONTLIST_FIRST | FONTLIST_LAST))
		return False;

	rdp->state = CONNECTION_STATE_ACTIVE;

	update_reset_state(rdp->update);
	rdp->update->switch_surface.bitmapId = SCREEN_BITMAP_SURFACE;
	IFCALL(rdp->update->SwitchSurface, rdp->update, &(rdp->update->switch_surface));

	return True;
}

boolean rdp_server_accept_nego(rdpRdp* rdp, STREAM* s)
{
	boolean ret;

	transport_set_blocking_mode(rdp->transport, True);

	if (!nego_read_request(rdp->nego, s))
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

	ret = False;
	if (rdp->nego->selected_protocol & PROTOCOL_NLA)
		ret = transport_accept_nla(rdp->transport);
	else if (rdp->nego->selected_protocol & PROTOCOL_TLS)
		ret = transport_accept_tls(rdp->transport);
	else if (rdp->nego->selected_protocol & PROTOCOL_RDP)
		ret = transport_accept_rdp(rdp->transport);

	if (!ret)
		return False;

	transport_set_blocking_mode(rdp->transport, False);

	rdp->state = CONNECTION_STATE_NEGO;

	return True;
}

boolean rdp_server_accept_mcs_connect_initial(rdpRdp* rdp, STREAM* s)
{
	int i;

	if (!mcs_recv_connect_initial(rdp->mcs, s))
		return False;

	printf("Accepted client: %s\n", rdp->settings->client_hostname);
	printf("Accepted channels:");
	for (i = 0; i < rdp->settings->num_channels; i++)
	{
		printf(" %s", rdp->settings->channels[i].name);
	}
	printf("\n");

	if (!mcs_send_connect_response(rdp->mcs))
		return False;

	rdp->state = CONNECTION_STATE_MCS_CONNECT;

	return True;
}

boolean rdp_server_accept_mcs_erect_domain_request(rdpRdp* rdp, STREAM* s)
{
	if (!mcs_recv_erect_domain_request(rdp->mcs, s))
		return False;

	rdp->state = CONNECTION_STATE_MCS_ERECT_DOMAIN;

	return True;
}

boolean rdp_server_accept_mcs_attach_user_request(rdpRdp* rdp, STREAM* s)
{
	if (!mcs_recv_attach_user_request(rdp->mcs, s))
		return False;

	if (!mcs_send_attach_user_confirm(rdp->mcs))
		return False;

	rdp->state = CONNECTION_STATE_MCS_ATTACH_USER;

	return True;
}

boolean rdp_server_accept_mcs_channel_join_request(rdpRdp* rdp, STREAM* s)
{
	int i;
	uint16 channel_id;
	boolean all_joined = True;

	if (!mcs_recv_channel_join_request(rdp->mcs, s, &channel_id))
		return False;

	if (!mcs_send_channel_join_confirm(rdp->mcs, channel_id))
		return False;

	if (channel_id == rdp->mcs->user_id)
		rdp->mcs->user_channel_joined = True;
	else if (channel_id == MCS_GLOBAL_CHANNEL_ID)
		rdp->mcs->global_channel_joined = True;

	for (i = 0; i < rdp->settings->num_channels; i++)
	{
		if (rdp->settings->channels[i].chan_id == channel_id)
			rdp->settings->channels[i].joined = True;

		if (!rdp->settings->channels[i].joined)
			all_joined = False;
	}

	if (rdp->mcs->user_channel_joined && rdp->mcs->global_channel_joined && all_joined)
		rdp->state = CONNECTION_STATE_MCS_CHANNEL_JOIN;

	return True;
}

boolean rdp_server_accept_client_info(rdpRdp* rdp, STREAM* s)
{
	if (!rdp_recv_client_info(rdp, s))
		return False;

	if (!license_send_valid_client_error_packet(rdp->license))
		return False;

	rdp->state = CONNECTION_STATE_LICENSE;

	if (!rdp_send_demand_active(rdp))
		return False;

	return True;
}

boolean rdp_server_accept_confirm_active(rdpRdp* rdp, STREAM* s)
{
	/**
	 * During reactivation sequence the client might sent some input before receiving
	 * the Deactivate All PDU. We need to ignore those noises here.
	 */
	if (!rdp_recv_confirm_active(rdp, s))
		return True;

	rdp->state = CONNECTION_STATE_ACTIVE;

	if (!rdp_send_server_synchronize_pdu(rdp))
		return False;

	if (!rdp_send_server_control_cooperate_pdu(rdp))
		return False;

	return True;
}

boolean rdp_server_reactivate(rdpRdp* rdp)
{
	if (!rdp_send_deactivate_all(rdp))
		return False;

	rdp->state = CONNECTION_STATE_LICENSE;

	if (!rdp_send_demand_active(rdp))
		return False;

	return True;
}

