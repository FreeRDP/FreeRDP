/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

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
BYTE t124_02_98_oid[6] = { 0, 0, 20, 124, 0, 1 };

BYTE h221_cs_key[4] = "Duca";
BYTE h221_sc_key[4] = "McDn";

/**
 * Read a GCC Conference Create Request.\n
 * @msdn{cc240836}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_conference_create_request(wStream* s, rdpSettings* settings)
{
	UINT16 length;
	BYTE choice;
	BYTE number;
	BYTE selection;

	/* ConnectData */
	if (!per_read_choice(s, &choice))
		return FALSE;
	if (!per_read_object_identifier(s, t124_02_98_oid))
		return FALSE;

	/* ConnectData::connectPDU (OCTET_STRING) */
	if (!per_read_length(s, &length))
		return FALSE;

	/* ConnectGCCPDU */
	if (!per_read_choice(s, &choice))
		return FALSE;
	if (!per_read_selection(s, &selection))
		return FALSE;

	/* ConferenceCreateRequest::conferenceName */
	if (!per_read_numeric_string(s, 1)) /* ConferenceName::numeric */
		return FALSE;
	if (!per_read_padding(s, 1)) /* padding */
		return FALSE;

	/* UserData (SET OF SEQUENCE) */
	if (!per_read_number_of_sets(s, &number) || number != 1) /* one set of UserData */
		return FALSE;
	if (!per_read_choice(s, &choice) || choice != 0xC0) /* UserData::value present + select h221NonStandard (1) */
		return FALSE;

	/* h221NonStandard */
	if (!per_read_octet_string(s, h221_cs_key, 4, 4)) /* h221NonStandard, client-to-server H.221 key, "Duca" */
		return FALSE;

	/* userData::value (OCTET_STRING) */
	if (!per_read_length(s, &length))
		return FALSE;
	if (Stream_GetRemainingLength(s) < length)
		return FALSE;
	if (!gcc_read_client_data_blocks(s, settings, length))
		return FALSE;

	return TRUE;
}

/**
 * Write a GCC Conference Create Request.\n
 * @msdn{cc240836}
 * @param s stream
 * @param user_data client data blocks
 */

void gcc_write_conference_create_request(wStream* s, wStream* user_data)
{
	/* ConnectData */
	per_write_choice(s, 0); /* From Key select object (0) of type OBJECT_IDENTIFIER */
	per_write_object_identifier(s, t124_02_98_oid); /* ITU-T T.124 (02/98) OBJECT_IDENTIFIER */

	/* ConnectData::connectPDU (OCTET_STRING) */
	per_write_length(s, Stream_GetPosition(user_data) + 14); /* connectPDU length */

	/* ConnectGCCPDU */
	per_write_choice(s, 0); /* From ConnectGCCPDU select conferenceCreateRequest (0) of type ConferenceCreateRequest */
	per_write_selection(s, 0x08); /* select optional userData from ConferenceCreateRequest */

	/* ConferenceCreateRequest::conferenceName */
	per_write_numeric_string(s, (BYTE*)"1", 1, 1); /* ConferenceName::numeric */
	per_write_padding(s, 1); /* padding */

	/* UserData (SET OF SEQUENCE) */
	per_write_number_of_sets(s, 1); /* one set of UserData */
	per_write_choice(s, 0xC0); /* UserData::value present + select h221NonStandard (1) */

	/* h221NonStandard */
	per_write_octet_string(s, h221_cs_key, 4, 4); /* h221NonStandard, client-to-server H.221 key, "Duca" */

	/* userData::value (OCTET_STRING) */
	per_write_octet_string(s, user_data->buffer, Stream_GetPosition(user_data), 0); /* array of client data blocks */
}

BOOL gcc_read_conference_create_response(wStream* s, rdpSettings* settings)
{
	UINT16 length;
	UINT32 tag;
	UINT16 nodeID;
	BYTE result;
	BYTE choice;
	BYTE number;

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
		return FALSE;

	/* userData (OCTET_STRING) */
	per_read_length(s, &length);
	if (!gcc_read_server_data_blocks(s, settings, length))
	{
		fprintf(stderr, "gcc_read_conference_create_response: gcc_read_server_data_blocks failed\n");
		return FALSE;
	}

	return TRUE;
}

void gcc_write_conference_create_response(wStream* s, wStream* user_data)
{
	/* ConnectData */
	per_write_choice(s, 0);
	per_write_object_identifier(s, t124_02_98_oid);

	/* ConnectData::connectPDU (OCTET_STRING) */
	per_write_length(s, Stream_GetPosition(user_data) + 2);

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
	per_write_octet_string(s, user_data->buffer, Stream_GetPosition(user_data), 0); /* array of server data blocks */
}

