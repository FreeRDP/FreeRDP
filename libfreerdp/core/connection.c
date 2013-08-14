/*
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "info.h"
#include "input.h"

#include "connection.h"
#include "transport.h"

#include <winpr/crt.h>

#include <freerdp/error.h>
#include <freerdp/listener.h>

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
 *
 * Connection Sequence
 *
 * 1.	Connection Initiation: The client initiates the connection by sending the server a
 * 	Class 0 X.224 Connection Request PDU (section 2.2.1.1). The server responds with a
 * 	Class 0 X.224 Connection Confirm PDU (section 2.2.1.2). From this point, all subsequent
 * 	data sent between client and server is wrapped in an X.224 Data Protocol Data Unit (PDU).
 *
 * 2.	Basic Settings Exchange: Basic settings are exchanged between the client and server by
 * 	using the MCS Connect Initial PDU (section 2.2.1.3) and MCS Connect Response PDU (section 2.2.1.4).
 * 	The Connect Initial PDU contains a Generic Conference Control (GCC) Conference Create Request,
 * 	while the Connect Response PDU contains a GCC Conference Create Response. These two GCC packets
 * 	contain concatenated blocks of settings data (such as core data, security data, and network data)
 * 	which are read by client and server.
 *
 * 3.	Channel Connection: The client sends an MCS Erect Domain Request PDU (section 2.2.1.5),
 * 	followed by an MCS Attach User Request PDU (section 2.2.1.6) to attach the primary user identity
 * 	to the MCS domain. The server responds with an MCS Attach User Confirm PDU (section 2.2.1.7)
 * 	containing the User Channel ID. The client then proceeds to join the user channel, the
 * 	input/output (I/O) channel, and all of the static virtual channels (the I/O and static virtual
 * 	channel IDs are obtained from the data embedded in the GCC packets) by using multiple MCS Channel
 * 	Join Request PDUs (section 2.2.1.8). The server confirms each channel with an MCS Channel Join
 * 	Confirm PDU (section 2.2.1.9). (The client only sends a Channel Join Request after it has received
 * 	the Channel Join Confirm for the previously sent request.)
 *
 * 	From this point, all subsequent data sent from the client to the server is wrapped in an MCS Send
 * 	Data Request PDU, while data sent from the server to the client is wrapped in an MCS Send Data
 * 	Indication PDU. This is in addition to the data being wrapped by an X.224 Data PDU.
 *
 * 4.	RDP Security Commencement: If Standard RDP Security mechanisms (section 5.3) are being employed and
 * 	encryption is in force (this is determined by examining the data embedded in the GCC Conference Create
 * 	Response packet) then the client sends a Security Exchange PDU (section 2.2.1.10) containing an encrypted
 * 	32-byte random number to the server. This random number is encrypted with the public key of the server
 * 	as described in section 5.3.4.1 (the server's public key, as well as a 32-byte server-generated random
 * 	number, are both obtained from the data embedded in the GCC Conference Create Response packet). The client
 * 	and server then utilize the two 32-byte random numbers to generate session keys which are used to encrypt
 * 	and validate the integrity of subsequent RDP traffic.
 *
 * 	From this point, all subsequent RDP traffic can be encrypted and a security header is included with the
 * 	data if encryption is in force. (The Client Info PDU (section 2.2.1.11) and licensing PDUs ([MS-RDPELE]
 * 	section 2.2.2) are an exception in that they always have a security header). The Security Header follows
 * 	the X.224 and MCS Headers and indicates whether the attached data is encrypted. Even if encryption is in
 * 	force, server-to-client traffic may not always be encrypted, while client-to-server traffic must always be
 * 	encrypted (encryption of licensing PDUs is optional, however).
 *
 * 5.	Secure Settings Exchange: Secure client data (such as the username, password, and auto-reconnect cookie)
 * 	is sent to the server by using the Client Info PDU (section 2.2.1.11).
 *
 * 6.	Optional Connect-Time Auto-Detection: During the optional connect-time auto-detect phase the goal is to
 * 	determine characteristics of the network, such as the round-trip latency time and the bandwidth of the link
 * 	between the server and client. This is accomplished by exchanging a collection of PDUs (specified in section 2.2.1.4)
 * 	over a predetermined period of time with enough data to ensure that the results are statistically relevant.
 *
 * 7.	Licensing: The goal of the licensing exchange is to transfer a license from the server to the client.
 * 	The client stores this license and on subsequent connections sends the license to the server for validation.
 * 	However, in some situations the client may not be issued a license to store. In effect, the packets exchanged
 * 	during this phase of the protocol depend on the licensing mechanisms employed by the server. Within the context
 * 	of this document, it is assumed that the client will not be issued a license to store. For details regarding
 * 	more advanced licensing scenarios that take place during the Licensing Phase, see [MS-RDPELE] section 1.3.
 *
 * 8.	Optional Multitransport Bootstrapping: After the connection has been secured and the Licensing Phase has run
 * 	to completion, the server can choose to initiate multitransport connections ([MS-RDPEMT] section 1.3).
 * 	The Initiate Multitransport Request PDU (section 2.2.15.1) is sent by the server to the client and results
 * 	in the out-of-band creation of a multitransport connection using messages from the RDP-UDP, TLS, DTLS, and
 * 	multitransport protocols ([MS-RDPEMT] section 1.3.1).
 *
 * 9.	Capabilities Exchange: The server sends the set of capabilities it supports to the client in a Demand Active PDU
 * 	(section 2.2.1.13.1). The client responds with its capabilities by sending a Confirm Active PDU (section 2.2.1.13.2).
 *
 * 10.	Connection Finalization: The client and server exchange PDUs to finalize the connection details. The client-to-server
 * 	PDUs sent during this phase have no dependencies on any of the server-to-client PDUs; they may be sent as a single batch,
 * 	provided that sequencing is maintained.
 *
 * 	- The Client Synchronize PDU (section 2.2.1.14) is sent after transmitting the Confirm Active PDU.
 * 	- The Client Control (Cooperate) PDU (section 2.2.1.15) is sent after transmitting the Client Synchronize PDU.
 * 	- The Client Control (Request Control) PDU (section 2.2.1.16) is sent after transmitting the Client Control (Cooperate) PDU.
 * 	- The optional Persistent Key List PDUs (section 2.2.1.17) are sent after transmitting the Client Control (Request Control) PDU.
 * 	- The Font List PDU (section 2.2.1.18) is sent after transmitting the Persistent Key List PDUs or, if the Persistent Key List
 * 	  PDUs were not sent, it is sent after transmitting the Client Control (Request Control) PDU (section 2.2.1.16).
 *
 *	The server-to-client PDUs sent during the Connection Finalization Phase have dependencies on the client-to-server PDUs.
 *
 *	- The optional Monitor Layout PDU (section 2.2.12.1) has no dependency on any client-to-server PDUs and is sent after the Demand Active PDU.
 *	- The Server Synchronize PDU (section 2.2.1.19) is sent in response to the Confirm Active PDU.
 *	- The Server Control (Cooperate) PDU (section 2.2.1.20) is sent after transmitting the Server Synchronize PDU.
 *	- The Server Control (Granted Control) PDU (section 2.2.1.21) is sent in response to the Client Control (Request Control) PDU.
 *	- The Font Map PDU (section 2.2.1.22) is sent in response to the Font List PDU.
 *
 *	Once the client has sent the Confirm Active PDU, it can start sending mouse and keyboard input to the server, and upon receipt
 *	of the Font List PDU the server can start sending graphics output to the client.
 *
 *	Besides input and graphics data, other data that can be exchanged between client and server after the connection has been
 *	finalized includes connection management information and virtual channel messages (exchanged between client-side plug-ins
 *	and server-side applications).
 */

