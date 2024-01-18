/*
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Connection Sequence
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include "settings.h"

#include "info.h"
#include "input.h"
#include "rdp.h"
#include "peer.h"

#include "connection.h"
#include "transport.h"

#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/ssl.h>

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/listener.h>

#include "../cache/pointer.h"
#include "../crypto/crypto.h"
#include "../crypto/privatekey.h"
#include "../crypto/certificate.h"
#include "gateway/arm.h"

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

static BOOL rdp_client_wait_for_activation(rdpRdp* rdp)
{
	BOOL timedout = FALSE;
	WINPR_ASSERT(rdp);

	const rdpSettings* settings = rdp->settings;
	WINPR_ASSERT(settings);

	UINT64 now = GetTickCount64();
	UINT64 dueDate = now + freerdp_settings_get_uint32(settings, FreeRDP_TcpAckTimeout);

	for (; (now < dueDate) && !timedout; now = GetTickCount64())
	{
		HANDLE events[MAXIMUM_WAIT_OBJECTS] = { 0 };
		DWORD wstatus = 0;
		DWORD nevents = freerdp_get_event_handles(rdp->context, events, ARRAYSIZE(events));
		if (!nevents)
		{
			WLog_ERR(TAG, "error retrieving connection events");
			return FALSE;
		}

		wstatus = WaitForMultipleObjectsEx(nevents, events, FALSE, (dueDate - now), TRUE);
		switch (wstatus)
		{
			case WAIT_TIMEOUT:
				/* will make us quit with a timeout */
				timedout = TRUE;
				break;
			case WAIT_ABANDONED:
			case WAIT_FAILED:
				return FALSE;
			case WAIT_IO_COMPLETION:
				break;
			case WAIT_OBJECT_0:
			default:
				/* handles all WAIT_OBJECT_0 + [0 .. MAXIMUM_WAIT_OBJECTS-1] cases */
				if (rdp_check_fds(rdp) < 0)
				{
					freerdp_set_last_error_if_not(rdp->context,
					                              FREERDP_ERROR_CONNECT_TRANSPORT_FAILED);
					return FALSE;
				}
				break;
		}

		if (rdp_is_active_state(rdp))
			return TRUE;
	}

	WLog_ERR(TAG, "Timeout waiting for activation");
	freerdp_set_last_error_if_not(rdp->context, FREERDP_ERROR_CONNECT_ACTIVATION_TIMEOUT);
	return FALSE;
}
/**
 * Establish RDP Connection based on the settings given in the 'rdp' parameter.
 * msdn{cc240452}
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

	UINT32 TcpConnectTimeout = freerdp_settings_get_uint32(settings, FreeRDP_TcpConnectTimeout);
	if (settings->GatewayArmTransport)
	{
		if (!arm_resolve_endpoint(rdp->context, TcpConnectTimeout))
		{
			WLog_ERR(TAG, "error retrieving ARM configuration");
			return FALSE;
		}
	}

	const char* hostname = settings->ServerHostname;
	if (!hostname)
	{
		WLog_ERR(TAG, "Missing hostname, can not connect to NULL target");
		return FALSE;
	}

	nego_init(rdp->nego);
	nego_set_target(rdp->nego, hostname, settings->ServerPort);

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

	nego_set_childsession_enabled(rdp->nego, settings->ConnectChildSession);
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
	nego_enable_rdstls(rdp->nego, settings->RdstlsSecurity);
	nego_enable_aad(rdp->nego, settings->AadSecurity);

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
		if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_NEGO))
			return FALSE;

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

		if ((SelectedProtocol & PROTOCOL_SSL) || (SelectedProtocol == PROTOCOL_RDP) ||
		    (SelectedProtocol == PROTOCOL_RDSTLS))
		{
			wStream s = { 0 };

			if ((settings->Username != NULL) &&
			    ((freerdp_settings_get_string(settings, FreeRDP_Password) != NULL) ||
			     (settings->RedirectionPassword != NULL &&
			      settings->RedirectionPasswordLength > 0)))
				settings->AutoLogonEnabled = TRUE;

			if (rdp_recv_callback(rdp->transport, &s, rdp) < 0)
				return FALSE;
		}

		transport_set_blocking_mode(rdp->transport, FALSE);
	}
	else
	{
		if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_MCS_CREATE_REQUEST))
			return FALSE;
	}

	/* everything beyond this point is event-driven and non blocking */
	if (!transport_set_recv_callbacks(rdp->transport, rdp_recv_callback, rdp))
		return FALSE;

	return rdp_client_wait_for_activation(rdp);
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

	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_INITIAL))
		return FALSE;

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

	if ((settings->RedirectionFlags & LB_LOAD_BALANCE_INFO) == 0)
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

	settings->RdstlsSecurity =
	    (settings->RedirectionFlags & LB_PASSWORD_IS_PK_ENCRYPTED) != 0 ? TRUE : FALSE;

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
	wStream* s = NULL;
	int status = 0;
	BOOL ret = FALSE;

	WINPR_ASSERT(rdp);
	rdpSettings* settings = rdp->settings;
	BYTE* crypt_client_random = NULL;

	WINPR_ASSERT(settings);
	if (!settings->UseRdpSecurityLayer)
	{
		/* no RDP encryption */
		return TRUE;
	}

	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT))
		return FALSE;

	/* encrypt client random */
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ClientRandom, NULL,
	                                      CLIENT_RANDOM_LENGTH))
		return FALSE;
	winpr_RAND(settings->ClientRandom, settings->ClientRandomLength);

	const rdpCertInfo* info = freerdp_certificate_get_info(settings->RdpServerCertificate);
	if (!info)
	{
		WLog_ERR(TAG, "Failed to get rdpCertInfo from RdpServerCertificate");
		return FALSE;
	}

	/*
	 * client random must be (bitlen / 8) + 8 - see [MS-RDPBCGR] 5.3.4.1
	 * for details
	 */
	crypt_client_random = calloc(info->ModulusLength, 1);

	if (!crypt_client_random)
		return FALSE;

	crypto_rsa_public_encrypt(settings->ClientRandom, settings->ClientRandomLength, info,
	                          crypt_client_random, info->ModulusLength);
	/* send crypt client random to server */
	const size_t length =
	    RDP_PACKET_HEADER_MAX_LENGTH + RDP_SECURITY_HEADER_LENGTH + 4 + info->ModulusLength + 8;
	s = Stream_New(NULL, length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto end;
	}

	if (!rdp_write_header(rdp, s, length, MCS_GLOBAL_CHANNEL_ID))
		goto end;
	if (!rdp_write_security_header(rdp, s, SEC_EXCHANGE_PKT | SEC_LICENSE_ENCRYPT_SC))
		goto end;

	Stream_Write_UINT32(s, info->ModulusLength + 8);
	Stream_Write(s, crypt_client_random, info->ModulusLength);
	Stream_Zero(s, 8);
	Stream_SealLength(s);
	status = transport_write(rdp->mcs->transport, s);
	Stream_Free(s, TRUE);

	if (status < 0)
		goto end;

	rdp->do_crypt_license = TRUE;

	/* now calculate encrypt / decrypt and update keys */
	if (!security_establish_keys(rdp))
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

	if (!rdp_reset_rc4_encrypt_keys(rdp))
		goto end;
	if (!rdp_reset_rc4_decrypt_keys(rdp))
		goto end;

	ret = TRUE;
