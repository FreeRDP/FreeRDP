/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * T.124 Generic Conference Control (GCC)
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2014 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/assert.h>

#include <freerdp/log.h>

#include "gcc.h"
#include "certificate.h"

#define TAG FREERDP_TAG("core.gcc")

static BOOL gcc_read_client_cluster_data(wStream* s, rdpMcs* mcs, UINT16 blockLength);
static BOOL gcc_read_client_core_data(wStream* s, rdpMcs* mcs, UINT16 blockLength);
static BOOL gcc_read_client_data_blocks(wStream* s, rdpMcs* mcs, UINT16 length);
static BOOL gcc_read_server_data_blocks(wStream* s, rdpMcs* mcs, UINT16 length);
static BOOL gcc_read_user_data_header(wStream* s, UINT16* type, UINT16* length);
static BOOL gcc_write_user_data_header(wStream* s, UINT16 type, UINT16 length);

static BOOL gcc_write_client_core_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_read_server_core_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_server_core_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_client_security_data(wStream* s, rdpMcs* mcs, UINT16 blockLength);
static BOOL gcc_write_client_security_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_server_security_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_server_security_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_read_client_network_data(wStream* s, rdpMcs* mcs, UINT16 blockLength);
static BOOL gcc_write_client_network_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_server_network_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_server_network_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_write_client_cluster_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_client_monitor_data(wStream* s, rdpMcs* mcs, UINT16 blockLength);
static BOOL gcc_write_client_monitor_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_client_monitor_extended_data(wStream* s, rdpMcs* mcs, UINT16 blockLength);
static BOOL gcc_write_client_monitor_extended_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_client_message_channel_data(wStream* s, rdpMcs* mcs, UINT16 blockLength);
static BOOL gcc_write_client_message_channel_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_server_message_channel_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_server_message_channel_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_client_multitransport_channel_data(wStream* s, rdpMcs* mcs,
                                                        UINT16 blockLength);
static BOOL gcc_write_client_multitransport_channel_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_server_multitransport_channel_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_server_multitransport_channel_data(wStream* s, const rdpMcs* mcs);

static DWORD rdp_version_common(DWORD serverVersion, DWORD clientVersion)
{
	DWORD version = MIN(serverVersion, clientVersion);

	switch (version)
	{
		case RDP_VERSION_4:
		case RDP_VERSION_5_PLUS:
		case RDP_VERSION_10_0:
		case RDP_VERSION_10_1:
		case RDP_VERSION_10_2:
		case RDP_VERSION_10_3:
		case RDP_VERSION_10_4:
		case RDP_VERSION_10_5:
		case RDP_VERSION_10_6:
		case RDP_VERSION_10_7:
			return version;

		default:
			WLog_ERR(TAG, "Invalid client [%" PRId32 "] and server [%" PRId32 "] versions",
			         serverVersion, clientVersion);
			return version;
	}
}

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
static const BYTE t124_02_98_oid[6] = { 0, 0, 20, 124, 0, 1 };

static const BYTE h221_cs_key[4] = "Duca";
static const BYTE h221_sc_key[4] = "McDn";

/**
 * Read a GCC Conference Create Request.\n
 * @msdn{cc240836}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_conference_create_request(wStream* s, rdpMcs* mcs)
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

	if (!per_read_choice(s, &choice) ||
	    choice != 0xC0) /* UserData::value present + select h221NonStandard (1) */
		return FALSE;

	/* h221NonStandard */
	if (!per_read_octet_string(s, h221_cs_key, 4,
	                           4)) /* h221NonStandard, client-to-server H.221 key, "Duca" */
		return FALSE;

	/* userData::value (OCTET_STRING) */
	if (!per_read_length(s, &length))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, length))
		return FALSE;

	if (!gcc_read_client_data_blocks(s, mcs, length))
		return FALSE;

	return TRUE;
}

/**
 * Write a GCC Conference Create Request.\n
 * @msdn{cc240836}
 * @param s stream
 * @param user_data client data blocks
 */

BOOL gcc_write_conference_create_request(wStream* s, wStream* userData)
{
	/* ConnectData */
	if (!per_write_choice(s, 0)) /* From Key select object (0) of type OBJECT_IDENTIFIER */
		return FALSE;
	if (!per_write_object_identifier(s, t124_02_98_oid)) /* ITU-T T.124 (02/98) OBJECT_IDENTIFIER */
		return FALSE;
	/* ConnectData::connectPDU (OCTET_STRING) */
	if (!per_write_length(s, Stream_GetPosition(userData) + 14)) /* connectPDU length */
		return FALSE;
	/* ConnectGCCPDU */
	if (!per_write_choice(s, 0)) /* From ConnectGCCPDU select conferenceCreateRequest (0) of type
	                                 ConferenceCreateRequest */
		return FALSE;
	if (!per_write_selection(s, 0x08)) /* select optional userData from ConferenceCreateRequest */
		return FALSE;
	/* ConferenceCreateRequest::conferenceName */
	if (!per_write_numeric_string(s, (BYTE*)"1", 1, 1)) /* ConferenceName::numeric */
		return FALSE;
	if (!per_write_padding(s, 1)) /* padding */
		return FALSE;
	/* UserData (SET OF SEQUENCE) */
	if (!per_write_number_of_sets(s, 1)) /* one set of UserData */
		return FALSE;
	if (!per_write_choice(s, 0xC0)) /* UserData::value present + select h221NonStandard (1) */
		return FALSE;
	/* h221NonStandard */
	if (!per_write_octet_string(s, h221_cs_key, 4,
	                            4)) /* h221NonStandard, client-to-server H.221 key, "Duca" */
		return FALSE;
	/* userData::value (OCTET_STRING) */
	return per_write_octet_string(s, Stream_Buffer(userData), Stream_GetPosition(userData),
	                              0); /* array of client data blocks */
}

BOOL gcc_read_conference_create_response(wStream* s, rdpMcs* mcs)
{
	UINT16 length;
	UINT32 tag;
	UINT16 nodeID;
	BYTE result;
	BYTE choice;
	BYTE number;

	/* ConnectData */
	if (!per_read_choice(s, &choice) || !per_read_object_identifier(s, t124_02_98_oid))
		return FALSE;

	/* ConnectData::connectPDU (OCTET_STRING) */
	if (!per_read_length(s, &length))
		return FALSE;

	/* ConnectGCCPDU */
	if (!per_read_choice(s, &choice))
		return FALSE;

	/* ConferenceCreateResponse::nodeID (UserID) */
	if (!per_read_integer16(s, &nodeID, 1001))
		return FALSE;

	/* ConferenceCreateResponse::tag (INTEGER) */
	if (!per_read_integer(s, &tag))
		return FALSE;

	/* ConferenceCreateResponse::result (ENUMERATED) */
	if (!per_read_enumerated(s, &result, MCS_Result_enum_length))
		return FALSE;

	/* number of UserData sets */
	if (!per_read_number_of_sets(s, &number))
		return FALSE;

	/* UserData::value present + select h221NonStandard (1) */
	if (!per_read_choice(s, &choice))
		return FALSE;

	/* h221NonStandard */
	if (!per_read_octet_string(s, h221_sc_key, 4,
	                           4)) /* h221NonStandard, server-to-client H.221 key, "McDn" */
		return FALSE;

	/* userData (OCTET_STRING) */
	if (!per_read_length(s, &length))
		return FALSE;

	if (!gcc_read_server_data_blocks(s, mcs, length))
	{
		WLog_ERR(TAG, "gcc_read_conference_create_response: gcc_read_server_data_blocks failed");
		return FALSE;
	}

	return TRUE;
}