/**
 * Establish RDP Connection based on the settings given in the 'rdp' parameter.
 * @msdn{cc240452}
 * @param rdp RDP module
 * @return true if the connection succeeded. FALSE otherwise.
 */

BOOL rdp_client_connect(rdpRdp* rdp)
{
	rdpSettings* settings = rdp->settings;

	nego_init(rdp->nego);
	nego_set_target(rdp->nego, settings->ServerHostname, settings->ServerPort);

	if (settings->GatewayEnabled)
	{
		char* user;
		char* domain;
		char* cookie;
		int user_length = 0;
		int domain_length;
		int cookie_length;

		if (settings->Username)
		{
			user = settings->Username;
			user_length = strlen(settings->Username);
		}

		if (settings->Domain)
			domain = settings->Domain;
		else
			domain = settings->ComputerName;

		domain_length = strlen(domain);

		cookie_length = domain_length + 1 + user_length;
		cookie = (char*) malloc(cookie_length + 1);

		CopyMemory(cookie, domain, domain_length);
		CharUpperBuffA(cookie, domain_length);
		cookie[domain_length] = '\\';

		if (settings->Username)
			CopyMemory(&cookie[domain_length + 1], user, user_length);

		cookie[cookie_length] = '\0';

		nego_set_cookie(rdp->nego, cookie);
		free(cookie);

		settings->RdpSecurity = TRUE;
		settings->TlsSecurity = FALSE;
		settings->NlaSecurity = FALSE;
		settings->ExtSecurity = FALSE;

		//settings->TlsSecurity = TRUE;
		//settings->NlaSecurity = TRUE;
	}
	else
	{
		nego_set_cookie(rdp->nego, settings->Username);
	}

	nego_set_send_preconnection_pdu(rdp->nego, settings->SendPreconnectionPdu);
	nego_set_preconnection_id(rdp->nego, settings->PreconnectionId);
	nego_set_preconnection_blob(rdp->nego, settings->PreconnectionBlob);

	nego_set_negotiation_enabled(rdp->nego, settings->NegotiateSecurityLayer);

	nego_enable_rdp(rdp->nego, settings->RdpSecurity);
	nego_enable_tls(rdp->nego, settings->TlsSecurity);
	nego_enable_nla(rdp->nego, settings->NlaSecurity);
	nego_enable_ext(rdp->nego, settings->ExtSecurity);

	if (settings->MstscCookieMode)
		settings->CookieMaxLength = MSTSC_COOKIE_MAX_LENGTH;

	nego_set_cookie_max_length(rdp->nego, settings->CookieMaxLength);

	if (settings->LoadBalanceInfo)
		nego_set_routing_token(rdp->nego, settings->LoadBalanceInfo, settings->LoadBalanceInfoLength);

	if (!nego_connect(rdp->nego))
	{
		fprintf(stderr, "Error: protocol security negotiation or connection failure\n");
		return FALSE;
	}

	if ((rdp->nego->selected_protocol & PROTOCOL_TLS) || (rdp->nego->selected_protocol == PROTOCOL_RDP))
	{
		if ((settings->Username != NULL) && ((settings->Password != NULL) ||
				(settings->RedirectionPassword != NULL && settings->RedirectionPasswordLength > 0)))
			settings->AutoLogonEnabled = TRUE;
	}

	rdp_set_blocking_mode(rdp, FALSE);

	rdp_client_transition_to_state(rdp, CONNECTION_STATE_NEGO);
	rdp->finalize_sc_pdus = 0;

	if (!mcs_send_connect_initial(rdp->mcs))
	{
		if (!connectErrorCode)
		{
			connectErrorCode = MCSCONNECTINITIALERROR;                      
		}
		fprintf(stderr, "Error: unable to send MCS Connect Initial\n");
		return FALSE;
	}

	while (rdp->state != CONNECTION_STATE_ACTIVE)
	{
		if (rdp_check_fds(rdp) < 0)
			return FALSE;
	}

	return TRUE;
}

