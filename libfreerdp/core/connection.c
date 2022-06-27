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

#include <freerdp/config.h>

#include "info.h"
#include "input.h"
#include "rdp.h"

#include "connection.h"
#include "transport.h"

#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/ssl.h>

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/listener.h>
#include <freerdp/cache/pointer.h>

#include "utils.h"

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
 * 	using the MCS Connect Initial PDU (section 2.2.1.3) and MCS Connect Response PDU
 *(section 2.2.1.4). The Connect Initial PDU contains a Generic Conference Control (GCC) Conference
 *Create Request, while the Connect Response PDU contains a GCC Conference Create Response. These
 *two GCC packets contain concatenated blocks of settings data (such as core data, security data,
 *and network data) which are read by client and server.
 *
 * 3.	Channel Connection: The client sends an MCS Erect Domain Request PDU (section 2.2.1.5),
 * 	followed by an MCS Attach User Request PDU (section 2.2.1.6) to attach the primary user identity
 * 	to the MCS domain. The server responds with an MCS Attach User Confirm PDU (section 2.2.1.7)
 * 	containing the User Channel ID. The client then proceeds to join the user channel, the
 * 	input/output (I/O) channel, and all of the static virtual channels (the I/O and static virtual
 * 	channel IDs are obtained from the data embedded in the GCC packets) by using multiple MCS
 *Channel Join Request PDUs (section 2.2.1.8). The server confirms each channel with an MCS Channel
 *Join Confirm PDU (section 2.2.1.9). (The client only sends a Channel Join Request after it has
 *received the Channel Join Confirm for the previously sent request.)
 *
 * 	From this point, all subsequent data sent from the client to the server is wrapped in an MCS
 *Send Data Request PDU, while data sent from the server to the client is wrapped in an MCS Send
 *Data Indication PDU. This is in addition to the data being wrapped by an X.224 Data PDU.
 *
 * 4.	RDP Security Commencement: If Standard RDP Security mechanisms (section 5.3) are being
 *employed and encryption is in force (this is determined by examining the data embedded in the GCC
 *Conference Create Response packet) then the client sends a Security Exchange PDU
 *(section 2.2.1.10) containing an encrypted 32-byte random number to the server. This random number
 *is encrypted with the public key of the server as described in section 5.3.4.1 (the server's
 *public key, as well as a 32-byte server-generated random number, are both obtained from the data
 *embedded in the GCC Conference Create Response packet). The client and server then utilize the two
 *32-byte random numbers to generate session keys which are used to encrypt and validate the
 *integrity of subsequent RDP traffic.
 *
 * 	From this point, all subsequent RDP traffic can be encrypted and a security header is included
 *with the data if encryption is in force. (The Client Info PDU (section 2.2.1.11) and licensing
 *PDUs ([MS-RDPELE] section 2.2.2) are an exception in that they always have a security header). The
 *Security Header follows the X.224 and MCS Headers and indicates whether the attached data is
 *encrypted. Even if encryption is in force, server-to-client traffic may not always be encrypted,
 *while client-to-server traffic must always be encrypted (encryption of licensing PDUs is optional,
 *however).
 *
 * 5.	Secure Settings Exchange: Secure client data (such as the username, password, and
 *auto-reconnect cookie) is sent to the server by using the Client Info PDU (section 2.2.1.11).
 *
 * 6.	Optional Connect-Time Auto-Detection: During the optional connect-time auto-detect phase the
 *goal is to determine characteristics of the network, such as the round-trip latency time and the
 *bandwidth of the link between the server and client. This is accomplished by exchanging a
 *collection of PDUs (specified in section 2.2.1.4) over a predetermined period of time with enough
 *data to ensure that the results are statistically relevant.
 *
 * 7.	Licensing: The goal of the licensing exchange is to transfer a license from the server to
 *the client. The client stores this license and on subsequent connections sends the license to the
 *server for validation. However, in some situations the client may not be issued a license to
 *store. In effect, the packets exchanged during this phase of the protocol depend on the licensing
 *mechanisms employed by the server. Within the context of this document, it is assumed that the
 *client will not be issued a license to store. For details regarding more advanced licensing
 *scenarios that take place during the Licensing Phase, see [MS-RDPELE] section 1.3.
 *
 * 8.	Optional Multitransport Bootstrapping: After the connection has been secured and the
 *Licensing Phase has run to completion, the server can choose to initiate multitransport
 *connections ([MS-RDPEMT] section 1.3). The Initiate Multitransport Request PDU (section 2.2.15.1)
 *is sent by the server to the client and results in the out-of-band creation of a multitransport
 *connection using messages from the RDP-UDP, TLS, DTLS, and multitransport protocols ([MS-RDPEMT]
 *section 1.3.1).
 *
 * 9.	Capabilities Exchange: The server sends the set of capabilities it supports to the client in
 *a Demand Active PDU (section 2.2.1.13.1). The client responds with its capabilities by sending a
 *Confirm Active PDU (section 2.2.1.13.2).
 *
 * 10.	Connection Finalization: The client and server exchange PDUs to finalize the connection
 *details. The client-to-server PDUs sent during this phase have no dependencies on any of the
 *server-to-client PDUs; they may be sent as a single batch, provided that sequencing is maintained.
 *
 * 	- The Client Synchronize PDU (section 2.2.1.14) is sent after transmitting the Confirm Active
 *PDU.
 * 	- The Client Control (Cooperate) PDU (section 2.2.1.15) is sent after transmitting the Client
 *Synchronize PDU.
 * 	- The Client Control (Request Control) PDU (section 2.2.1.16) is sent after transmitting the
 *Client Control (Cooperate) PDU.
 * 	- The optional Persistent Key List PDUs (section 2.2.1.17) are sent after transmitting the
 *Client Control (Request Control) PDU.
 * 	- The Font List PDU (section 2.2.1.18) is sent after transmitting the Persistent Key List PDUs
 *or, if the Persistent Key List PDUs were not sent, it is sent after transmitting the Client
 *Control (Request Control) PDU (section 2.2.1.16).
 *
 *	The server-to-client PDUs sent during the Connection Finalization Phase have dependencies on the
 *client-to-server PDUs.
 *
 *	- The optional Monitor Layout PDU (section 2.2.12.1) has no dependency on any client-to-server
 *PDUs and is sent after the Demand Active PDU.
 *	- The Server Synchronize PDU (section 2.2.1.19) is sent in response to the Confirm Active PDU.
 *	- The Server Control (Cooperate) PDU (section 2.2.1.20) is sent after transmitting the Server
 *Synchronize PDU.
 *	- The Server Control (Granted Control) PDU (section 2.2.1.21) is sent in response to the Client
 *Control (Request Control) PDU.
 *	- The Font Map PDU (section 2.2.1.22) is sent in response to the Font List PDU.
 *
 *	Once the client has sent the Confirm Active PDU, it can start sending mouse and keyboard input
 *to the server, and upon receipt of the Font List PDU the server can start sending graphics output
 *to the client.
 *
 *	Besides input and graphics data, other data that can be exchanged between client and server
 *after the connection has been finalized includes connection management information and virtual
 *channel messages (exchanged between client-side plug-ins and server-side applications).
 */