BOOL gcc_write_conference_create_response(wStream* s, wStream* userData)
{
	/* ConnectData */
	if (!per_write_choice(s, 0))
		return FALSE;
	if (!per_write_object_identifier(s, t124_02_98_oid))
		return FALSE;
	/* ConnectData::connectPDU (OCTET_STRING) */
	/* This length MUST be ignored by the client according to [MS-RDPBCGR] */
	if (!per_write_length(s, 0x2A))
		return FALSE;
	/* ConnectGCCPDU */
	if (!per_write_choice(s, 0x14))
		return FALSE;
	/* ConferenceCreateResponse::nodeID (UserID) */
	if (!per_write_integer16(s, 0x79F3, 1001))
		return FALSE;
	/* ConferenceCreateResponse::tag (INTEGER) */
	if (!per_write_integer(s, 1))
		return FALSE;
	/* ConferenceCreateResponse::result (ENUMERATED) */
	if (!per_write_enumerated(s, 0, MCS_Result_enum_length))
		return FALSE;
	/* number of UserData sets */
	if (!per_write_number_of_sets(s, 1))
		return FALSE;
	/* UserData::value present + select h221NonStandard (1) */
	if (!per_write_choice(s, 0xC0))
		return FALSE;
	/* h221NonStandard */
	if (!per_write_octet_string(s, h221_sc_key, 4,
	                            4)) /* h221NonStandard, server-to-client H.221 key, "McDn" */
		return FALSE;
	/* userData (OCTET_STRING) */
	return per_write_octet_string(s, Stream_Buffer(userData), Stream_GetPosition(userData),
	                              0); /* array of server data blocks */
}

BOOL gcc_read_client_data_blocks(wStream* s, rdpMcs* mcs, UINT16 length)
{
	UINT16 type;
	UINT16 blockLength;
	size_t begPos, endPos;

	while (length > 0)
	{
		begPos = Stream_GetPosition(s);

		if (!gcc_read_user_data_header(s, &type, &blockLength))
			return FALSE;

		if (!Stream_CheckAndLogRequiredLength(TAG, s, (size_t)(blockLength - 4)))
			return FALSE;

		switch (type)
		{
			case CS_CORE:
				if (!gcc_read_client_core_data(s, mcs, blockLength - 4))
					return FALSE;

				break;

			case CS_SECURITY:
				if (!gcc_read_client_security_data(s, mcs, blockLength - 4))
					return FALSE;

				break;

			case CS_NET:
				if (!gcc_read_client_network_data(s, mcs, blockLength - 4))
					return FALSE;

				break;

			case CS_CLUSTER:
				if (!gcc_read_client_cluster_data(s, mcs, blockLength - 4))
					return FALSE;

				break;

			case CS_MONITOR:
				if (!gcc_read_client_monitor_data(s, mcs, blockLength - 4))
					return FALSE;

				break;

			case CS_MCS_MSGCHANNEL:
				if (!gcc_read_client_message_channel_data(s, mcs, blockLength - 4))
					return FALSE;

				break;

			case CS_MONITOR_EX:
				if (!gcc_read_client_monitor_extended_data(s, mcs, blockLength - 4))
					return FALSE;

				break;

			case 0xC009:
			case CS_MULTITRANSPORT:
				if (!gcc_read_client_multitransport_channel_data(s, mcs, blockLength - 4))
					return FALSE;

				break;

			default:
				WLog_ERR(TAG, "Unknown GCC client data block: 0x%04" PRIX16 "", type);
				Stream_Seek(s, blockLength - 4);
				break;
		}

		endPos = Stream_GetPosition(s);

		if (endPos != (begPos + blockLength))
		{
			WLog_ERR(TAG,
			         "Error parsing GCC client data block 0x%04" PRIX16
			         ": Actual Offset: %d Expected Offset: %d",
			         type, endPos, begPos + blockLength);
		}

		length -= blockLength;
		Stream_SetPosition(s, begPos + blockLength);
	}

	return TRUE;
}

BOOL gcc_write_client_data_blocks(wStream* s, rdpMcs* mcs)
{
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!gcc_write_client_core_data(s, mcs) || !gcc_write_client_cluster_data(s, mcs) ||
	    !gcc_write_client_security_data(s, mcs) || !gcc_write_client_network_data(s, mcs))
		return FALSE;

	/* extended client data supported */

	if (settings->NegotiationFlags & EXTENDED_CLIENT_DATA_SUPPORTED)
	{
		if (settings->UseMultimon && !settings->SpanMonitors)
		{
			if (!gcc_write_client_monitor_data(s, mcs) ||
			    !gcc_write_client_monitor_extended_data(s, mcs))
				return FALSE;
		}

		if (!gcc_write_client_message_channel_data(s, mcs) ||
		    !gcc_write_client_multitransport_channel_data(s, mcs))
			return FALSE;
	}
	else
	{
		if (settings->UseMultimon && !settings->SpanMonitors)
		{
			WLog_ERR(TAG, "WARNING: true multi monitor support was not advertised by server!");

			if (settings->ForceMultimon)
			{
				WLog_ERR(TAG, "Sending multi monitor information anyway (may break connectivity!)");
				if (!gcc_write_client_monitor_data(s, mcs) ||
				    !gcc_write_client_monitor_extended_data(s, mcs))
					return FALSE;
			}
			else
			{
				WLog_ERR(TAG, "Use /multimon:force to force sending multi monitor information");
			}
		}
	}
	return TRUE;
}

BOOL gcc_read_server_data_blocks(wStream* s, rdpMcs* mcs, UINT16 length)
{
	UINT16 type;
	UINT16 offset = 0;
	UINT16 blockLength;
	BYTE* holdp;

	while (offset < length)
	{
		size_t rest;
		wStream subbuffer;
		wStream* sub;

		if (!gcc_read_user_data_header(s, &type, &blockLength))
		{
			WLog_ERR(TAG, "gcc_read_server_data_blocks: gcc_read_user_data_header failed");
			return FALSE;
		}
		holdp = Stream_Pointer(s);
		sub = Stream_StaticInit(&subbuffer, holdp, blockLength - 4);
		if (!Stream_SafeSeek(s, blockLength - 4))
		{
			WLog_ERR(TAG, "gcc_read_server_data_blocks: stream too short");
			return FALSE;
		}
		offset += blockLength;

		switch (type)
		{
			case SC_CORE:
				if (!gcc_read_server_core_data(sub, mcs))
				{
					WLog_ERR(TAG, "gcc_read_server_data_blocks: gcc_read_server_core_data failed");
					return FALSE;
				}

				break;

			case SC_SECURITY:
				if (!gcc_read_server_security_data(sub, mcs))
				{
					WLog_ERR(TAG,
					         "gcc_read_server_data_blocks: gcc_read_server_security_data failed");
					return FALSE;
				}

				break;

			case SC_NET:
				if (!gcc_read_server_network_data(sub, mcs))
				{
					WLog_ERR(TAG,
					         "gcc_read_server_data_blocks: gcc_read_server_network_data failed");
					return FALSE;
				}

				break;

			case SC_MCS_MSGCHANNEL:
				if (!gcc_read_server_message_channel_data(sub, mcs))
				{
					WLog_ERR(
					    TAG,
					    "gcc_read_server_data_blocks: gcc_read_server_message_channel_data failed");
					return FALSE;
				}

				break;

			case SC_MULTITRANSPORT:
				if (!gcc_read_server_multitransport_channel_data(sub, mcs))
				{
					WLog_ERR(TAG, "gcc_read_server_data_blocks: "
					              "gcc_read_server_multitransport_channel_data failed");
					return FALSE;
				}

				break;

			default:
				WLog_ERR(TAG, "gcc_read_server_data_blocks: ignoring type=%" PRIu16 "", type);
				break;
		}

		rest = Stream_GetRemainingLength(sub);
		if (rest > 0)
		{
			WLog_WARN(
			    TAG, "gcc_read_server_data_blocks: ignoring %" PRIuz " bytes with type=%" PRIu16 "",
			    rest, type);
		}
	}

	return TRUE;
}

BOOL gcc_write_server_data_blocks(wStream* s, rdpMcs* mcs)
{
	return gcc_write_server_core_data(s, mcs) &&          /* serverCoreData */
	       gcc_write_server_network_data(s, mcs) &&       /* serverNetworkData */
	       gcc_write_server_security_data(s, mcs) &&      /* serverSecurityData */
	       gcc_write_server_message_channel_data(s, mcs); /* serverMessageChannelData */
	/* TODO: Send these GCC data blocks only when the client sent them */
	// gcc_write_server_multitransport_channel_data(s, settings); /* serverMultitransportChannelData
	// */
}

