/*
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

#include "info.h"
#include "input.h"

#include "connection.h"
#include "transport.h"

#include <freerdp/errorcodes.h>

/**
 *                                      Connection Sequence\n
 *     client                                                                    server\n
 *        |                                                                         |\n
 *        |-----------------------X.224 Connection Request PDU--------------------->|\n
 *        |<----------------------X.224 Connection Confirm PDU----------------------|\n
 *        |-------MCS Connect-Initial PDU with GCC Conference Create Request------->|\n
 *        |<-----MCS Connect-Response PDU with GCC Conference Create Response-------|\n
 *        |------------------------MCS Erect Domain Request PDU-------------------->|\n
 *        |------------------------MCS Attach User Request PDU--------------------->|\n
 *        |<-----------------------MCS Attach User Confirm PDU----------------------|\n
 *        |------------------------MCS Channel Join Request PDU-------------------->|\n
 *        |<-----------------------MCS Channel Join Confirm PDU---------------------|\n
 *        |----------------------------Security Exchange PDU----------------------->|\n
 *        |-------------------------------Client Info PDU-------------------------->|\n
 *        |<---------------------License Error PDU - Valid Client-------------------|\n
 *        |<-----------------------------Demand Active PDU--------------------------|\n
 *        |------------------------------Confirm Active PDU------------------------>|\n
 *        |-------------------------------Synchronize PDU-------------------------->|\n
 *        |---------------------------Control PDU - Cooperate---------------------->|\n
 *        |------------------------Control PDU - Request Control------------------->|\n
 *        |--------------------------Persistent Key List PDU(s)-------------------->|\n
 *        |--------------------------------Font List PDU--------------------------->|\n
 *        |<------------------------------Synchronize PDU---------------------------|\n
 *        |<--------------------------Control PDU - Cooperate-----------------------|\n
 *        |<-----------------------Control PDU - Granted Control--------------------|\n
 *        |<-------------------------------Font Map PDU-----------------------------|\n
 *
 */

/**
 * Establish RDP Connection based on the settings given in the 'rdp' paremeter.
 * @msdn{cc240452}
 * @param rdp RDP module
 * @return true if the connection succeeded. false otherwise.
 */

boolean rdp_client_connect(rdpRdp* rdp)
{
	rdpSettings* settings = rdp->settings;

	nego_init(rdp->nego);
	nego_set_target(rdp->nego, settings->hostname, settings->port);
	nego_set_cookie(rdp->nego, settings->username);
	nego_set_send_preconnection_pdu(rdp->nego, settings->send_preconnection_pdu);
	nego_set_preconnection_id(rdp->nego, settings->preconnection_id);
	nego_set_preconnection_blob(rdp->nego, settings->preconnection_blob);

	nego_set_negotiation_enabled(rdp->nego, settings->security_layer_negotiation);
	nego_enable_rdp(rdp->nego, settings->rdp_security);

	if (!settings->ts_gateway)
	{
		nego_enable_nla(rdp->nego, settings->nla_security);
		nego_enable_tls(rdp->nego, settings->tls_security);
	}

	if (!nego_connect(rdp->nego))
	{
		printf("Error: protocol security negotiation or connection failure\n");
		return false;
	}

	if ((rdp->nego->selected_protocol & PROTOCOL_TLS) || (rdp->nego->selected_protocol == PROTOCOL_RDP))
	{
		if ((settings->username != NULL) && ((settings->password != NULL) || (settings->password_cookie != NULL && settings->password_cookie->length > 0)))
			settings->autologon = true;
	}

	rdp_set_blocking_mode(rdp, false);
	rdp->state = CONNECTION_STATE_NEGO;
	rdp->finalize_sc_pdus = 0;

	if (!mcs_send_connect_initial(rdp->mcs))
	{
		if (!connectErrorCode)
		{
			connectErrorCode = MCSCONNECTINITIALERROR;                      
		}
		printf("Error: unable to send MCS Connect Initial\n");
		return false;
	}

	rdp->transport->process_single_pdu = true;
	while (rdp->state != CONNECTION_STATE_ACTIVE)
	{
		if (rdp_check_fds(rdp) < 0)
			return false;
	}
	rdp->transport->process_single_pdu = false;

	return true;
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

	/* FIXME: this is a subset of rdp_free */
	crypto_rc4_free(rdp->rc4_decrypt_key);
	crypto_rc4_free(rdp->rc4_encrypt_key);
	crypto_des3_free(rdp->fips_encrypt);
	crypto_des3_free(rdp->fips_decrypt);
	crypto_hmac_free(rdp->fips_hmac);
	mcs_free(rdp->mcs);
	nego_free(rdp->nego);
	license_free(rdp->license);
	transport_free(rdp->transport);

	/* FIXME: this is a subset of settings_free */
	freerdp_blob_free(settings->server_random);
	freerdp_blob_free(settings->server_certificate);
	xfree(settings->ip_address);

	rdp->transport = transport_new(settings);
	rdp->license = license_new(rdp);
	rdp->nego = nego_new(rdp->transport);
	rdp->mcs = mcs_new(rdp->transport);

	rdp->transport->layer = TRANSPORT_LAYER_TCP;
	settings->redirected_session_id = redirection->sessionID;

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
	{
		nego_set_routing_token(rdp->nego, &redirection->loadBalanceInfo);
	}
	else
	{
		if (redirection->flags & LB_TARGET_NET_ADDRESS)
		{
			xfree(settings->hostname);
			settings->hostname = xstrdup(redirection->targetNetAddress.ascii);
		}
		else if (redirection->flags & LB_TARGET_FQDN)
		{
			xfree(settings->hostname);
			settings->hostname = xstrdup(redirection->targetFQDN.ascii);
		}
		else if (redirection->flags & LB_TARGET_NETBIOS_NAME)
		{
			xfree(settings->hostname);
			settings->hostname = xstrdup(redirection->targetNetBiosName.ascii);
		}
	}

	if (redirection->flags & LB_USERNAME)
	{
		xfree(settings->username);
		settings->username = xstrdup(redirection->username.ascii);
	}

	if (redirection->flags & LB_DOMAIN)
	{
		xfree(settings->domain);
		settings->domain = xstrdup(redirection->domain.ascii);
	}

	if (redirection->flags & LB_PASSWORD)
	{
		settings->password_cookie = &redirection->password_cookie;
	}

	return rdp_client_connect(rdp);
}

