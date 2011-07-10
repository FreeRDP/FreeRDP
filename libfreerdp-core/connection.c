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
 *
 */

void connection_client_connect(rdpConnection* connection)
{
	int i;
	uint16 channelId;

	connection->transport = transport_new(connection->settings);
	connection->nego = nego_new(connection->transport);

	nego_init(connection->nego);
	nego_set_target(connection->nego, connection->settings->hostname, 3389);
	nego_set_cookie(connection->nego, connection->settings->username);
	nego_set_protocols(connection->nego, 1, 1, 1);
	nego_connect(connection->nego);

	transport_connect_nla(connection->transport);

	connection->mcs = mcs_new(connection->transport);

	mcs_send_connect_initial(connection->mcs);
	mcs_recv_connect_response(connection->mcs);

	mcs_send_erect_domain_request(connection->mcs);
	mcs_send_attach_user_request(connection->mcs);
	mcs_recv_attach_user_confirm(connection->mcs);

	mcs_send_channel_join_request(connection->mcs, connection->mcs->user_id);
	mcs_recv_channel_join_confirm(connection->mcs);

	for (i = 0; i < connection->settings->num_channels; i++)
	{
		channelId = connection->settings->channels[i].chan_id;
		mcs_send_channel_join_request(connection->mcs, channelId);
		mcs_recv_channel_join_confirm(connection->mcs);
	}

	connection_send_client_info(connection);

	mcs_recv(connection->mcs);
}

void connection_write_system_time(STREAM* s, SYSTEM_TIME* system_time)
{
	stream_write_uint16(s, system_time->wYear); /* wYear, must be set to 0 */
	stream_write_uint16(s, system_time->wMonth); /* wMonth */
	stream_write_uint16(s, system_time->wDayOfWeek); /* wDayOfWeek */
	stream_write_uint16(s, system_time->wDay); /* wDay */
	stream_write_uint16(s, system_time->wHour); /* wHour */
	stream_write_uint16(s, system_time->wMinute); /* wMinute */
	stream_write_uint16(s, system_time->wSecond); /* wSecond */
	stream_write_uint16(s, system_time->wMilliseconds); /* wMilliseconds */
}

void connection_get_client_time_zone(STREAM* s, rdpSettings* settings)
{
	time_t t;
	struct tm* local_time;
	TIME_ZONE_INFORMATION* clientTimeZone;

	time(&t);
	local_time = localtime(&t);
	clientTimeZone = &settings->client_time_zone;

#if defined(sun)
	if(local_time->tm_isdst > 0)
		clientTimeZone->bias = (uint32) (altzone / 3600);
	else
		clientTimeZone->bias = (uint32) (timezone / 3600);
#elif defined(HAVE_TM_GMTOFF)
	if(local_time->tm_gmtoff >= 0)
		clientTimeZone->bias = (uint32) (local_time->tm_gmtoff / 60);
	else
		clientTimeZone->bias = (uint32) ((-1 * local_time->tm_gmtoff) / 60 + 720);
#else
	clientTimeZone->bias = 0;
#endif

	if(local_time->tm_isdst > 0)
	{
		clientTimeZone->standardBias = clientTimeZone->bias - 60;
		clientTimeZone->daylightBias = clientTimeZone->bias;
	}
	else
	{
		clientTimeZone->standardBias = clientTimeZone->bias;
		clientTimeZone->daylightBias = clientTimeZone->bias + 60;
	}

	strftime(clientTimeZone->standardName, 32, "%Z, Standard Time", local_time);
	clientTimeZone->standardName[31] = 0;
	strftime(clientTimeZone->daylightName, 32, "%Z, Summer Time", local_time);
	clientTimeZone->daylightName[31] = 0;
}