BOOL gcc_read_user_data_header(wStream* s, UINT16* type, UINT16* length)
{
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, *type);   /* type */
	Stream_Read_UINT16(s, *length); /* length */

	if ((*length < 4) || (!Stream_CheckAndLogRequiredLength(TAG, s, (size_t)(*length - 4))))
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

BOOL gcc_write_user_data_header(wStream* s, UINT16 type, UINT16 length)
{
	if (!Stream_EnsureRemainingCapacity(s, 4 + length))
		return FALSE;
	Stream_Write_UINT16(s, type);   /* type */
	Stream_Write_UINT16(s, length); /* length */
	return TRUE;
}

/**
 * Read a client core data block (TS_UD_CS_CORE).\n
 * @msdn{cc240510}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_core_data(wStream* s, rdpMcs* mcs, UINT16 blockLength)
{
	char strbuffer[65] = { 0 };
	char* strptr = strbuffer;
	UINT32 version;
	BYTE connectionType = 0;
	UINT32 clientColorDepth;
	UINT16 colorDepth = 0;
	UINT16 postBeta2ColorDepth = 0;
	UINT16 highColorDepth = 0;
	UINT16 supportedColorDepths = 0;
	UINT32 serverSelectedProtocol = 0;
	UINT16 earlyCapabilityFlags = 0;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	/* Length of all required fields, until imeFileName */
	if (blockLength < 128)
		return FALSE;

	Stream_Read_UINT32(s, version); /* version (4 bytes) */
	settings->RdpVersion = rdp_version_common(version, settings->RdpVersion);
	Stream_Read_UINT16(s, settings->DesktopWidth);  /* DesktopWidth (2 bytes) */
	Stream_Read_UINT16(s, settings->DesktopHeight); /* DesktopHeight (2 bytes) */
	Stream_Read_UINT16(s, colorDepth);              /* ColorDepth (2 bytes) */
	Stream_Seek_UINT16(s); /* SASSequence (Secure Access Sequence) (2 bytes) */
	Stream_Read_UINT32(s, settings->KeyboardLayout); /* KeyboardLayout (4 bytes) */
	Stream_Read_UINT32(s, settings->ClientBuild);    /* ClientBuild (4 bytes) */

	/* clientName (32 bytes, null-terminated unicode, truncated to 15 characters) */
	if (ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)Stream_Pointer(s), 32 / 2, &strptr,
	                       ARRAYSIZE(strbuffer), NULL, NULL) < 1)
	{
		WLog_ERR(TAG, "failed to convert client host name");
		return FALSE;
	}

	Stream_Seek(s, 32);
	freerdp_settings_set_string(settings, FreeRDP_ClientHostname, strbuffer);
	Stream_Read_UINT32(s, settings->KeyboardType);        /* KeyboardType (4 bytes) */
	Stream_Read_UINT32(s, settings->KeyboardSubType);     /* KeyboardSubType (4 bytes) */
	Stream_Read_UINT32(s, settings->KeyboardFunctionKey); /* KeyboardFunctionKey (4 bytes) */
	Stream_Seek(s, 64);                                   /* imeFileName (64 bytes) */
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

		Stream_Read_UINT16(s, postBeta2ColorDepth); /* postBeta2ColorDepth (2 bytes) */
		blockLength -= 2;

		if (blockLength < 2)
			break;

		Stream_Seek_UINT16(s); /* clientProductID (2 bytes) */
		blockLength -= 2;

		if (blockLength < 4)
			break;

		Stream_Seek_UINT32(s); /* serialNumber (4 bytes) */
		blockLength -= 4;

		if (blockLength < 2)
			break;

		Stream_Read_UINT16(s, highColorDepth); /* highColorDepth (2 bytes) */
		blockLength -= 2;

		if (blockLength < 2)
			break;

		Stream_Read_UINT16(s, supportedColorDepths); /* supportedColorDepths (2 bytes) */
		blockLength -= 2;

		if (blockLength < 2)
			break;

		Stream_Read_UINT16(s, earlyCapabilityFlags); /* earlyCapabilityFlags (2 bytes) */
		settings->EarlyCapabilityFlags = (UINT32)earlyCapabilityFlags;
		blockLength -= 2;

		/* clientDigProductId (64 bytes): Contains a value that uniquely identifies the client */

		if (blockLength < 64)
			break;

		if (ConvertFromUnicode(CP_UTF8, 0, (WCHAR*)Stream_Pointer(s), 64 / 2, &strptr,
		                       ARRAYSIZE(strbuffer), NULL, NULL) < 1)
		{
			WLog_ERR(TAG, "failed to convert the client product identifier");
			return FALSE;
		}

		Stream_Seek(s, 64); /* clientDigProductId (64 bytes) */
		freerdp_settings_set_string(settings, FreeRDP_ClientProductId, strbuffer);
		blockLength -= 64;

		if (blockLength < 1)
			break;

		Stream_Read_UINT8(s, connectionType); /* connectionType (1 byte) */
		blockLength -= 1;

		if (blockLength < 1)
			break;

		Stream_Seek_UINT8(s); /* pad1octet (1 byte) */
		blockLength -= 1;

		if (blockLength < 4)
			break;

		Stream_Read_UINT32(s, serverSelectedProtocol); /* serverSelectedProtocol (4 bytes) */
		blockLength -= 4;

		if (blockLength < 4)
			break;

		Stream_Read_UINT32(s, settings->DesktopPhysicalWidth); /* desktopPhysicalWidth (4 bytes) */
		blockLength -= 4;

		if (blockLength < 4)
			break;

		Stream_Read_UINT32(s,
		                   settings->DesktopPhysicalHeight); /* desktopPhysicalHeight (4 bytes) */
		blockLength -= 4;

		if (blockLength < 2)
			break;

		Stream_Read_UINT16(s, settings->DesktopOrientation); /* desktopOrientation (2 bytes) */
		blockLength -= 2;

		if (blockLength < 4)
			break;

		Stream_Read_UINT32(s, settings->DesktopScaleFactor); /* desktopScaleFactor (4 bytes) */
		blockLength -= 4;

		if (blockLength < 4)
			break;

		Stream_Read_UINT32(s, settings->DeviceScaleFactor); /* deviceScaleFactor (4 bytes) */

		if (settings->SelectedProtocol != serverSelectedProtocol)
			return FALSE;
	} while (0);

	if (highColorDepth > 0)
	{
		if (earlyCapabilityFlags & RNS_UD_CS_WANT_32BPP_SESSION)
			clientColorDepth = 32;
		else
			clientColorDepth = highColorDepth;
	}
	else if (postBeta2ColorDepth > 0)
	{
		switch (postBeta2ColorDepth)
		{
			case RNS_UD_COLOR_4BPP:
				clientColorDepth = 4;
				break;

			case RNS_UD_COLOR_8BPP:
				clientColorDepth = 8;
				break;

			case RNS_UD_COLOR_16BPP_555:
				clientColorDepth = 15;
				break;

			case RNS_UD_COLOR_16BPP_565:
				clientColorDepth = 16;
				break;

			case RNS_UD_COLOR_24BPP:
				clientColorDepth = 24;
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
				clientColorDepth = 4;
				break;

			case RNS_UD_COLOR_8BPP:
				clientColorDepth = 8;
				break;

			default:
				return FALSE;
		}
	}

	/*
	 * If we are in server mode, accept client's color depth only if
	 * it is smaller than ours. This is what Windows server does.
	 */
	if ((clientColorDepth < settings->ColorDepth) || !settings->ServerMode)
		settings->ColorDepth = clientColorDepth;

	if (settings->NetworkAutoDetect)
		settings->NetworkAutoDetect =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_NETCHAR_AUTODETECT) ? TRUE : FALSE;

	if (settings->SupportHeartbeatPdu)
		settings->SupportHeartbeatPdu =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_HEARTBEAT_PDU) ? TRUE : FALSE;

	if (settings->SupportGraphicsPipeline)
		settings->SupportGraphicsPipeline =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL) ? TRUE : FALSE;

	if (settings->SupportDynamicTimeZone)
		settings->SupportDynamicTimeZone =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_DYNAMIC_TIME_ZONE) ? TRUE : FALSE;

	if (settings->SupportMonitorLayoutPdu)
		settings->SupportMonitorLayoutPdu =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU) ? TRUE : FALSE;

	if (settings->SupportStatusInfoPdu)
		settings->SupportStatusInfoPdu =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_STATUSINFO_PDU) ? TRUE : FALSE;

	if (!(earlyCapabilityFlags & RNS_UD_CS_VALID_CONNECTION_TYPE))
		connectionType = 0;

	settings->SupportErrorInfoPdu = earlyCapabilityFlags & RNS_UD_CS_SUPPORT_ERRINFO_PDU;
	settings->ConnectionType = connectionType;
	return TRUE;
}