BOOL gcc_read_client_data_blocks(wStream* s, rdpSettings* settings, int length)
{
	UINT16 type;
	UINT16 blockLength;
	int pos;

	while (length > 0)
	{
		pos = Stream_GetPosition(s);
		if(!gcc_read_user_data_header(s, &type, &blockLength))
			return FALSE;

		switch (type)
		{
			case CS_CORE:
				if (!gcc_read_client_core_data(s, settings, blockLength - 4))
					return FALSE;
				break;

			case CS_SECURITY:
				if (!gcc_read_client_security_data(s, settings, blockLength - 4))
					return FALSE;
				break;

			case CS_NET:
				if (!gcc_read_client_network_data(s, settings, blockLength - 4))
					return FALSE;
				break;

			case CS_CLUSTER:
				if (!gcc_read_client_cluster_data(s, settings, blockLength - 4))
					return FALSE;
				break;

			case CS_MONITOR:
				if (!gcc_read_client_monitor_data(s, settings, blockLength - 4))
					return FALSE;
				break;

			default:
				break;
		}

		length -= blockLength;
		Stream_SetPosition(s, pos + blockLength);
	}

	return TRUE;
}

void gcc_write_client_data_blocks(wStream* s, rdpSettings* settings)
{
	gcc_write_client_core_data(s, settings);
	gcc_write_client_cluster_data(s, settings);
	gcc_write_client_security_data(s, settings);
	gcc_write_client_network_data(s, settings);

	/* extended client data supported */

	if (settings->NegotiationFlags & EXTENDED_CLIENT_DATA_SUPPORTED)
	{
		if (!settings->SpanMonitors)
		{
			gcc_write_client_monitor_data(s, settings);
		}
	}
	else
	{
		if (settings->UseMultimon)
		{
			fprintf(stderr, "WARNING: true multi monitor support was not advertised by server!\n");

			if (settings->ForceMultimon)
			{
				fprintf(stderr, "Sending multi monitor information anyway (may break connectivity!)\n");
				gcc_write_client_monitor_data(s, settings);
			}
			else
			{
				fprintf(stderr, "Use /multimon:force to force sending multi monitor information\n");
			}
		}
	}
}

BOOL gcc_read_server_data_blocks(wStream* s, rdpSettings* settings, int length)
{
	UINT16 type;
	UINT16 offset = 0;
	UINT16 blockLength;
	BYTE* holdp;

	while (offset < length)
	{
		holdp = Stream_Pointer(s);

		if (!gcc_read_user_data_header(s, &type, &blockLength))
		{
			fprintf(stderr, "gcc_read_server_data_blocks: gcc_read_user_data_header failed\n");
			return FALSE;
		}

		switch (type)
		{
			case SC_CORE:
				if (!gcc_read_server_core_data(s, settings))
				{
					fprintf(stderr, "gcc_read_server_data_blocks: gcc_read_server_core_data failed\n");
					return FALSE;
				}
				break;

			case SC_SECURITY:
				if (!gcc_read_server_security_data(s, settings))
				{
					fprintf(stderr, "gcc_read_server_data_blocks: gcc_read_server_security_data failed\n");
					return FALSE;
				}
				break;

			case SC_NET:
				if (!gcc_read_server_network_data(s, settings))
				{
					fprintf(stderr, "gcc_read_server_data_blocks: gcc_read_server_network_data failed\n");
					return FALSE;
				}
				break;

			default:
				fprintf(stderr, "gcc_read_server_data_blocks: ignoring type=%hu\n", type);
				break;
		}
		offset += blockLength;
		Stream_Pointer(s) = holdp + blockLength;
	}

	return TRUE;
}

void gcc_write_server_data_blocks(wStream* s, rdpSettings* settings)
{
	gcc_write_server_core_data(s, settings);
	gcc_write_server_network_data(s, settings);
	gcc_write_server_security_data(s, settings);
}

BOOL gcc_read_user_data_header(wStream* s, UINT16* type, UINT16* length)
{
	if (Stream_GetRemainingLength(s) < 4)
		return FALSE;

	Stream_Read_UINT16(s, *type); /* type */
	Stream_Read_UINT16(s, *length); /* length */

	if (Stream_GetRemainingLength(s) < *length - 4)
		return FALSE;

	return TRUE;
}

/**
 * Write a user data header (TS_UD_HEADER).\n
 * @msdn{cc240509}
 * @param s stream
 * @param type data block type
 * @param length data block length
 */