void connection_write_client_time_zone(STREAM* s, rdpSettings* settings)
{
	size_t length;
	uint8* standardName;
	uint8* daylightName;
	size_t standardNameLength;
	size_t daylightNameLength;
	TIME_ZONE_INFORMATION* clientTimeZone;

	connection_get_client_time_zone(s, settings);
	clientTimeZone = &settings->client_time_zone;

	standardName = freerdp_uniconv_out(settings->uniconv, clientTimeZone->standardName, &length);
	standardNameLength = length;

	daylightName = freerdp_uniconv_out(settings->uniconv, clientTimeZone->daylightName, &length);
	daylightNameLength = length;

	if (standardNameLength > 62)
		standardNameLength = 62;

	if (daylightNameLength > 62)
		daylightNameLength = 62;

	stream_write_uint32(s, clientTimeZone->bias); /* Bias */

	/* standardName (64 bytes) */
	stream_write(s, standardName, standardNameLength);
	stream_write_zero(s, 64 - standardNameLength);

	connection_write_system_time(s, &clientTimeZone->standardDate); /* StandardDate */
	stream_write_uint32(s, clientTimeZone->standardBias); /* StandardBias */

	/* daylightName (64 bytes) */
	stream_write(s, daylightName, daylightNameLength);
	stream_write_zero(s, 64 - daylightNameLength);

	connection_write_system_time(s, &clientTimeZone->daylightDate); /* DaylightDate */
	stream_write_uint32(s, clientTimeZone->daylightBias); /* DaylightBias */

	xfree(standardName);
	xfree(daylightName);
}

void connection_write_auto_reconnect_cookie(STREAM* s, rdpSettings* settings)
{
	ARC_CS_PRIVATE_PACKET* autoReconnectCookie;
	autoReconnectCookie = &settings->auto_reconnect_cookie;

	stream_write_uint32(s, autoReconnectCookie->cbLen); /* cbLen (4 bytes) */
	stream_write_uint32(s, autoReconnectCookie->version); /* version (4 bytes) */
	stream_write_uint32(s, autoReconnectCookie->logonId); /* LogonId (4 bytes) */
	stream_write(s, autoReconnectCookie->securityVerifier, 16); /* SecurityVerifier */
}

void connection_write_extended_info_packet(STREAM* s, rdpSettings* settings)
{
	size_t length;
	uint16 clientAddressFamily;
	uint8* clientAddress;
	uint16 cbClientAddress;
	uint8* clientDir;
	uint16 cbClientDir;
	uint16 cbAutoReconnectLen;

	clientAddressFamily = settings->ipv6 ? ADDRESS_FAMILY_INET6 : ADDRESS_FAMILY_INET;

	clientAddress = freerdp_uniconv_out(settings->uniconv, settings->ip_address, &length);
	cbClientAddress = length;

	clientDir = freerdp_uniconv_out(settings->uniconv, settings->client_dir, &length);
	cbClientDir = length;

	cbAutoReconnectLen = settings->auto_reconnect_cookie.cbLen;

	stream_write_uint16(s, clientAddressFamily); /* clientAddressFamily */

	stream_write_uint16(s, cbClientAddress + 2); /* cbClientAddress */

	if (cbClientAddress > 0)
		stream_write(s, clientAddress, cbClientAddress); /* clientAddress */
	stream_write_uint16(s, 0);

	stream_write_uint16(s, cbClientDir + 2); /* cbClientDir */

	if (cbClientDir > 0)
		stream_write(s, clientDir, cbClientDir); /* clientDir */
	stream_write_uint16(s, 0);

	connection_write_client_time_zone(s, settings); /* clientTimeZone */

	stream_write_uint32(s, 0); /* clientSessionId, should be set to 0 */
	stream_write_uint32(s, settings->performance_flags); /* performanceFlags */

	stream_write_uint16(s, cbAutoReconnectLen); /* cbAutoReconnectLen */

	if (cbAutoReconnectLen > 0)
		connection_write_auto_reconnect_cookie(s, settings); /* autoReconnectCookie */

	/* reserved1 (2 bytes) */
	/* reserved2 (2 bytes) */

	xfree(clientAddress);
	xfree(clientDir);
}

