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

#include <freerdp/utils/print.h>

#include "gcc.h"
#include "certificate.h"

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
 * Read a GCC Conference Create Request.\n
 * @msdn{cc240836}
 * @param s stream
 * @param settings rdp settings
 */

boolean gcc_read_conference_create_request(STREAM* s, rdpSettings* settings)
{
	uint16 length;
	uint8 choice;
	uint8 number;
	uint8 selection;

	/* ConnectData */
	if (!per_read_choice(s, &choice))
		return false;
	if (!per_read_object_identifier(s, t124_02_98_oid))
		return false;

	/* ConnectData::connectPDU (OCTET_STRING) */
	if (!per_read_length(s, &length))
		return false;

	/* ConnectGCCPDU */
	if (!per_read_choice(s, &choice))
		return false;
	if (!per_read_selection(s, &selection))
		return false;

	/* ConferenceCreateRequest::conferenceName */
	if (!per_read_numeric_string(s, 1)) /* ConferenceName::numeric */
		return false;
	if (!per_read_padding(s, 1)) /* padding */
		return false;

	/* UserData (SET OF SEQUENCE) */
	if (!per_read_number_of_sets(s, &number) || number != 1) /* one set of UserData */
		return false;
	if (!per_read_choice(s, &choice) || choice != 0xC0) /* UserData::value present + select h221NonStandard (1) */
		return false;

	/* h221NonStandard */
	if (!per_read_octet_string(s, h221_cs_key, 4, 4)) /* h221NonStandard, client-to-server H.221 key, "Duca" */
		return false;

	/* userData::value (OCTET_STRING) */
	if (!per_read_length(s, &length))
		return false;
	if (stream_get_left(s) < length)
		return false;
	if (!gcc_read_client_data_blocks(s, settings, length))
		return false;

	return true;
}

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

boolean gcc_read_conference_create_response(STREAM* s, rdpSettings* settings)
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
	if (!per_read_octet_string(s, h221_sc_key, 4, 4)) /* h221NonStandard, server-to-client H.221 key, "McDn" */
		return false;

	/* userData (OCTET_STRING) */
	per_read_length(s, &length);
	if (!gcc_read_server_data_blocks(s, settings, length))
	{
		printf("gcc_read_conference_create_response: gcc_read_server_data_blocks failed\n");
		return false;
	}

	return true;
}

void gcc_write_conference_create_response(STREAM* s, STREAM* user_data)
{
	/* ConnectData */
	per_write_choice(s, 0);
	per_write_object_identifier(s, t124_02_98_oid);

	/* ConnectData::connectPDU (OCTET_STRING) */
	per_write_length(s, stream_get_length(user_data) + 2);

	/* ConnectGCCPDU */
	per_write_choice(s, 0x14);

	/* ConferenceCreateResponse::nodeID (UserID) */
	per_write_integer16(s, 0x79F3, 1001);

	/* ConferenceCreateResponse::tag (INTEGER) */
	per_write_integer(s, 1);

	/* ConferenceCreateResponse::result (ENUMERATED) */
	per_write_enumerated(s, 0, MCS_Result_enum_length);

	/* number of UserData sets */
	per_write_number_of_sets(s, 1);

	/* UserData::value present + select h221NonStandard (1) */
	per_write_choice(s, 0xC0);

	/* h221NonStandard */
	per_write_octet_string(s, h221_sc_key, 4, 4); /* h221NonStandard, server-to-client H.221 key, "McDn" */

	/* userData (OCTET_STRING) */
	per_write_octet_string(s, user_data->data, stream_get_length(user_data), 0); /* array of server data blocks */
}