static int rdp_client_connect_finalize(rdpRdp* rdp);
static BOOL rdp_set_state(rdpRdp* rdp, CONNECTION_STATE state);

static BOOL rdp_client_reset_codecs(rdpContext* context)
{
	rdpSettings* settings;

	if (!context || !context->settings)
		return FALSE;

	settings = context->settings;

	if (!freerdp_settings_get_bool(settings, FreeRDP_DeactivateClientDecoding))
	{
		codecs_free(context->codecs);
		context->codecs = codecs_new(context);

		if (!context->codecs)
			return FALSE;

		if (!freerdp_client_codecs_prepare(context->codecs,
		                                   freerdp_settings_get_codecs_flags(settings),
		                                   settings->DesktopWidth, settings->DesktopHeight))
			return FALSE;

/* Runtime H264 detection. (only available if dynamic backend loading is defined)
 * If no backend is available disable it before the channel is loaded.
 */
#if defined(WITH_GFX_H264) && defined(WITH_OPENH264_LOADING)
		if (!context->codecs->h264)
		{
			settings->GfxH264 = FALSE;
			settings->GfxAVC444 = FALSE;
			settings->GfxAVC444v2 = FALSE;
		}
#endif
	}

	return TRUE;
}

/**
 * Establish RDP Connection based on the settings given in the 'rdp' parameter.
 * @msdn{cc240452}
 * @param rdp RDP module
 * @return true if the connection succeeded. FALSE otherwise.
 */