end:
	free(crypt_client_random);

	if (!ret)
	{
		winpr_Cipher_Free(rdp->fips_decrypt);
		winpr_Cipher_Free(rdp->fips_encrypt);
		rdp->fips_decrypt = NULL;
		rdp->fips_encrypt = NULL;

		rdp_free_rc4_decrypt_keys(rdp);
		rdp_free_rc4_encrypt_keys(rdp);
	}

	return ret;
}

static BOOL rdp_update_client_random(rdpSettings* settings, const BYTE* crypt_random,
                                     size_t crypt_random_len)
{
	const size_t length = 32;
	WINPR_ASSERT(settings);

	const rdpPrivateKey* rsa = freerdp_settings_get_pointer(settings, FreeRDP_RdpServerRsaKey);
	WINPR_ASSERT(rsa);

	const rdpCertInfo* cinfo = freerdp_key_get_info(rsa);
	WINPR_ASSERT(cinfo);

	if (crypt_random_len != cinfo->ModulusLength + 8)
	{
		WLog_ERR(TAG, "invalid encrypted client random length");
		return FALSE;
	}
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ClientRandom, NULL, length))
		return FALSE;

	BYTE* client_random = freerdp_settings_get_pointer_writable(settings, FreeRDP_ClientRandom);
	WINPR_ASSERT(client_random);
	return crypto_rsa_private_decrypt(crypt_random, crypt_random_len - 8, rsa, client_random,
	                                  length) > 0;
}

BOOL rdp_server_establish_keys(rdpRdp* rdp, wStream* s)
{
	UINT32 rand_len = 0;
	UINT16 channel_id = 0;
	UINT16 length = 0;
	UINT16 sec_flags = 0;
	BOOL ret = FALSE;

	WINPR_ASSERT(rdp);

	if (!rdp->settings->UseRdpSecurityLayer)
	{
		/* No RDP Security. */
		return TRUE;
	}

	if (!rdp_read_header(rdp, s, &length, &channel_id))
		return FALSE;

	if (!rdp_read_security_header(rdp, s, &sec_flags, NULL))
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

	const BYTE* crypt_random = Stream_ConstPointer(s);
	if (!Stream_SafeSeek(s, rand_len))
		goto end;
	if (!rdp_update_client_random(rdp->settings, crypt_random, rand_len))
		goto end;

	/* now calculate encrypt / decrypt and update keys */
	if (!security_establish_keys(rdp))
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

	if (!rdp_reset_rc4_encrypt_keys(rdp))
		goto end;

	if (!rdp_reset_rc4_decrypt_keys(rdp))
		goto end;

	ret = tpkt_ensure_stream_consumed(s, length);
end:

	if (!ret)
	{
		winpr_Cipher_Free(rdp->fips_encrypt);
		winpr_Cipher_Free(rdp->fips_decrypt);
		rdp->fips_encrypt = NULL;
		rdp->fips_decrypt = NULL;

		rdp_free_rc4_encrypt_keys(rdp);
		rdp_free_rc4_decrypt_keys(rdp);
	}

	return ret;
}

static BOOL rdp_client_send_client_info_and_change_state(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	if (!rdp_client_establish_keys(rdp))
		return FALSE;
	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE))
		return FALSE;
	if (!rdp_send_client_info(rdp))
		return FALSE;
	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT_REQUEST))
		return FALSE;
	return TRUE;
}

BOOL rdp_client_skip_mcs_channel_join(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);

	rdpMcs* mcs = rdp->mcs;
	WINPR_ASSERT(mcs);

	mcs->userChannelJoined = TRUE;
	mcs->globalChannelJoined = TRUE;
	mcs->messageChannelJoined = TRUE;

	for (UINT32 i = 0; i < mcs->channelCount; i++)
	{
		rdpMcsChannel* cur = &mcs->channels[i];
		WLog_DBG(TAG, " %s [%" PRIu16 "]", cur->Name, cur->ChannelId);
		cur->joined = TRUE;
	}

	return rdp_client_send_client_info_and_change_state(rdp);
}

static BOOL rdp_client_join_channel(rdpRdp* rdp, UINT16 ChannelId)
{
	WINPR_ASSERT(rdp);

	rdpMcs* mcs = rdp->mcs;
	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_MCS_CHANNEL_JOIN_REQUEST))
		return FALSE;
	if (!mcs_send_channel_join_request(mcs, ChannelId))
		return FALSE;
	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_MCS_CHANNEL_JOIN_RESPONSE))
		return FALSE;
	return TRUE;
}

