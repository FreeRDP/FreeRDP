/**
 * FreeRDP: A Remote Desktop Protocol Client
 * T.124 Generic Conference Control (GCC)
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

#include "gcc.h"

/**
 * T.124 GCC is defined in:
 *
 * http://www.itu.int/rec/T-REC-T.124-199802-S/en
 * ITU-T T.124 (02/98): Generic Conference Control
 */

/**
 * ConnectData ::= SEQUENCE
 * {
 * 	t124Identifier	Key,
 * 	connectPDU	OCTET_STRING
 * }
 *
 * Key ::= CHOICE
 * {
 * 	object				OBJECT_IDENTIFIER,
 * 	h221NonStandard			H221NonStandardIdentifier
 * }
 *
 * ConnectGCCPDU ::= CHOICE
 * {
 * 	conferenceCreateRequest		ConferenceCreateRequest,
 * 	conferenceCreateResponse	ConferenceCreateResponse,
 * 	conferenceQueryRequest		ConferenceQueryRequest,
 * 	conferenceQueryResponse		ConferenceQueryResponse,
 * 	conferenceJoinRequest		ConferenceJoinRequest,
 *	conferenceJoinResponse		ConferenceJoinResponse,
 *	conferenceInviteRequest		ConferenceInviteRequest,
 *	conferenceInviteResponse	ConferenceInviteResponse,
 *	...
 * }
 *
 * ConferenceCreateRequest ::= SEQUENCE
 * {
 * 	conferenceName			ConferenceName,
 * 	convenerPassword		Password OPTIONAL,
 * 	password			Password OPTIONAL,
 * 	lockedConference		BOOLEAN,
 * 	listedConference		BOOLEAN,
 * 	conductibleConference		BOOLEAN,
 * 	terminationMethod		TerminationMethod,
 * 	conductorPrivileges		SET OF Privilege OPTIONAL,
 * 	conductedPrivileges		SET OF Privilege OPTIONAL,
 * 	nonConductedPrivileges		SET OF Privilege OPTIONAL,
 * 	conferenceDescription		TextString OPTIONAL,
 * 	callerIdentifier		TextString OPTIONAL,
 * 	userData			UserData OPTIONAL,
 * 	...,
 * 	conferencePriority		ConferencePriority OPTIONAL,
 * 	conferenceMode			ConferenceMode OPTIONAL
 * }
 *
 * ConferenceCreateResponse ::= SEQUENCE
 * {
 * 	nodeID				UserID,
 * 	tag				INTEGER,
 * 	result				ENUMERATED
 * 	{
 * 		success				(0),
 * 		userRejected			(1),
 * 		resourcesNotAvailable		(2),
 * 		rejectedForSymmetryBreaking	(3),
 * 		lockedConferenceNotSupported	(4)
 * 	},
 * 	userData			UserData OPTIONAL,
 * 	...
 * }
 *
 * ConferenceName ::= SEQUENCE
 * {
 * 	numeric				SimpleNumericString
 * 	text				SimpleTextString OPTIONAL,
 * 	...
 * }
 *
 * SimpleNumericString ::= NumericString (SIZE (1..255)) (FROM ("0123456789"))
 *
 * UserData ::= SET OF SEQUENCE
 * {
 * 	key				Key,
 * 	value				OCTET_STRING OPTIONAL
 * }
 *
 * H221NonStandardIdentifier ::= OCTET STRING (SIZE (4..255))
 *
 * UserID ::= DynamicChannelID
 *
 * ChannelID ::= INTEGER (1..65535)
 * StaticChannelID ::= INTEGER (1..1000)
 * DynamicChannelID ::= INTEGER (1001..65535)
 *
 */

/*
 * OID = 0.0.20.124.0.1
 * { itu-t(0) recommendation(0) t(20) t124(124) version(0) 1 }
 * v.1 of ITU-T Recommendation T.124 (Feb 1998): "Generic Conference Control"
 */
uint8 t124_02_98_oid[6] = { 0, 0, 20, 124, 0, 1 };

uint8 h221_cs_key[4] = "Duca";
uint8 h221_sc_key[4] = "McDn";

/**
 * Write a GCC Conference Create Request.\n
 * @msdn{cc240836}
 * @param s stream
 * @param user_data client data blocks
 */