BOOL rdp_client_connect(rdpRdp* rdp)
{
	UINT32 SelectedProtocol;
	BOOL status;
	rdpSettings* settings;
	/* make sure SSL is initialize for earlier enough for crypto, by taking advantage of winpr SSL
	 * FIPS flag for openssl initialization */
	DWORD flags = WINPR_SSL_INIT_DEFAULT;
	UINT32 timeout;

	WINPR_ASSERT(rdp);

	settings = rdp->settings;
	WINPR_ASSERT(settings);

	if (!rdp_client_reset_codecs(rdp->context))
		return FALSE;

	if (settings->FIPSMode)
		flags |= WINPR_SSL_INIT_ENABLE_FIPS;

	winpr_InitializeSSL(flags);

	/* FIPS Mode forces the following and overrides the following(by happening later */
	/* in the command line processing): */
	/* 1. Disables NLA Security since NLA in freerdp uses NTLM(no Kerberos support yet) which uses
	 * algorithms */
	/*      not allowed in FIPS for sensitive data. So, we disallow NLA when FIPS is required. */
	/* 2. Forces the only supported RDP encryption method to be FIPS. */
	if (settings->FIPSMode || winpr_FIPSMode())
	{
		settings->NlaSecurity = FALSE;
		settings->EncryptionMethods = ENCRYPTION_METHOD_FIPS;
	}

	nego_init(rdp->nego);
	nego_set_target(rdp->nego, settings->ServerHostname, settings->ServerPort);

	if (settings->GatewayEnabled)
	{
		char* user = NULL;
		char* domain = NULL;
		char* cookie = NULL;
		size_t user_length = 0;
		size_t domain_length = 0;
		size_t cookie_length = 0;

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
		cookie = (char*)malloc(cookie_length + 1);

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

	if (settings->LoadBalanceInfo && (settings->LoadBalanceInfoLength > 0))
	{
		if (!nego_set_routing_token(rdp->nego, settings->LoadBalanceInfo,
		                            settings->LoadBalanceInfoLength))
			return FALSE;
	}

	if (!freerdp_settings_get_bool(settings, FreeRDP_TransportDumpReplay))
	{
		rdp_client_transition_to_state(rdp, CONNECTION_STATE_NEGO);

		if (!nego_connect(rdp->nego))
		{
			if (!freerdp_get_last_error(rdp->context))
			{
				freerdp_set_last_error_log(rdp->context,
				                           FREERDP_ERROR_SECURITY_NEGO_CONNECT_FAILED);
				WLog_ERR(TAG, "Error: protocol security negotiation or connection failure");
			}

			return FALSE;
		}

		SelectedProtocol = nego_get_selected_protocol(rdp->nego);

		if ((SelectedProtocol & PROTOCOL_SSL) || (SelectedProtocol == PROTOCOL_RDP))
		{
			if ((settings->Username != NULL) &&
			    ((freerdp_settings_get_string(settings, FreeRDP_Password) != NULL) ||
			     (settings->RedirectionPassword != NULL &&
			      settings->RedirectionPasswordLength > 0)))
				settings->AutoLogonEnabled = TRUE;
		}
		transport_set_blocking_mode(rdp->transport, FALSE);
	}

	/* everything beyond this point is event-driven and non blocking */
	transport_set_recv_callbacks(rdp->transport, rdp_recv_callback, rdp);

	if (rdp_get_state(rdp) != CONNECTION_STATE_NLA)
	{
		rdp_client_transition_to_state(rdp, CONNECTION_STATE_MCS_CONNECT);
		if (!mcs_client_begin(rdp->mcs))
			return FALSE;
	}

	for (timeout = 0; timeout < freerdp_settings_get_uint32(settings, FreeRDP_TcpAckTimeout);
	     timeout += 100)
	{
		if (rdp_check_fds(rdp) < 0)
		{
			freerdp_set_last_error_if_not(rdp->context, FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
			return FALSE;
		}

		if (rdp_get_state(rdp) == CONNECTION_STATE_ACTIVE)
			return TRUE;

		Sleep(100);
	}

	WLog_ERR(TAG, "Timeout waiting for activation");
	freerdp_set_last_error_if_not(rdp->context, FREERDP_ERROR_CONNECT_ACTIVATION_TIMEOUT);
	return FALSE;
}

BOOL rdp_client_disconnect(rdpRdp* rdp)
{
	rdpContext* context;

	if (!rdp || !rdp->settings || !rdp->context)
		return FALSE;

	context = rdp->context;

	if (rdp->nego)
	{
		if (!nego_disconnect(rdp->nego))
			return FALSE;
	}

	if (!rdp_reset(rdp))
		return FALSE;

	rdp_client_transition_to_state(rdp, CONNECTION_STATE_INITIAL);

	if (freerdp_channels_disconnect(context->channels, context->instance) != CHANNEL_RC_OK)
		return FALSE;

	codecs_free(context->codecs);
	context->codecs = NULL;
	return TRUE;
}

BOOL rdp_client_disconnect_and_clear(rdpRdp* rdp)
{
	rdpContext* context;

	if (!rdp_client_disconnect(rdp))
		return FALSE;

	WINPR_ASSERT(rdp);

	context = rdp->context;
	WINPR_ASSERT(context);

	if (freerdp_get_last_error(context) == FREERDP_ERROR_CONNECT_CANCELLED)
		return FALSE;

	context->LastError = FREERDP_ERROR_SUCCESS;
	clearChannelError(context);
	return utils_reset_abort(rdp);
}

static BOOL rdp_client_reconnect_channels(rdpRdp* rdp, BOOL redirect)
{
	BOOL status = FALSE;
	rdpContext* context;

	if (!rdp || !rdp->context || !rdp->context->channels)
		return FALSE;

	context = rdp->context;

	if (context->instance->ConnectionCallbackState == CLIENT_STATE_INITIAL)
		return FALSE;

	if (context->instance->ConnectionCallbackState == CLIENT_STATE_PRECONNECT_PASSED)
	{
		if (redirect)
			return TRUE;

		pointer_cache_register_callbacks(context->update);

		if (!IFCALLRESULT(FALSE, context->instance->PostConnect, context->instance))
			return FALSE;

		context->instance->ConnectionCallbackState = CLIENT_STATE_POSTCONNECT_PASSED;
	}

	if (context->instance->ConnectionCallbackState == CLIENT_STATE_POSTCONNECT_PASSED)
		status =
		    (freerdp_channels_post_connect(context->channels, context->instance) == CHANNEL_RC_OK);

	return status;
}

static BOOL rdp_client_redirect_resolvable(const char* host)
{
	struct addrinfo* result = freerdp_tcp_resolve_host(host, -1, 0);

	if (!result)
		return FALSE;

	freeaddrinfo(result);
	return TRUE;
}

static BOOL rdp_client_redirect_try_fqdn(rdpSettings* settings)
{
	if (settings->RedirectionFlags & LB_TARGET_FQDN)
	{
		if (settings->GatewayEnabled ||
		    rdp_client_redirect_resolvable(settings->RedirectionTargetFQDN))
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname,
			                                 settings->RedirectionTargetFQDN))
				return FALSE;

			return TRUE;
		}
	}

	return FALSE;
}