BOOL rdp_client_connect_mcs_channel_join_confirm(rdpRdp* rdp, wStream* s)
{
	UINT32 i;
	UINT16 channelId;
	BOOL allJoined = TRUE;

	WINPR_ASSERT(rdp);
	rdpMcs* mcs = rdp->mcs;

	if (!mcs_recv_channel_join_confirm(mcs, s, &channelId))
		return FALSE;

	if (!mcs->userChannelJoined)
	{
		if (channelId != mcs->userId)
		{
			WLog_ERR(TAG, "expected user channel id %" PRIu16 ", but received %" PRIu16,
			         mcs->userId, channelId);
			return FALSE;
		}

		mcs->userChannelJoined = TRUE;
		if (!rdp_client_join_channel(rdp, MCS_GLOBAL_CHANNEL_ID))
			return FALSE;
	}
	else if (!mcs->globalChannelJoined)
	{
		if (channelId != MCS_GLOBAL_CHANNEL_ID)
		{
			WLog_ERR(TAG, "expected uglobalser channel id %" PRIu16 ", but received %" PRIu16,
			         MCS_GLOBAL_CHANNEL_ID, channelId);
			return FALSE;
		}
		mcs->globalChannelJoined = TRUE;

		if (mcs->messageChannelId != 0)
		{
			if (!rdp_client_join_channel(rdp, mcs->messageChannelId))
				return FALSE;
			allJoined = FALSE;
		}
		else
		{
			if (mcs->channelCount > 0)
			{
				const rdpMcsChannel* cur = &mcs->channels[0];
				if (!rdp_client_join_channel(rdp, cur->ChannelId))
					return FALSE;
				allJoined = FALSE;
			}
		}
	}
	else if ((mcs->messageChannelId != 0) && !mcs->messageChannelJoined)
	{
		if (channelId != mcs->messageChannelId)
		{
			WLog_ERR(TAG, "expected messageChannelId=%" PRIu16 ", got %" PRIu16,
			         mcs->messageChannelId, channelId);
			return FALSE;
		}

		mcs->messageChannelJoined = TRUE;

		if (mcs->channelCount > 0)
		{
			const rdpMcsChannel* cur = &mcs->channels[0];
			if (!rdp_client_join_channel(rdp, cur->ChannelId))
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
			{
				WLog_ERR(TAG, "expected channel id %" PRIu16 ", but received %" PRIu16,
				         MCS_GLOBAL_CHANNEL_ID, channelId);
				return FALSE;
			}
			cur->joined = TRUE;
			break;
		}

		if (i + 1 < mcs->channelCount)
		{
			const rdpMcsChannel* cur = &mcs->channels[i + 1];
			if (!rdp_client_join_channel(rdp, cur->ChannelId))
				return FALSE;
			allJoined = FALSE;
		}
	}

	if (mcs->userChannelJoined && mcs->globalChannelJoined && allJoined)
	{
		if (!rdp_client_send_client_info_and_change_state(rdp))
			return FALSE;
	}

	return TRUE;
}

BOOL rdp_client_connect_auto_detect(rdpRdp* rdp, wStream* s)
{
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->mcs);

	const UINT16 messageChannelId = rdp->mcs->messageChannelId;
	/* If the MCS message channel has been joined... */
	if (messageChannelId != 0)
	{
		/* Process any MCS message channel PDUs. */
		const size_t pos = Stream_GetPosition(s);
		UINT16 length = 0;
		UINT16 channelId = 0;

		if (rdp_read_header(rdp, s, &length, &channelId))
		{
			if (channelId == messageChannelId)
			{
				UINT16 securityFlags = 0;

				if (!rdp_read_security_header(rdp, s, &securityFlags, &length))
					return FALSE;

				if (securityFlags & SEC_ENCRYPT)
				{
					if (!rdp_decrypt(rdp, s, &length, securityFlags))
						return FALSE;
				}

				if (rdp_recv_message_channel_pdu(rdp, s, securityFlags) == STATE_RUN_SUCCESS)
					return tpkt_ensure_stream_consumed(s, length);
			}
		}
		else
			WLog_WARN(TAG, "expected messageChannelId=%" PRIu16 ", got %" PRIu16, messageChannelId,
			          channelId);

		Stream_SetPosition(s, pos);
	}
	else
		WLog_WARN(TAG, "messageChannelId == 0");

	return FALSE;
}

state_run_t rdp_client_connect_license(rdpRdp* rdp, wStream* s)
{
	state_run_t status = STATE_RUN_FAILED;
	LICENSE_STATE state;
	UINT16 length, channelId, securityFlags;

	WINPR_ASSERT(rdp);
	if (!rdp_read_header(rdp, s, &length, &channelId))
		return STATE_RUN_FAILED;

	if (!rdp_read_security_header(rdp, s, &securityFlags, &length))
		return STATE_RUN_FAILED;

	if (securityFlags & SEC_ENCRYPT)
	{
		if (!rdp_decrypt(rdp, s, &length, securityFlags))
			return STATE_RUN_FAILED;
	}

	/* there might be autodetect messages mixed in between licensing messages.
	 * that has been observed with 2k12 R2 and 2k19
	 */
	const UINT16 messageChannelId = rdp->mcs->messageChannelId;
	if ((channelId == messageChannelId) || (securityFlags & SEC_AUTODETECT_REQ))
	{
		return rdp_recv_message_channel_pdu(rdp, s, securityFlags);
	}

	if (channelId != MCS_GLOBAL_CHANNEL_ID)
		WLog_WARN(TAG, "unexpected message for channel %u, expected %u", channelId,
		          MCS_GLOBAL_CHANNEL_ID);

	if ((securityFlags & SEC_LICENSE_PKT) == 0)
	{
		char buffer[512] = { 0 };
		char lbuffer[32] = { 0 };
		WLog_ERR(TAG, "securityFlags=%s, missing required flag %s",
		         rdp_security_flag_string(securityFlags, buffer, sizeof(buffer)),
		         rdp_security_flag_string(SEC_LICENSE_PKT, lbuffer, sizeof(lbuffer)));
		return STATE_RUN_FAILED;
	}

	status = license_recv(rdp->license, s);

	if (state_run_failed(status))
		return status;

	state = license_get_state(rdp->license);
	switch (state)
	{
		case LICENSE_STATE_ABORTED:
			WLog_ERR(TAG, "license connection sequence aborted.");
			return STATE_RUN_FAILED;
		case LICENSE_STATE_COMPLETED:
			if (rdp->settings->MultitransportFlags)
			{
				if (!rdp_client_transition_to_state(
				        rdp, CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING_REQUEST))
					return STATE_RUN_FAILED;
			}
			else
			{
				if (!rdp_client_transition_to_state(
				        rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE_DEMAND_ACTIVE))
					return STATE_RUN_FAILED;
			}
			return STATE_RUN_SUCCESS;
		default:
			return STATE_RUN_SUCCESS;
	}
}