void gcc_write_user_data_header(wStream* s, UINT16 type, UINT16 length)
{
	Stream_Write_UINT16(s, type); /* type */
	Stream_Write_UINT16(s, length); /* length */
}

/**
 * Read a client core data block (TS_UD_CS_CORE).\n
 * @msdn{cc240510}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_core_data(wStream* s, rdpSettings* settings, UINT16 blockLength)
{
	char* str = NULL;
	UINT32 version;
	UINT32 color_depth;
	UINT16 colorDepth = 0;
	UINT16 postBeta2ColorDepth = 0;
	UINT16 highColorDepth = 0;
	UINT16 supportedColorDepths = 0;
	UINT32 serverSelectedProtocol = 0;

	/* Length of all required fields, until imeFileName */
	if (blockLength < 128)
		return FALSE;

	Stream_Read_UINT32(s, version); /* version */
	settings->RdpVersion = (version == RDP_VERSION_4 ? 4 : 7);

	Stream_Read_UINT16(s, settings->DesktopWidth); /* DesktopWidth */
	Stream_Read_UINT16(s, settings->DesktopHeight); /* DesktopHeight */
	Stream_Read_UINT16(s, colorDepth); /* ColorDepth */
	Stream_Seek_UINT16(s); /* SASSequence (Secure Access Sequence) */
	Stream_Read_UINT32(s, settings->KeyboardLayout); /* KeyboardLayout */
	Stream_Read_UINT32(s, settings->ClientBuild); /* ClientBuild */

	/* clientName (32 bytes, null-terminated unicode, truncated to 15 characters) */
	ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s), 32 / 2, &str, 0, NULL, NULL);
	Stream_Seek(s, 32);
	sprintf_s(settings->ClientHostname, 31, "%s", str);
	settings->ClientHostname[31] = 0;
	free(str);
	str = NULL;

	Stream_Read_UINT32(s, settings->KeyboardType); /* KeyboardType */
	Stream_Read_UINT32(s, settings->KeyboardSubType); /* KeyboardSubType */
	Stream_Read_UINT32(s, settings->KeyboardFunctionKey); /* KeyboardFunctionKey */

	Stream_Seek(s, 64); /* imeFileName */

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
		Stream_Read_UINT16(s, postBeta2ColorDepth); /* postBeta2ColorDepth */
		blockLength -= 2;

		if (blockLength < 2)
			break;
		Stream_Seek_UINT16(s); /* clientProductID */
		blockLength -= 2;

		if (blockLength < 4)
			break;
		Stream_Seek_UINT32(s); /* serialNumber */
		blockLength -= 4;

		if (blockLength < 2)
			break;
		Stream_Read_UINT16(s, highColorDepth); /* highColorDepth */
		blockLength -= 2;

		if (blockLength < 2)
			break;
		Stream_Read_UINT16(s, supportedColorDepths); /* supportedColorDepths */
		blockLength -= 2;

		if (blockLength < 2)
			break;
		Stream_Read_UINT16(s, settings->EarlyCapabilityFlags); /* earlyCapabilityFlags */
		blockLength -= 2;

		if (blockLength < 64)
			break;

		ConvertFromUnicode(CP_UTF8, 0, (WCHAR*) Stream_Pointer(s), 64 / 2, &str, 0, NULL, NULL);
		Stream_Seek(s, 64);
		sprintf_s(settings->ClientProductId, 32, "%s", str);
		free(str);
		blockLength -= 64;

		if (blockLength < 1)
			break;
		Stream_Read_UINT8(s, settings->PerformanceFlags); /* connectionType */
		blockLength -= 1;

		if (blockLength < 1)
			break;
		Stream_Seek_UINT8(s); /* pad1octet */
		blockLength -= 1;

		if (blockLength < 4)
			break;
		Stream_Read_UINT32(s, serverSelectedProtocol); /* serverSelectedProtocol */
		blockLength -= 4;

		if (settings->SelectedProtocol != serverSelectedProtocol)
			return FALSE;
	} while (0);

	if (highColorDepth > 0)
	{
		if (settings->EarlyCapabilityFlags & RNS_UD_CS_WANT_32BPP_SESSION)
			color_depth = 32;
		else
			color_depth = highColorDepth;
	}
	else if (postBeta2ColorDepth > 0)
	{
		switch (postBeta2ColorDepth)
		{
			case RNS_UD_COLOR_4BPP:
				color_depth = 4;
				break;
			case RNS_UD_COLOR_8BPP:
				color_depth = 8;
				break;
			case RNS_UD_COLOR_16BPP_555:
				color_depth = 15;
				break;
			case RNS_UD_COLOR_16BPP_565:
				color_depth = 16;
				break;
			case RNS_UD_COLOR_24BPP:
				color_depth = 24;
				break;
			default:
				return FALSE;
		}
	}
	else
	{
		switch (colorDepth)
		{
			case RNS_UD_COLOR_4BPP:
				color_depth = 4;
				break;
			case RNS_UD_COLOR_8BPP:
				color_depth = 8;
				break;
			default:
				return FALSE;
		}
	}

	/*
	 * If we are in server mode, accept client's color depth only if
	 * it is smaller than ours. This is what Windows server does.
	 */
	if ((color_depth < settings->ColorDepth) || !settings->ServerMode)
		settings->ColorDepth = color_depth;

	return TRUE;
}