static BOOL rdp_client_redirect_try_ip(rdpSettings* settings)
{
	if (settings->RedirectionFlags & LB_TARGET_NET_ADDRESS)
	{
		if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname,
		                                 settings->TargetNetAddress))
			return FALSE;

		return TRUE;
	}

	return FALSE;
}

static BOOL rdp_client_redirect_try_netbios(rdpSettings* settings)
{
	if (settings->RedirectionFlags & LB_TARGET_NETBIOS_NAME)
	{
		if (settings->GatewayEnabled ||
		    rdp_client_redirect_resolvable(settings->RedirectionTargetNetBiosName))
		{
			if (!freerdp_settings_set_string(settings, FreeRDP_ServerHostname,
			                                 settings->RedirectionTargetNetBiosName))
				return FALSE;

			return TRUE;
		}
	}

	return FALSE;
}

BOOL rdp_client_redirect(rdpRdp* rdp)
{
	BOOL status;
	rdpSettings* settings;

	if (!rdp_client_disconnect_and_clear(rdp))
		return FALSE;

	freerdp_channels_disconnect(rdp->context->channels, rdp->context->instance);
	freerdp_channels_close(rdp->context->channels, rdp->context->instance);
	freerdp_channels_free(rdp->context->channels);
	rdp->context->channels = freerdp_channels_new(rdp->context->instance);
	WINPR_ASSERT(rdp->context->channels);

	if (rdp_redirection_apply_settings(rdp) != 0)
		return FALSE;

	WINPR_ASSERT(rdp);

	settings = rdp->settings;
	WINPR_ASSERT(settings);

	if (settings->RedirectionFlags & LB_LOAD_BALANCE_INFO)
	{
		if (settings->LoadBalanceInfo && (settings->LoadBalanceInfoLength > 0))
		{
			if (!nego_set_routing_token(rdp->nego, settings->LoadBalanceInfo,
			                            settings->LoadBalanceInfoLength))
				return FALSE;
		}
	}
	else
	{
		BOOL haveRedirectAddress = FALSE;
		UINT32 redirectionMask = settings->RedirectionPreferType;

		do
		{
			const BOOL tryFQDN = (redirectionMask & 0x01) == 0;
			const BOOL tryNetAddress = (redirectionMask & 0x02) == 0;
			const BOOL tryNetbios = (redirectionMask & 0x04) == 0;

			if (tryFQDN && !haveRedirectAddress)
				haveRedirectAddress = rdp_client_redirect_try_fqdn(settings);

			if (tryNetAddress && !haveRedirectAddress)
				haveRedirectAddress = rdp_client_redirect_try_ip(settings);

			if (tryNetbios && !haveRedirectAddress)
				haveRedirectAddress = rdp_client_redirect_try_netbios(settings);

			redirectionMask >>= 3;
		} while (!haveRedirectAddress && (redirectionMask != 0));
	}

	if (settings->RedirectionFlags & LB_USERNAME)
	{
		if (!freerdp_settings_set_string(
		        settings, FreeRDP_Username,
		        freerdp_settings_get_string(settings, FreeRDP_RedirectionUsername)))
			return FALSE;
	}

	if (settings->RedirectionFlags & LB_DOMAIN)
	{
		if (!freerdp_settings_set_string(
		        settings, FreeRDP_Domain,
		        freerdp_settings_get_string(settings, FreeRDP_RedirectionDomain)))
			return FALSE;
	}

	WINPR_ASSERT(rdp->context);
	WINPR_ASSERT(rdp->context->instance);
	if (!IFCALLRESULT(TRUE, rdp->context->instance->Redirect, rdp->context->instance))
		return FALSE;

	BOOL ok = IFCALLRESULT(TRUE, rdp->context->instance->LoadChannels, rdp->context->instance);
	if (!ok)
		return FALSE;

	if (CHANNEL_RC_OK !=
	    freerdp_channels_pre_connect(rdp->context->channels, rdp->context->instance))
		return FALSE;

	status = rdp_client_connect(rdp);

	if (status)
		status = rdp_client_reconnect_channels(rdp, TRUE);

	return status;
}