state_run_t rdp_client_connect_demand_active(rdpRdp* rdp, wStream* s)
{
	UINT16 length = 0;
	UINT16 channelId = 0;
	UINT16 pduType = 0;
	UINT16 pduSource = 0;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);
	WINPR_ASSERT(rdp->settings);

	if (!rdp_recv_get_active_header(rdp, s, &channelId, &length))
		return STATE_RUN_FAILED;

	if (freerdp_shall_disconnect_context(rdp->context))
		return STATE_RUN_QUIT_SESSION;

	if (!rdp_read_share_control_header(rdp, s, NULL, NULL, &pduType, &pduSource))
		return STATE_RUN_FAILED;

	switch (pduType)
	{
		case PDU_TYPE_DEMAND_ACTIVE:
			if (!rdp_recv_demand_active(rdp, s, pduSource, length))
				return STATE_RUN_FAILED;
			return STATE_RUN_ACTIVE;
		default:
			return rdp_recv_out_of_sequence_pdu(rdp, s, pduType, length);
	}
}

state_run_t rdp_client_connect_finalize(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	/**
	 * [MS-RDPBCGR] 1.3.1.1 - 8.
	 * The client-to-server PDUs sent during this phase have no dependencies on any of the
	 * server-to- client PDUs; they may be sent as a single batch, provided that sequencing is
	 * maintained.
	 */
	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_SYNC))
		return STATE_RUN_FAILED;

	if (!rdp_send_client_synchronize_pdu(rdp))
		return STATE_RUN_FAILED;

	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_COOPERATE))
		return STATE_RUN_FAILED;
	if (!rdp_send_client_control_pdu(rdp, CTRLACTION_COOPERATE))
		return STATE_RUN_FAILED;

	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_REQUEST_CONTROL))
		return STATE_RUN_FAILED;
	if (!rdp_send_client_control_pdu(rdp, CTRLACTION_REQUEST_CONTROL))
		return STATE_RUN_FAILED;

	/**
	 * [MS-RDPBCGR] 2.2.1.17
	 * Client persistent key list must be sent if a bitmap is
	 * stored in persistent bitmap cache or the server has advertised support for bitmap
	 * host cache and a deactivation reactivation sequence is *not* in progress.
	 */

	if (!rdp_finalize_is_flag_set(rdp, FINALIZE_DEACTIVATE_REACTIVATE) &&
	    freerdp_settings_get_bool(rdp->settings, FreeRDP_BitmapCachePersistEnabled))
	{
		if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_PERSISTENT_KEY_LIST))
			return STATE_RUN_FAILED;
		if (!rdp_send_client_persistent_key_list_pdu(rdp))
			return STATE_RUN_FAILED;
	}

	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_FONT_LIST))
		return STATE_RUN_FAILED;
	if (!rdp_send_client_font_list_pdu(rdp, FONTLIST_FIRST | FONTLIST_LAST))
		return STATE_RUN_FAILED;

	if (!rdp_client_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_CLIENT_SYNC))
		return STATE_RUN_FAILED;
	return STATE_RUN_SUCCESS;
}