boolean gcc_read_client_data_blocks(STREAM* s, rdpSettings* settings, int length)
{
	uint16 type;
	uint16 blockLength;
	int pos;

	while (length > 0)
	{
		pos = stream_get_pos(s);
		gcc_read_user_data_header(s, &type, &blockLength);

		switch (type)
		{
			case CS_CORE:
				if (!gcc_read_client_core_data(s, settings, blockLength - 4))
					return false;
				break;

			case CS_SECURITY:
				if (!gcc_read_client_security_data(s, settings, blockLength - 4))
					return false;
				break;

			case CS_NET:
				if (!gcc_read_client_network_data(s, settings, blockLength - 4))
					return false;
				break;

			case CS_CLUSTER:
				if (!gcc_read_client_cluster_data(s, settings, blockLength - 4))
					return false;
				break;

			case CS_MONITOR:
				if (!gcc_read_client_monitor_data(s, settings, blockLength - 4))
					return false;
				break;

			default:
				break;
		}

		length -= blockLength;
		stream_set_pos(s, pos + blockLength);
	}

	return true;
}

void gcc_write_client_data_blocks(STREAM* s, rdpSettings* settings)
{
	gcc_write_client_core_data(s, settings);
	gcc_write_client_cluster_data(s, settings);
	gcc_write_client_security_data(s, settings);
	gcc_write_client_network_data(s, settings);

	/* extended client data supported */

	if (settings->negotiationFlags)
		gcc_write_client_monitor_data(s, settings);
}

boolean gcc_read_server_data_blocks(STREAM* s, rdpSettings* settings, int length)
{
	uint16 type;
	uint16 offset = 0;
	uint16 blockLength;
	uint8* holdp;

	while (offset < length)
	{
		holdp = s->p;

		if (!gcc_read_user_data_header(s, &type, &blockLength))
		{
			printf("gcc_read_server_data_blocks: gcc_read_user_data_header failed\n");
			return false;
		}

		switch (type)
		{
			case SC_CORE:
				if (!gcc_read_server_core_data(s, settings))
				{
					printf("gcc_read_server_data_blocks: gcc_read_server_core_data failed\n");
					return false;
				}
				break;

			case SC_SECURITY:
				if (!gcc_read_server_security_data(s, settings))
				{
					printf("gcc_read_server_data_blocks: gcc_read_server_security_data failed\n");
					return false;
				}
				break;

			case SC_NET:
				if (!gcc_read_server_network_data(s, settings))
				{
					printf("gcc_read_server_data_blocks: gcc_read_server_network_data failed\n");
					return false;
				}
				break;

			default:
				printf("gcc_read_server_data_blocks: ignoring type=%hu\n", type);
				break;
		}
		offset += blockLength;
		s->p = holdp + blockLength;
	}

	return true;
}

void gcc_write_server_data_blocks(STREAM* s, rdpSettings* settings)
{
	gcc_write_server_core_data(s, settings);
	gcc_write_server_network_data(s, settings);
	gcc_write_server_security_data(s, settings);
}