BOOL rdp_client_reconnect(rdpRdp* rdp)
{
	BOOL status;

	if (!rdp_client_disconnect_and_clear(rdp))
		return FALSE;

	status = rdp_client_connect(rdp);

	if (status)
		status = rdp_client_reconnect_channels(rdp, FALSE);

	return status;
}

static const BYTE fips_ivec[8] = { 0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF };

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
	crypt_client_random = calloc(key_len + 8, 1);

	if (!crypt_client_random)
		return FALSE;

	crypto_rsa_public_encrypt(settings->ClientRandom, settings->ClientRandomLength, key_len, mod,
	                          exp, crypt_client_random);
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
		rdp->fips_encrypt = winpr_Cipher_New(WINPR_CIPHER_DES_EDE3_CBC, WINPR_ENCRYPT,
		                                     rdp->fips_encrypt_key, fips_ivec);

		if (!rdp->fips_encrypt)
		{
			WLog_ERR(TAG, "unable to allocate des3 encrypt key");
			goto end;
		}

		rdp->fips_decrypt = winpr_Cipher_New(WINPR_CIPHER_DES_EDE3_CBC, WINPR_DECRYPT,
		                                     rdp->fips_decrypt_key, fips_ivec);

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
		return FALSE;

	if (!rdp_read_security_header(s, &sec_flags, NULL))
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

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, rand_len);

	/* rand_len already includes 8 bytes of padding */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, rand_len))
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

	if (crypto_rsa_private_decrypt(crypt_client_random, rand_len - 8, key_len, mod, priv_exp,
	                               client_random) <= 0)
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
		rdp->fips_encrypt = winpr_Cipher_New(WINPR_CIPHER_DES_EDE3_CBC, WINPR_ENCRYPT,
		                                     rdp->fips_encrypt_key, fips_ivec);

		if (!rdp->fips_encrypt)
		{
			WLog_ERR(TAG, "unable to allocate des3 encrypt key");
			goto end;
		}

		rdp->fips_decrypt = winpr_Cipher_New(WINPR_CIPHER_DES_EDE3_CBC, WINPR_DECRYPT,
		                                     rdp->fips_decrypt_key, fips_ivec);

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

	ret = tpkt_ensure_stream_consumed(s, length);
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
				const rdpMcsChannel* cur = &mcs->channels[0];
				if (!mcs_send_channel_join_request(mcs, cur->ChannelId))
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
			const rdpMcsChannel* cur = &mcs->channels[0];
			if (!mcs_send_channel_join_request(mcs, cur->ChannelId))
				return FALSE;

			allJoined = FALSE;
		}
	}
	else
	{
		for (i = 0; i < mcs->channelCount; i++)
		{
			rdpMcsChannel* cur = &mcs->channels[i];
			if (cur->joined)
				continue;

			if (cur->ChannelId != channelId)
				return FALSE;

			cur->joined = TRUE;
			break;
		}

		if (i + 1 < mcs->channelCount)
		{
			const rdpMcsChannel* cur = &mcs->channels[i + 1];
			if (!mcs_send_channel_join_request(mcs, cur->ChannelId))
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
	size_t pos;
	UINT16 length;
	UINT16 channelId;

	/* If the MCS message channel has been joined... */
	if (rdp->mcs->messageChannelId != 0)
	{
		/* Process any MCS message channel PDUs. */
		pos = Stream_GetPosition(s);

		if (rdp_read_header(rdp, s, &length, &channelId))
		{
			if (channelId == rdp->mcs->messageChannelId)
			{
				UINT16 securityFlags = 0;

				if (!rdp_read_security_header(s, &securityFlags, &length))
					return FALSE;

				if (securityFlags & SEC_ENCRYPT)
				{
					if (!rdp_decrypt(rdp, s, &length, securityFlags))
					{
						WLog_ERR(TAG, "rdp_decrypt failed");
						return FALSE;
					}
				}

				if (rdp_recv_message_channel_pdu(rdp, s, securityFlags) == 0)
					return tpkt_ensure_stream_consumed(s, length);
			}
		}

		Stream_SetPosition(s, pos);
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
	size_t pos;
	UINT16 width;
	UINT16 height;
	UINT16 length;
	width = rdp->settings->DesktopWidth;
	height = rdp->settings->DesktopHeight;

	pos = Stream_GetPosition(s);

	if (!rdp_recv_demand_active(rdp, s))
	{
		int rc;
		UINT16 channelId;

		Stream_SetPosition(s, pos);
		if (!rdp_recv_get_active_header(rdp, s, &channelId, &length))
			return -1;
		/* Was Stream_Seek(s, RDP_PACKET_HEADER_MAX_LENGTH);
		 * but the headers aren't always that length,
		 * so that could result in a bad offset.
		 */
		rc = rdp_recv_out_of_sequence_pdu(rdp, s);
		if (rc < 0)
			return rc;
		if (!tpkt_ensure_stream_consumed(s, length))
			return -1;
		return rc;
	}

	if (freerdp_shall_disconnect_context(rdp->context))
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
	 * The client-to-server PDUs sent during this phase have no dependencies on any of the
	 * server-to- client PDUs; they may be sent as a single batch, provided that sequencing is
	 * maintained.
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

	if (!rdp_finalize_is_flag_set(rdp, FINALIZE_DEACTIVATE_REACTIVATE) &&
	    rdp->settings->BitmapCachePersistEnabled)
	{
		if (!rdp_send_client_persistent_key_list_pdu(rdp))
			return -1;
	}

	if (!rdp_send_client_font_list_pdu(rdp, FONTLIST_FIRST | FONTLIST_LAST))
		return -1;

	return 0;
}

int rdp_client_transition_to_state(rdpRdp* rdp, CONNECTION_STATE state)
{
	int status = 0;

	WLog_DBG(TAG, "%s %s --> %s", __FUNCTION__, rdp_get_state_string(rdp), rdp_state_string(state));
	rdp_set_state(rdp, state);

	switch (state)
	{
		case CONNECTION_STATE_FINALIZATION:
			update_reset_state(rdp->update);
			rdp_finalize_reset_flags(rdp, FALSE);
			break;

		case CONNECTION_STATE_ACTIVE:
		{
			ActivatedEventArgs activatedEvent;
			rdpContext* context = rdp->context;
			EventArgsInit(&activatedEvent, "libfreerdp");
			activatedEvent.firstActivation =
			    !rdp_finalize_is_flag_set(rdp, FINALIZE_DEACTIVATE_REACTIVATE);
			PubSub_OnActivated(context->pubSub, context, &activatedEvent);
		}

		break;

		default:
			break;
	}

	{
		ConnectionStateChangeEventArgs stateEvent;
		rdpContext* context = rdp->context;
		EventArgsInit(&stateEvent, "libfreerdp");
		stateEvent.state = rdp_get_state(rdp);
		stateEvent.active = rdp_get_state(rdp) == CONNECTION_STATE_ACTIVE;
		PubSub_OnConnectionStateChange(context->pubSub, context, &stateEvent);
	}

	return status;
}

BOOL rdp_server_accept_nego(rdpRdp* rdp, wStream* s)
{
	UINT32 SelectedProtocol = 0;
	UINT32 RequestedProtocols;
	BOOL status;
	rdpSettings* settings;
	rdpNego* nego;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	settings = rdp->settings;
	WINPR_ASSERT(settings);

	nego = rdp->nego;
	WINPR_ASSERT(nego);

	transport_set_blocking_mode(rdp->transport, TRUE);

	if (!nego_read_request(nego, s))
		return FALSE;

	RequestedProtocols = nego_get_requested_protocols(nego);
	WLog_INFO(TAG, "Client Security: NLA:%d TLS:%d RDP:%d",
	          (RequestedProtocols & PROTOCOL_HYBRID) ? 1 : 0,
	          (RequestedProtocols & PROTOCOL_SSL) ? 1 : 0,
	          (RequestedProtocols == PROTOCOL_RDP) ? 1 : 0);
	WLog_INFO(TAG, "Server Security: NLA:%" PRId32 " TLS:%" PRId32 " RDP:%" PRId32 "",
	          settings->NlaSecurity, settings->TlsSecurity, settings->RdpSecurity);

	if ((settings->NlaSecurity) && (RequestedProtocols & PROTOCOL_HYBRID))
	{
		SelectedProtocol = PROTOCOL_HYBRID;
	}
	else if ((settings->TlsSecurity) && (RequestedProtocols & PROTOCOL_SSL))
	{
		SelectedProtocol = PROTOCOL_SSL;
	}
	else if ((settings->RdpSecurity) && (RequestedProtocols == PROTOCOL_RDP))
	{
		SelectedProtocol = PROTOCOL_RDP;
	}
	else
	{
		/*
		 * when here client and server aren't compatible, we select the right
		 * error message to return to the client in the nego failure packet
		 */
		SelectedProtocol = PROTOCOL_FAILED_NEGO;

		if (settings->RdpSecurity)
		{
			WLog_ERR(TAG, "server supports only Standard RDP Security");
			SelectedProtocol |= SSL_NOT_ALLOWED_BY_SERVER;
		}
		else
		{
			if (settings->NlaSecurity && !settings->TlsSecurity)
			{
				WLog_WARN(TAG, "server supports only NLA Security");
				SelectedProtocol |= HYBRID_REQUIRED_BY_SERVER;
			}
			else
			{
				WLog_WARN(TAG, "server supports only a SSL based Security (TLS or NLA)");
				SelectedProtocol |= SSL_REQUIRED_BY_SERVER;
			}
		}

		WLog_ERR(TAG, "Protocol security negotiation failure");
	}

	if (!(SelectedProtocol & PROTOCOL_FAILED_NEGO))
	{
		WLog_INFO(TAG, "Negotiated Security: NLA:%d TLS:%d RDP:%d",
		          (SelectedProtocol & PROTOCOL_HYBRID) ? 1 : 0,
		          (SelectedProtocol & PROTOCOL_SSL) ? 1 : 0,
		          (SelectedProtocol == PROTOCOL_RDP) ? 1 : 0);
	}

	if (!nego_set_selected_protocol(nego, SelectedProtocol))
		return FALSE;

	if (!nego_send_negotiation_response(nego))
		return FALSE;

	SelectedProtocol = nego_get_selected_protocol(nego);
	status = FALSE;

	if (SelectedProtocol & PROTOCOL_HYBRID)
		status = transport_accept_nla(rdp->transport);
	else if (SelectedProtocol & PROTOCOL_SSL)
		status = transport_accept_tls(rdp->transport);
	else if (SelectedProtocol == PROTOCOL_RDP) /* 0 */
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
		rdpMcsChannel* cur = &mcs->channels[i];
		WLog_INFO(TAG, " %s", cur->Name);
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
		rdpMcsChannel* cur = &mcs->channels[i];
		if (cur->ChannelId == channelId)
			cur->joined = TRUE;

		if (!cur->joined)
			allJoined = FALSE;
	}

	if ((mcs->userChannelJoined) && (mcs->globalChannelJoined) &&
	    (mcs->messageChannelId == 0 || mcs->messageChannelJoined) && allJoined)
	{
		rdp_server_transition_to_state(rdp, CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT);
	}

	return TRUE;
}

BOOL rdp_server_accept_confirm_active(rdpRdp* rdp, wStream* s, UINT16 pduLength)
{
	freerdp_peer* peer = rdp->context->peer;

	if (rdp_get_state(rdp) != CONNECTION_STATE_CAPABILITIES_EXCHANGE)
		return FALSE;

	if (!rdp_recv_confirm_active(rdp, s, pduLength))
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

BOOL rdp_server_transition_to_state(rdpRdp* rdp, CONNECTION_STATE state)
{
	BOOL status = FALSE;
	freerdp_peer* client = NULL;
	const CONNECTION_STATE cstate = rdp_get_state(rdp);

	if (cstate >= CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT)
		client = rdp->context->peer;

	if (cstate < CONNECTION_STATE_ACTIVE)
	{
		if (client)
			client->activated = FALSE;
	}

	WLog_DBG(TAG, "%s %s --> %s", __FUNCTION__, rdp_get_state_string(rdp), rdp_state_string(state));
	if (!rdp_set_state(rdp, state))
		goto fail;
	switch (state)
	{
		case CONNECTION_STATE_CAPABILITIES_EXCHANGE:
			rdp->AwaitCapabilities = FALSE;
			break;

		case CONNECTION_STATE_FINALIZATION:
			if (!rdp_finalize_reset_flags(rdp, FALSE))
				goto fail;
			break;

		case CONNECTION_STATE_ACTIVE:
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
						goto fail;
				}

				if (rdp_get_state(rdp) >= CONNECTION_STATE_ACTIVE)
				{
					IFCALLRET(client->Activate, client->activated, client);

					if (!client->activated)
						goto fail;
				}
			}

			break;

		default:
			break;
	}

	status = TRUE;
fail:
	return status;
}