BOOL rdp_client_transition_to_state(rdpRdp* rdp, CONNECTION_STATE state)
{
	const char* name = rdp_state_string(state);

	WINPR_ASSERT(rdp);
	WLog_Print(rdp->log, WLOG_DEBUG, "%s --> %s", rdp_get_state_string(rdp), name);

	if (!rdp_set_state(rdp, state))
		return FALSE;

	switch (state)
	{
		case CONNECTION_STATE_FINALIZATION_SYNC:
		case CONNECTION_STATE_FINALIZATION_COOPERATE:
		case CONNECTION_STATE_FINALIZATION_REQUEST_CONTROL:
		case CONNECTION_STATE_FINALIZATION_PERSISTENT_KEY_LIST:
		case CONNECTION_STATE_FINALIZATION_FONT_LIST:
			update_reset_state(rdp->update);
			break;

		case CONNECTION_STATE_CAPABILITIES_EXCHANGE_CONFIRM_ACTIVE:
		{
			ActivatedEventArgs activatedEvent = { 0 };
			rdpContext* context = rdp->context;
			EventArgsInit(&activatedEvent, "libfreerdp");
			activatedEvent.firstActivation =
			    !rdp_finalize_is_flag_set(rdp, FINALIZE_DEACTIVATE_REACTIVATE);
			PubSub_OnActivated(rdp->pubSub, context, &activatedEvent);
		}

		break;

		default:
			break;
	}

	{
		ConnectionStateChangeEventArgs stateEvent = { 0 };
		rdpContext* context = rdp->context;
		EventArgsInit(&stateEvent, "libfreerdp");
		stateEvent.state = rdp_get_state(rdp);
		stateEvent.active = rdp_is_active_state(rdp);
		PubSub_OnConnectionStateChange(rdp->pubSub, context, &stateEvent);
	}

	return TRUE;
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
	WLog_DBG(TAG, "Client Security: RDSTLS:%d NLA:%d TLS:%d RDP:%d",
	         (RequestedProtocols & PROTOCOL_RDSTLS) ? 1 : 0,
	         (RequestedProtocols & PROTOCOL_HYBRID) ? 1 : 0,
	         (RequestedProtocols & PROTOCOL_SSL) ? 1 : 0,
	         (RequestedProtocols == PROTOCOL_RDP) ? 1 : 0);
	WLog_DBG(TAG,
	         "Server Security: RDSTLS:%" PRId32 " NLA:%" PRId32 " TLS:%" PRId32 " RDP:%" PRId32 "",
	         settings->RdstlsSecurity, settings->NlaSecurity, settings->TlsSecurity,
	         settings->RdpSecurity);

	if ((settings->RdstlsSecurity) && (RequestedProtocols & PROTOCOL_RDSTLS))
	{
		SelectedProtocol = PROTOCOL_RDSTLS;
	}
	else if ((settings->NlaSecurity) && (RequestedProtocols & PROTOCOL_HYBRID))
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
		WLog_DBG(TAG, "Negotiated Security: RDSTLS:%d NLA:%d TLS:%d RDP:%d",
		         (SelectedProtocol & PROTOCOL_RDSTLS) ? 1 : 0,
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

	if (SelectedProtocol & PROTOCOL_RDSTLS)
		status = transport_accept_rdstls(rdp->transport);
	else if (SelectedProtocol & PROTOCOL_HYBRID)
		status = transport_accept_nla(rdp->transport);
	else if (SelectedProtocol & PROTOCOL_SSL)
		status = transport_accept_tls(rdp->transport);
	else if (SelectedProtocol == PROTOCOL_RDP) /* 0 */
		status = transport_accept_rdp(rdp->transport);

	if (!status)
		return FALSE;

	transport_set_blocking_mode(rdp->transport, FALSE);

	return TRUE;
}

static BOOL rdp_update_encryption_level(rdpSettings* settings)
{
	WINPR_ASSERT(settings);

	UINT32 EncryptionLevel = freerdp_settings_get_uint32(settings, FreeRDP_EncryptionLevel);
	UINT32 EncryptionMethods = freerdp_settings_get_uint32(settings, FreeRDP_EncryptionMethods);

	/**
	 * Re: settings->EncryptionLevel:
	 * This is configured/set by the server implementation and serves the same
	 * purpose as the "Encryption Level" setting in the RDP-Tcp configuration
	 * dialog of Microsoft's Remote Desktop Session Host Configuration.
	 * Re: settings->EncryptionMethods:
	 * at this point this setting contains the client's supported encryption
	 * methods we've received in gcc_read_client_security_data()
	 */

	if (!settings->UseRdpSecurityLayer)
	{
		/* TLS/NLA is used: disable rdp style encryption */
		EncryptionLevel = ENCRYPTION_LEVEL_NONE;
	}
	else
	{
		/* verify server encryption level value */
		switch (EncryptionLevel)
		{
			case ENCRYPTION_LEVEL_NONE:
				WLog_INFO(TAG, "Active rdp encryption level: NONE");
				break;

			case ENCRYPTION_LEVEL_FIPS:
				WLog_INFO(TAG, "Active rdp encryption level: FIPS Compliant");
				break;

			case ENCRYPTION_LEVEL_HIGH:
				WLog_INFO(TAG, "Active rdp encryption level: HIGH");
				break;

			case ENCRYPTION_LEVEL_LOW:
				WLog_INFO(TAG, "Active rdp encryption level: LOW");
				break;

			case ENCRYPTION_LEVEL_CLIENT_COMPATIBLE:
				WLog_INFO(TAG, "Active rdp encryption level: CLIENT-COMPATIBLE");
				break;

			default:
				WLog_ERR(TAG, "Invalid server encryption level 0x%08" PRIX32 "", EncryptionLevel);
				WLog_ERR(TAG, "Switching to encryption level CLIENT-COMPATIBLE");
				EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
		}
	}

	/* choose rdp encryption method based on server level and client methods */
	switch (EncryptionLevel)
	{
		case ENCRYPTION_LEVEL_NONE:
			/* The only valid method is NONE in this case */
			EncryptionMethods = ENCRYPTION_METHOD_NONE;
			break;

		case ENCRYPTION_LEVEL_FIPS:

			/* The only valid method is FIPS in this case */
			if (!(EncryptionMethods & ENCRYPTION_METHOD_FIPS))
			{
				WLog_WARN(TAG, "client does not support FIPS as required by server configuration");
			}

			EncryptionMethods = ENCRYPTION_METHOD_FIPS;
			break;

		case ENCRYPTION_LEVEL_HIGH:

			/* Maximum key strength supported by the server must be used (128 bit)*/
			if (!(EncryptionMethods & ENCRYPTION_METHOD_128BIT))
			{
				WLog_WARN(TAG, "client does not support 128 bit encryption method as required by "
				               "server configuration");
			}

			EncryptionMethods = ENCRYPTION_METHOD_128BIT;
			break;

		case ENCRYPTION_LEVEL_LOW:
		case ENCRYPTION_LEVEL_CLIENT_COMPATIBLE:

			/* Maximum key strength supported by the client must be used */
			if (EncryptionMethods & ENCRYPTION_METHOD_128BIT)
				EncryptionMethods = ENCRYPTION_METHOD_128BIT;
			else if (EncryptionMethods & ENCRYPTION_METHOD_56BIT)
				EncryptionMethods = ENCRYPTION_METHOD_56BIT;
			else if (EncryptionMethods & ENCRYPTION_METHOD_40BIT)
				EncryptionMethods = ENCRYPTION_METHOD_40BIT;
			else if (EncryptionMethods & ENCRYPTION_METHOD_FIPS)
				EncryptionMethods = ENCRYPTION_METHOD_FIPS;
			else
			{
				WLog_WARN(TAG, "client has not announced any supported encryption methods");
				EncryptionMethods = ENCRYPTION_METHOD_128BIT;
			}

			break;

		default:
			WLog_ERR(TAG, "internal error: unknown encryption level");
			return FALSE;
	}

	/* log selected encryption method */
	if (settings->UseRdpSecurityLayer)
	{
		switch (EncryptionMethods)
		{
			case ENCRYPTION_METHOD_NONE:
				WLog_INFO(TAG, "Selected rdp encryption method: NONE");
				break;

			case ENCRYPTION_METHOD_40BIT:
				WLog_INFO(TAG, "Selected rdp encryption method: 40BIT");
				break;

			case ENCRYPTION_METHOD_56BIT:
				WLog_INFO(TAG, "Selected rdp encryption method: 56BIT");
				break;

			case ENCRYPTION_METHOD_128BIT:
				WLog_INFO(TAG, "Selected rdp encryption method: 128BIT");
				break;

			case ENCRYPTION_METHOD_FIPS:
				WLog_INFO(TAG, "Selected rdp encryption method: FIPS");
				break;

			default:
				WLog_ERR(TAG, "internal error: unknown encryption method");
				return FALSE;
		}
	}

	if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionLevel, EncryptionLevel))
		return FALSE;
	if (!freerdp_settings_set_uint32(settings, FreeRDP_EncryptionMethods, EncryptionMethods))
		return FALSE;
	return TRUE;
}