boolean gcc_read_user_data_header(STREAM* s, uint16* type, uint16* length)
{
	stream_read_uint16(s, *type); /* type */
	stream_read_uint16(s, *length); /* length */

	if (*length < 4)
		return false;

	if (stream_get_left(s) < *length - 4)
		return false;

	return true;
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
 * Read a client core data block (TS_UD_CS_CORE).\n
 * @msdn{cc240510}
 * @param s stream
 * @param settings rdp settings
 */

boolean gcc_read_client_core_data(STREAM* s, rdpSettings* settings, uint16 blockLength)
{
	uint32 version;
	uint16 colorDepth = 0;
	uint16 postBeta2ColorDepth = 0;
	uint16 highColorDepth = 0;
	uint16 supportedColorDepths = 0;
	uint16 earlyCapabilityFlags = 0;
	uint32 serverSelectedProtocol = 0;
	char* str;

	/* Length of all required fields, until imeFileName */
	if (blockLength < 128)
		return false;

	stream_read_uint32(s, version); /* version */
	settings->rdp_version = (version == RDP_VERSION_4 ? 4 : 7);

	stream_read_uint16(s, settings->width); /* desktopWidth */
	stream_read_uint16(s, settings->height); /* desktopHeight */
	stream_read_uint16(s, colorDepth); /* colorDepth */
	stream_seek_uint16(s); /* SASSequence (Secure Access Sequence) */
	stream_read_uint32(s, settings->kbd_layout); /* keyboardLayout */
	stream_read_uint32(s, settings->client_build); /* clientBuild */
	
	/* clientName (32 bytes, null-terminated unicode, truncated to 15 characters) */
	str = freerdp_uniconv_in(settings->uniconv, stream_get_tail(s), 32);
	stream_seek(s, 32);
	snprintf(settings->client_hostname, sizeof(settings->client_hostname), "%s", str);
	xfree(str);

	stream_read_uint32(s, settings->kbd_type); /* keyboardType */
	stream_read_uint32(s, settings->kbd_subtype); /* keyboardSubType */
	stream_read_uint32(s, settings->kbd_fn_keys); /* keyboardFunctionKey */

	stream_seek(s, 64); /* imeFileName */

	blockLength -= 128;

	/**
	 * The following fields are all optional. If one field is present, all of the preceding
	 * fields MUST also be present. If one field is not present, all of the subsequent fields
	 * MUST NOT be present.
	 * We must check the bytes left before reading each field.
	 */

	do
	{
		if (blockLength < 2)
			break;
		stream_read_uint16(s, postBeta2ColorDepth); /* postBeta2ColorDepth */
		blockLength -= 2;

		if (blockLength < 2)
			break;
		stream_seek_uint16(s); /* clientProductID */
		blockLength -= 2;

		if (blockLength < 4)
			break;
		stream_seek_uint32(s); /* serialNumber */
		blockLength -= 4;

		if (blockLength < 2)
			break;
		stream_read_uint16(s, highColorDepth); /* highColorDepth */
		blockLength -= 2;

		if (blockLength < 2)
			break;
		stream_read_uint16(s, supportedColorDepths); /* supportedColorDepths */
		blockLength -= 2;

		if (blockLength < 2)
			break;
		stream_read_uint16(s, earlyCapabilityFlags); /* earlyCapabilityFlags */
		blockLength -= 2;

		if (blockLength < 64)
			break;
		str = freerdp_uniconv_in(settings->uniconv, stream_get_tail(s), 64);
		stream_seek(s, 64);
		snprintf(settings->client_product_id, sizeof(settings->client_product_id), "%s", str);
		xfree(str);
		blockLength -= 64;

		if (blockLength < 1)
			break;
		stream_read_uint8(s, settings->performance_flags); /* connectionType */
		blockLength -= 1;

		if (blockLength < 1)
			break;
		stream_seek_uint8(s); /* pad1octet */
		blockLength -= 1;

		if (blockLength < 4)
			break;
		stream_read_uint32(s, serverSelectedProtocol); /* serverSelectedProtocol */
		blockLength -= 4;

		if (settings->selected_protocol != serverSelectedProtocol)
			return false;
	} while (0);

	if (highColorDepth > 0)
		settings->color_depth = highColorDepth;
	else if (postBeta2ColorDepth > 0)
	{
		switch (postBeta2ColorDepth)
		{
			case RNS_UD_COLOR_4BPP:
				settings->color_depth = 4;
				break;
			case RNS_UD_COLOR_8BPP:
				settings->color_depth = 8;
				break;
			case RNS_UD_COLOR_16BPP_555:
				settings->color_depth = 15;
				break;
			case RNS_UD_COLOR_16BPP_565:
				settings->color_depth = 16;
				break;
			case RNS_UD_COLOR_24BPP:
				settings->color_depth = 24;
				break;
			default:
				return false;
		}
	}
	else
	{
		switch (colorDepth)
		{
			case RNS_UD_COLOR_4BPP:
				settings->color_depth = 4;
				break;
			case RNS_UD_COLOR_8BPP:
				settings->color_depth = 8;
				break;
			default:
				return false;
		}
	}

	return true;
}

/**
 * Write a client core data block (TS_UD_CS_CORE).\n
 * @msdn{cc240510}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_core_data(STREAM* s, rdpSettings* settings)
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

	connectionType = settings->connection_type;
	earlyCapabilityFlags = RNS_UD_CS_SUPPORT_ERRINFO_PDU;

	if (settings->rfx_codec)
		connectionType = CONNECTION_TYPE_LAN;

	if (connectionType != 0)
		earlyCapabilityFlags |= RNS_UD_CS_VALID_CONNECTION_TYPE;

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

boolean gcc_read_server_core_data(STREAM* s, rdpSettings* settings)
{
	uint32 version;
	uint32 clientRequestedProtocols;

	stream_read_uint32(s, version); /* version */
	stream_read_uint32(s, clientRequestedProtocols); /* clientRequestedProtocols */

	if (version == RDP_VERSION_4 && settings->rdp_version > 4)
		settings->rdp_version = 4;
	else if (version == RDP_VERSION_5_PLUS && settings->rdp_version < 5)
		settings->rdp_version = 7;

	return true;
}