static boolean rdp_client_establish_keys(rdpRdp* rdp)
{
	uint8 client_random[CLIENT_RANDOM_LENGTH];
	uint8 crypt_client_random[256 + 8];
	uint32 key_len;
	uint8* mod;
	uint8* exp;
	uint32 length;
	STREAM* s;

	if (rdp->settings->encryption == false)
	{
		/* no RDP encryption */
		return true;
	}

	/* encrypt client random */
	memset(crypt_client_random, 0, sizeof(crypt_client_random));
	crypto_nonce(client_random, sizeof(client_random));
	key_len = rdp->settings->server_cert->cert_info.modulus.length;
	mod = rdp->settings->server_cert->cert_info.modulus.data;
	exp = rdp->settings->server_cert->cert_info.exponent;
	crypto_rsa_public_encrypt(client_random, sizeof(client_random), key_len, mod, exp, crypt_client_random);

	/* send crypt client random to server */
	length = RDP_PACKET_HEADER_MAX_LENGTH + RDP_SECURITY_HEADER_LENGTH + 4 + key_len + 8;
	s = transport_send_stream_init(rdp->mcs->transport, length);
	rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID);
	rdp_write_security_header(s, SEC_EXCHANGE_PKT);
	length = key_len + 8;
	stream_write_uint32(s, length);
	stream_write(s, crypt_client_random, length);
	if (transport_write(rdp->mcs->transport, s) < 0)
	{
		return false;
	}

	/* now calculate encrypt / decrypt and update keys */
	if (!security_establish_keys(client_random, rdp))
	{
		return false;
	}

	rdp->do_crypt = true;
	if (rdp->settings->salted_checksum)
		rdp->do_secure_checksum = true;

	if (rdp->settings->encryption_method == ENCRYPTION_METHOD_FIPS)
	{
		uint8 fips_ivec[8] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };
		rdp->fips_encrypt = crypto_des3_encrypt_init(rdp->fips_encrypt_key, fips_ivec);
		rdp->fips_decrypt = crypto_des3_decrypt_init(rdp->fips_decrypt_key, fips_ivec);

		rdp->fips_hmac = crypto_hmac_new();
		return true;
	}

	rdp->rc4_decrypt_key = crypto_rc4_init(rdp->decrypt_key, rdp->rc4_key_len);
	rdp->rc4_encrypt_key = crypto_rc4_init(rdp->encrypt_key, rdp->rc4_key_len);

	return true;
}