/**
 * Write a client core data block (TS_UD_CS_CORE).\n
 * @msdn{cc240510}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_write_client_core_data(wStream* s, rdpMcs* mcs)
{
	WCHAR* clientName = NULL;
	int clientNameLength;
	BYTE connectionType;
	UINT16 highColorDepth;
	UINT16 supportedColorDepths;
	UINT16 earlyCapabilityFlags;
	WCHAR* clientDigProductId = NULL;
	int clientDigProductIdLength;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!gcc_write_user_data_header(s, CS_CORE, 234))
		return FALSE;
	clientNameLength = ConvertToUnicode(CP_UTF8, 0, settings->ClientHostname, -1, &clientName, 0);
	clientDigProductIdLength =
	    ConvertToUnicode(CP_UTF8, 0, settings->ClientProductId, -1, &clientDigProductId, 0);
	Stream_Write_UINT32(s, settings->RdpVersion);    /* Version */
	Stream_Write_UINT16(s, settings->DesktopWidth);  /* DesktopWidth */
	Stream_Write_UINT16(s, settings->DesktopHeight); /* DesktopHeight */
	Stream_Write_UINT16(s,
	                    RNS_UD_COLOR_8BPP); /* ColorDepth, ignored because of postBeta2ColorDepth */
	Stream_Write_UINT16(s, RNS_UD_SAS_DEL); /* SASSequence (Secure Access Sequence) */
	Stream_Write_UINT32(s, settings->KeyboardLayout); /* KeyboardLayout */
	Stream_Write_UINT32(s, settings->ClientBuild);    /* ClientBuild */

	/* clientName (32 bytes, null-terminated unicode, truncated to 15 characters) */

	if (clientNameLength >= 16)
	{
		clientNameLength = 16;
		clientName[clientNameLength - 1] = 0;
	}
	if (!Stream_EnsureRemainingCapacity(s, 32 + 12 + 64 + 8))
		return FALSE;

	Stream_Write(s, clientName, (clientNameLength * 2));
	Stream_Zero(s, 32 - (clientNameLength * 2));
	free(clientName);
	Stream_Write_UINT32(s, settings->KeyboardType);        /* KeyboardType */
	Stream_Write_UINT32(s, settings->KeyboardSubType);     /* KeyboardSubType */
	Stream_Write_UINT32(s, settings->KeyboardFunctionKey); /* KeyboardFunctionKey */
	Stream_Zero(s, 64);                                    /* imeFileName */
	Stream_Write_UINT16(s, RNS_UD_COLOR_8BPP);             /* postBeta2ColorDepth */
	Stream_Write_UINT16(s, 1);                             /* clientProductID */
	Stream_Write_UINT32(s, 0); /* serialNumber (should be initialized to 0) */
	highColorDepth = MIN(settings->ColorDepth, 24);
	supportedColorDepths = RNS_UD_24BPP_SUPPORT | RNS_UD_16BPP_SUPPORT | RNS_UD_15BPP_SUPPORT;
	earlyCapabilityFlags = RNS_UD_CS_SUPPORT_ERRINFO_PDU;

	connectionType = settings->ConnectionType;

	if (connectionType)
		earlyCapabilityFlags |= RNS_UD_CS_VALID_CONNECTION_TYPE;

	if (settings->ColorDepth == 32)
	{
		supportedColorDepths |= RNS_UD_32BPP_SUPPORT;
		earlyCapabilityFlags |= RNS_UD_CS_WANT_32BPP_SESSION;
	}

	if (settings->NetworkAutoDetect)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_NETCHAR_AUTODETECT;

	if (settings->SupportHeartbeatPdu)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_HEARTBEAT_PDU;

	if (settings->SupportGraphicsPipeline)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL;

	if (settings->SupportDynamicTimeZone)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_DYNAMIC_TIME_ZONE;

	if (settings->SupportMonitorLayoutPdu)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU;

	if (settings->SupportStatusInfoPdu)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_STATUSINFO_PDU;
	if (!Stream_EnsureRemainingCapacity(s, 6))
		return FALSE;

	Stream_Write_UINT16(s, highColorDepth);       /* highColorDepth */
	Stream_Write_UINT16(s, supportedColorDepths); /* supportedColorDepths */
	Stream_Write_UINT16(s, earlyCapabilityFlags); /* earlyCapabilityFlags */

	/* clientDigProductId (64 bytes, null-terminated unicode, truncated to 31 characters) */
	if (clientDigProductIdLength >= 32)
	{
		clientDigProductIdLength = 32;
		clientDigProductId[clientDigProductIdLength - 1] = 0;
	}

	if (!Stream_EnsureRemainingCapacity(s, 64 + 24))
		return FALSE;
	Stream_Write(s, clientDigProductId, (clientDigProductIdLength * 2));
	Stream_Zero(s, 64 - (clientDigProductIdLength * 2));
	free(clientDigProductId);
	Stream_Write_UINT8(s, connectionType);                   /* connectionType */
	Stream_Write_UINT8(s, 0);                                /* pad1octet */
	Stream_Write_UINT32(s, settings->SelectedProtocol);      /* serverSelectedProtocol */
	Stream_Write_UINT32(s, settings->DesktopPhysicalWidth);  /* desktopPhysicalWidth */
	Stream_Write_UINT32(s, settings->DesktopPhysicalHeight); /* desktopPhysicalHeight */
	Stream_Write_UINT16(s, settings->DesktopOrientation);    /* desktopOrientation */
	Stream_Write_UINT32(s, settings->DesktopScaleFactor);    /* desktopScaleFactor */
	Stream_Write_UINT32(s, settings->DeviceScaleFactor);     /* deviceScaleFactor */
	return TRUE;
}

BOOL gcc_read_server_core_data(wStream* s, rdpMcs* mcs)
{
	UINT32 serverVersion;
	UINT32 clientRequestedProtocols;
	UINT32 earlyCapabilityFlags;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, serverVersion); /* version */
	settings->RdpVersion = rdp_version_common(serverVersion, settings->RdpVersion);

	if (Stream_GetRemainingLength(s) >= 4)
	{
		Stream_Read_UINT32(s, clientRequestedProtocols); /* clientRequestedProtocols */
	}

	if (Stream_GetRemainingLength(s) >= 4)
	{
		Stream_Read_UINT32(s, earlyCapabilityFlags); /* earlyCapabilityFlags */
	}

	return TRUE;
}

BOOL gcc_write_server_core_data(wStream* s, const rdpMcs* mcs)
{
	UINT32 earlyCapabilityFlags = 0;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!gcc_write_user_data_header(s, SC_CORE, 16))
		return FALSE;

	if (settings->SupportDynamicTimeZone)
		earlyCapabilityFlags |= RNS_UD_SC_DYNAMIC_DST_SUPPORTED;

	Stream_Write_UINT32(s, settings->RdpVersion);         /* version (4 bytes) */
	Stream_Write_UINT32(s, settings->RequestedProtocols); /* clientRequestedProtocols (4 bytes) */
	Stream_Write_UINT32(s, earlyCapabilityFlags);         /* earlyCapabilityFlags (4 bytes) */
	return TRUE;
}