BOOL rdp_client_disconnect(rdpRdp* rdp)
{
	return transport_disconnect(rdp->transport);
}

BOOL rdp_client_redirect(rdpRdp* rdp)
{
	rdpSettings* settings = rdp->settings;
	rdpRedirection* redirection = rdp->redirection;

	rdp_client_disconnect(rdp);

	/* FIXME: this is a subset of rdp_free */
	/* --> this should really go into rdp.c */
	crypto_rc4_free(rdp->rc4_decrypt_key);
	rdp->rc4_decrypt_key = NULL ;
	crypto_rc4_free(rdp->rc4_encrypt_key);
	rdp->rc4_encrypt_key = NULL;
	crypto_des3_free(rdp->fips_encrypt);
	rdp->fips_encrypt = NULL ;
	crypto_des3_free(rdp->fips_decrypt);
	rdp->fips_decrypt = NULL ;
	crypto_hmac_free(rdp->fips_hmac);
	rdp->fips_hmac = NULL ;

	free(settings->ServerRandom);
	settings->ServerRandom = NULL ;
	free(settings->ServerCertificate);
	settings->ServerCertificate = NULL ;
	free(settings->ClientAddress);
	settings->ClientAddress = NULL ;

	mppc_enc_free(rdp->mppc_enc);
	mppc_dec_free(rdp->mppc_dec);
	mcs_free(rdp->mcs);
	nego_free(rdp->nego);
	license_free(rdp->license);
	transport_free(rdp->transport);

	rdp->transport = transport_new(settings);
	rdp->license = license_new(rdp);
	rdp->nego = nego_new(rdp->transport);
	rdp->mcs = mcs_new(rdp->transport);
	rdp->mppc_dec = mppc_dec_new();
	rdp->mppc_enc = mppc_enc_new(PROTO_RDP_50);

	rdp->transport->layer = TRANSPORT_LAYER_TCP;
	settings->RedirectedSessionId = redirection->sessionID;

	if (redirection->flags & LB_LOAD_BALANCE_INFO)
	{
		nego_set_routing_token(rdp->nego, redirection->LoadBalanceInfo, redirection->LoadBalanceInfoLength);
	}
	else
	{
		if (redirection->flags & LB_TARGET_NET_ADDRESS)
		{
			free(settings->ServerHostname);
			settings->ServerHostname = _strdup(redirection->targetNetAddress.ascii);
		}
		else if (redirection->flags & LB_TARGET_FQDN)
		{
			free(settings->ServerHostname);
			settings->ServerHostname = _strdup(redirection->targetFQDN.ascii);
		}
		else if (redirection->flags & LB_TARGET_NETBIOS_NAME)
		{
			free(settings->ServerHostname);
			settings->ServerHostname = _strdup(redirection->targetNetBiosName.ascii);
		}
	}

	if (redirection->flags & LB_USERNAME)
	{
		free(settings->Username);
		settings->Username = _strdup(redirection->username.ascii);
	}

	if (redirection->flags & LB_DOMAIN)
	{
		free(settings->Domain);
		settings->Domain = _strdup(redirection->domain.ascii);
	}

	if (redirection->flags & LB_PASSWORD)
	{
		settings->RedirectionPassword = redirection->PasswordCookie;
		settings->RedirectionPasswordLength = redirection->PasswordCookieLength;
	}

	return rdp_client_connect(rdp);
}