static boolean rdp_server_establish_keys(rdpRdp* rdp, STREAM* s)
{
	uint8 client_random[64]; /* Should be only 32 after successfull decryption, but on failure might take up to 64 bytes. */
	uint8 crypt_client_random[256 + 8];
	uint32 rand_len, key_len;
	uint16 channel_id, length, sec_flags;
	uint8* mod;
	uint8* priv_exp;

	if (rdp->settings->encryption == false)
	{
		/* No RDP Security. */
		return true;
	}

	if (!rdp_read_header(rdp, s, &length, &channel_id))
	{
		printf("rdp_server_establish_keys: invalid RDP header\n");
		return false;
	}
	rdp_read_security_header(s, &sec_flags);
	if ((sec_flags & SEC_EXCHANGE_PKT) == 0)
	{
		printf("rdp_server_establish_keys: missing SEC_EXCHANGE_PKT in security header\n");
		return false;
	}
	stream_read_uint32(s, rand_len);
	key_len = rdp->settings->server_key->modulus.length;
	if (rand_len != key_len + 8)
	{
		printf("rdp_server_establish_keys: invalid encrypted client random length\n");
		return false;
	}
	memset(crypt_client_random, 0, sizeof(crypt_client_random));
	stream_read(s, crypt_client_random, rand_len);
	/* 8 zero bytes of padding */
	stream_seek(s, 8);
	mod = rdp->settings->server_key->modulus.data;
	priv_exp = rdp->settings->server_key->private_exponent.data;
	crypto_rsa_private_decrypt(crypt_client_random, rand_len - 8, key_len, mod, priv_exp, client_random);

	/* now calculate encrypt / decrypt and update keys */
	if (!security_establish_keys(client_random, rdp))
	{
		return false;
	}

	rdp->do_crypt = true;
	if (rdp->settings->salted_checksum)
		rdp->do_secure_checksum = true;

	if (rdp->settings->encryption_method == ENCRYPTION_METHOD_FIPS)
	{
		uint8 fips_ivec[8] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };
		rdp->fips_encrypt = crypto_des3_encrypt_init(rdp->fips_encrypt_key, fips_ivec);
		rdp->fips_decrypt = crypto_des3_decrypt_init(rdp->fips_decrypt_key, fips_ivec);

		rdp->fips_hmac = crypto_hmac_new();
		return true;
	}

	rdp->rc4_decrypt_key = crypto_rc4_init(rdp->decrypt_key, rdp->rc4_key_len);
	rdp->rc4_encrypt_key = crypto_rc4_init(rdp->encrypt_key, rdp->rc4_key_len);

	return true;
}

boolean rdp_client_connect_mcs_connect_response(rdpRdp* rdp, STREAM* s)
{
	if (!mcs_recv_connect_response(rdp->mcs, s))
	{
		printf("rdp_client_connect_mcs_connect_response: mcs_recv_connect_response failed\n");
		return false;
	}
	if (!mcs_send_erect_domain_request(rdp->mcs))
		return false;
	if (!mcs_send_attach_user_request(rdp->mcs))
		return false;

	rdp->state = CONNECTION_STATE_MCS_ATTACH_USER;

	return true;
}

boolean rdp_client_connect_mcs_attach_user_confirm(rdpRdp* rdp, STREAM* s)
{
	if (!mcs_recv_attach_user_confirm(rdp->mcs, s))
		return false;

	if (!mcs_send_channel_join_request(rdp->mcs, rdp->mcs->user_id))
		return false;

	rdp->state = CONNECTION_STATE_MCS_CHANNEL_JOIN;

	return true;
}

boolean rdp_client_connect_mcs_channel_join_confirm(rdpRdp* rdp, STREAM* s)
{
	int i;
	uint16 channel_id;
	boolean all_joined = true;

	if (!mcs_recv_channel_join_confirm(rdp->mcs, s, &channel_id))
		return false;

	if (!rdp->mcs->user_channel_joined)
	{
		if (channel_id != rdp->mcs->user_id)
			return false;
		rdp->mcs->user_channel_joined = true;

		if (!mcs_send_channel_join_request(rdp->mcs, MCS_GLOBAL_CHANNEL_ID))
			return false;
	}
	else if (!rdp->mcs->global_channel_joined)
	{
		if (channel_id != MCS_GLOBAL_CHANNEL_ID)
			return false;
		rdp->mcs->global_channel_joined = true;

		if (rdp->settings->num_channels > 0)
		{
			if (!mcs_send_channel_join_request(rdp->mcs, rdp->settings->channels[0].channel_id))
				return false;

			all_joined = false;
		}
	}
	else
	{
		for (i = 0; i < rdp->settings->num_channels; i++)
		{
			if (rdp->settings->channels[i].joined)
				continue;

			if (rdp->settings->channels[i].channel_id != channel_id)
				return false;

			rdp->settings->channels[i].joined = true;
			break;
		}
		if (i + 1 < rdp->settings->num_channels)
		{
			if (!mcs_send_channel_join_request(rdp->mcs, rdp->settings->channels[i + 1].channel_id))
				return false;

			all_joined = false;
		}
	}

	if (rdp->mcs->user_channel_joined && rdp->mcs->global_channel_joined && all_joined)
	{
		if (!rdp_client_establish_keys(rdp))
			return false;
		if (!rdp_send_client_info(rdp))
			return false;
		rdp->state = CONNECTION_STATE_LICENSE;
	}

	return true;
}