/**
 * Read a client security data block (TS_UD_CS_SEC).\n
 * @msdn{cc240511}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_security_data(wStream* s, rdpMcs* mcs, UINT16 blockLength)
{
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (blockLength < 8)
		return FALSE;

	if (settings->UseRdpSecurityLayer)
	{
		Stream_Read_UINT32(s, settings->EncryptionMethods); /* encryptionMethods */

		if (settings->EncryptionMethods == 0)
			Stream_Read_UINT32(s, settings->EncryptionMethods); /* extEncryptionMethods */
		else
			Stream_Seek(s, 4);
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

BOOL gcc_write_client_security_data(wStream* s, const rdpMcs* mcs)
{
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!gcc_write_user_data_header(s, CS_SECURITY, 12))
		return FALSE;

	if (settings->UseRdpSecurityLayer)
	{
		Stream_Write_UINT32(s, settings->EncryptionMethods); /* encryptionMethods */
		Stream_Write_UINT32(s, 0);                           /* extEncryptionMethods */
	}
	else
	{
		/* French locale, disable encryption */
		Stream_Write_UINT32(s, 0);                           /* encryptionMethods */
		Stream_Write_UINT32(s, settings->EncryptionMethods); /* extEncryptionMethods */
	}
	return TRUE;
}

BOOL gcc_read_server_security_data(wStream* s, rdpMcs* mcs)
{
	const BYTE* data;
	UINT32 length;
	BOOL validCryptoConfig = FALSE;
	UINT32 serverEncryptionMethod;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, serverEncryptionMethod);    /* encryptionMethod */
	Stream_Read_UINT32(s, settings->EncryptionLevel); /* encryptionLevel */

	/* Only accept valid/known encryption methods */
	switch (serverEncryptionMethod)
	{
		case ENCRYPTION_METHOD_NONE:
			WLog_DBG(TAG, "Server rdp encryption method: NONE");
			break;

		case ENCRYPTION_METHOD_40BIT:
			WLog_DBG(TAG, "Server rdp encryption method: 40BIT");
			break;

		case ENCRYPTION_METHOD_56BIT:
			WLog_DBG(TAG, "Server rdp encryption method: 56BIT");
			break;

		case ENCRYPTION_METHOD_128BIT:
			WLog_DBG(TAG, "Server rdp encryption method: 128BIT");
			break;

		case ENCRYPTION_METHOD_FIPS:
			WLog_DBG(TAG, "Server rdp encryption method: FIPS");
			break;

		default:
			WLog_ERR(TAG, "Received unknown encryption method %08" PRIX32 "",
			         serverEncryptionMethod);
			return FALSE;
	}

	if (settings->UseRdpSecurityLayer && !(settings->EncryptionMethods & serverEncryptionMethod))
	{
		WLog_WARN(TAG, "Server uses non-advertised encryption method 0x%08" PRIX32 "",
		          serverEncryptionMethod);
		/* FIXME: Should we return FALSE; in this case ?? */
	}

	settings->EncryptionMethods = serverEncryptionMethod;

	/* Verify encryption level/method combinations according to MS-RDPBCGR Section 5.3.2 */
	switch (settings->EncryptionLevel)
	{
		case ENCRYPTION_LEVEL_NONE:
			if (settings->EncryptionMethods == ENCRYPTION_METHOD_NONE)
			{
				validCryptoConfig = TRUE;
			}

			break;

		case ENCRYPTION_LEVEL_FIPS:
			if (settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
			{
				validCryptoConfig = TRUE;
			}

			break;

		case ENCRYPTION_LEVEL_LOW:
		case ENCRYPTION_LEVEL_HIGH:
		case ENCRYPTION_LEVEL_CLIENT_COMPATIBLE:
			if (settings->EncryptionMethods == ENCRYPTION_METHOD_40BIT ||
			    settings->EncryptionMethods == ENCRYPTION_METHOD_56BIT ||
			    settings->EncryptionMethods == ENCRYPTION_METHOD_128BIT ||
			    settings->EncryptionMethods == ENCRYPTION_METHOD_FIPS)
			{
				validCryptoConfig = TRUE;
			}

			break;

		default:
			WLog_ERR(TAG, "Received unknown encryption level 0x%08" PRIX32 "",
			         settings->EncryptionLevel);
	}

	if (!validCryptoConfig)
	{
		WLog_ERR(TAG,
		         "Received invalid cryptographic configuration (level=0x%08" PRIX32
		         " method=0x%08" PRIX32 ")",
		         settings->EncryptionLevel, settings->EncryptionMethods);
		return FALSE;
	}

	if (settings->EncryptionLevel == ENCRYPTION_LEVEL_NONE)
	{
		/* serverRandomLen and serverCertLen must not be present */
		settings->UseRdpSecurityLayer = FALSE;
		return TRUE;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, settings->ServerRandomLength);      /* serverRandomLen */
	Stream_Read_UINT32(s, settings->ServerCertificateLength); /* serverCertLen */

	if ((settings->ServerRandomLength == 0) || (settings->ServerCertificateLength == 0))
		return FALSE;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, settings->ServerRandomLength))
		return FALSE;

	/* serverRandom */
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerRandom, NULL,
	                                      settings->ServerRandomLength))
		goto fail;

	Stream_Read(s, settings->ServerRandom, settings->ServerRandomLength);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, settings->ServerCertificateLength))
		goto fail;

	/* serverCertificate */
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerCertificate, NULL,
	                                      settings->ServerCertificateLength))
		goto fail;

	Stream_Read(s, settings->ServerCertificate, settings->ServerCertificateLength);

	data = settings->ServerCertificate;
	length = settings->ServerCertificateLength;

	if (!certificate_read_server_certificate(settings->RdpServerCertificate, data, length))
		goto fail;

	return TRUE;
fail:
	free(settings->ServerRandom);
	free(settings->ServerCertificate);
	settings->ServerRandom = NULL;
	settings->ServerCertificate = NULL;
	return FALSE;
}

static const BYTE initial_signature[] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01
};

/*
 * Terminal Services Signing Keys.
 * Yes, Terminal Services Private Key is publicly available.
 */

const BYTE tssk_modulus[] = { 0x3d, 0x3a, 0x5e, 0xbd, 0x72, 0x43, 0x3e, 0xc9, 0x4d, 0xbb, 0xc1,
	                          0x1e, 0x4a, 0xba, 0x5f, 0xcb, 0x3e, 0x88, 0x20, 0x87, 0xef, 0xf5,
	                          0xc1, 0xe2, 0xd7, 0xb7, 0x6b, 0x9a, 0xf2, 0x52, 0x45, 0x95, 0xce,
	                          0x63, 0x65, 0x6b, 0x58, 0x3a, 0xfe, 0xef, 0x7c, 0xe7, 0xbf, 0xfe,
	                          0x3d, 0xf6, 0x5c, 0x7d, 0x6c, 0x5e, 0x06, 0x09, 0x1a, 0xf5, 0x61,
	                          0xbb, 0x20, 0x93, 0x09, 0x5f, 0x05, 0x6d, 0xea, 0x87 };

const BYTE tssk_privateExponent[] = {
	0x87, 0xa7, 0x19, 0x32, 0xda, 0x11, 0x87, 0x55, 0x58, 0x00, 0x16, 0x16, 0x25, 0x65, 0x68, 0xf8,
	0x24, 0x3e, 0xe6, 0xfa, 0xe9, 0x67, 0x49, 0x94, 0xcf, 0x92, 0xcc, 0x33, 0x99, 0xe8, 0x08, 0x60,
	0x17, 0x9a, 0x12, 0x9f, 0x24, 0xdd, 0xb1, 0x24, 0x99, 0xc7, 0x3a, 0xb8, 0x0a, 0x7b, 0x0d, 0xdd,
	0x35, 0x07, 0x79, 0x17, 0x0b, 0x51, 0x9b, 0xb3, 0xc7, 0x10, 0x01, 0x13, 0xe7, 0x3f, 0xf3, 0x5f
};

const BYTE tssk_exponent[] = { 0x5b, 0x7b, 0x88, 0xc0 };