const char* rdp_client_connection_state_string(int state)
{
	switch (state)
	{
		case CLIENT_STATE_INITIAL:
			return "CLIENT_STATE_INITIAL";
		case CLIENT_STATE_PRECONNECT_PASSED:
			return "CLIENT_STATE_PRECONNECT_PASSED";
		case CLIENT_STATE_POSTCONNECT_PASSED:
			return "CLIENT_STATE_POSTCONNECT_PASSED";
		default:
			return "UNKNOWN";
	}
}

const char* rdp_state_string(CONNECTION_STATE state)
{
	switch (state)
	{
		case CONNECTION_STATE_INITIAL:
			return "CONNECTION_STATE_INITIAL";
		case CONNECTION_STATE_NEGO:
			return "CONNECTION_STATE_NEGO";
		case CONNECTION_STATE_NLA:
			return "CONNECTION_STATE_NLA";
		case CONNECTION_STATE_MCS_CONNECT:
			return "CONNECTION_STATE_MCS_CONNECT";
		case CONNECTION_STATE_MCS_ERECT_DOMAIN:
			return "CONNECTION_STATE_MCS_ERECT_DOMAIN";
		case CONNECTION_STATE_MCS_ATTACH_USER:
			return "CONNECTION_STATE_MCS_ATTACH_USER";
		case CONNECTION_STATE_MCS_CHANNEL_JOIN:
			return "CONNECTION_STATE_MCS_CHANNEL_JOIN";
		case CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT:
			return "CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT";
		case CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE:
			return "CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE";
		case CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT:
			return "CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT";
		case CONNECTION_STATE_LICENSING:
			return "CONNECTION_STATE_LICENSING";
		case CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING:
			return "CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING";
		case CONNECTION_STATE_CAPABILITIES_EXCHANGE:
			return "CONNECTION_STATE_CAPABILITIES_EXCHANGE";
		case CONNECTION_STATE_FINALIZATION:
			return "CONNECTION_STATE_FINALIZATION";
		case CONNECTION_STATE_ACTIVE:
			return "CONNECTION_STATE_ACTIVE";
		default:
			return "UNKNOWN";
	}
}

CONNECTION_STATE rdp_get_state(const rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	return rdp->state;
}

BOOL rdp_set_state(rdpRdp* rdp, CONNECTION_STATE state)
{
	WINPR_ASSERT(rdp);
	rdp->state = state;
	return TRUE;
}

const char* rdp_get_state_string(rdpRdp* rdp)
{
	CONNECTION_STATE state = rdp_get_state(rdp);
	return rdp_state_string(state);
}

BOOL rdp_channels_from_mcs(rdpSettings* settings, const rdpRdp* rdp)
{
	size_t x;
	const rdpMcs* mcs;

	WINPR_ASSERT(rdp);

	mcs = rdp->mcs;
	WINPR_ASSERT(mcs);

	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ChannelDefArray, NULL,
	                                      mcs->channelCount))
		return FALSE;

	for (x = 0; x < mcs->channelCount; x++)
	{
		const rdpMcsChannel* mchannel = &mcs->channels[x];
		CHANNEL_DEF cur = { 0 };

		memcpy(cur.name, mchannel->Name, sizeof(cur.name));
		cur.options = mchannel->options;
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_ChannelDefArray, x, &cur))
			return FALSE;
	}

	return TRUE;
}