void gcc_write_server_core_data(STREAM* s, rdpSettings* settings)
{
	gcc_write_user_data_header(s, SC_CORE, 12);

	stream_write_uint32(s, settings->rdp_version == 4 ? RDP_VERSION_4 : RDP_VERSION_5_PLUS);
	stream_write_uint32(s, settings->requested_protocols); /* clientRequestedProtocols */
}

/**
 * Read a client security data block (TS_UD_CS_SEC).\n
 * @msdn{cc240511}
 * @param s stream
 * @param settings rdp settings
 */

boolean gcc_read_client_security_data(STREAM* s, rdpSettings* settings, uint16 blockLength)
{
	if (blockLength < 8)
		return false;

	if (settings->encryption)
	{
		stream_read_uint32(s, settings->encryption_method); /* encryptionMethods */
		if (settings->encryption_method == 0)
			stream_read_uint32(s, settings->encryption_method); /* extEncryptionMethods */
	}
	else
	{
		stream_seek(s, 8);
	}
	return true;
}

/**
 * Write a client security data block (TS_UD_CS_SEC).\n
 * @msdn{cc240511}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_security_data(STREAM* s, rdpSettings* settings)
{
	gcc_write_user_data_header(s, CS_SECURITY, 12);

	if (settings->encryption)
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

boolean gcc_read_server_security_data(STREAM* s, rdpSettings* settings)
{
	uint8* data;
	uint32 length;
	uint32 serverRandomLen;
	uint32 serverCertLen;

	stream_read_uint32(s, settings->encryption_method); /* encryptionMethod */
	stream_read_uint32(s, settings->encryption_level); /* encryptionLevel */

	if (settings->encryption_method == 0 && settings->encryption_level == 0)
	{
		/* serverRandom and serverRandom must not be present */
		settings->encryption = false;
		settings->encryption_method = ENCRYPTION_METHOD_NONE;
		settings->encryption_level = ENCRYPTION_LEVEL_NONE;
		return true;
	}

	stream_read_uint32(s, serverRandomLen); /* serverRandomLen */
	stream_read_uint32(s, serverCertLen); /* serverCertLen */

	if (serverRandomLen > 0)
	{
		/* serverRandom */
		freerdp_blob_alloc(settings->server_random, serverRandomLen);
		stream_read(s, settings->server_random->data, serverRandomLen);
	}
	else
	{
		return false;
	}

	if (serverCertLen > 0)
	{
		/* serverCertificate */
		freerdp_blob_alloc(settings->server_certificate, serverCertLen);
		stream_read(s, settings->server_certificate->data, serverCertLen);
		certificate_free(settings->server_cert);
		settings->server_cert = certificate_new();
		data = settings->server_certificate->data;
		length = settings->server_certificate->length;

		if (!certificate_read_server_certificate(settings->server_cert, data, length))
			return false;
	}
	else
	{
		return false;
	}

	return true;
}

static const uint8 initial_signature[] =
{
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01
};

