/*
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Connection Sequence
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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
#include "rdp.h"

#include "connection.h"
#include "transport.h"

#include <winpr/crt.h>
#include <winpr/crypto.h>

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/listener.h>

#define TAG FREERDP_TAG("core.connection")

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
	BOOL status;
	rdpSettings* settings = rdp->settings;

	nego_init(rdp->nego);
	nego_set_target(rdp->nego, settings->ServerHostname, settings->ServerPort);

	if (settings->GatewayEnabled)
	{
		char* user = NULL;
		char* domain = NULL;
		char* cookie = NULL;
		int user_length = 0;
		int domain_length = 0;
		int cookie_length = 0;

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

		if (!cookie)
			return FALSE;

		CopyMemory(cookie, domain, domain_length);
		CharUpperBuffA(cookie, domain_length);
		cookie[domain_length] = '\\';

		if (settings->Username)
			CopyMemory(&cookie[domain_length + 1], user, user_length);

		cookie[cookie_length] = '\0';

		status = nego_set_cookie(rdp->nego, cookie);
		free(cookie);
	}
	else
	{
		status = nego_set_cookie(rdp->nego, settings->Username);
	}

	if (!status)
		return FALSE;

	nego_set_send_preconnection_pdu(rdp->nego, settings->SendPreconnectionPdu);
	nego_set_preconnection_id(rdp->nego, settings->PreconnectionId);
	nego_set_preconnection_blob(rdp->nego, settings->PreconnectionBlob);

	nego_set_negotiation_enabled(rdp->nego, settings->NegotiateSecurityLayer);
	nego_set_restricted_admin_mode_required(rdp->nego, settings->RestrictedAdminModeRequired);

	nego_set_gateway_enabled(rdp->nego, settings->GatewayEnabled);
	nego_set_gateway_bypass_local(rdp->nego, settings->GatewayBypassLocal);

	nego_enable_rdp(rdp->nego, settings->RdpSecurity);
	nego_enable_tls(rdp->nego, settings->TlsSecurity);
	nego_enable_nla(rdp->nego, settings->NlaSecurity);
	nego_enable_ext(rdp->nego, settings->ExtSecurity);

	if (settings->MstscCookieMode)
		settings->CookieMaxLength = MSTSC_COOKIE_MAX_LENGTH;

	nego_set_cookie_max_length(rdp->nego, settings->CookieMaxLength);

	if (settings->LoadBalanceInfo)
	{
		if (!nego_set_routing_token(rdp->nego, settings->LoadBalanceInfo, settings->LoadBalanceInfoLength))
			return FALSE;
	}

	rdp_client_transition_to_state(rdp, CONNECTION_STATE_NEGO);

	if (!nego_connect(rdp->nego))
	{
		if (!freerdp_get_last_error(rdp->context))
			freerdp_set_last_error(rdp->context, FREERDP_ERROR_SECURITY_NEGO_CONNECT_FAILED);

		WLog_ERR(TAG, "Error: protocol security negotiation or connection failure");
		return FALSE;
	}

	if ((rdp->nego->SelectedProtocol & PROTOCOL_TLS) || (rdp->nego->SelectedProtocol == PROTOCOL_RDP))
	{
		if ((settings->Username != NULL) && ((settings->Password != NULL) ||
				(settings->RedirectionPassword != NULL && settings->RedirectionPasswordLength > 0)))
			settings->AutoLogonEnabled = TRUE;
	}

	/* everything beyond this point is event-driven and non blocking */

	rdp->transport->ReceiveCallback = rdp_recv_callback;
	rdp->transport->ReceiveExtra = rdp;
	transport_set_blocking_mode(rdp->transport, FALSE);

	if (rdp->state != CONNECTION_STATE_NLA)
	{
		if (!mcs_client_begin(rdp->mcs))
			return FALSE;
	}

	while (rdp->state != CONNECTION_STATE_ACTIVE)
	{
		if (rdp_check_fds(rdp) < 0)
		{
			if (!freerdp_get_last_error(rdp->context))
				freerdp_set_last_error(rdp->context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
			return FALSE;
		}
	}

	return TRUE;
}