static BOOL rdp_client_establish_keys(rdpRdp* rdp)
{
	BYTE* mod;
	BYTE* exp;
	wStream* s;
	UINT32 length;
	UINT32 key_len;
	BYTE crypt_client_random[256 + 8];
	BYTE client_random[CLIENT_RANDOM_LENGTH];

	if (!rdp->settings->DisableEncryption)
	{
		/* no RDP encryption */
		return TRUE;
	}

	/* encrypt client random */
	ZeroMemory(crypt_client_random, sizeof(crypt_client_random));
	crypto_nonce(client_random, sizeof(client_random));
	key_len = rdp->settings->RdpServerCertificate->cert_info.ModulusLength;
	mod = rdp->settings->RdpServerCertificate->cert_info.Modulus;
	exp = rdp->settings->RdpServerCertificate->cert_info.exponent;
	crypto_rsa_public_encrypt(client_random, sizeof(client_random), key_len, mod, exp, crypt_client_random);

	/* send crypt client random to server */
	length = RDP_PACKET_HEADER_MAX_LENGTH + RDP_SECURITY_HEADER_LENGTH + 4 + key_len + 8;
	s = Stream_New(NULL, length);

	rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID);
	rdp_write_security_header(s, SEC_EXCHANGE_PKT);
	length = key_len + 8;

	Stream_Write_UINT32(s, length);
	Stream_Write(s, crypt_client_random, length);
	Stream_SealLength(s);

	if (transport_write(rdp->mcs->transport, s) < 0)
	{
		return FALSE;
	}

	Stream_Free(s, TRUE);

	/* now calculate encrypt / decrypt and update keys */
	if (!security_establish_keys(client_random, rdp))
	{
		return FALSE;
	}

	rdp->do_crypt = TRUE;

	if (rdp->settings->SaltedChecksum)
		rdp->do_secure_checksum = TRUE;

	if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
	{
		BYTE fips_ivec[8] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };
		rdp->fips_encrypt = crypto_des3_encrypt_init(rdp->fips_encrypt_key, fips_ivec);
		rdp->fips_decrypt = crypto_des3_decrypt_init(rdp->fips_decrypt_key, fips_ivec);

		rdp->fips_hmac = crypto_hmac_new();
		return TRUE;
	}

	rdp->rc4_decrypt_key = crypto_rc4_init(rdp->decrypt_key, rdp->rc4_key_len);
	rdp->rc4_encrypt_key = crypto_rc4_init(rdp->encrypt_key, rdp->rc4_key_len);

	return TRUE;
}