/**
 * Write a client core data block (TS_UD_CS_CORE).\n
 * @msdn{cc240510}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_core_data(wStream* s, rdpSettings* settings)
{
	UINT32 version;
	WCHAR* clientName = NULL;
	int clientNameLength;
	BYTE connectionType;
	UINT16 highColorDepth;
	UINT16 supportedColorDepths;
	UINT16 earlyCapabilityFlags;
	WCHAR* clientDigProductId = NULL;
	int clientDigProductIdLength;

	gcc_write_user_data_header(s, CS_CORE, 216);

	version = settings->RdpVersion >= 5 ? RDP_VERSION_5_PLUS : RDP_VERSION_4;

	clientNameLength = ConvertToUnicode(CP_UTF8, 0, settings->ClientHostname, -1, &clientName, 0);
	clientDigProductIdLength = ConvertToUnicode(CP_UTF8, 0, settings->ClientProductId, -1, &clientDigProductId, 0);

	Stream_Write_UINT32(s, version); /* Version */
	Stream_Write_UINT16(s, settings->DesktopWidth); /* DesktopWidth */
	Stream_Write_UINT16(s, settings->DesktopHeight); /* DesktopHeight */
	Stream_Write_UINT16(s, RNS_UD_COLOR_8BPP); /* ColorDepth, ignored because of postBeta2ColorDepth */
	Stream_Write_UINT16(s, RNS_UD_SAS_DEL);	/* SASSequence (Secure Access Sequence) */
	Stream_Write_UINT32(s, settings->KeyboardLayout); /* KeyboardLayout */
	Stream_Write_UINT32(s, settings->ClientBuild); /* ClientBuild */

	/* clientName (32 bytes, null-terminated unicode, truncated to 15 characters) */

	if (clientNameLength >= 16)
	{
		clientNameLength = 16;
		clientName[clientNameLength-1] = 0;
	}

	Stream_Write(s, clientName, (clientNameLength * 2));
	Stream_Zero(s, 32 - (clientNameLength * 2));
	free(clientName);

	Stream_Write_UINT32(s, settings->KeyboardType); /* KeyboardType */
	Stream_Write_UINT32(s, settings->KeyboardSubType); /* KeyboardSubType */
	Stream_Write_UINT32(s, settings->KeyboardFunctionKey); /* KeyboardFunctionKey */

	Stream_Zero(s, 64); /* imeFileName */

	Stream_Write_UINT16(s, RNS_UD_COLOR_8BPP); /* postBeta2ColorDepth */
	Stream_Write_UINT16(s, 1); /* clientProductID */
	Stream_Write_UINT32(s, 0); /* serialNumber (should be initialized to 0) */

	highColorDepth = MIN(settings->ColorDepth, 24);

	supportedColorDepths =
			RNS_UD_24BPP_SUPPORT |
			RNS_UD_16BPP_SUPPORT |
			RNS_UD_15BPP_SUPPORT;

	connectionType = settings->ConnectionType;
	earlyCapabilityFlags = RNS_UD_CS_SUPPORT_ERRINFO_PDU;

	if (settings->RemoteFxCodec)
		connectionType = CONNECTION_TYPE_LAN;

	if (connectionType != 0)
		earlyCapabilityFlags |= RNS_UD_CS_VALID_CONNECTION_TYPE;

	if (settings->ColorDepth == 32)
	{
		supportedColorDepths |= RNS_UD_32BPP_SUPPORT;
		earlyCapabilityFlags |= RNS_UD_CS_WANT_32BPP_SESSION;
	}

	Stream_Write_UINT16(s, highColorDepth); /* highColorDepth */
	Stream_Write_UINT16(s, supportedColorDepths); /* supportedColorDepths */

	Stream_Write_UINT16(s, earlyCapabilityFlags); /* earlyCapabilityFlags */

	/* clientDigProductId (64 bytes, null-terminated unicode, truncated to 31 characters) */
	if (clientDigProductIdLength >= 32)
	{
		clientDigProductIdLength = 32;
		clientDigProductId[clientDigProductIdLength-1] = 0;
	}
	Stream_Write(s, clientDigProductId, (clientDigProductIdLength * 2) );
	Stream_Zero(s, 64 - (clientDigProductIdLength * 2) );
	free(clientDigProductId);

	Stream_Write_UINT8(s, connectionType); /* connectionType */
	Stream_Write_UINT8(s, 0); /* pad1octet */

	Stream_Write_UINT32(s, settings->SelectedProtocol); /* serverSelectedProtocol */
}