void gcc_write_conference_create_request(STREAM* s, STREAM* user_data)
{
	/* ConnectData */
	per_write_choice(s, 0); /* From Key select object (0) of type OBJECT_IDENTIFIER */
	per_write_object_identifier(s, t124_02_98_oid); /* ITU-T T.124 (02/98) OBJECT_IDENTIFIER */

	/* ConnectData::connectPDU (OCTET_STRING) */
	per_write_length(s, stream_get_length(user_data) + 14); /* connectPDU length */

	/* ConnectGCCPDU */
	per_write_choice(s, 0); /* From ConnectGCCPDU select conferenceCreateRequest (0) of type ConferenceCreateRequest */
	per_write_selection(s, 0x08); /* select optional userData from ConferenceCreateRequest */

	/* ConferenceCreateRequest::conferenceName */
	per_write_numeric_string(s, (uint8*)"1", 1, 1); /* ConferenceName::numeric */
	per_write_padding(s, 1); /* padding */

	/* UserData (SET OF SEQUENCE) */
	per_write_number_of_sets(s, 1); /* one set of UserData */
	per_write_choice(s, 0xC0); /* UserData::value present + select h221NonStandard (1) */

	/* h221NonStandard */
	per_write_octet_string(s, h221_cs_key, 4, 4); /* h221NonStandard, client-to-server H.221 key, "Duca" */

	/* userData::value (OCTET_STRING) */
	per_write_octet_string(s, user_data->data, stream_get_length(user_data), 0); /* array of client data blocks */
}

void gcc_read_conference_create_response(STREAM* s, rdpSettings* settings)
{
	uint16 length;
	uint32 tag;
	uint16 nodeID;
	uint8 result;
	uint8 choice;
	uint8 number;

	/* ConnectData */
	per_read_choice(s, &choice);
	per_read_object_identifier(s, t124_02_98_oid);

	/* ConnectData::connectPDU (OCTET_STRING) */
	per_read_length(s, &length);

	/* ConnectGCCPDU */
	per_read_choice(s, &choice);

	/* ConferenceCreateResponse::nodeID (UserID) */
	per_read_integer16(s, &nodeID, 1001);

	/* ConferenceCreateResponse::tag (INTEGER) */
	per_read_integer(s, &tag);

	/* ConferenceCreateResponse::result (ENUMERATED) */
	per_read_enumerated(s, &result, MCS_Result_enum_length);

	/* number of UserData sets */
	per_read_number_of_sets(s, &number);

	/* UserData::value present + select h221NonStandard (1) */
	per_read_choice(s, &choice);

	/* h221NonStandard */
	per_read_octet_string(s, h221_sc_key, 4, 4); /* h221NonStandard, server-to-client H.221 key, "McDn" */

	/* userData (OCTET_STRING) */
	per_read_length(s, &length);
	gcc_read_server_data_blocks(s, settings, length);
}

void gcc_write_client_data_blocks(STREAM* s, rdpSettings *settings)
{
	gcc_write_client_core_data(s, settings);
	gcc_write_client_cluster_data(s, settings);
	gcc_write_client_security_data(s, settings);
	gcc_write_client_network_data(s, settings);
	gcc_write_client_monitor_data(s, settings);
}

void gcc_read_server_data_blocks(STREAM* s, rdpSettings *settings, int length)
{
	uint16 type;
	uint16 offset = 0;
	uint16 blockLength;

	while (offset < length)
	{
		gcc_read_user_data_header(s, &type, &blockLength);

		switch (type)
		{
			case SC_CORE:
				gcc_read_server_core_data(s, settings);
				break;

			case SC_SECURITY:
				gcc_read_server_security_data(s, settings);
				break;

			case SC_NET:
				gcc_read_server_network_data(s, settings);
				break;

			default:
				break;
		}

		offset += blockLength;
	}
}

void gcc_read_user_data_header(STREAM* s, uint16* type, uint16* length)
{
	stream_read_uint16(s, *type); /* type */
	stream_read_uint16(s, *length); /* length */
}

/**
 * Write a user data header (TS_UD_HEADER).\n
 * @msdn{cc240509}
 * @param s stream
 * @param type data block type
 * @param length data block length
 */

void gcc_write_user_data_header(STREAM* s, uint16 type, uint16 length)
{
	stream_write_uint16(s, type); /* type */
	stream_write_uint16(s, length); /* length */
}