void connection_write_info_packet(STREAM* s, rdpSettings* settings)
{
	size_t length;
	uint32 flags;
	uint8* domain;
	uint16 cbDomain;
	uint8* userName;
	uint16 cbUserName;
	uint8* password;
	uint16 cbPassword;
	uint8* alternateShell;
	uint16 cbAlternateShell;
	uint8* workingDir;
	uint16 cbWorkingDir;

	flags = INFO_UNICODE |
		INFO_LOGONERRORS |
		INFO_LOGONNOTIFY |
		INFO_ENABLEWINDOWSKEY |
		RNS_INFO_AUDIOCAPTURE;

	if (settings->autologon)
		flags |= INFO_AUTOLOGON;

	if (settings->compression)
		flags |= INFO_COMPRESSION | PACKET_COMPR_TYPE_64K;

	domain = freerdp_uniconv_out(settings->uniconv, settings->domain, &length);
	cbDomain = length;

	userName = freerdp_uniconv_out(settings->uniconv, settings->username, &length);
	cbUserName = length;

	password = freerdp_uniconv_out(settings->uniconv, settings->password, &length);
	cbPassword = length;

	alternateShell = freerdp_uniconv_out(settings->uniconv, settings->shell, &length);
	cbAlternateShell = length;

	workingDir = freerdp_uniconv_out(settings->uniconv, settings->directory, &length);
	cbWorkingDir = length;

	stream_write_uint32(s, 0); /* CodePage */
	stream_write_uint32(s, flags); /* flags */

	stream_write_uint16(s, cbDomain); /* cbDomain */
	stream_write_uint16(s, cbUserName); /* cbUserName */
	stream_write_uint16(s, cbPassword); /* cbPassword */
	stream_write_uint16(s, cbAlternateShell); /* cbAlternateShell */
	stream_write_uint16(s, cbWorkingDir); /* cbWorkingDir */

	if (cbDomain > 0)
		stream_write(s, domain, cbDomain);
	stream_write_uint16(s, 0);

	if (cbUserName > 0)
		stream_write(s, userName, cbUserName);
	stream_write_uint16(s, 0);

	if (cbPassword > 0)
		stream_write(s, password, cbPassword);
	stream_write_uint16(s, 0);

	if (cbAlternateShell > 0)
		stream_write(s, alternateShell, cbAlternateShell);
	stream_write_uint16(s, 0);

	if (cbWorkingDir > 0)
		stream_write(s, workingDir, cbWorkingDir);
	stream_write_uint16(s, 0);

	xfree(domain);
	xfree(userName);
	xfree(password);
	xfree(alternateShell);
	xfree(workingDir);

	if (settings->rdp_version >= 5)
		connection_write_extended_info_packet(s, settings); /* extraInfo */
}

void connection_send_client_info(rdpConnection* connection)
{
	STREAM* s;
	int length;
	s = stream_new(1024);

	uint8 *bm, *em;

	stream_get_mark(s, bm);
	stream_seek(s, 15);

	/* security header */
	stream_write_uint16(s, SEC_INFO_PKT); /* flags */
	stream_write_uint16(s, 0); /* flagsHi */

	connection_write_info_packet(s, connection->settings);

	stream_get_mark(s, em);
	length = (em - bm);
	stream_set_mark(s, bm);

	tpkt_write_header(s, length);
	tpdu_write_data(s);
	per_write_choice(s, DomainMCSPDU_SendDataRequest << 2);
	per_write_integer16(s, connection->mcs->user_id, MCS_BASE_CHANNEL_ID); /* initiator */
	per_write_integer16(s, MCS_GLOBAL_CHANNEL_ID, 0); /* channelId */
	stream_write_uint8(s, 0x70); /* dataPriority + segmentation */
	per_write_length(s, length - 15); /* userData (OCTET_STRING) */

	stream_set_mark(s, em);

	tls_write(connection->transport->tls, s->data, stream_get_length(s));
}

/**
 * Instantiate new connection module.
 * @return new connection module
 */

rdpConnection* connection_new(rdpSettings* settings)
{
	rdpConnection* connection;

	connection = (rdpConnection*) xzalloc(sizeof(rdpConnection));

	if (connection != NULL)
	{
		connection->settings = settings;
	}

	return connection;
}

/**
 * Free connection module.
 * @param connection connect module to be freed
 */

void connection_free(rdpConnection* connection)
{
	if (connection != NULL)
	{
		xfree(connection);
	}
}