BOOL rdp_client_disconnect(rdpRdp* rdp)
{
	BOOL status;

	status = nego_disconnect(rdp->nego);

	rdp_reset(rdp);

	rdp_client_transition_to_state(rdp, CONNECTION_STATE_INITIAL);

	return status;
}

BOOL rdp_client_redirect(rdpRdp* rdp)
{
	BOOL status;
	rdpSettings* settings = rdp->settings;

	rdp_client_disconnect(rdp);
	if (rdp_redirection_apply_settings(rdp) != 0)
		return FALSE;

	if (settings->RedirectionFlags & LB_LOAD_BALANCE_INFO)
	{
		if (!nego_set_routing_token(rdp->nego, settings->LoadBalanceInfo, settings->LoadBalanceInfoLength))
			return FALSE;
	}
	else
	{
		if (settings->RedirectionFlags & LB_TARGET_FQDN)
		{
			free(settings->ServerHostname);
			settings->ServerHostname = _strdup(settings->RedirectionTargetFQDN);
			if (!settings->ServerHostname)
				return FALSE;
		}
		else if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESS)
		{
			free(settings->ServerHostname);
			settings->ServerHostname = _strdup(settings->TargetNetAddress);
			if (!settings->ServerHostname)
				return FALSE;
		}
		else if (settings->RedirectionFlags & LB_TARGET_NETBIOS_NAME)
		{
			free(settings->ServerHostname);
			settings->ServerHostname = _strdup(settings->RedirectionTargetNetBiosName);
			if (!settings->ServerHostname)
				return FALSE;
		}
	}

	if (settings->RedirectionFlags & LB_USERNAME)
	{
		free(settings->Username);
		settings->Username = _strdup(settings->RedirectionUsername);
		if (!settings->Username)
			return FALSE;
	}

	if (settings->RedirectionFlags & LB_DOMAIN)
	{
		free(settings->Domain);
		settings->Domain = _strdup(settings->RedirectionDomain);
		if (!settings->Domain)
			return FALSE;
	}

	status = rdp_client_connect(rdp);

	return status;
}

BOOL rdp_client_reconnect(rdpRdp* rdp)
{
	BOOL status;
	rdpContext* context = rdp->context;
	rdpChannels* channels = context->channels;

	freerdp_channels_disconnect(channels, context->instance);
	rdp_client_disconnect(rdp);

	status = rdp_client_connect(rdp);

	if (status)
		status = (freerdp_channels_post_connect(channels, context->instance) == CHANNEL_RC_OK);

	return status;
}

static BYTE fips_ivec[8] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };

static BOOL rdp_client_establish_keys(rdpRdp* rdp)
{
	BYTE* mod;
	BYTE* exp;
	wStream* s;
	UINT32 length;
	UINT32 key_len;
	int status = 0;
	BOOL ret = FALSE;
	rdpSettings* settings;
	BYTE* crypt_client_random = NULL;

	settings = rdp->settings;

	if (!settings->UseRdpSecurityLayer)
	{
		/* no RDP encryption */
		return TRUE;
	}

	/* encrypt client random */

	free(settings->ClientRandom);

	settings->ClientRandomLength = CLIENT_RANDOM_LENGTH;
	settings->ClientRandom = malloc(settings->ClientRandomLength);

	if (!settings->ClientRandom)
		return FALSE;

	winpr_RAND(settings->ClientRandom, settings->ClientRandomLength);
	key_len = settings->RdpServerCertificate->cert_info.ModulusLength;
	mod = settings->RdpServerCertificate->cert_info.Modulus;
	exp = settings->RdpServerCertificate->cert_info.exponent;

	/*
	 * client random must be (bitlen / 8) + 8 - see [MS-RDPBCGR] 5.3.4.1
	 * for details
	 */
	crypt_client_random = calloc(1, key_len + 8);

	if (!crypt_client_random)
		return FALSE;

	crypto_rsa_public_encrypt(settings->ClientRandom, settings->ClientRandomLength, key_len, mod, exp, crypt_client_random);

	/* send crypt client random to server */
	length = RDP_PACKET_HEADER_MAX_LENGTH + RDP_SECURITY_HEADER_LENGTH + 4 + key_len + 8;
	s = Stream_New(NULL, length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto end;
	}

	rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID);
	rdp_write_security_header(s, SEC_EXCHANGE_PKT | SEC_LICENSE_ENCRYPT_SC);
	length = key_len + 8;

	Stream_Write_UINT32(s, length);
	Stream_Write(s, crypt_client_random, length);
	Stream_SealLength(s);

	status = transport_write(rdp->mcs->transport, s);
	Stream_Free(s, TRUE);

	if (status < 0)
		goto end;

	rdp->do_crypt_license = TRUE;

	/* now calculate encrypt / decrypt and update keys */
	if (!security_establish_keys(settings->ClientRandom, rdp))
		goto end;

	rdp->do_crypt = TRUE;

	if (settings->SaltedChecksum)
		rdp->do_secure_checksum = TRUE;

	if (settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
	{
		rdp->fips_encrypt = winpr_Cipher_New(WINPR_CIPHER_DES_EDE3_CBC,
		                                     WINPR_ENCRYPT,
		                                     rdp->fips_encrypt_key,
		                                     fips_ivec);
		if (!rdp->fips_encrypt)
		{
			WLog_ERR(TAG, "unable to allocate des3 encrypt key");
			goto end;
		}
		rdp->fips_decrypt = winpr_Cipher_New(WINPR_CIPHER_DES_EDE3_CBC,
		                                     WINPR_DECRYPT,
		                                     rdp->fips_decrypt_key,
		                                     fips_ivec);
		if (!rdp->fips_decrypt)
		{
			WLog_ERR(TAG, "unable to allocate des3 decrypt key");
			goto end;
		}

		ret = TRUE;
		goto end;
	}

	rdp->rc4_decrypt_key = winpr_RC4_New(rdp->decrypt_key, rdp->rc4_key_len);
	rdp->rc4_encrypt_key = winpr_RC4_New(rdp->encrypt_key, rdp->rc4_key_len);
	if (!rdp->rc4_decrypt_key || !rdp->rc4_encrypt_key)
		goto end;

	ret = TRUE;
end:
	free(crypt_client_random);
	if (!ret)
	{
		winpr_Cipher_Free(rdp->fips_decrypt);
		winpr_Cipher_Free(rdp->fips_encrypt);
		winpr_RC4_Free(rdp->rc4_decrypt_key);
		winpr_RC4_Free(rdp->rc4_encrypt_key);

		rdp->fips_decrypt = NULL;
		rdp->fips_encrypt = NULL;
		rdp->rc4_decrypt_key = NULL;
		rdp->rc4_encrypt_key = NULL;
	}

	return ret;
}

