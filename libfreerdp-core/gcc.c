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
 */

/*
 * OID = 0.0.20.124.0.1
 * { itu-t(0) recommendation(0) t(20) t124(124) version(0) 1 }
 * v.1 of ITU-T Recommendation T.124 (Feb 1998): "Generic Conference Control"
 */
uint8 t124_02_98_oid[6] = { 0, 0, 20, 124, 0, 1 };

/**
 * Write a GCC Conference Create Request.\n
 * @msdn{cc240836}
 * @param s stream
 * @param user_data client data blocks
 */

void
gcc_write_create_conference_request(STREAM* s, STREAM* user_data)
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
	per_write_numeric_string(s, "1", 1, 1); /* ConferenceName::numeric */
	per_write_padding(s, 1); /* padding */

	/* UserData (SET OF SEQUENCE) */
	per_write_number_of_sets(s, 1); /* one set of UserData */
	per_write_choice(s, 0xC0); /* UserData::value present + select h221NonStandard (1) */

	/* h221NonStandard */
	per_write_octet_string(s, "Duca", 4, 4); /* h221NonStandard, client-to-server H.221 key, "Duca" */

	/* userData::value (OCTET_STRING) */
	per_write_octet_string(s, user_data->data, stream_get_length(user_data), 0); /* array of client data blocks */
}

/**
 * Write a user data header (TS_UD_HEADER).\n
 * @msdn{cc240509}
 * @param s stream
 * @param type data block type
 * @param length data block length
 */

void
gcc_write_user_data_header(STREAM* s, uint16 type, uint16 length)
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

void
gcc_write_client_core_data(STREAM* s, rdpSettings *settings)
{
	uint32 version;
	uint16 highColorDepth;
	uint16 supportedColorDepths;
	uint16 earlyCapabilityFlags;
	uint8 connectionType;

	gcc_write_user_data_header(s, CS_CORE, 216);

	version = settings->rdp_version >= 5 ? RDP_VERSION_5_PLUS : RDP_VERSION_4;

	stream_write_uint32(s, version); /* version */
	stream_write_uint16(s, settings->width); /* desktopWidth */
	stream_write_uint16(s, settings->height); /* desktopHeight */
	stream_write_uint16(s, RNS_UD_COLOR_8BPP); /* colorDepth, ignored because of postBeta2ColorDepth */
	stream_write_uint16(s, RNS_UD_SAS_DEL);	/* SASSequence (Secure Access Sequence) */
	stream_write_uint32(s, settings->kbd_layout); /* keyboardLayout */
	stream_write_uint32(s, 2600); /* clientBuild */

	stream_write_padding(s, 32); /* clientName */

	stream_write_uint32(s, settings->kbd_type); /* keyboardType */
	stream_write_uint32(s, settings->kbd_subtype); /* keyboardSubType */
	stream_write_uint32(s, settings->kbd_fn_keys); /* keyboardFunctionKey */

	stream_write_padding(s, 64); /* imeFileName */

	stream_write_uint16(s, RNS_UD_COLOR_8BPP); /* postBeta2ColorDepth */
	stream_write_uint16(s, 1); /* clientProductID */
	stream_write_uint16(s, 0); /* serialNumber (should be initialized to 0) */

	highColorDepth = MIN(settings->color_depth, 24);
	supportedColorDepths = RNS_UD_32BPP_SUPPORT | RNS_UD_24BPP_SUPPORT | RNS_UD_16BPP_SUPPORT | RNS_UD_15BPP_SUPPORT;

	stream_write_uint16(s, highColorDepth); /* highColorDepth */
	stream_write_uint16(s, supportedColorDepths); /* supportedColorDepths */

	connectionType = 0;
	earlyCapabilityFlags = RNS_UD_CS_SUPPORT_ERRINFO_PDU;

	if (settings->performance_flags == PERF_FLAG_NONE)
	{
		earlyCapabilityFlags |= RNS_UD_CS_VALID_CONNECTION_TYPE;
		connectionType = CONNECTION_TYPE_LAN;
	}

	if (settings->color_depth == 32)
		earlyCapabilityFlags |= RNS_UD_CS_WANT_32BPP_SESSION;

	stream_write_uint16(s, earlyCapabilityFlags); /* earlyCapabilityFlags */
	stream_write_padding(s, 64); /* clientDigProductId (64 bytes) */

	stream_write_uint8(s, connectionType); /* connectionType */
	stream_write_uint8(s, 0); /* pad1octet */

	stream_write_uint32(s, settings->selected_protocol); /* serverSelectedProtocol */
}

/**
 * Write a client security data block (TS_UD_CS_SEC).\n
 * @msdn{cc240511}
 * @param s stream
 * @param settings rdp settings
 */

void
gcc_write_client_security_data(STREAM* s, rdpSettings *settings)
{
	uint16 encryptionMethods;

	gcc_write_user_data_header(s, CS_SECURITY, 12);

	encryptionMethods = ENCRYPTION_40BIT_FLAG | ENCRYPTION_128BIT_FLAG;

	if (settings->encryption)
	{
		stream_write_uint32(s, encryptionMethods); /* encryptionMethods */
		stream_write_uint32(s, 0); /* extEncryptionMethods */
	}
	else
	{
		/* French locale, disable encryption */
		stream_write_uint32(s, 0); /* encryptionMethods */
		stream_write_uint32(s, encryptionMethods); /* extEncryptionMethods */
	}
}

/**
 * Write a client network data block (TS_UD_CS_NET).\n
 * @msdn{cc240512}
 * @param s stream
 * @param settings rdp settings
 */

void
gcc_write_client_network_data(STREAM* s, rdpSettings *settings)
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

/**
 * Write a client cluster data block (TS_UD_CS_CLUSTER).\n
 * @msdn{cc240514}
 * @param s stream
 * @param settings rdp settings
 */

void
gcc_write_client_cluster_data(STREAM* s, rdpSettings *settings)
{
	uint32 flags;
	uint32 redirectedSessionID;

	gcc_write_user_data_header(s, CS_CLUSTER, 12);

	flags = REDIRECTION_SUPPORTED | REDIRECTION_VERSION4;

	if (settings->console_session || settings->session_id)
		flags |= REDIRECTED_SESSIONID_FIELD_VALID;

	stream_write_uint32(s, flags); /* flags */
	stream_write_uint32(s, settings->session_id); /* redirectedSessionID */
}

/**
 * Write a client monitor data block (TS_UD_CS_MONITOR).\n
 * @msdn{dd305336}
 * @param s stream
 * @param settings rdp settings
 */

void
gcc_write_client_monitor_data(STREAM* s, rdpSettings *settings)
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