BOOL rdp_server_establish_keys(rdpRdp* rdp, wStream* s)
{
	BYTE client_random[64]; /* Should be only 32 after successful decryption, but on failure might take up to 64 bytes. */
	BYTE crypt_client_random[256 + 8];
	UINT32 rand_len, key_len;
	UINT16 channel_id, length, sec_flags;
	BYTE* mod;
	BYTE* priv_exp;

	if (!rdp->settings->DisableEncryption)
	{
		/* No RDP Security. */
		return TRUE;
	}

	if (!rdp_read_header(rdp, s, &length, &channel_id))
	{
		fprintf(stderr, "rdp_server_establish_keys: invalid RDP header\n");
		return FALSE;
	}

	if (!rdp_read_security_header(s, &sec_flags))
		return FALSE;

	if ((sec_flags & SEC_EXCHANGE_PKT) == 0)
	{
		fprintf(stderr, "rdp_server_establish_keys: missing SEC_EXCHANGE_PKT in security header\n");
		return FALSE;
	}

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, rand_len);

	if (Stream_GetRemainingLength(s) < rand_len + 8)  /* include 8 bytes of padding */
		return FALSE;

	key_len = rdp->settings->RdpServerRsaKey->ModulusLength;

	if (rand_len != key_len + 8)
	{
		fprintf(stderr, "rdp_server_establish_keys: invalid encrypted client random length\n");
		return FALSE;
	}

	ZeroMemory(crypt_client_random, sizeof(crypt_client_random));
	Stream_Read(s, crypt_client_random, rand_len);
	/* 8 zero bytes of padding */
	Stream_Seek(s, 8);
	mod = rdp->settings->RdpServerRsaKey->Modulus;
	priv_exp = rdp->settings->RdpServerRsaKey->PrivateExponent;
	crypto_rsa_private_decrypt(crypt_client_random, rand_len - 8, key_len, mod, priv_exp, client_random);

	/* now calculate encrypt / decrypt and update keys */
	if (!security_establish_keys(client_random, rdp))
	{
		return FALSE;
	}

	rdp->do_crypt = TRUE;

	if (rdp->settings->SaltedChecksum)
		rdp->do_secure_checksum = TRUE;

	if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
	{
		BYTE fips_ivec[8] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };
		rdp->fips_encrypt = crypto_des3_encrypt_init(rdp->fips_encrypt_key, fips_ivec);
		rdp->fips_decrypt = crypto_des3_decrypt_init(rdp->fips_decrypt_key, fips_ivec);

		rdp->fips_hmac = crypto_hmac_new();
		return TRUE;
	}

	rdp->rc4_decrypt_key = crypto_rc4_init(rdp->decrypt_key, rdp->rc4_key_len);
	rdp->rc4_encrypt_key = crypto_rc4_init(rdp->encrypt_key, rdp->rc4_key_len);

	return TRUE;
}

BOOL rdp_client_connect_mcs_connect_response(rdpRdp* rdp, wStream* s)
{
	if (!mcs_recv_connect_response(rdp->mcs, s))
	{
		fprintf(stderr, "rdp_client_connect_mcs_connect_response: mcs_recv_connect_response failed\n");
		return FALSE;
	}

	if (!mcs_send_erect_domain_request(rdp->mcs))
		return FALSE;

	if (!mcs_send_attach_user_request(rdp->mcs))
		return FALSE;

	rdp_client_transition_to_state(rdp, CONNECTION_STATE_MCS_ATTACH_USER);

	return TRUE;
}

BOOL rdp_client_connect_mcs_attach_user_confirm(rdpRdp* rdp, wStream* s)
{
	if (!mcs_recv_attach_user_confirm(rdp->mcs, s))
		return FALSE;

	if (!mcs_send_channel_join_request(rdp->mcs, rdp->mcs->user_id))
		return FALSE;

	rdp_client_transition_to_state(rdp, CONNECTION_STATE_MCS_CHANNEL_JOIN);

	return TRUE;
}

BOOL rdp_client_connect_mcs_channel_join_confirm(rdpRdp* rdp, wStream* s)
{
	int i;
	UINT16 channel_id;
	BOOL all_joined = TRUE;

	if (!mcs_recv_channel_join_confirm(rdp->mcs, s, &channel_id))
		return FALSE;

	if (!rdp->mcs->user_channel_joined)
	{
		if (channel_id != rdp->mcs->user_id)
			return FALSE;

		rdp->mcs->user_channel_joined = TRUE;

		if (!mcs_send_channel_join_request(rdp->mcs, MCS_GLOBAL_CHANNEL_ID))
			return FALSE;
	}
	else if (!rdp->mcs->global_channel_joined)
	{
		if (channel_id != MCS_GLOBAL_CHANNEL_ID)
			return FALSE;

		rdp->mcs->global_channel_joined = TRUE;

		if (rdp->settings->ChannelCount > 0)
		{
			if (!mcs_send_channel_join_request(rdp->mcs, rdp->settings->ChannelDefArray[0].ChannelId))
				return FALSE;

			all_joined = FALSE;
		}
	}
	else
	{
		for (i = 0; i < rdp->settings->ChannelCount; i++)
		{
			if (rdp->settings->ChannelDefArray[i].joined)
				continue;

			if (rdp->settings->ChannelDefArray[i].ChannelId != channel_id)
				return FALSE;

			rdp->settings->ChannelDefArray[i].joined = TRUE;
			break;
		}

		if (i + 1 < rdp->settings->ChannelCount)
		{
			if (!mcs_send_channel_join_request(rdp->mcs, rdp->settings->ChannelDefArray[i + 1].ChannelId))
				return FALSE;

			all_joined = FALSE;
		}
	}

	if (rdp->mcs->user_channel_joined && rdp->mcs->global_channel_joined && all_joined)
	{
		if (!rdp_client_establish_keys(rdp))
			return FALSE;

		if (!rdp_send_client_info(rdp))
			return FALSE;

		rdp_client_transition_to_state(rdp, CONNECTION_STATE_LICENSING);
	}

	return TRUE;
}