BOOL rdp_server_establish_keys(rdpRdp* rdp, wStream* s)
{
	BYTE* client_random = NULL;
	BYTE* crypt_client_random = NULL;
	UINT32 rand_len, key_len;
	UINT16 channel_id, length, sec_flags;
	BYTE* mod;
	BYTE* priv_exp;
	BOOL ret = FALSE;

	if (!rdp->settings->UseRdpSecurityLayer)
	{
		/* No RDP Security. */
		return TRUE;
	}

	if (!rdp_read_header(rdp, s, &length, &channel_id))
	{
		WLog_ERR(TAG, "invalid RDP header");
		return FALSE;
	}

	if (!rdp_read_security_header(s, &sec_flags))
	{
		WLog_ERR(TAG, "invalid security header");
		return FALSE;
	}

	if ((sec_flags & SEC_EXCHANGE_PKT) == 0)
	{
		WLog_ERR(TAG, "missing SEC_EXCHANGE_PKT in security header");
		return FALSE;
	}

	rdp->do_crypt_license = (sec_flags & SEC_LICENSE_ENCRYPT_SC) != 0 ? TRUE : FALSE;

	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT32(s, rand_len);

	/* rand_len already includes 8 bytes of padding */
	if (Stream_GetRemainingLength(s) < rand_len)
		return FALSE;

	key_len = rdp->settings->RdpServerRsaKey->ModulusLength;
	client_random = malloc(key_len);
	if (!client_random)
		return FALSE;

	if (rand_len != key_len + 8)
	{
		WLog_ERR(TAG, "invalid encrypted client random length");
		free(client_random);
		goto end;
	}

	crypt_client_random = calloc(1, rand_len);
	if (!crypt_client_random)
	{
		free(client_random);
		goto end;
	}

	Stream_Read(s, crypt_client_random, rand_len);

	mod = rdp->settings->RdpServerRsaKey->Modulus;
	priv_exp = rdp->settings->RdpServerRsaKey->PrivateExponent;
	if (crypto_rsa_private_decrypt(crypt_client_random, rand_len - 8, key_len, mod, priv_exp, client_random) <= 0)
	{
		free(client_random);
		goto end;
	}

	rdp->settings->ClientRandom = client_random;
	rdp->settings->ClientRandomLength = 32;

	/* now calculate encrypt / decrypt and update keys */
	if (!security_establish_keys(client_random, rdp))
		goto end;

	rdp->do_crypt = TRUE;

	if (rdp->settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
	{
		rdp->fips_encrypt = winpr_Cipher_New(WINPR_CIPHER_DES_EDE3_CBC,
						     WINPR_ENCRYPT,
						     rdp->fips_encrypt_key,
						     fips_ivec);
		if (!rdp->fips_encrypt)
		{
			WLog_ERR(TAG, "unable to allocate des3 encrypt key");
			goto end;
		}

		rdp->fips_decrypt = winpr_Cipher_New(WINPR_CIPHER_DES_EDE3_CBC,
						     WINPR_DECRYPT,
						     rdp->fips_decrypt_key,
						     fips_ivec);
		if (!rdp->fips_decrypt)
		{
			WLog_ERR(TAG, "unable to allocate des3 decrypt key");
			goto end;
		}

		ret = TRUE;
		goto end;
	}

	rdp->rc4_decrypt_key = winpr_RC4_New(rdp->decrypt_key, rdp->rc4_key_len);
	rdp->rc4_encrypt_key = winpr_RC4_New(rdp->encrypt_key, rdp->rc4_key_len);
	if (!rdp->rc4_decrypt_key || !rdp->rc4_encrypt_key)
		goto end;

	ret = TRUE;
end:
	free(crypt_client_random);

	if (!ret)
	{
		winpr_Cipher_Free(rdp->fips_encrypt);
		winpr_Cipher_Free(rdp->fips_decrypt);
		winpr_RC4_Free(rdp->rc4_encrypt_key);
		winpr_RC4_Free(rdp->rc4_decrypt_key);

		rdp->fips_encrypt = NULL;
		rdp->fips_decrypt = NULL;
		rdp->rc4_encrypt_key = NULL;
		rdp->rc4_decrypt_key = NULL;
	}
	return ret;
}

BOOL rdp_client_connect_mcs_channel_join_confirm(rdpRdp* rdp, wStream* s)
{
	UINT32 i;
	UINT16 channelId;
	BOOL allJoined = TRUE;
	rdpMcs* mcs = rdp->mcs;

	if (!mcs_recv_channel_join_confirm(mcs, s, &channelId))
		return FALSE;

	if (!mcs->userChannelJoined)
	{
		if (channelId != mcs->userId)
			return FALSE;

		mcs->userChannelJoined = TRUE;

		if (!mcs_send_channel_join_request(mcs, MCS_GLOBAL_CHANNEL_ID))
			return FALSE;
	}
	else if (!mcs->globalChannelJoined)
	{
		if (channelId != MCS_GLOBAL_CHANNEL_ID)
			return FALSE;

		mcs->globalChannelJoined = TRUE;

		if (mcs->messageChannelId != 0)
		{
			if (!mcs_send_channel_join_request(mcs, mcs->messageChannelId))
				return FALSE;

			allJoined = FALSE;
		}
		else
		{
			if (mcs->channelCount > 0)
			{
				if (!mcs_send_channel_join_request(mcs, mcs->channels[0].ChannelId))
					return FALSE;

				allJoined = FALSE;
			}
		}
	}
	else if ((mcs->messageChannelId != 0) && !mcs->messageChannelJoined)
	{
		if (channelId != mcs->messageChannelId)
			return FALSE;

		mcs->messageChannelJoined = TRUE;

		if (mcs->channelCount > 0)
		{
			if (!mcs_send_channel_join_request(mcs, mcs->channels[0].ChannelId))
				return FALSE;

			allJoined = FALSE;
		}
	}
	else
	{
		for (i = 0; i < mcs->channelCount; i++)
		{
			if (mcs->channels[i].joined)
				continue;

			if (mcs->channels[i].ChannelId != channelId)
				return FALSE;

			mcs->channels[i].joined = TRUE;

			break;
		}

		if (i + 1 < mcs->channelCount)
		{
			if (!mcs_send_channel_join_request(mcs, mcs->channels[i + 1].ChannelId))
				return FALSE;

			allJoined = FALSE;
		}
	}

	if (mcs->userChannelJoined && mcs->globalChannelJoined && allJoined)
	{
		if (!rdp_client_establish_keys(rdp))
			return FALSE;

		if (!rdp_send_client_info(rdp))
			return FALSE;

		rdp_client_transition_to_state(rdp, CONNECTION_STATE_LICENSING);
	}

	return TRUE;
}

BOOL rdp_client_connect_auto_detect(rdpRdp* rdp, wStream* s)
{
	BYTE* mark;
	UINT16 length;
	UINT16 channelId;

	/* If the MCS message channel has been joined... */
	if (rdp->mcs->messageChannelId != 0)
	{
		/* Process any MCS message channel PDUs. */
		Stream_GetPointer(s, mark);

		if (rdp_read_header(rdp, s, &length, &channelId))
		{
			if (channelId == rdp->mcs->messageChannelId)
			{
				UINT16 securityFlags = 0;

				if (!rdp_read_security_header(s, &securityFlags))
					return FALSE;

				if (securityFlags & SEC_ENCRYPT)
				{
					if (!rdp_decrypt(rdp, s, length - 4, securityFlags))
					{
						WLog_ERR(TAG, "rdp_decrypt failed");
						return FALSE;
					}
				}

				if (rdp_recv_message_channel_pdu(rdp, s, securityFlags) == 0)
					return TRUE;
			}
		}

		Stream_SetPointer(s, mark);
	}

	return FALSE;
}

int rdp_client_connect_license(rdpRdp* rdp, wStream* s)
{
	int status;

	status = license_recv(rdp->license, s);

	if (status < 0)
		return status;

	if (rdp->license->state == LICENSE_STATE_ABORTED)
	{
		WLog_ERR(TAG, "license connection sequence aborted.");
		return -1;
	}

	if (rdp->license->state == LICENSE_STATE_COMPLETED)
	{
		rdp_client_transition_to_state(rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE);
	}

	return 0;
}

int rdp_client_connect_demand_active(rdpRdp* rdp, wStream* s)
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

		return rdp_recv_out_of_sequence_pdu(rdp, s);
	}

	if (freerdp_shall_disconnect(rdp->instance))
		return 0;

	if (!rdp_send_confirm_active(rdp))
		return -1;

	if (!input_register_client_callbacks(rdp->input))
	{
		WLog_ERR(TAG, "error registering client callbacks");
		return -1;
	}

	/**
	 * The server may request a different desktop size during Deactivation-Reactivation sequence.
	 * In this case, the UI should be informed and do actual window resizing at this point.
	 */
	if (width != rdp->settings->DesktopWidth || height != rdp->settings->DesktopHeight)
	{
		BOOL status = TRUE;

		IFCALLRET(rdp->update->DesktopResize, status, rdp->update->context);

		if (!status)
		{
			WLog_ERR(TAG, "client desktop resize callback failed");
			return -1;
		}
	}

	rdp_client_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION);

	return rdp_client_connect_finalize(rdp);
}