BOOL gcc_read_server_core_data(wStream* s, rdpSettings* settings)
{
	UINT32 version;
	UINT32 clientRequestedProtocols;

	if(Stream_GetRemainingLength(s) < 8)
		return FALSE;
	Stream_Read_UINT32(s, version); /* version */
	Stream_Read_UINT32(s, clientRequestedProtocols); /* clientRequestedProtocols */

	if (version == RDP_VERSION_4 && settings->RdpVersion > 4)
		settings->RdpVersion = 4;
	else if (version == RDP_VERSION_5_PLUS && settings->RdpVersion < 5)
		settings->RdpVersion = 7;

	return TRUE;
}

void gcc_write_server_core_data(wStream* s, rdpSettings* settings)
{
	gcc_write_user_data_header(s, SC_CORE, 12);

	Stream_Write_UINT32(s, settings->RdpVersion == 4 ? RDP_VERSION_4 : RDP_VERSION_5_PLUS);
	Stream_Write_UINT32(s, settings->RequestedProtocols); /* clientRequestedProtocols */
}

/**
 * Read a client security data block (TS_UD_CS_SEC).\n
 * @msdn{cc240511}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_security_data(wStream* s, rdpSettings* settings, UINT16 blockLength)
{
	if (blockLength < 8)
		return FALSE;

	if (settings->DisableEncryption)
	{
		Stream_Read_UINT32(s, settings->EncryptionMethods); /* encryptionMethods */
		if (settings->EncryptionMethods == 0)
			Stream_Read_UINT32(s, settings->EncryptionMethods); /* extEncryptionMethods */
	}
	else
	{
		Stream_Seek(s, 8);
	}
	return TRUE;
}

/**
 * Write a client security data block (TS_UD_CS_SEC).\n
 * @msdn{cc240511}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_security_data(wStream* s, rdpSettings* settings)
{
	gcc_write_user_data_header(s, CS_SECURITY, 12);

	if (settings->DisableEncryption)
	{
		Stream_Write_UINT32(s, settings->EncryptionMethods); /* encryptionMethods */
		Stream_Write_UINT32(s, 0); /* extEncryptionMethods */
	}
	else
	{
		/* French locale, disable encryption */
		Stream_Write_UINT32(s, 0); /* encryptionMethods */
		Stream_Write_UINT32(s, settings->EncryptionMethods); /* extEncryptionMethods */
	}
}