BOOL rdp_server_accept_mcs_connect_initial(rdpRdp* rdp, wStream* s)
{
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	rdpMcs* mcs = rdp->mcs;
	WINPR_ASSERT(mcs);

	WINPR_ASSERT(rdp_get_state(rdp) == CONNECTION_STATE_MCS_CREATE_REQUEST);
	if (!mcs_recv_connect_initial(mcs, s))
		return FALSE;
	WINPR_ASSERT(rdp->settings);

	if (!mcs_server_apply_to_settings(mcs, rdp->settings))
		return FALSE;

	WLog_DBG(TAG, "Accepted client: %s", rdp->settings->ClientHostname);
	WLog_DBG(TAG, "Accepted channels:");

	WINPR_ASSERT(mcs->channels || (mcs->channelCount == 0));
	for (UINT32 i = 0; i < mcs->channelCount; i++)
	{
		ADDIN_ARGV* arg;
		rdpMcsChannel* cur = &mcs->channels[i];
		const char* params[1] = { cur->Name };
		WLog_DBG(TAG, " %s [%" PRIu16 "]", cur->Name, cur->ChannelId);
		arg = freerdp_addin_argv_new(ARRAYSIZE(params), params);
		if (!arg)
			return FALSE;

		if (!freerdp_static_channel_collection_add(rdp->settings, arg))
		{
			freerdp_addin_argv_free(arg);
			return FALSE;
		}
	}

	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_MCS_CREATE_RESPONSE))
		return FALSE;
	if (!rdp_update_encryption_level(rdp->settings))
		return FALSE;
	if (!mcs_send_connect_response(mcs))
		return FALSE;
	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_MCS_ERECT_DOMAIN))
		return FALSE;

	return TRUE;
}

BOOL rdp_server_accept_mcs_erect_domain_request(rdpRdp* rdp, wStream* s)
{
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);
	WINPR_ASSERT(rdp_get_state(rdp) == CONNECTION_STATE_MCS_ERECT_DOMAIN);

	if (!mcs_recv_erect_domain_request(rdp->mcs, s))
		return FALSE;

	return rdp_server_transition_to_state(rdp, CONNECTION_STATE_MCS_ATTACH_USER);
}

static BOOL rdp_server_skip_mcs_channel_join(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);

	rdpMcs* mcs = rdp->mcs;
	WINPR_ASSERT(mcs);

	mcs->userChannelJoined = TRUE;
	mcs->globalChannelJoined = TRUE;
	mcs->messageChannelJoined = TRUE;

	for (UINT32 i = 0; i < mcs->channelCount; i++)
	{
		rdpMcsChannel* cur = &mcs->channels[i];
		WLog_DBG(TAG, " %s [%" PRIu16 "]", cur->Name, cur->ChannelId);
		cur->joined = TRUE;
	}
	return rdp_server_transition_to_state(rdp, CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT);
}

BOOL rdp_server_accept_mcs_attach_user_request(rdpRdp* rdp, wStream* s)
{
	if (!mcs_recv_attach_user_request(rdp->mcs, s))
		return FALSE;

	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_MCS_ATTACH_USER_CONFIRM))
		return FALSE;

	if (!mcs_send_attach_user_confirm(rdp->mcs))
		return FALSE;

	if (freerdp_settings_get_bool(rdp->settings, FreeRDP_SupportSkipChannelJoin))
		return rdp_server_skip_mcs_channel_join(rdp);
	return rdp_server_transition_to_state(rdp, CONNECTION_STATE_MCS_CHANNEL_JOIN_REQUEST);
}

BOOL rdp_server_accept_mcs_channel_join_request(rdpRdp* rdp, wStream* s)
{
	UINT32 i;
	UINT16 channelId;
	BOOL allJoined = TRUE;
	rdpMcs* mcs;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->context);

	mcs = rdp->mcs;
	WINPR_ASSERT(mcs);

	WINPR_ASSERT(rdp_get_state(rdp) == CONNECTION_STATE_MCS_CHANNEL_JOIN_REQUEST);

	if (!mcs_recv_channel_join_request(mcs, rdp->settings, s, &channelId))
		return FALSE;

	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_MCS_CHANNEL_JOIN_RESPONSE))
		return FALSE;

	if (!mcs_send_channel_join_confirm(mcs, channelId))
		return FALSE;

	if (channelId == mcs->userId)
		mcs->userChannelJoined = TRUE;
	if (channelId == MCS_GLOBAL_CHANNEL_ID)
		mcs->globalChannelJoined = TRUE;
	if (channelId == mcs->messageChannelId)
		mcs->messageChannelJoined = TRUE;

	for (i = 0; i < mcs->channelCount; i++)
	{
		rdpMcsChannel* cur = &mcs->channels[i];
		WLog_DBG(TAG, " %s [%" PRIu16 "]", cur->Name, cur->ChannelId);
		if (cur->ChannelId == channelId)
			cur->joined = TRUE;

		if (!cur->joined)
			allJoined = FALSE;
	}

	CONNECTION_STATE rc;
	if ((mcs->userChannelJoined) && (mcs->globalChannelJoined) &&
	    (mcs->messageChannelId == 0 || mcs->messageChannelJoined) && allJoined)
		rc = CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT;
	else
		rc = CONNECTION_STATE_MCS_CHANNEL_JOIN_REQUEST;

	return rdp_server_transition_to_state(rdp, rc);
}