BOOL rdp_client_connect_license(rdpRdp* rdp, wStream* s)
{
	if (!license_recv(rdp->license, s))
		return FALSE;

	if (rdp->license->state == LICENSE_STATE_ABORTED)
	{
		fprintf(stderr, "license connection sequence aborted.\n");
		return FALSE;
	}

	if (rdp->license->state == LICENSE_STATE_COMPLETED)
	{
		rdp_client_transition_to_state(rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE);
	}

	return TRUE;
}

BOOL rdp_client_connect_demand_active(rdpRdp* rdp, wStream* s)
{
	BYTE* mark;
	UINT16 width;
	UINT16 height;

	width = rdp->settings->DesktopWidth;
	height = rdp->settings->DesktopHeight;

	Stream_GetPointer(s, mark);

	if (!rdp_recv_demand_active(rdp, s))
	{
		UINT16 channelId;
		Stream_SetPointer(s, mark);
		rdp_recv_get_active_header(rdp, s, &channelId);
		/* Was Stream_Seek(s, RDP_PACKET_HEADER_MAX_LENGTH);
		 * but the headers aren't always that length,
		 * so that could result in a bad offset.
		 */

		if (rdp_recv_out_of_sequence_pdu(rdp, s) != TRUE)
			return FALSE;

		return TRUE;
	}

	if (rdp->disconnect)
		return TRUE;

	if (!rdp_send_confirm_active(rdp))
		return FALSE;

	input_register_client_callbacks(rdp->input);

	/**
	 * The server may request a different desktop size during Deactivation-Reactivation sequence.
	 * In this case, the UI should be informed and do actual window resizing at this point.
	 */
	if (width != rdp->settings->DesktopWidth || height != rdp->settings->DesktopHeight)
	{
		IFCALL(rdp->update->DesktopResize, rdp->update->context);
	}

	rdp_client_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION);

	return rdp_client_connect_finalize(rdp);
}

BOOL rdp_client_connect_finalize(rdpRdp* rdp)
{
	/**
	 * [MS-RDPBCGR] 1.3.1.1 - 8.
	 * The client-to-server PDUs sent during this phase have no dependencies on any of the server-to-
	 * client PDUs; they may be sent as a single batch, provided that sequencing is maintained.
	 */

	if (!rdp_send_client_synchronize_pdu(rdp))
		return FALSE;

	if (!rdp_send_client_control_pdu(rdp, CTRLACTION_COOPERATE))
		return FALSE;

	if (!rdp_send_client_control_pdu(rdp, CTRLACTION_REQUEST_CONTROL))
		return FALSE;
	/**
	 * [MS-RDPBCGR] 2.2.1.17
	 * Client persistent key list must be sent if a bitmap is
	 * stored in persistent bitmap cache or the server has advertised support for bitmap
	 * host cache and a deactivation reactivation sequence is *not* in progress.
	 */

	if (!rdp->deactivation_reactivation && rdp->settings->BitmapCachePersistEnabled)
	{
		if (!rdp_send_client_persistent_key_list_pdu(rdp))
			return FALSE;
	}

	if (!rdp_send_client_font_list_pdu(rdp, FONTLIST_FIRST | FONTLIST_LAST))
		return FALSE;

	return TRUE;
}

int rdp_client_transition_to_state(rdpRdp* rdp, int state)
{
	int status = 0;

	switch (state)
	{
		case CONNECTION_STATE_INITIAL:
			rdp->state = CONNECTION_STATE_INITIAL;
			break;

		case CONNECTION_STATE_NEGO:
			rdp->state = CONNECTION_STATE_NEGO;
			break;

		case CONNECTION_STATE_MCS_CONNECT:
			rdp->state = CONNECTION_STATE_MCS_CONNECT;
			break;

		case CONNECTION_STATE_MCS_ERECT_DOMAIN:
			rdp->state = CONNECTION_STATE_MCS_ERECT_DOMAIN;
			break;

		case CONNECTION_STATE_MCS_ATTACH_USER:
			rdp->state = CONNECTION_STATE_MCS_ATTACH_USER;
			break;

		case CONNECTION_STATE_MCS_CHANNEL_JOIN:
			rdp->state = CONNECTION_STATE_MCS_CHANNEL_JOIN;
			break;

		case CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT:
			rdp->state = CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT;
			break;

		case CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE:
			rdp->state = CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE;
			break;

		case CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT:
			rdp->state = CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT;
			break;

		case CONNECTION_STATE_LICENSING:
			rdp->state = CONNECTION_STATE_LICENSING;
			break;

		case CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING:
			rdp->state = CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING;
			break;

		case CONNECTION_STATE_CAPABILITIES_EXCHANGE:
			rdp->state = CONNECTION_STATE_CAPABILITIES_EXCHANGE;
			break;

		case CONNECTION_STATE_FINALIZATION:
			rdp->state = CONNECTION_STATE_FINALIZATION;
			update_reset_state(rdp->update);
			rdp->finalize_sc_pdus = 0;
			break;

		case CONNECTION_STATE_ACTIVE:
			rdp->state = CONNECTION_STATE_ACTIVE;
			break;

		default:
			status = -1;
			break;
	}

	return status;
}