/**
 * Write a client core data block (TS_UD_CS_CORE).\n
 * @msdn{cc240510}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_core_data(STREAM* s, rdpSettings *settings)
{
	uint32 version;
	char* clientName;
	size_t clientNameLength;
	uint8 connectionType;
	uint16 highColorDepth;
	uint16 supportedColorDepths;
	uint16 earlyCapabilityFlags;
	char* clientDigProductId;
	size_t clientDigProductIdLength;

	gcc_write_user_data_header(s, CS_CORE, 216);

	version = settings->rdp_version >= 5 ? RDP_VERSION_5_PLUS : RDP_VERSION_4;
	clientName = freerdp_uniconv_out(settings->uniconv, settings->client_hostname, &clientNameLength);
	clientDigProductId = freerdp_uniconv_out(settings->uniconv, settings->client_product_id, &clientDigProductIdLength);

	stream_write_uint32(s, version); /* version */
	stream_write_uint16(s, settings->width); /* desktopWidth */
	stream_write_uint16(s, settings->height); /* desktopHeight */
	stream_write_uint16(s, RNS_UD_COLOR_8BPP); /* colorDepth, ignored because of postBeta2ColorDepth */
	stream_write_uint16(s, RNS_UD_SAS_DEL);	/* SASSequence (Secure Access Sequence) */
	stream_write_uint32(s, settings->kbd_layout); /* keyboardLayout */
	stream_write_uint32(s, settings->client_build); /* clientBuild */

	/* clientName (32 bytes, null-terminated unicode, truncated to 15 characters) */
	if (clientNameLength > 30)
	{
		clientNameLength = 30;
		clientName[clientNameLength] = 0;
		clientName[clientNameLength + 1] = 0;
	}
	stream_write(s, clientName, clientNameLength + 2);
	stream_write_zero(s, 32 - clientNameLength - 2);
	xfree(clientName);

	stream_write_uint32(s, settings->kbd_type); /* keyboardType */
	stream_write_uint32(s, settings->kbd_subtype); /* keyboardSubType */
	stream_write_uint32(s, settings->kbd_fn_keys); /* keyboardFunctionKey */

	stream_write_zero(s, 64); /* imeFileName */

	stream_write_uint16(s, RNS_UD_COLOR_8BPP); /* postBeta2ColorDepth */
	stream_write_uint16(s, 1); /* clientProductID */
	stream_write_uint32(s, 0); /* serialNumber (should be initialized to 0) */

	highColorDepth = MIN(settings->color_depth, 24);

	supportedColorDepths =
			RNS_UD_24BPP_SUPPORT |
			RNS_UD_16BPP_SUPPORT |
			RNS_UD_15BPP_SUPPORT;

	connectionType = 0;
	earlyCapabilityFlags = RNS_UD_CS_SUPPORT_ERRINFO_PDU;

	if (settings->performance_flags == PERF_FLAG_NONE)
	{
		earlyCapabilityFlags |= RNS_UD_CS_VALID_CONNECTION_TYPE;
		connectionType = CONNECTION_TYPE_LAN;
	}

	if (settings->color_depth == 32)
	{
		supportedColorDepths |= RNS_UD_32BPP_SUPPORT;
		earlyCapabilityFlags |= RNS_UD_CS_WANT_32BPP_SESSION;
	}

	stream_write_uint16(s, highColorDepth); /* highColorDepth */
	stream_write_uint16(s, supportedColorDepths); /* supportedColorDepths */

	stream_write_uint16(s, earlyCapabilityFlags); /* earlyCapabilityFlags */

	/* clientDigProductId (64 bytes, null-terminated unicode, truncated to 30 characters) */
	if (clientDigProductIdLength > 62)
	{
		clientDigProductIdLength = 62;
		clientDigProductId[clientDigProductIdLength] = 0;
		clientDigProductId[clientDigProductIdLength + 1] = 0;
	}
	stream_write(s, clientDigProductId, clientDigProductIdLength + 2);
	stream_write_zero(s, 64 - clientDigProductIdLength - 2);
	xfree(clientDigProductId);

	stream_write_uint8(s, connectionType); /* connectionType */
	stream_write_uint8(s, 0); /* pad1octet */

	stream_write_uint32(s, settings->selected_protocol); /* serverSelectedProtocol */
}

void gcc_read_server_core_data(STREAM* s, rdpSettings *settings)
{
	uint32 version;
	uint32 clientRequestedProtocols;

	stream_read_uint32(s, version); /* version */
	stream_read_uint32(s, clientRequestedProtocols); /* clientRequestedProtocols */

	if (version == RDP_VERSION_4 && settings->rdp_version > 4)
		settings->rdp_version = 4;
	else if (version == RDP_VERSION_5_PLUS && settings->rdp_version < 5)
		settings->rdp_version = 7;
}

/**
 * Write a client security data block (TS_UD_CS_SEC).\n
 * @msdn{cc240511}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_security_data(STREAM* s, rdpSettings *settings)
{
	gcc_write_user_data_header(s, CS_SECURITY, 12);

	if (settings->encryption > 0)
	{
		stream_write_uint32(s, settings->encryption_method); /* encryptionMethods */
		stream_write_uint32(s, 0); /* extEncryptionMethods */
	}
	else
	{
		/* French locale, disable encryption */
		stream_write_uint32(s, 0); /* encryptionMethods */
		stream_write_uint32(s, settings->encryption_method); /* extEncryptionMethods */
	}
}