int rdp_client_connect_finalize(rdpRdp* rdp)
{
	/**
	 * [MS-RDPBCGR] 1.3.1.1 - 8.
	 * The client-to-server PDUs sent during this phase have no dependencies on any of the server-to-
	 * client PDUs; they may be sent as a single batch, provided that sequencing is maintained.
	 */

	if (!rdp_send_client_synchronize_pdu(rdp))
		return -1;

	if (!rdp_send_client_control_pdu(rdp, CTRLACTION_COOPERATE))
		return -1;

	if (!rdp_send_client_control_pdu(rdp, CTRLACTION_REQUEST_CONTROL))
		return -1;
	/**
	 * [MS-RDPBCGR] 2.2.1.17
	 * Client persistent key list must be sent if a bitmap is
	 * stored in persistent bitmap cache or the server has advertised support for bitmap
	 * host cache and a deactivation reactivation sequence is *not* in progress.
	 */

	if (!rdp->deactivation_reactivation && rdp->settings->BitmapCachePersistEnabled)
	{
		if (!rdp_send_client_persistent_key_list_pdu(rdp))
			return -1;
	}

	if (!rdp_send_client_font_list_pdu(rdp, FONTLIST_FIRST | FONTLIST_LAST))
		return -1;

	return 0;
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

		case CONNECTION_STATE_NLA:
			rdp->state = CONNECTION_STATE_NLA;
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
	rdpNego *nego = rdp->nego;

	transport_set_blocking_mode(rdp->transport, TRUE);

	if (!nego_read_request(nego, s))
		return FALSE;

	nego->SelectedProtocol = 0;
	WLog_INFO(TAG, "Client Security: NLA:%d TLS:%d RDP:%d",
			 (nego->RequestedProtocols & PROTOCOL_NLA) ? 1 : 0,
			 (nego->RequestedProtocols & PROTOCOL_TLS) ? 1 : 0,
			 (nego->RequestedProtocols == PROTOCOL_RDP) ? 1 : 0
			);
	WLog_INFO(TAG, "Server Security: NLA:%"PRId32" TLS:%"PRId32" RDP:%"PRId32"",
			 settings->NlaSecurity, settings->TlsSecurity, settings->RdpSecurity);

	if ((settings->NlaSecurity) && (nego->RequestedProtocols & PROTOCOL_NLA))
	{
		nego->SelectedProtocol = PROTOCOL_NLA;
	}
	else if ((settings->TlsSecurity) && (nego->RequestedProtocols & PROTOCOL_TLS))
	{
		nego->SelectedProtocol = PROTOCOL_TLS;
	}
	else if ((settings->RdpSecurity) && (nego->RequestedProtocols == PROTOCOL_RDP))
	{
		nego->SelectedProtocol = PROTOCOL_RDP;
	}
	else
	{
		/*
		 * when here client and server aren't compatible, we select the right
		 * error message to return to the client in the nego failure packet
		 */
		nego->SelectedProtocol = PROTOCOL_FAILED_NEGO;

		if (settings->RdpSecurity)
		{
			WLog_ERR(TAG, "server supports only Standard RDP Security");
			nego->SelectedProtocol |= SSL_NOT_ALLOWED_BY_SERVER;
		}
		else
		{
			if (settings->NlaSecurity && !settings->TlsSecurity)
			{
				WLog_WARN(TAG, "server supports only NLA Security");
				nego->SelectedProtocol |= HYBRID_REQUIRED_BY_SERVER;
			}
			else
			{
				WLog_WARN(TAG, "server supports only a SSL based Security (TLS or NLA)");
				nego->SelectedProtocol |= SSL_REQUIRED_BY_SERVER;
			}
		}

		WLog_ERR(TAG, "Protocol security negotiation failure");
	}

	if (!(nego->SelectedProtocol & PROTOCOL_FAILED_NEGO))
	{
		WLog_INFO(TAG, "Negotiated Security: NLA:%d TLS:%d RDP:%d",
				 (nego->SelectedProtocol & PROTOCOL_NLA) ? 1 : 0,
				 (nego->SelectedProtocol & PROTOCOL_TLS) ? 1 : 0,
				 (nego->SelectedProtocol == PROTOCOL_RDP) ? 1: 0
				);
	}

	if (!nego_send_negotiation_response(nego))
		return FALSE;

	status = FALSE;

	if (nego->SelectedProtocol & PROTOCOL_NLA)
		status = transport_accept_nla(rdp->transport);
	else if (nego->SelectedProtocol & PROTOCOL_TLS)
		status = transport_accept_tls(rdp->transport);
	else if (nego->SelectedProtocol == PROTOCOL_RDP) /* 0 */
		status = transport_accept_rdp(rdp->transport);

	if (!status)
		return FALSE;

	transport_set_blocking_mode(rdp->transport, FALSE);

	rdp_server_transition_to_state(rdp, CONNECTION_STATE_NEGO);

	return TRUE;
}