BOOL rdp_server_accept_nego(rdpRdp* rdp, wStream* s)
{
	BOOL status;
	rdpSettings* settings = rdp->settings;

	transport_set_blocking_mode(rdp->transport, TRUE);

	if (!nego_read_request(rdp->nego, s))
		return FALSE;

	rdp->nego->selected_protocol = 0;

	fprintf(stderr, "Client Security: NLA:%d TLS:%d RDP:%d\n",
			(rdp->nego->requested_protocols & PROTOCOL_NLA) ? 1 : 0,
					(rdp->nego->requested_protocols & PROTOCOL_TLS)	? 1 : 0,
							(rdp->nego->requested_protocols == PROTOCOL_RDP) ? 1: 0);

	fprintf(stderr, "Server Security: NLA:%d TLS:%d RDP:%d\n",
			settings->NlaSecurity, settings->TlsSecurity, settings->RdpSecurity);

	if ((settings->NlaSecurity) && (rdp->nego->requested_protocols & PROTOCOL_NLA))
	{
		rdp->nego->selected_protocol = PROTOCOL_NLA;
	}
	else if ((settings->TlsSecurity) && (rdp->nego->requested_protocols & PROTOCOL_TLS))
	{
		rdp->nego->selected_protocol = PROTOCOL_TLS;
	}
	else if ((settings->RdpSecurity) && (rdp->nego->selected_protocol == PROTOCOL_RDP))
	{
		rdp->nego->selected_protocol = PROTOCOL_RDP;
	}
	else
	{
		fprintf(stderr, "Protocol security negotiation failure\n");
	}

	fprintf(stderr, "Negotiated Security: NLA:%d TLS:%d RDP:%d\n",
			(rdp->nego->selected_protocol & PROTOCOL_NLA) ? 1 : 0,
					(rdp->nego->selected_protocol & PROTOCOL_TLS)	? 1 : 0,
							(rdp->nego->selected_protocol == PROTOCOL_RDP) ? 1: 0);

	if (!nego_send_negotiation_response(rdp->nego))
		return FALSE;

	status = FALSE;

	if (rdp->nego->selected_protocol & PROTOCOL_NLA)
		status = transport_accept_nla(rdp->transport);
	else if (rdp->nego->selected_protocol & PROTOCOL_TLS)
		status = transport_accept_tls(rdp->transport);
	else if (rdp->nego->selected_protocol == PROTOCOL_RDP) /* 0 */
		status = transport_accept_rdp(rdp->transport);

	if (!status)
		return FALSE;

	transport_set_blocking_mode(rdp->transport, FALSE);

	rdp_server_transition_to_state(rdp, CONNECTION_STATE_NEGO);

	return TRUE;
}

BOOL rdp_server_accept_mcs_connect_initial(rdpRdp* rdp, wStream* s)
{
	int i;

	if (!mcs_recv_connect_initial(rdp->mcs, s))
		return FALSE;

	fprintf(stderr, "Accepted client: %s\n", rdp->settings->ClientHostname);
	fprintf(stderr, "Accepted channels:");

	for (i = 0; i < rdp->settings->ChannelCount; i++)
	{
		fprintf(stderr, " %s", rdp->settings->ChannelDefArray[i].Name);
	}
	fprintf(stderr, "\n");

	if (!mcs_send_connect_response(rdp->mcs))
		return FALSE;

	rdp_server_transition_to_state(rdp, CONNECTION_STATE_MCS_CONNECT);

	return TRUE;
}

BOOL rdp_server_accept_mcs_erect_domain_request(rdpRdp* rdp, wStream* s)
{
	if (!mcs_recv_erect_domain_request(rdp->mcs, s))
		return FALSE;

	rdp_server_transition_to_state(rdp, CONNECTION_STATE_MCS_ERECT_DOMAIN);

	return TRUE;
}

BOOL rdp_server_accept_mcs_attach_user_request(rdpRdp* rdp, wStream* s)
{
	if (!mcs_recv_attach_user_request(rdp->mcs, s))
		return FALSE;

	if (!mcs_send_attach_user_confirm(rdp->mcs))
		return FALSE;

	rdp_server_transition_to_state(rdp, CONNECTION_STATE_MCS_ATTACH_USER);

	return TRUE;
}