boolean rdp_client_connect_license(rdpRdp* rdp, STREAM* s)
{
	if (!license_recv(rdp->license, s))
		return false;

	if (rdp->license->state == LICENSE_STATE_ABORTED)
	{
		printf("license connection sequence aborted.\n");
		return false;
	}

	if (rdp->license->state == LICENSE_STATE_COMPLETED)
	{
		rdp->state = CONNECTION_STATE_CAPABILITY;
	}

	return true;
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
		stream_seek(s, RDP_PACKET_HEADER_MAX_LENGTH);

		if (rdp_recv_out_of_sequence_pdu(rdp, s) != true)
			return false;

		return true;
	}

	if (rdp->disconnect)
		return true;

	if (!rdp_send_confirm_active(rdp))
		return false;

	input_register_client_callbacks(rdp->input);

	/**
	 * The server may request a different desktop size during Deactivation-Reactivation sequence.
	 * In this case, the UI should be informed and do actual window resizing at this point.
	 */
	if (width != rdp->settings->width || height != rdp->settings->height)
	{
		IFCALL(rdp->update->DesktopResize, rdp->update->context);
	}

	rdp->state = CONNECTION_STATE_FINALIZATION;
	update_reset_state(rdp->update);

	rdp_client_connect_finalize(rdp);

	return true;
}

boolean rdp_client_connect_finalize(rdpRdp* rdp)
{
	/**
	 * [MS-RDPBCGR] 1.3.1.1 - 8.
	 * The client-to-server PDUs sent during this phase have no dependencies on any of the server-to-
	 * client PDUs; they may be sent as a single batch, provided that sequencing is maintained.
	 */

	if (!rdp_send_client_synchronize_pdu(rdp))
		return false;
	if (!rdp_send_client_control_pdu(rdp, CTRLACTION_COOPERATE))
		return false;
	if (!rdp_send_client_control_pdu(rdp, CTRLACTION_REQUEST_CONTROL))
		return false;
	if (!rdp_send_client_persistent_key_list_pdu(rdp))
		return false;
	if (!rdp_send_client_font_list_pdu(rdp, FONTLIST_FIRST | FONTLIST_LAST))
		return false;

	return true;
}

boolean rdp_server_accept_nego(rdpRdp* rdp, STREAM* s)
{
	boolean status;
	rdpSettings* settings = rdp->settings;

	transport_set_blocking_mode(rdp->transport, true);

	if (!nego_read_request(rdp->nego, s))
		return false;

	rdp->nego->selected_protocol = 0;

	printf("Client Security: NLA:%d TLS:%d RDP:%d\n",
			(rdp->nego->requested_protocols & PROTOCOL_NLA) ? 1 : 0,
			(rdp->nego->requested_protocols & PROTOCOL_TLS)	? 1 : 0,
			(rdp->nego->requested_protocols == PROTOCOL_RDP) ? 1: 0);

	printf("Server Security: NLA:%d TLS:%d RDP:%d\n",
			settings->nla_security, settings->tls_security, settings->rdp_security);

	if ((settings->nla_security) && (rdp->nego->requested_protocols & PROTOCOL_NLA))
	{
		rdp->nego->selected_protocol = PROTOCOL_NLA;
	}
	else if ((settings->tls_security) && (rdp->nego->requested_protocols & PROTOCOL_TLS))
	{
		rdp->nego->selected_protocol = PROTOCOL_TLS;
	}
	else if ((settings->rdp_security) && (rdp->nego->selected_protocol == PROTOCOL_RDP))
	{
		rdp->nego->selected_protocol = PROTOCOL_RDP;
	}
	else
	{
		printf("Protocol security negotiation failure\n");
	}

	printf("Negotiated Security: NLA:%d TLS:%d RDP:%d\n",
			(rdp->nego->selected_protocol & PROTOCOL_NLA) ? 1 : 0,
			(rdp->nego->selected_protocol & PROTOCOL_TLS)	? 1 : 0,
			(rdp->nego->selected_protocol == PROTOCOL_RDP) ? 1: 0);

	if (!nego_send_negotiation_response(rdp->nego))
		return false;

	status = false;
	if (rdp->nego->selected_protocol & PROTOCOL_NLA)
		status = transport_accept_nla(rdp->transport);
	else if (rdp->nego->selected_protocol & PROTOCOL_TLS)
		status = transport_accept_tls(rdp->transport);
	else if (rdp->nego->selected_protocol == PROTOCOL_RDP) /* 0 */
		status = transport_accept_rdp(rdp->transport);

	if (!status)
		return false;

	transport_set_blocking_mode(rdp->transport, false);

	rdp->state = CONNECTION_STATE_NEGO;

	return true;
}