void gcc_write_server_security_data(STREAM* s, rdpSettings* settings)
{
	CryptoMd5 md5;
	uint8* sigData;
	int expLen, keyLen, sigDataLen;
	uint8 encryptedSignature[TSSK_KEY_LENGTH];
	uint8 signature[sizeof(initial_signature)];
	uint32 headerLen, serverRandomLen, serverCertLen, wPublicKeyBlobLen;

	if (!settings->encryption)
	{
		settings->encryption_method = ENCRYPTION_METHOD_NONE;
		settings->encryption_level = ENCRYPTION_LEVEL_NONE;
	}
	else if ((settings->encryption_method & ENCRYPTION_METHOD_FIPS) != 0)
	{
		settings->encryption_method = ENCRYPTION_METHOD_FIPS;
	}
	else if ((settings->encryption_method & ENCRYPTION_METHOD_128BIT) != 0)
	{
		settings->encryption_method = ENCRYPTION_METHOD_128BIT;
	}
	else if ((settings->encryption_method & ENCRYPTION_METHOD_40BIT) != 0)
	{
		settings->encryption_method = ENCRYPTION_METHOD_40BIT;
	}

	if (settings->encryption_method != ENCRYPTION_METHOD_NONE)
		settings->encryption_level = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;

	headerLen = 12;
	keyLen = 0;
	wPublicKeyBlobLen = 0;
	serverRandomLen = 0;
	serverCertLen = 0;

	if (settings->encryption_method != ENCRYPTION_METHOD_NONE ||
	    settings->encryption_level != ENCRYPTION_LEVEL_NONE)
	{
		serverRandomLen = 32;

		keyLen = settings->server_key->modulus.length;
		expLen = sizeof(settings->server_key->exponent);
		wPublicKeyBlobLen = 4; /* magic (RSA1) */
		wPublicKeyBlobLen += 4; /* keylen */
		wPublicKeyBlobLen += 4; /* bitlen */
		wPublicKeyBlobLen += 4; /* datalen */
		wPublicKeyBlobLen += expLen;
		wPublicKeyBlobLen += keyLen;
		wPublicKeyBlobLen += 8; /* 8 bytes of zero padding */

		serverCertLen = 4; /* dwVersion */
		serverCertLen += 4; /* dwSigAlgId */
		serverCertLen += 4; /* dwKeyAlgId */
		serverCertLen += 2; /* wPublicKeyBlobType */
		serverCertLen += 2; /* wPublicKeyBlobLen */
		serverCertLen += wPublicKeyBlobLen;
		serverCertLen += 2; /* wSignatureBlobType */
		serverCertLen += 2; /* wSignatureBlobLen */
		serverCertLen += sizeof(encryptedSignature); /* SignatureBlob */
		serverCertLen += 8; /* 8 bytes of zero padding */

		headerLen += sizeof(serverRandomLen);
		headerLen += sizeof(serverCertLen);
		headerLen += serverRandomLen;
		headerLen += serverCertLen;
	}

	gcc_write_user_data_header(s, SC_SECURITY, headerLen);

	stream_write_uint32(s, settings->encryption_method); /* encryptionMethod */
	stream_write_uint32(s, settings->encryption_level); /* encryptionLevel */

	if (settings->encryption_method == ENCRYPTION_METHOD_NONE &&
	    settings->encryption_level == ENCRYPTION_LEVEL_NONE)
	{
		return;
	}

	stream_write_uint32(s, serverRandomLen); /* serverRandomLen */
	stream_write_uint32(s, serverCertLen); /* serverCertLen */

	freerdp_blob_alloc(settings->server_random, serverRandomLen);
	crypto_nonce(settings->server_random->data, serverRandomLen);
	stream_write(s, settings->server_random->data, serverRandomLen);

	sigData = stream_get_tail(s);

	stream_write_uint32(s, CERT_CHAIN_VERSION_1); /* dwVersion (4 bytes) */
	stream_write_uint32(s, SIGNATURE_ALG_RSA); /* dwSigAlgId */
	stream_write_uint32(s, KEY_EXCHANGE_ALG_RSA); /* dwKeyAlgId */
	stream_write_uint16(s, BB_RSA_KEY_BLOB); /* wPublicKeyBlobType */

	stream_write_uint16(s, wPublicKeyBlobLen); /* wPublicKeyBlobLen */
	stream_write(s, "RSA1", 4); /* magic */
	stream_write_uint32(s, keyLen + 8); /* keylen */
	stream_write_uint32(s, keyLen * 8); /* bitlen */
	stream_write_uint32(s, keyLen - 1); /* datalen */

	stream_write(s, settings->server_key->exponent, expLen);
	stream_write(s, settings->server_key->modulus.data, keyLen);
	stream_write_zero(s, 8);

	sigDataLen = stream_get_tail(s) - sigData;

	stream_write_uint16(s, BB_RSA_SIGNATURE_BLOB); /* wSignatureBlobType */
	stream_write_uint16(s, keyLen + 8); /* wSignatureBlobLen */

	memcpy(signature, initial_signature, sizeof(initial_signature));

	md5 = crypto_md5_init();
	crypto_md5_update(md5, sigData, sigDataLen);
	crypto_md5_final(md5, signature);

	crypto_rsa_private_encrypt(signature, sizeof(signature), TSSK_KEY_LENGTH,
		tssk_modulus, tssk_privateExponent, encryptedSignature);

	stream_write(s, encryptedSignature, sizeof(encryptedSignature));
	stream_write_zero(s, 8);
}