BOOL rdp_server_accept_mcs_channel_join_request(rdpRdp* rdp, wStream* s)
{
	int i;
	UINT16 channel_id;
	BOOL all_joined = TRUE;

	if (!mcs_recv_channel_join_request(rdp->mcs, s, &channel_id))
		return FALSE;

	if (!mcs_send_channel_join_confirm(rdp->mcs, channel_id))
		return FALSE;

	if (channel_id == rdp->mcs->user_id)
		rdp->mcs->user_channel_joined = TRUE;
	else if (channel_id == MCS_GLOBAL_CHANNEL_ID)
		rdp->mcs->global_channel_joined = TRUE;

	for (i = 0; i < rdp->settings->ChannelCount; i++)
	{
		if (rdp->settings->ChannelDefArray[i].ChannelId == channel_id)
			rdp->settings->ChannelDefArray[i].joined = TRUE;

		if (!rdp->settings->ChannelDefArray[i].joined)
			all_joined = FALSE;
	}

	if ((rdp->mcs->user_channel_joined) && (rdp->mcs->global_channel_joined) && all_joined)
	{
		rdp_server_transition_to_state(rdp, CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT);
	}

	return TRUE;
}

BOOL rdp_server_accept_confirm_active(rdpRdp* rdp, wStream* s)
{
	if (rdp->state != CONNECTION_STATE_CAPABILITIES_EXCHANGE)
		return FALSE;

	if (!rdp_recv_confirm_active(rdp, s))
		return FALSE;

	rdp_server_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION);

	if (!rdp_send_server_synchronize_pdu(rdp))
		return FALSE;

	if (!rdp_send_server_control_cooperate_pdu(rdp))
		return FALSE;

	return TRUE;
}

BOOL rdp_server_reactivate(rdpRdp* rdp)
{
	if (!rdp_send_deactivate_all(rdp))
		return FALSE;

	rdp_server_transition_to_state(rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE);

	if (!rdp_send_demand_active(rdp))
		return FALSE;

	return TRUE;
}

int rdp_server_transition_to_state(rdpRdp* rdp, int state)
{
	int status = 0;
	freerdp_peer* client = NULL;

	if (rdp->state >= CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT)
		client = rdp->context->peer;

	if (rdp->state < CONNECTION_STATE_ACTIVE)
	{
		if (client)
			client->activated = FALSE;
	}

	switch (state)
	{
		case CONNECTION_STATE_INITIAL:
			rdp->state = CONNECTION_STATE_INITIAL;
			break;

		case CONNECTION_STATE_NEGO:
			rdp->state = CONNECTION_STATE_NEGO;
			break;

		case CONNECTION_STATE_MCS_CONNECT:
			rdp->state = CONNECTION_STATE_MCS_CONNECT;
			break;

		case CONNECTION_STATE_MCS_ERECT_DOMAIN:
			rdp->state = CONNECTION_STATE_MCS_ERECT_DOMAIN;
			break;

		case CONNECTION_STATE_MCS_ATTACH_USER:
			rdp->state = CONNECTION_STATE_MCS_ATTACH_USER;
			break;

		case CONNECTION_STATE_MCS_CHANNEL_JOIN:
			rdp->state = CONNECTION_STATE_MCS_CHANNEL_JOIN;
			break;

		case CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT:
			rdp->state = CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT;
			break;

		case CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE:
			rdp->state = CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE;
			break;

		case CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT:
			rdp->state = CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT;
			break;

		case CONNECTION_STATE_LICENSING:
			rdp->state = CONNECTION_STATE_LICENSING;
			break;

		case CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING:
			rdp->state = CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING;
			break;

		case CONNECTION_STATE_CAPABILITIES_EXCHANGE:
			rdp->state = CONNECTION_STATE_CAPABILITIES_EXCHANGE;
			rdp->AwaitCapabilities = FALSE;
			break;

		case CONNECTION_STATE_FINALIZATION:
			rdp->state = CONNECTION_STATE_FINALIZATION;
			rdp->finalize_sc_pdus = 0;
			break;

		case CONNECTION_STATE_ACTIVE:
			rdp->state = CONNECTION_STATE_ACTIVE;
			update_reset_state(rdp->update);

			if (client)
			{
				if (!client->connected)
				{
					/**
					 * PostConnect should only be called once and should not
					 * be called after a reactivation sequence.
					 */

					IFCALLRET(client->PostConnect, client->connected, client);

					if (!client->connected)
						return -1;
				}

				IFCALLRET(client->Activate, client->activated, client);

				if (!client->activated)
					return -1;
			}

			break;

		default:
			status = -1;
			break;
	}

	return status;
}