BOOL gcc_read_server_security_data(wStream* s, rdpSettings* settings)
{
	BYTE* data;
	UINT32 length;

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;
	Stream_Read_UINT32(s, settings->EncryptionMethods); /* encryptionMethod */
	Stream_Read_UINT32(s, settings->EncryptionLevel); /* encryptionLevel */

	if (settings->EncryptionMethods == 0 && settings->EncryptionLevel == 0)
	{
		/* serverRandom and serverRandom must not be present */
		settings->DisableEncryption = FALSE;
		settings->EncryptionMethods = ENCRYPTION_METHOD_NONE;
		settings->EncryptionLevel = ENCRYPTION_LEVEL_NONE;
		return TRUE;
	}

	if (Stream_GetRemainingLength(s) < 8)
		return FALSE;
	Stream_Read_UINT32(s, settings->ServerRandomLength); /* serverRandomLen */
	Stream_Read_UINT32(s, settings->ServerCertificateLength); /* serverCertLen */

	if (Stream_GetRemainingLength(s) < settings->ServerRandomLength + settings->ServerCertificateLength)
		return FALSE;

	if (settings->ServerRandomLength > 0)
	{
		/* serverRandom */
		settings->ServerRandom = (BYTE*) malloc(settings->ServerRandomLength);
		Stream_Read(s, settings->ServerRandom, settings->ServerRandomLength);
	}
	else
	{
		return FALSE;
	}

	if (settings->ServerCertificateLength > 0)
	{
		/* serverCertificate */
		settings->ServerCertificate = (BYTE*) malloc(settings->ServerCertificateLength);
		Stream_Read(s, settings->ServerCertificate, settings->ServerCertificateLength);

		certificate_free(settings->RdpServerCertificate);
		settings->RdpServerCertificate = certificate_new();
		data = settings->ServerCertificate;
		length = settings->ServerCertificateLength;

		if (certificate_read_server_certificate(settings->RdpServerCertificate, data, length) < 1)
			return FALSE;
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

static const BYTE initial_signature[] =
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

/*
 * Terminal Services Signing Keys.
 * Yes, Terminal Services Private Key is publicly available.
 */

const BYTE tssk_modulus[] =
{
	0x3d, 0x3a, 0x5e, 0xbd, 0x72, 0x43, 0x3e, 0xc9,
	0x4d, 0xbb, 0xc1, 0x1e, 0x4a, 0xba, 0x5f, 0xcb,
	0x3e, 0x88, 0x20, 0x87, 0xef, 0xf5, 0xc1, 0xe2,
	0xd7, 0xb7, 0x6b, 0x9a, 0xf2, 0x52, 0x45, 0x95,
	0xce, 0x63, 0x65, 0x6b, 0x58, 0x3a, 0xfe, 0xef,
	0x7c, 0xe7, 0xbf, 0xfe, 0x3d, 0xf6, 0x5c, 0x7d,
	0x6c, 0x5e, 0x06, 0x09, 0x1a, 0xf5, 0x61, 0xbb,
	0x20, 0x93, 0x09, 0x5f, 0x05, 0x6d, 0xea, 0x87
};

const BYTE tssk_privateExponent[] =
{
	0x87, 0xa7, 0x19, 0x32, 0xda, 0x11, 0x87, 0x55,
	0x58, 0x00, 0x16, 0x16, 0x25, 0x65, 0x68, 0xf8,
	0x24, 0x3e, 0xe6, 0xfa, 0xe9, 0x67, 0x49, 0x94,
	0xcf, 0x92, 0xcc, 0x33, 0x99, 0xe8, 0x08, 0x60,
	0x17, 0x9a, 0x12, 0x9f, 0x24, 0xdd, 0xb1, 0x24,
	0x99, 0xc7, 0x3a, 0xb8, 0x0a, 0x7b, 0x0d, 0xdd,
	0x35, 0x07, 0x79, 0x17, 0x0b, 0x51, 0x9b, 0xb3,
	0xc7, 0x10, 0x01, 0x13, 0xe7, 0x3f, 0xf3, 0x5f
};

const BYTE tssk_exponent[] =
{
	0x5b, 0x7b, 0x88, 0xc0
};

void gcc_write_server_security_data(wStream* s, rdpSettings* settings)
{
	CryptoMd5 md5;
	BYTE* sigData;
	int expLen, keyLen, sigDataLen;
	BYTE encryptedSignature[TSSK_KEY_LENGTH];
	BYTE signature[sizeof(initial_signature)];
	UINT32 headerLen, serverRandomLen, serverCertLen, wPublicKeyBlobLen;

	if (!settings->DisableEncryption)
	{
		settings->EncryptionMethods = ENCRYPTION_METHOD_NONE;
		settings->EncryptionLevel = ENCRYPTION_LEVEL_NONE;
	}
	else if ((settings->EncryptionMethods & ENCRYPTION_METHOD_FIPS) != 0)
	{
		settings->EncryptionMethods = ENCRYPTION_METHOD_FIPS;
	}
	else if ((settings->EncryptionMethods & ENCRYPTION_METHOD_128BIT) != 0)
	{
		settings->EncryptionMethods = ENCRYPTION_METHOD_128BIT;
	}
	else if ((settings->EncryptionMethods & ENCRYPTION_METHOD_40BIT) != 0)
	{
		settings->EncryptionMethods = ENCRYPTION_METHOD_40BIT;
	}

	if (settings->EncryptionMethods != ENCRYPTION_METHOD_NONE)
		settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;

	headerLen = 12;
	keyLen = 0;
	wPublicKeyBlobLen = 0;
	serverRandomLen = 0;
	serverCertLen = 0;

	if (settings->EncryptionMethods != ENCRYPTION_METHOD_NONE ||
	    settings->EncryptionLevel != ENCRYPTION_LEVEL_NONE)
	{
		serverRandomLen = 32;

		keyLen = settings->RdpServerRsaKey->ModulusLength;
		expLen = sizeof(settings->RdpServerRsaKey->exponent);
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

	Stream_Write_UINT32(s, settings->EncryptionMethods); /* encryptionMethod */
	Stream_Write_UINT32(s, settings->EncryptionLevel); /* encryptionLevel */

	if (settings->EncryptionMethods == ENCRYPTION_METHOD_NONE &&
	    settings->EncryptionLevel == ENCRYPTION_LEVEL_NONE)
	{
		return;
	}

	Stream_Write_UINT32(s, serverRandomLen); /* serverRandomLen */
	Stream_Write_UINT32(s, serverCertLen); /* serverCertLen */

	settings->ServerRandomLength = serverRandomLen;
	settings->ServerRandom = (BYTE*) malloc(serverRandomLen);
	crypto_nonce(settings->ServerRandom, serverRandomLen);
	Stream_Write(s, settings->ServerRandom, serverRandomLen);

	sigData = Stream_Pointer(s);

	Stream_Write_UINT32(s, CERT_CHAIN_VERSION_1); /* dwVersion (4 bytes) */
	Stream_Write_UINT32(s, SIGNATURE_ALG_RSA); /* dwSigAlgId */
	Stream_Write_UINT32(s, KEY_EXCHANGE_ALG_RSA); /* dwKeyAlgId */
	Stream_Write_UINT16(s, BB_RSA_KEY_BLOB); /* wPublicKeyBlobType */

	Stream_Write_UINT16(s, wPublicKeyBlobLen); /* wPublicKeyBlobLen */
	Stream_Write(s, "RSA1", 4); /* magic */
	Stream_Write_UINT32(s, keyLen + 8); /* keylen */
	Stream_Write_UINT32(s, keyLen * 8); /* bitlen */
	Stream_Write_UINT32(s, keyLen - 1); /* datalen */

	Stream_Write(s, settings->RdpServerRsaKey->exponent, expLen);
	Stream_Write(s, settings->RdpServerRsaKey->Modulus, keyLen);
	Stream_Zero(s, 8);

	sigDataLen = Stream_Pointer(s) - sigData;

	Stream_Write_UINT16(s, BB_RSA_SIGNATURE_BLOB); /* wSignatureBlobType */
	Stream_Write_UINT16(s, keyLen + 8); /* wSignatureBlobLen */

	memcpy(signature, initial_signature, sizeof(initial_signature));

	md5 = crypto_md5_init();
	crypto_md5_update(md5, sigData, sigDataLen);
	crypto_md5_final(md5, signature);

	crypto_rsa_private_encrypt(signature, sizeof(signature), TSSK_KEY_LENGTH,
		tssk_modulus, tssk_privateExponent, encryptedSignature);

	Stream_Write(s, encryptedSignature, sizeof(encryptedSignature));
	Stream_Zero(s, 8);
}

/**
 * Read a client network data block (TS_UD_CS_NET).\n
 * @msdn{cc240512}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_network_data(wStream* s, rdpSettings* settings, UINT16 blockLength)
{
	int i;

	if (blockLength < 4)
		return FALSE;

	Stream_Read_UINT32(s, settings->ChannelCount); /* channelCount */
	if (blockLength < 4 + settings->ChannelCount * 12)
		return FALSE;
	if (settings->ChannelCount > 16)
		return FALSE;

	/* channelDefArray */
	for (i = 0; i < settings->ChannelCount; i++)
	{
		/* CHANNEL_DEF */
		Stream_Read(s, settings->ChannelDefArray[i].Name, 8); /* name (8 bytes) */
		Stream_Read_UINT32(s, settings->ChannelDefArray[i].options); /* options (4 bytes) */
		settings->ChannelDefArray[i].ChannelId = MCS_GLOBAL_CHANNEL_ID + 1 + i;
	}

	return TRUE;
}

/**
 * Write a client network data block (TS_UD_CS_NET).\n
 * @msdn{cc240512}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_network_data(wStream* s, rdpSettings* settings)
{
	int i;
	UINT16 length;

	if (settings->ChannelCount > 0)
	{
		length = settings->ChannelCount * 12 + 8;
		gcc_write_user_data_header(s, CS_NET, length);

		Stream_Write_UINT32(s, settings->ChannelCount); /* channelCount */

		/* channelDefArray */
		for (i = 0; i < settings->ChannelCount; i++)
		{
			/* CHANNEL_DEF */
			Stream_Write(s, settings->ChannelDefArray[i].Name, 8); /* name (8 bytes) */
			Stream_Write_UINT32(s, settings->ChannelDefArray[i].options); /* options (4 bytes) */
		}
	}
}