boolean rdp_server_accept_mcs_connect_initial(rdpRdp* rdp, STREAM* s)
{
	int i;

	if (!mcs_recv_connect_initial(rdp->mcs, s))
		return false;

	printf("Accepted client: %s\n", rdp->settings->client_hostname);
	printf("Accepted channels:");
	for (i = 0; i < rdp->settings->num_channels; i++)
	{
		printf(" %s", rdp->settings->channels[i].name);
	}
	printf("\n");

	if (!mcs_send_connect_response(rdp->mcs))
		return false;

	rdp->state = CONNECTION_STATE_MCS_CONNECT;

	return true;
}

boolean rdp_server_accept_mcs_erect_domain_request(rdpRdp* rdp, STREAM* s)
{
	if (!mcs_recv_erect_domain_request(rdp->mcs, s))
		return false;

	rdp->state = CONNECTION_STATE_MCS_ERECT_DOMAIN;

	return true;
}

boolean rdp_server_accept_mcs_attach_user_request(rdpRdp* rdp, STREAM* s)
{
	if (!mcs_recv_attach_user_request(rdp->mcs, s))
		return false;

	if (!mcs_send_attach_user_confirm(rdp->mcs))
		return false;

	rdp->state = CONNECTION_STATE_MCS_ATTACH_USER;

	return true;
}

boolean rdp_server_accept_mcs_channel_join_request(rdpRdp* rdp, STREAM* s)
{
	int i;
	uint16 channel_id;
	boolean all_joined = true;

	if (!mcs_recv_channel_join_request(rdp->mcs, s, &channel_id))
		return false;

	if (!mcs_send_channel_join_confirm(rdp->mcs, channel_id))
		return false;

	if (channel_id == rdp->mcs->user_id)
		rdp->mcs->user_channel_joined = true;
	else if (channel_id == MCS_GLOBAL_CHANNEL_ID)
		rdp->mcs->global_channel_joined = true;

	for (i = 0; i < rdp->settings->num_channels; i++)
	{
		if (rdp->settings->channels[i].channel_id == channel_id)
			rdp->settings->channels[i].joined = true;

		if (!rdp->settings->channels[i].joined)
			all_joined = false;
	}

	if (rdp->mcs->user_channel_joined && rdp->mcs->global_channel_joined && all_joined)
		rdp->state = CONNECTION_STATE_MCS_CHANNEL_JOIN;

	return true;
}

boolean rdp_server_accept_client_keys(rdpRdp* rdp, STREAM* s)
{

	if (!rdp_server_establish_keys(rdp, s))
		return false;

	rdp->state = CONNECTION_STATE_ESTABLISH_KEYS;

	return true;
}

boolean rdp_server_accept_client_info(rdpRdp* rdp, STREAM* s)
{

	if (!rdp_recv_client_info(rdp, s))
		return false;

	if (!license_send_valid_client_error_packet(rdp->license))
		return false;

	rdp->state = CONNECTION_STATE_LICENSE;

	return true;
}

boolean rdp_server_accept_confirm_active(rdpRdp* rdp, STREAM* s)
{
	if (!rdp_recv_confirm_active(rdp, s))
		return false;

	rdp->state = CONNECTION_STATE_ACTIVE;
	update_reset_state(rdp->update);

	if (!rdp_send_server_synchronize_pdu(rdp))
		return false;

	if (!rdp_send_server_control_cooperate_pdu(rdp))
		return false;

	return true;
}

boolean rdp_server_reactivate(rdpRdp* rdp)
{
	if (!rdp_send_deactivate_all(rdp))
		return false;

	rdp->state = CONNECTION_STATE_LICENSE;

	if (!rdp_send_demand_active(rdp))
		return false;

	return true;
}