BOOL rdp_server_accept_mcs_connect_initial(rdpRdp* rdp, wStream* s)
{
	UINT32 i;
	rdpMcs* mcs = rdp->mcs;

	if (!mcs_recv_connect_initial(mcs, s))
		return FALSE;

	WLog_INFO(TAG, "Accepted client: %s", rdp->settings->ClientHostname);
	WLog_INFO(TAG, "Accepted channels:");

	for (i = 0; i < mcs->channelCount; i++)
	{
		WLog_INFO(TAG, " %s", mcs->channels[i].Name);
	}

	if (!mcs_send_connect_response(mcs))
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
	UINT32 i;
	UINT16 channelId;
	BOOL allJoined = TRUE;
	rdpMcs* mcs = rdp->mcs;

	if (!mcs_recv_channel_join_request(mcs, s, &channelId))
		return FALSE;

	if (!mcs_send_channel_join_confirm(mcs, channelId))
		return FALSE;

	if (channelId == mcs->userId)
		mcs->userChannelJoined = TRUE;
	else if (channelId == MCS_GLOBAL_CHANNEL_ID)
		mcs->globalChannelJoined = TRUE;
	else if (channelId == mcs->messageChannelId)
		mcs->messageChannelJoined = TRUE;

	for (i = 0; i < mcs->channelCount; i++)
	{
		if (mcs->channels[i].ChannelId == channelId)
			mcs->channels[i].joined = TRUE;

		if (!mcs->channels[i].joined)
			allJoined = FALSE;
	}

	if ((mcs->userChannelJoined) && (mcs->globalChannelJoined) && (mcs->messageChannelId == 0 || mcs->messageChannelJoined) && allJoined)
	{
		rdp_server_transition_to_state(rdp, CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT);
	}

	return TRUE;
}