BOOL gcc_write_server_security_data(wStream* s, rdpMcs* mcs)
{
	BYTE* sigData;
	size_t expLen = 0, keyLen, sigDataLen;
	BYTE encryptedSignature[TSSK_KEY_LENGTH];
	BYTE signature[sizeof(initial_signature)];
	UINT32 headerLen, serverRandomLen, serverCertLen, wPublicKeyBlobLen;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

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
		settings->EncryptionLevel = ENCRYPTION_LEVEL_NONE;
	}

	/* verify server encryption level value */
	switch (settings->EncryptionLevel)
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
			WLog_ERR(TAG, "Invalid server encryption level 0x%08" PRIX32 "",
			         settings->EncryptionLevel);
			WLog_ERR(TAG, "Switching to encryption level CLIENT-COMPATIBLE");
			settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
	}

	/* choose rdp encryption method based on server level and client methods */
	switch (settings->EncryptionLevel)
	{
		case ENCRYPTION_LEVEL_NONE:
			/* The only valid method is NONE in this case */
			settings->EncryptionMethods = ENCRYPTION_METHOD_NONE;
			break;

		case ENCRYPTION_LEVEL_FIPS:

			/* The only valid method is FIPS in this case */
			if (!(settings->EncryptionMethods & ENCRYPTION_METHOD_FIPS))
			{
				WLog_WARN(TAG, "client does not support FIPS as required by server configuration");
			}

			settings->EncryptionMethods = ENCRYPTION_METHOD_FIPS;
			break;

		case ENCRYPTION_LEVEL_HIGH:

			/* Maximum key strength supported by the server must be used (128 bit)*/
			if (!(settings->EncryptionMethods & ENCRYPTION_METHOD_128BIT))
			{
				WLog_WARN(TAG, "client does not support 128 bit encryption method as required by "
				               "server configuration");
			}

			settings->EncryptionMethods = ENCRYPTION_METHOD_128BIT;
			break;

		case ENCRYPTION_LEVEL_LOW:
		case ENCRYPTION_LEVEL_CLIENT_COMPATIBLE:

			/* Maximum key strength supported by the client must be used */
			if (settings->EncryptionMethods & ENCRYPTION_METHOD_128BIT)
				settings->EncryptionMethods = ENCRYPTION_METHOD_128BIT;
			else if (settings->EncryptionMethods & ENCRYPTION_METHOD_56BIT)
				settings->EncryptionMethods = ENCRYPTION_METHOD_56BIT;
			else if (settings->EncryptionMethods & ENCRYPTION_METHOD_40BIT)
				settings->EncryptionMethods = ENCRYPTION_METHOD_40BIT;
			else if (settings->EncryptionMethods & ENCRYPTION_METHOD_FIPS)
				settings->EncryptionMethods = ENCRYPTION_METHOD_FIPS;
			else
			{
				WLog_WARN(TAG, "client has not announced any supported encryption methods");
				settings->EncryptionMethods = ENCRYPTION_METHOD_128BIT;
			}

			break;

		default:
			WLog_ERR(TAG, "internal error: unknown encryption level");
			return FALSE;
	}

	/* log selected encryption method */
	switch (settings->EncryptionMethods)
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

	headerLen = 12;
	keyLen = 0;
	wPublicKeyBlobLen = 0;
	serverRandomLen = 0;
	serverCertLen = 0;

	if (settings->EncryptionMethods != ENCRYPTION_METHOD_NONE)
	{
		serverRandomLen = 32;
		keyLen = settings->RdpServerRsaKey->ModulusLength;
		expLen = sizeof(settings->RdpServerRsaKey->exponent);
		wPublicKeyBlobLen = 4;  /* magic (RSA1) */
		wPublicKeyBlobLen += 4; /* keylen */
		wPublicKeyBlobLen += 4; /* bitlen */
		wPublicKeyBlobLen += 4; /* datalen */
		wPublicKeyBlobLen += expLen;
		wPublicKeyBlobLen += keyLen;
		wPublicKeyBlobLen += 8; /* 8 bytes of zero padding */
		serverCertLen = 4;      /* dwVersion */
		serverCertLen += 4;     /* dwSigAlgId */
		serverCertLen += 4;     /* dwKeyAlgId */
		serverCertLen += 2;     /* wPublicKeyBlobType */
		serverCertLen += 2;     /* wPublicKeyBlobLen */
		serverCertLen += wPublicKeyBlobLen;
		serverCertLen += 2;                          /* wSignatureBlobType */
		serverCertLen += 2;                          /* wSignatureBlobLen */
		serverCertLen += sizeof(encryptedSignature); /* SignatureBlob */
		serverCertLen += 8;                          /* 8 bytes of zero padding */
		headerLen += sizeof(serverRandomLen);
		headerLen += sizeof(serverCertLen);
		headerLen += serverRandomLen;
		headerLen += serverCertLen;
	}

	if (!gcc_write_user_data_header(s, SC_SECURITY, headerLen))
		return FALSE;

	Stream_Write_UINT32(s, settings->EncryptionMethods); /* encryptionMethod */
	Stream_Write_UINT32(s, settings->EncryptionLevel);   /* encryptionLevel */

	if (settings->EncryptionMethods == ENCRYPTION_METHOD_NONE)
	{
		return TRUE;
	}

	Stream_Write_UINT32(s, serverRandomLen); /* serverRandomLen */
	Stream_Write_UINT32(s, serverCertLen);   /* serverCertLen */
	settings->ServerRandomLength = serverRandomLen;
	settings->ServerRandom = (BYTE*)malloc(serverRandomLen);

	if (!settings->ServerRandom)
	{
		return FALSE;
	}

	winpr_RAND(settings->ServerRandom, serverRandomLen);
	Stream_Write(s, settings->ServerRandom, serverRandomLen);
	sigData = Stream_Pointer(s);
	Stream_Write_UINT32(s, CERT_CHAIN_VERSION_1); /* dwVersion (4 bytes) */
	Stream_Write_UINT32(s, SIGNATURE_ALG_RSA);    /* dwSigAlgId */
	Stream_Write_UINT32(s, KEY_EXCHANGE_ALG_RSA); /* dwKeyAlgId */
	Stream_Write_UINT16(s, BB_RSA_KEY_BLOB);      /* wPublicKeyBlobType */
	Stream_Write_UINT16(s, wPublicKeyBlobLen);    /* wPublicKeyBlobLen */
	Stream_Write(s, "RSA1", 4);                   /* magic */
	WINPR_ASSERT(keyLen > 0);
	WINPR_ASSERT(keyLen <= UINT32_MAX / 8);
	Stream_Write_UINT32(s, (UINT32)keyLen + 8); /* keylen */
	Stream_Write_UINT32(s, (UINT32)keyLen * 8); /* bitlen */
	Stream_Write_UINT32(s, (UINT32)keyLen - 1); /* datalen */
	Stream_Write(s, settings->RdpServerRsaKey->exponent, expLen);
	Stream_Write(s, settings->RdpServerRsaKey->Modulus, keyLen);
	Stream_Zero(s, 8);
	sigDataLen = Stream_Pointer(s) - sigData;
	Stream_Write_UINT16(s, BB_RSA_SIGNATURE_BLOB);          /* wSignatureBlobType */
	Stream_Write_UINT16(s, sizeof(encryptedSignature) + 8); /* wSignatureBlobLen */
	memcpy(signature, initial_signature, sizeof(initial_signature));

	if (!winpr_Digest(WINPR_MD_MD5, sigData, sigDataLen, signature, sizeof(signature)))
		return FALSE;

	crypto_rsa_private_encrypt(signature, sizeof(signature), TSSK_KEY_LENGTH, tssk_modulus,
	                           tssk_privateExponent, encryptedSignature);
	Stream_Write(s, encryptedSignature, sizeof(encryptedSignature));
	Stream_Zero(s, 8);
	return TRUE;
}