BOOL gcc_read_server_network_data(wStream* s, rdpSettings* settings)
{
	int i;
	UINT16 MCSChannelId;
	UINT16 channelCount;
	UINT16 channelId;

	if(Stream_GetRemainingLength(s) < 4)
		return FALSE;
	Stream_Read_UINT16(s, MCSChannelId); /* MCSChannelId */
	Stream_Read_UINT16(s, channelCount); /* channelCount */

	if (channelCount != settings->ChannelCount)
	{
		fprintf(stderr, "requested %d channels, got %d instead\n",
				settings->ChannelCount, channelCount);
	}

	if(Stream_GetRemainingLength(s) < channelCount * 2)
		return FALSE;

	for (i = 0; i < channelCount; i++)
	{
		Stream_Read_UINT16(s, channelId); /* channelId */
		settings->ChannelDefArray[i].ChannelId = channelId;
	}

	if (channelCount % 2 == 1)
		return Stream_SafeSeek(s, 2); /* padding */

	return TRUE;
}

void gcc_write_server_network_data(wStream* s, rdpSettings* settings)
{
	int i;

	gcc_write_user_data_header(s, SC_NET, 8 + settings->ChannelCount * 2 + (settings->ChannelCount % 2 == 1 ? 2 : 0));

	Stream_Write_UINT16(s, MCS_GLOBAL_CHANNEL_ID); /* MCSChannelId */
	Stream_Write_UINT16(s, settings->ChannelCount); /* channelCount */

	for (i = 0; i < settings->ChannelCount; i++)
	{
		Stream_Write_UINT16(s, settings->ChannelDefArray[i].ChannelId);
	}

	if (settings->ChannelCount % 2 == 1)
		Stream_Write_UINT16(s, 0);
}