static BOOL rdp_server_send_sync(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);

	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_CLIENT_SYNC))
		return FALSE;
	if (!rdp_send_server_synchronize_pdu(rdp))
		return FALSE;
	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_CLIENT_COOPERATE))
		return FALSE;
	if (!rdp_send_server_control_cooperate_pdu(rdp))
		return FALSE;
	if (!rdp_finalize_reset_flags(rdp, FALSE))
		return FALSE;
	return rdp_server_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_SYNC);
}

BOOL rdp_server_accept_confirm_active(rdpRdp* rdp, wStream* s, UINT16 pduLength)
{
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->context);
	WINPR_ASSERT(rdp->settings);
	WINPR_ASSERT(s);

	freerdp_peer* peer = rdp->context->peer;
	WINPR_ASSERT(peer);

	if (rdp_get_state(rdp) != CONNECTION_STATE_CAPABILITIES_EXCHANGE_CONFIRM_ACTIVE)
	{
		if (freerdp_settings_get_bool(rdp->settings, FreeRDP_TransportDumpReplay))
			rdp_finalize_set_flag(rdp, FINALIZE_DEACTIVATE_REACTIVATE);
		else
		{
			WLog_WARN(TAG, "Invalid state, got %s, expected %s", rdp_get_state_string(rdp),
			          rdp_state_string(CONNECTION_STATE_CAPABILITIES_EXCHANGE_CONFIRM_ACTIVE));
			return FALSE;
		}
	}

	if (!rdp_recv_confirm_active(rdp, s, pduLength))
		return FALSE;

	if (peer->ClientCapabilities && !peer->ClientCapabilities(peer))
	{
		WLog_WARN(TAG, "peer->ClientCapabilities failed");
		return FALSE;
	}

	if (rdp->settings->SaltedChecksum)
		rdp->do_secure_checksum = TRUE;

	return rdp_server_send_sync(rdp);
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

	rdp_finalize_set_flag(rdp, FINALIZE_DEACTIVATE_REACTIVATE);
	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_CAPABILITIES_EXCHANGE_DEMAND_ACTIVE))
		return FALSE;

	state_run_t rc = rdp_peer_handle_state_demand_active(client);
	return state_run_success(rc);
}

static BOOL rdp_is_active_peer_state(CONNECTION_STATE state)
{
	/* [MS-RDPBCGR] 1.3.1.1 Connection Sequence states:
	 * 'upon receipt of the Font List PDU the server can start sending graphics
	 *  output to the client'
	 */
	switch (state)
	{
		case CONNECTION_STATE_FINALIZATION_CLIENT_SYNC:
		case CONNECTION_STATE_FINALIZATION_CLIENT_COOPERATE:
		case CONNECTION_STATE_FINALIZATION_CLIENT_GRANTED_CONTROL:
		case CONNECTION_STATE_FINALIZATION_CLIENT_FONT_MAP:
		case CONNECTION_STATE_ACTIVE:
			return TRUE;
		default:
			return FALSE;
	}
}

static BOOL rdp_is_active_client_state(CONNECTION_STATE state)
{
	/* [MS-RDPBCGR] 1.3.1.1 Connection Sequence states:
	 * 'Once the client has sent the Confirm Active PDU, it can start sending
	 *  mouse and keyboard input to the server'
	 */
	switch (state)
	{
		case CONNECTION_STATE_FINALIZATION_SYNC:
		case CONNECTION_STATE_FINALIZATION_COOPERATE:
		case CONNECTION_STATE_FINALIZATION_REQUEST_CONTROL:
		case CONNECTION_STATE_FINALIZATION_PERSISTENT_KEY_LIST:
		case CONNECTION_STATE_FINALIZATION_FONT_LIST:
		case CONNECTION_STATE_FINALIZATION_CLIENT_SYNC:
		case CONNECTION_STATE_FINALIZATION_CLIENT_COOPERATE:
		case CONNECTION_STATE_FINALIZATION_CLIENT_GRANTED_CONTROL:
		case CONNECTION_STATE_FINALIZATION_CLIENT_FONT_MAP:
		case CONNECTION_STATE_ACTIVE:
			return TRUE;
		default:
			return FALSE;
	}
}

BOOL rdp_is_active_state(const rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->context);

	const CONNECTION_STATE state = rdp_get_state(rdp);
	if (freerdp_settings_get_bool(rdp->context->settings, FreeRDP_ServerMode))
		return rdp_is_active_peer_state(state);
	else
		return rdp_is_active_client_state(state);
}