/**
 * Read a client network data block (TS_UD_CS_NET).\n
 * @msdn{cc240512}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_network_data(wStream* s, rdpMcs* mcs, UINT16 blockLength)
{
	UINT32 i;

	if (blockLength < 4)
		return FALSE;

	Stream_Read_UINT32(s, mcs->channelCount); /* channelCount */

	if (blockLength < 4 + mcs->channelCount * 12)
		return FALSE;

	if (mcs->channelCount > CHANNEL_MAX_COUNT)
		return FALSE;

	/* channelDefArray */
	for (i = 0; i < mcs->channelCount; i++)
	{
		/**
		 * CHANNEL_DEF
		 * - name: an 8-byte array containing a null-terminated collection
		 *   of seven ANSI characters that uniquely identify the channel.
		 * - options: a 32-bit, unsigned integer. Channel option flags
		 */
		rdpMcsChannel* channel = &mcs->channels[i];
		Stream_Read(s, channel->Name, CHANNEL_NAME_LEN + 1); /* name (8 bytes) */

		if (!memchr(channel->Name, 0, CHANNEL_NAME_LEN + 1))
		{
			WLog_ERR(
			    TAG,
			    "protocol violation: received a static channel name with missing null-termination");
			return FALSE;
		}

		Stream_Read_UINT32(s, channel->options); /* options (4 bytes) */
		channel->ChannelId = mcs->baseChannelId++;
	}

	return TRUE;
}

/**
 * Write a client network data block (TS_UD_CS_NET).\n
 * @msdn{cc240512}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_write_client_network_data(wStream* s, const rdpMcs* mcs)
{
	UINT32 i;
	UINT16 length;

	if (mcs->channelCount > 0)
	{
		length = mcs->channelCount * 12 + 8;
		if (!gcc_write_user_data_header(s, CS_NET, length))
			return FALSE;
		Stream_Write_UINT32(s, mcs->channelCount); /* channelCount */

		/* channelDefArray */
		for (i = 0; i < mcs->channelCount; i++)
		{
			/* CHANNEL_DEF */
			rdpMcsChannel* channel = &mcs->channels[i];
			Stream_Write(s, channel->Name, CHANNEL_NAME_LEN + 1); /* name (8 bytes) */
			Stream_Write_UINT32(s, channel->options);             /* options (4 bytes) */
		}
	}
	return TRUE;
}

BOOL gcc_read_server_network_data(wStream* s, rdpMcs* mcs)
{
	int i;
	UINT16 channelId;
	UINT16 MCSChannelId;
	UINT16 channelCount;
	UINT16 parsedChannelCount;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, MCSChannelId); /* MCSChannelId */
	Stream_Read_UINT16(s, channelCount); /* channelCount */
	parsedChannelCount = channelCount;

	if (channelCount != mcs->channelCount)
	{
		WLog_ERR(TAG, "requested %" PRIu32 " channels, got %" PRIu16 " instead", mcs->channelCount,
		         channelCount);

		/* we ensure that the response is not bigger than the request */

		if (channelCount > mcs->channelCount)
			parsedChannelCount = mcs->channelCount;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2ull * channelCount))
		return FALSE;

	for (i = 0; i < parsedChannelCount; i++)
	{
		rdpMcsChannel* channel = &mcs->channels[i];
		Stream_Read_UINT16(s, channelId); /* channelId */
		channel->ChannelId = channelId;
	}

	if (channelCount % 2 == 1)
		return Stream_SafeSeek(s, 2); /* padding */

	return TRUE;
}

BOOL gcc_write_server_network_data(wStream* s, const rdpMcs* mcs)
{
	UINT32 i;
	const size_t payloadLen = 8 + mcs->channelCount * 2 + (mcs->channelCount % 2 == 1 ? 2 : 0);

	if (!gcc_write_user_data_header(s, SC_NET, payloadLen))
		return FALSE;

	Stream_Write_UINT16(s, MCS_GLOBAL_CHANNEL_ID); /* MCSChannelId */
	Stream_Write_UINT16(s, mcs->channelCount);     /* channelCount */

	for (i = 0; i < mcs->channelCount; i++)
	{
		const rdpMcsChannel* channel = &mcs->channels[i];
		Stream_Write_UINT16(s, channel->ChannelId);
	}

	if (mcs->channelCount % 2 == 1)
		Stream_Write_UINT16(s, 0);

	return TRUE;
}

/**
 * Read a client cluster data block (TS_UD_CS_CLUSTER).\n
 * @msdn{cc240514}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_cluster_data(wStream* s, rdpMcs* mcs, UINT16 blockLength)
{
	UINT32 flags;
	UINT32 redirectedSessionId;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (blockLength < 8)
		return FALSE;

	Stream_Read_UINT32(s, flags);               /* flags */
	Stream_Read_UINT32(s, redirectedSessionId); /* redirectedSessionId */

	if (flags & REDIRECTED_SESSIONID_FIELD_VALID)
	{
		settings->RedirectedSessionId = redirectedSessionId;
		settings->ConsoleSession = (redirectedSessionId == 0);
	}

	if (blockLength != 8)
	{
		if (Stream_GetRemainingLength(s) >= (size_t)(blockLength - 8))
		{
			/* The old Microsoft Mac RDP client can send a pad here */
			Stream_Seek(s, (blockLength - 8));
		}
	}

	return TRUE;
}

/**
 * Write a client cluster data block (TS_UD_CS_CLUSTER).\n
 * @msdn{cc240514}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_write_client_cluster_data(wStream* s, const rdpMcs* mcs)
{
	UINT32 flags;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!gcc_write_user_data_header(s, CS_CLUSTER, 12))
		return FALSE;
	flags = REDIRECTION_SUPPORTED | (REDIRECTION_VERSION4 << 2);

	if (settings->ConsoleSession || settings->RedirectedSessionId)
		flags |= REDIRECTED_SESSIONID_FIELD_VALID;

	if (settings->RedirectSmartCards)
		flags |= REDIRECTED_SMARTCARD;

	Stream_Write_UINT32(s, flags);                         /* flags */
	Stream_Write_UINT32(s, settings->RedirectedSessionId); /* redirectedSessionID */
	return TRUE;
}

/**
 * Read a client monitor data block (TS_UD_CS_MONITOR).\n
 * @msdn{dd305336}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_monitor_data(wStream* s, rdpMcs* mcs, UINT16 blockLength)
{
	UINT32 index;
	UINT32 flags;
	UINT32 monitorCount;
	UINT32 left, top, right, bottom;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (blockLength < 8)
		return FALSE;

	Stream_Read_UINT32(s, flags);        /* flags */
	Stream_Read_UINT32(s, monitorCount); /* monitorCount */

	/* 2.2.1.3.6 Client Monitor Data -
	 * monitorCount (4 bytes): A 32-bit, unsigned integer. The number of display
	 * monitor definitions in the monitorDefArray field (the maximum allowed is 16).
	 */
	if (monitorCount > 16)
	{
		WLog_ERR(TAG, "announced monitors(%" PRIu32 ") exceed the 16 limit", monitorCount);
		return FALSE;
	}

	if (monitorCount > settings->MonitorDefArraySize)
	{
		WLog_ERR(TAG, "too many announced monitors(%" PRIu32 "), clamping to %" PRIu32 "",
		         monitorCount, settings->MonitorDefArraySize);
		monitorCount = settings->MonitorDefArraySize;
	}

	if ((UINT32)((blockLength - 8) / 20) < monitorCount)
		return FALSE;

	settings->MonitorCount = monitorCount;

	for (index = 0; index < monitorCount; index++)
	{
		rdpMonitor* current = &settings->MonitorDefArray[index];

		Stream_Read_UINT32(s, left);   /* left */
		Stream_Read_UINT32(s, top);    /* top */
		Stream_Read_UINT32(s, right);  /* right */
		Stream_Read_UINT32(s, bottom); /* bottom */
		Stream_Read_UINT32(s, flags);  /* flags */
		current->x = left;
		current->y = top;
		current->width = right - left + 1;
		current->height = bottom - top + 1;
		current->is_primary = (flags & MONITOR_PRIMARY);
	}

	return TRUE;
}