/**
 * Read a client cluster data block (TS_UD_CS_CLUSTER).\n
 * @msdn{cc240514}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_cluster_data(wStream* s, rdpSettings* settings, UINT16 blockLength)
{
	UINT32 flags;

	if (blockLength < 4)
		return FALSE;

	Stream_Read_UINT32(s, flags); /* flags */

	if ((flags & REDIRECTED_SESSIONID_FIELD_VALID))
	{
		if(blockLength < 8)
			return FALSE;
		Stream_Read_UINT32(s, settings->RedirectedSessionId); /* redirectedSessionID */
	}

	return TRUE;
}

/**
 * Write a client cluster data block (TS_UD_CS_CLUSTER).\n
 * @msdn{cc240514}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_cluster_data(wStream* s, rdpSettings* settings)
{
	UINT32 flags;

	gcc_write_user_data_header(s, CS_CLUSTER, 12);

	flags = REDIRECTION_SUPPORTED | (REDIRECTION_VERSION4 << 2);

	if (settings->ConsoleSession || settings->RedirectedSessionId)
		flags |= REDIRECTED_SESSIONID_FIELD_VALID;

	Stream_Write_UINT32(s, flags); /* flags */
	Stream_Write_UINT32(s, settings->RedirectedSessionId); /* redirectedSessionID */
}

/**
 * Read a client monitor data block (TS_UD_CS_MONITOR).\n
 * @msdn{dd305336}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_monitor_data(wStream* s, rdpSettings* settings, UINT16 blockLength)
{
	fprintf(stderr, "CS_MONITOR\n");
	return TRUE;
}

/**
 * Write a client monitor data block (TS_UD_CS_MONITOR).\n
 * @msdn{dd305336}
 * @param s stream
 * @param settings rdp settings
 */

void gcc_write_client_monitor_data(wStream* s, rdpSettings* settings)
{
	int i;
	UINT16 length;
	UINT32 left, top, right, bottom, flags;

	if (settings->MonitorCount > 1)
	{
		length = (20 * settings->MonitorCount) + 12;
		gcc_write_user_data_header(s, CS_MONITOR, length);

		Stream_Write_UINT32(s, 0); /* flags */
		Stream_Write_UINT32(s, settings->MonitorCount); /* monitorCount */

		for (i = 0; i < settings->MonitorCount; i++)
		{
			left = settings->MonitorDefArray[i].x;
			top = settings->MonitorDefArray[i].y;
			right = settings->MonitorDefArray[i].x + settings->MonitorDefArray[i].width - 1;
			bottom = settings->MonitorDefArray[i].y + settings->MonitorDefArray[i].height - 1;
			flags = settings->MonitorDefArray[i].is_primary ? MONITOR_PRIMARY : 0;

			Stream_Write_UINT32(s, left); /* left */
			Stream_Write_UINT32(s, top); /* top */
			Stream_Write_UINT32(s, right); /* right */
			Stream_Write_UINT32(s, bottom); /* bottom */
			Stream_Write_UINT32(s, flags); /* flags */
		}
	}
}