void gcc_read_server_security_data(STREAM* s, rdpSettings *settings)
{
	uint32 encryptionMethod;
	uint32 encryptionLevel;
	uint32 serverRandomLen;
	uint32 serverCertLen;

	stream_read_uint32(s, encryptionMethod); /* encryptionMethod */
	stream_read_uint32(s, encryptionLevel); /* encryptionLevel */
	stream_read_uint32(s, serverRandomLen); /* serverRandomLen */
	stream_read_uint32(s, serverCertLen); /* serverCertLen */

	if (encryptionMethod == 0 && encryptionLevel == 0)
	{
		/* serverRandom and serverRandom must not be present */
		return;
	}

	if (serverRandomLen > 0)
	{
		/* serverRandom */
		freerdp_blob_alloc(&settings->server_random, serverRandomLen);
		memcpy(settings->server_random.data, s->p, serverRandomLen);
		stream_seek(s, serverRandomLen);
	}

	if (serverCertLen > 0)
	{
		/* serverCertificate */
		freerdp_blob_alloc(&settings->server_certificate, serverCertLen);
		memcpy(settings->server_certificate.data, s->p, serverCertLen);
		stream_seek(s, serverCertLen);
	}
}

/**
 * Write a client network data block (TS_UD_CS_NET).\n
 * @msdn{cc240512}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_network_data(STREAM* s, rdpSettings *settings)
{
	int i;
	uint16 length;

	if (settings->num_channels > 0)
	{
		length = settings->num_channels * 12 + 8;
		gcc_write_user_data_header(s, CS_NET, length);

		stream_write_uint32(s, settings->num_channels); /* channelCount */

		/* channelDefArray */
		for (i = 0; i < settings->num_channels; i++)
		{
			/* CHANNEL_DEF */
			stream_write(s, settings->channels[i].name, 8); /* name (8 bytes) */
			stream_write_uint32(s, settings->channels[i].options); /* options (4 bytes) */
		}
	}
}

void gcc_read_server_network_data(STREAM* s, rdpSettings *settings)
{
	int i;
	uint16 MCSChannelId;
	uint16 channelCount;
	uint16 channelId;

	stream_read_uint16(s, MCSChannelId); /* MCSChannelId */
	stream_read_uint16(s, channelCount); /* channelCount */

	if (channelCount != settings->num_channels)
	{
		printf("requested %d channels, got %d instead\n",
				settings->num_channels, channelCount);
	}

	for (i = 0; i < channelCount; i++)
	{
		stream_read_uint16(s, channelId); /* channelId */
		settings->channels[i].chan_id = channelId;
	}

	if (channelCount % 2 == 1)
		stream_seek(s, 2); /* padding */
}

/**
 * Write a client cluster data block (TS_UD_CS_CLUSTER).\n
 * @msdn{cc240514}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_cluster_data(STREAM* s, rdpSettings *settings)
{
	uint32 flags;

	gcc_write_user_data_header(s, CS_CLUSTER, 12);

	flags = REDIRECTION_SUPPORTED | (REDIRECTION_VERSION4 << 2);

	if (settings->console_session || settings->redirected_session_id)
		flags |= REDIRECTED_SESSIONID_FIELD_VALID;

	stream_write_uint32(s, flags); /* flags */
	stream_write_uint32(s, settings->redirected_session_id); /* redirectedSessionID */
}

/**
 * Write a client monitor data block (TS_UD_CS_MONITOR).\n
 * @msdn{dd305336}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_monitor_data(STREAM* s, rdpSettings *settings)
{
	int i;
	uint16 length;
	uint32 left, top, right, bottom, flags;

	if (settings->num_monitors > 1)
	{
		length = (20 * settings->num_monitors) + 12;
		gcc_write_user_data_header(s, CS_MONITOR, length);

		stream_write_uint32(s, 0); /* flags */
		stream_write_uint32(s, settings->num_monitors); /* monitorCount */

		for (i = 0; i < settings->num_monitors; i++)
		{
			left = settings->monitors[i].x;
			top = settings->monitors[i].y;
			right = settings->monitors[i].x + settings->monitors[i].width - 1;
			bottom = settings->monitors[i].y + settings->monitors[i].height - 1;
			flags = settings->monitors[i].is_primary ? MONITOR_PRIMARY : 0;

			stream_write_uint32(s, left); /* left */
			stream_write_uint32(s, top); /* top */
			stream_write_uint32(s, right); /* right */
			stream_write_uint32(s, bottom); /* bottom */
			stream_write_uint32(s, flags); /* flags */
		}
	}
}