/**
 * Write a client monitor data block (TS_UD_CS_MONITOR).\n
 * @msdn{dd305336}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_write_client_monitor_data(wStream* s, const rdpMcs* mcs)
{
	UINT32 i;
	UINT16 length;
	INT32 baseX = 0, baseY = 0;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	WLog_DBG(TAG, "[%s] MonitorCount=%" PRIu32, __FUNCTION__, settings->MonitorCount);
	if (settings->MonitorCount > 1)
	{
		length = (20 * settings->MonitorCount) + 12;
		if (!gcc_write_user_data_header(s, CS_MONITOR, length))
			return FALSE;
		Stream_Write_UINT32(s, 0);                      /* flags */
		Stream_Write_UINT32(s, settings->MonitorCount); /* monitorCount */

		/* first pass to get the primary monitor coordinates (it is supposed to be
		 * in (0,0) */
		for (i = 0; i < settings->MonitorCount; i++)
		{
			const rdpMonitor* current = &settings->MonitorDefArray[i];
			if (current->is_primary)
			{
				baseX = current->x;
				baseY = current->y;
				break;
			}
		}

		for (i = 0; i < settings->MonitorCount; i++)
		{
			const rdpMonitor* current = &settings->MonitorDefArray[i];
			const UINT32 left = current->x - baseX;
			const UINT32 top = current->y - baseY;
			const UINT32 right = left + current->width - 1;
			const UINT32 bottom = top + current->height - 1;
			const UINT32 flags = current->is_primary ? MONITOR_PRIMARY : 0;
			WLog_DBG(TAG,
			         "[%s] Monitor[%" PRIu32 "]: top=%" PRIu32 ", left=%" PRIu32 ", bottom=%" PRIu32
			         ", right=%" PRIu32 ", flags" PRIu32,
			         __FUNCTION__, i, top, left, bottom, right, flags);
			Stream_Write_UINT32(s, left);   /* left */
			Stream_Write_UINT32(s, top);    /* top */
			Stream_Write_UINT32(s, right);  /* right */
			Stream_Write_UINT32(s, bottom); /* bottom */
			Stream_Write_UINT32(s, flags);  /* flags */
		}
	}
	WLog_DBG(TAG, "[%s] FINISHED", __FUNCTION__);
	return TRUE;
}

BOOL gcc_read_client_monitor_extended_data(wStream* s, rdpMcs* mcs, UINT16 blockLength)
{
	UINT32 index;
	UINT32 flags;
	UINT32 monitorCount;
	UINT32 monitorAttributeSize;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (blockLength < 12)
		return FALSE;

	Stream_Read_UINT32(s, flags);                /* flags */
	Stream_Read_UINT32(s, monitorAttributeSize); /* monitorAttributeSize */
	Stream_Read_UINT32(s, monitorCount);         /* monitorCount */

	if (monitorAttributeSize != 20)
		return FALSE;

	if ((blockLength - 12) / monitorAttributeSize < monitorCount)
		return FALSE;

	if (settings->MonitorCount != monitorCount)
		return FALSE;

	settings->HasMonitorAttributes = TRUE;

	for (index = 0; index < monitorCount; index++)
	{
		rdpMonitor* current = &settings->MonitorDefArray[index];
		Stream_Read_UINT32(s, current->attributes.physicalWidth);      /* physicalWidth */
		Stream_Read_UINT32(s, current->attributes.physicalHeight);     /* physicalHeight */
		Stream_Read_UINT32(s, current->attributes.orientation);        /* orientation */
		Stream_Read_UINT32(s, current->attributes.desktopScaleFactor); /* desktopScaleFactor */
		Stream_Read_UINT32(s, current->attributes.deviceScaleFactor);  /* deviceScaleFactor */
	}

	return TRUE;
}

BOOL gcc_write_client_monitor_extended_data(wStream* s, const rdpMcs* mcs)
{
	UINT32 i;
	UINT16 length;
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (settings->HasMonitorAttributes)
	{
		length = (20 * settings->MonitorCount) + 16;
		if (!gcc_write_user_data_header(s, CS_MONITOR_EX, length))
			return FALSE;
		Stream_Write_UINT32(s, 0);                      /* flags */
		Stream_Write_UINT32(s, 20);                     /* monitorAttributeSize */
		Stream_Write_UINT32(s, settings->MonitorCount); /* monitorCount */

		for (i = 0; i < settings->MonitorCount; i++)
		{
			const rdpMonitor* current = &settings->MonitorDefArray[i];
			Stream_Write_UINT32(s, current->attributes.physicalWidth);      /* physicalWidth */
			Stream_Write_UINT32(s, current->attributes.physicalHeight);     /* physicalHeight */
			Stream_Write_UINT32(s, current->attributes.orientation);        /* orientation */
			Stream_Write_UINT32(s, current->attributes.desktopScaleFactor); /* desktopScaleFactor */
			Stream_Write_UINT32(s, current->attributes.deviceScaleFactor);  /* deviceScaleFactor */
		}
	}
	return TRUE;
}

/**
 * Read a client message channel data block (TS_UD_CS_MCS_MSGCHANNEL).\n
 * @msdn{jj217627}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_message_channel_data(wStream* s, rdpMcs* mcs, UINT16 blockLength)
{
	UINT32 flags;

	if (blockLength < 4)
		return FALSE;

	Stream_Read_UINT32(s, flags);
	mcs->messageChannelId = mcs->baseChannelId++;
	return TRUE;
}

/**
 * Write a client message channel data block (TS_UD_CS_MCS_MSGCHANNEL).\n
 * @msdn{jj217627}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_write_client_message_channel_data(wStream* s, const rdpMcs* mcs)
{
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (settings->NetworkAutoDetect || settings->SupportHeartbeatPdu ||
	    settings->SupportMultitransport)
	{
		if (!gcc_write_user_data_header(s, CS_MCS_MSGCHANNEL, 8))
			return FALSE;
		Stream_Write_UINT32(s, 0); /* flags */
	}
	return TRUE;
}

BOOL gcc_read_server_message_channel_data(wStream* s, rdpMcs* mcs)
{
	UINT16 MCSChannelId;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, MCSChannelId); /* MCSChannelId */
	/* Save the MCS message channel id */
	mcs->messageChannelId = MCSChannelId;
	return TRUE;
}

BOOL gcc_write_server_message_channel_data(wStream* s, const rdpMcs* mcs)
{
	if (mcs->messageChannelId == 0)
		return TRUE;

	if (!gcc_write_user_data_header(s, SC_MCS_MSGCHANNEL, 6))
		return FALSE;

	Stream_Write_UINT16(s, mcs->messageChannelId); /* mcsChannelId (2 bytes) */
	return TRUE;
}

/**
 * Read a client multitransport channel data block (TS_UD_CS_MULTITRANSPORT).\n
 * @msdn{jj217498}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_read_client_multitransport_channel_data(wStream* s, rdpMcs* mcs, UINT16 blockLength)
{
	UINT32 flags;

	if (blockLength < 4)
		return FALSE;

	Stream_Read_UINT32(s, flags);
	return TRUE;
}

/**
 * Write a client multitransport channel data block (TS_UD_CS_MULTITRANSPORT).\n
 * @msdn{jj217498}
 * @param s stream
 * @param settings rdp settings
 */

BOOL gcc_write_client_multitransport_channel_data(wStream* s, const rdpMcs* mcs)
{
	rdpContext* context;
	rdpSettings* settings;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	settings = context->settings;
	WINPR_ASSERT(settings);

	if (!gcc_write_user_data_header(s, CS_MULTITRANSPORT, 8))
		return FALSE;
	Stream_Write_UINT32(s, settings->MultitransportFlags); /* flags */
	return TRUE;
}

BOOL gcc_read_server_multitransport_channel_data(wStream* s, rdpMcs* mcs)
{
	UINT32 flags;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, flags); /* flags */
	return TRUE;
}

BOOL gcc_write_server_multitransport_channel_data(wStream* s, const rdpMcs* mcs)
{
	UINT32 flags = 0;
	if (!gcc_write_user_data_header(s, SC_MULTITRANSPORT, 8))
		return FALSE;
	Stream_Write_UINT32(s, flags); /* flags (4 bytes) */
	return TRUE;
}