/**
 * Read a client network data block (TS_UD_CS_NET).\n
 * @msdn{cc240512}
 * @param s stream
 * @param settings rdp settings
 */

boolean gcc_read_client_network_data(STREAM* s, rdpSettings* settings, uint16 blockLength)
{
	int i;

	if (blockLength < 4)
		return false;

	stream_read_uint32(s, settings->num_channels); /* channelCount */
	if (blockLength < 4 + settings->num_channels * 12)
		return false;
	if (settings->num_channels > 16)
		return false;

	/* channelDefArray */
	for (i = 0; i < settings->num_channels; i++)
	{
		/* CHANNEL_DEF */
		stream_read(s, settings->channels[i].name, 8); /* name (8 bytes) */
		stream_read_uint32(s, settings->channels[i].options); /* options (4 bytes) */
		settings->channels[i].channel_id = MCS_GLOBAL_CHANNEL_ID + 1 + i;
	}

	return true;
}

/**
 * Write a client network data block (TS_UD_CS_NET).\n
 * @msdn{cc240512}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_network_data(STREAM* s, rdpSettings* settings)
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

boolean gcc_read_server_network_data(STREAM* s, rdpSettings* settings)
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
		settings->channels[i].channel_id = channelId;
	}

	if (channelCount % 2 == 1)
		stream_seek(s, 2); /* padding */

	return true;
}

void gcc_write_server_network_data(STREAM* s, rdpSettings* settings)
{
	int i;

	gcc_write_user_data_header(s, SC_NET, 8 + settings->num_channels * 2 + (settings->num_channels % 2 == 1 ? 2 : 0));

	stream_write_uint16(s, MCS_GLOBAL_CHANNEL_ID); /* MCSChannelId */
	stream_write_uint16(s, settings->num_channels); /* channelCount */

	for (i = 0; i < settings->num_channels; i++)
	{
		stream_write_uint16(s, settings->channels[i].channel_id);
	}

	if (settings->num_channels % 2 == 1)
		stream_write_uint16(s, 0);
}

/**
 * Read a client cluster data block (TS_UD_CS_CLUSTER).\n
 * @msdn{cc240514}
 * @param s stream
 * @param settings rdp settings
 */

boolean gcc_read_client_cluster_data(STREAM* s, rdpSettings* settings, uint16 blockLength)
{
	uint32 flags;

	if (blockLength < 8)
		return false;

	stream_read_uint32(s, flags); /* flags */

	if ((flags | REDIRECTED_SESSIONID_FIELD_VALID))
		stream_read_uint32(s, settings->redirected_session_id); /* redirectedSessionID */

	return true;
}

/**
 * Write a client cluster data block (TS_UD_CS_CLUSTER).\n
 * @msdn{cc240514}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_cluster_data(STREAM* s, rdpSettings* settings)
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
 * Read a client monitor data block (TS_UD_CS_MONITOR).\n
 * @msdn{dd305336}
 * @param s stream
 * @param settings rdp settings
 */

boolean gcc_read_client_monitor_data(STREAM* s, rdpSettings* settings, uint16 blockLength)
{
	printf("CS_MONITOR\n");
	return true;
}

/**
 * Write a client monitor data block (TS_UD_CS_MONITOR).\n
 * @msdn{dd305336}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_monitor_data(STREAM* s, rdpSettings* settings)
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