BOOL rdp_server_accept_confirm_active(rdpRdp* rdp, wStream* s)
{
	freerdp_peer *peer = rdp->context->peer;

	if (rdp->state != CONNECTION_STATE_CAPABILITIES_EXCHANGE)
		return FALSE;

	if (!rdp_recv_confirm_active(rdp, s))
		return FALSE;

	if (peer->ClientCapabilities && !peer->ClientCapabilities(peer))
		return FALSE;

	if (rdp->settings->SaltedChecksum)
		rdp->do_secure_checksum = TRUE;

	rdp_server_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION);

	if (!rdp_send_server_synchronize_pdu(rdp))
		return FALSE;

	if (!rdp_send_server_control_cooperate_pdu(rdp))
		return FALSE;

	return TRUE;
}

BOOL rdp_server_reactivate(rdpRdp* rdp)
{
	freerdp_peer* client = NULL;

	if (rdp->context && rdp->context->peer)
		client = rdp->context->peer;

	if (client)
		client->activated = FALSE;

	if (!rdp_send_deactivate_all(rdp))
		return FALSE;

	rdp_server_transition_to_state(rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE);

	if (!rdp_send_demand_active(rdp))
		return FALSE;

	rdp->AwaitCapabilities = TRUE;
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

				if (rdp->state >= CONNECTION_STATE_ACTIVE)
				{
					IFCALLRET(client->Activate, client->activated, client);

					if (!client->activated)
						return -1;
				}
			}

			break;

		default:
			status = -1;
			break;
	}

	return status;
}