BOOL rdp_server_transition_to_state(rdpRdp* rdp, CONNECTION_STATE state)
{
	BOOL status = FALSE;
	freerdp_peer* client = NULL;
	const CONNECTION_STATE cstate = rdp_get_state(rdp);

	if (cstate >= CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT)
	{
		WINPR_ASSERT(rdp->context);
		client = rdp->context->peer;
	}

	if (!rdp_is_active_peer_state(cstate))
	{
		if (client)
			client->activated = FALSE;
	}

	WLog_Print(rdp->log, WLOG_DEBUG, "%s --> %s", rdp_get_state_string(rdp),
	           rdp_state_string(state));
	if (!rdp_set_state(rdp, state))
		goto fail;

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
		case CONNECTION_STATE_AAD:
			return "CONNECTION_STATE_AAD";
		case CONNECTION_STATE_MCS_CREATE_REQUEST:
			return "CONNECTION_STATE_MCS_CREATE_REQUEST";
		case CONNECTION_STATE_MCS_CREATE_RESPONSE:
			return "CONNECTION_STATE_MCS_CREATE_RESPONSE";
		case CONNECTION_STATE_MCS_ERECT_DOMAIN:
			return "CONNECTION_STATE_MCS_ERECT_DOMAIN";
		case CONNECTION_STATE_MCS_ATTACH_USER:
			return "CONNECTION_STATE_MCS_ATTACH_USER";
		case CONNECTION_STATE_MCS_ATTACH_USER_CONFIRM:
			return "CONNECTION_STATE_MCS_ATTACH_USER_CONFIRM";
		case CONNECTION_STATE_MCS_CHANNEL_JOIN_REQUEST:
			return "CONNECTION_STATE_MCS_CHANNEL_JOIN_REQUEST";
		case CONNECTION_STATE_MCS_CHANNEL_JOIN_RESPONSE:
			return "CONNECTION_STATE_MCS_CHANNEL_JOIN_RESPONSE";
		case CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT:
			return "CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT";
		case CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE:
			return "CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE";
		case CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT_REQUEST:
			return "CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT_REQUEST";
		case CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT_RESPONSE:
			return "CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT_RESPONSE";
		case CONNECTION_STATE_LICENSING:
			return "CONNECTION_STATE_LICENSING";
		case CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING_REQUEST:
			return "CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING_REQUEST";
		case CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING_RESPONSE:
			return "CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING_RESPONSE";
		case CONNECTION_STATE_CAPABILITIES_EXCHANGE_DEMAND_ACTIVE:
			return "CONNECTION_STATE_CAPABILITIES_EXCHANGE_DEMAND_ACTIVE";
		case CONNECTION_STATE_CAPABILITIES_EXCHANGE_MONITOR_LAYOUT:
			return "CONNECTION_STATE_CAPABILITIES_EXCHANGE_MONITOR_LAYOUT";
		case CONNECTION_STATE_CAPABILITIES_EXCHANGE_CONFIRM_ACTIVE:
			return "CONNECTION_STATE_CAPABILITIES_EXCHANGE_CONFIRM_ACTIVE";
		case CONNECTION_STATE_FINALIZATION_SYNC:
			return "CONNECTION_STATE_FINALIZATION_SYNC";
		case CONNECTION_STATE_FINALIZATION_COOPERATE:
			return "CONNECTION_STATE_FINALIZATION_COOPERATE";
		case CONNECTION_STATE_FINALIZATION_REQUEST_CONTROL:
			return "CONNECTION_STATE_FINALIZATION_REQUEST_CONTROL";
		case CONNECTION_STATE_FINALIZATION_PERSISTENT_KEY_LIST:
			return "CONNECTION_STATE_FINALIZATION_PERSISTENT_KEY_LIST";
		case CONNECTION_STATE_FINALIZATION_FONT_LIST:
			return "CONNECTION_STATE_FINALIZATION_FONT_LIST";
		case CONNECTION_STATE_FINALIZATION_CLIENT_SYNC:
			return "CONNECTION_STATE_FINALIZATION_CLIENT_SYNC";
		case CONNECTION_STATE_FINALIZATION_CLIENT_COOPERATE:
			return "CONNECTION_STATE_FINALIZATION_CLIENT_COOPERATE";
		case CONNECTION_STATE_FINALIZATION_CLIENT_GRANTED_CONTROL:
			return "CONNECTION_STATE_FINALIZATION_CLIENT_GRANTED_CONTROL";
		case CONNECTION_STATE_FINALIZATION_CLIENT_FONT_MAP:
			return "CONNECTION_STATE_FINALIZATION_CLIENT_FONT_MAP";
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

const char* rdp_get_state_string(const rdpRdp* rdp)
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
	                                      CHANNEL_MAX_COUNT))
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

	return freerdp_settings_set_uint32(settings, FreeRDP_ChannelCount, mcs->channelCount);
}

/* Here we are in client state CONFIRM_ACTIVE.
 *
 * This means:
 * 1. send the CONFIRM_ACTIVE PDU to the server
 * 2. register callbacks, the server can now start sending stuff
 */
state_run_t rdp_client_connect_confirm_active(rdpRdp* rdp, wStream* s)
{
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->settings);
	WINPR_ASSERT(s);

	const UINT32 width = rdp->settings->DesktopWidth;
	const UINT32 height = rdp->settings->DesktopHeight;

	if (!rdp_send_confirm_active(rdp))
		return STATE_RUN_FAILED;

	if (!input_register_client_callbacks(rdp->input))
	{
		WLog_ERR(TAG, "error registering client callbacks");
		return STATE_RUN_FAILED;
	}

	/**
	 * The server may request a different desktop size during Deactivation-Reactivation sequence.
	 * In this case, the UI should be informed and do actual window resizing at this point.
	 */
	const BOOL deactivate_reactivate =
	    rdp->was_deactivated && ((rdp->deactivated_width != rdp->settings->DesktopWidth) ||
	                             (rdp->deactivated_height != rdp->settings->DesktopHeight));
	const BOOL resolution_change =
	    ((width != rdp->settings->DesktopWidth) || (height != rdp->settings->DesktopHeight));
	if (deactivate_reactivate || resolution_change)
	{
		BOOL status = TRUE;
		IFCALLRET(rdp->update->DesktopResize, status, rdp->update->context);

		if (!status)
		{
			WLog_ERR(TAG, "client desktop resize callback failed");
			return STATE_RUN_FAILED;
		}
	}

	WINPR_ASSERT(rdp->context);
	if (freerdp_shall_disconnect_context(rdp->context))
		return STATE_RUN_SUCCESS;

	state_run_t status = STATE_RUN_SUCCESS;
	if (!rdp->settings->SupportMonitorLayoutPdu)
		status = rdp_client_connect_finalize(rdp);
	else
	{
		if (!rdp_client_transition_to_state(rdp,
		                                    CONNECTION_STATE_CAPABILITIES_EXCHANGE_MONITOR_LAYOUT))
			status = STATE_RUN_FAILED;
	}
	if (!rdp_finalize_reset_flags(rdp, FALSE))
		status = STATE_RUN_FAILED;
	return status;
}
