/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * T.124 Generic Conference Control (GCC)
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
 * Copyright 2014 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/assert.h>
#include <winpr/cast.h>

#include <freerdp/log.h>
#include <freerdp/utils/string.h>
#include <freerdp/crypto/certificate.h>

#include "utils.h"
#include "gcc.h"
#include "nego.h"

#include "../crypto/certificate.h"

typedef enum
{
	HIGH_COLOR_4BPP = 0x04,
	HIGH_COLOR_8BPP = 0x08,
	HIGH_COLOR_15BPP = 0x0F,
	HIGH_COLOR_16BPP = 0x10,
	HIGH_COLOR_24BPP = 0x18,
} HIGH_COLOR_DEPTH;

static const char* HighColorToString(HIGH_COLOR_DEPTH color)
{
	switch (color)
	{
		case HIGH_COLOR_4BPP:
			return "HIGH_COLOR_4BPP";
		case HIGH_COLOR_8BPP:
			return "HIGH_COLOR_8BPP";
		case HIGH_COLOR_15BPP:
			return "HIGH_COLOR_15BPP";
		case HIGH_COLOR_16BPP:
			return "HIGH_COLOR_16BPP";
		case HIGH_COLOR_24BPP:
			return "HIGH_COLOR_24BPP";
		default:
			return "HIGH_COLOR_UNKNOWN";
	}
}

static HIGH_COLOR_DEPTH ColorDepthToHighColor(UINT32 bpp)
{
	switch (bpp)
	{
		case 4:
			return HIGH_COLOR_4BPP;
		case 8:
			return HIGH_COLOR_8BPP;
		case 15:
			return HIGH_COLOR_15BPP;
		case 16:
			return HIGH_COLOR_16BPP;
		default:
			return HIGH_COLOR_24BPP;
	}
}

static char* gcc_block_type_string(UINT16 type, char* buffer, size_t size);
static BOOL gcc_read_client_cluster_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_read_client_core_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_read_client_data_blocks(wStream* s, rdpMcs* mcs, UINT16 length);
static BOOL gcc_read_server_data_blocks(wStream* s, rdpMcs* mcs, UINT16 length);
static BOOL gcc_read_user_data_header(wLog* log, wStream* s, UINT16* type, UINT16* length);
static BOOL gcc_write_user_data_header(wStream* s, UINT16 type, UINT16 length);

static BOOL gcc_write_client_core_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_server_core_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_server_core_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_read_client_security_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_client_security_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_server_security_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_server_security_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_read_client_network_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_client_network_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_server_network_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_server_network_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_write_client_cluster_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_client_monitor_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_client_monitor_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_client_monitor_extended_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_client_monitor_extended_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_client_message_channel_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_client_message_channel_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_server_message_channel_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_server_message_channel_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_client_multitransport_channel_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_client_multitransport_channel_data(wStream* s, const rdpMcs* mcs);
static BOOL gcc_read_server_multitransport_channel_data(wStream* s, rdpMcs* mcs);
static BOOL gcc_write_server_multitransport_channel_data(wStream* s, const rdpMcs* mcs);

static rdpSettings* mcs_get_settings(rdpMcs* mcs)
{
	WINPR_ASSERT(mcs);
	WINPR_ASSERT(mcs->context);

	return mcs->context->settings;
}

static const rdpSettings* mcs_get_const_settings(const rdpMcs* mcs)
{
	WINPR_ASSERT(mcs);
	WINPR_ASSERT(mcs->context);

	return mcs->context->settings;
}

static char* rdp_early_server_caps_string(UINT32 flags, char* buffer, size_t size)
{
	char msg[32] = { 0 };
	const UINT32 mask = RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V1 | RNS_UD_SC_DYNAMIC_DST_SUPPORTED |
	                    RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V2 | RNS_UD_SC_SKIP_CHANNELJOIN_SUPPORTED;
	const UINT32 unknown = flags & (~mask);

	if (flags & RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V1)
		winpr_str_append("RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V1", buffer, size, "|");
	if (flags & RNS_UD_SC_DYNAMIC_DST_SUPPORTED)
		winpr_str_append("RNS_UD_SC_DYNAMIC_DST_SUPPORTED", buffer, size, "|");
	if (flags & RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V2)
		winpr_str_append("RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V2", buffer, size, "|");
	if (flags & RNS_UD_SC_SKIP_CHANNELJOIN_SUPPORTED)
		winpr_str_append("RNS_UD_SC_SKIP_CHANNELJOIN_SUPPORTED", buffer, size, "|");

	if (unknown != 0)
	{
		(void)_snprintf(msg, sizeof(msg), "RNS_UD_SC_UNKNOWN[0x%08" PRIx32 "]", unknown);
		winpr_str_append(msg, buffer, size, "|");
	}
	(void)_snprintf(msg, sizeof(msg), "[0x%08" PRIx32 "]", flags);
	winpr_str_append(msg, buffer, size, "|");
	return buffer;
}

static const char* rdp_early_client_caps_string(UINT32 flags, char* buffer, size_t size)
{
	char msg[32] = { 0 };
	const UINT32 mask = RNS_UD_CS_SUPPORT_ERRINFO_PDU | RNS_UD_CS_WANT_32BPP_SESSION |
	                    RNS_UD_CS_SUPPORT_STATUSINFO_PDU | RNS_UD_CS_STRONG_ASYMMETRIC_KEYS |
	                    RNS_UD_CS_RELATIVE_MOUSE_INPUT | RNS_UD_CS_VALID_CONNECTION_TYPE |
	                    RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU |
	                    RNS_UD_CS_SUPPORT_NETCHAR_AUTODETECT |
	                    RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL | RNS_UD_CS_SUPPORT_DYNAMIC_TIME_ZONE |
	                    RNS_UD_CS_SUPPORT_HEARTBEAT_PDU | RNS_UD_CS_SUPPORT_SKIP_CHANNELJOIN;
	const UINT32 unknown = flags & (~mask);

	if (flags & RNS_UD_CS_SUPPORT_ERRINFO_PDU)
		winpr_str_append("RNS_UD_CS_SUPPORT_ERRINFO_PDU", buffer, size, "|");
	if (flags & RNS_UD_CS_WANT_32BPP_SESSION)
		winpr_str_append("RNS_UD_CS_WANT_32BPP_SESSION", buffer, size, "|");
	if (flags & RNS_UD_CS_SUPPORT_STATUSINFO_PDU)
		winpr_str_append("RNS_UD_CS_SUPPORT_STATUSINFO_PDU", buffer, size, "|");
	if (flags & RNS_UD_CS_STRONG_ASYMMETRIC_KEYS)
		winpr_str_append("RNS_UD_CS_STRONG_ASYMMETRIC_KEYS", buffer, size, "|");
	if (flags & RNS_UD_CS_RELATIVE_MOUSE_INPUT)
		winpr_str_append("RNS_UD_CS_RELATIVE_MOUSE_INPUT", buffer, size, "|");
	if (flags & RNS_UD_CS_VALID_CONNECTION_TYPE)
		winpr_str_append("RNS_UD_CS_VALID_CONNECTION_TYPE", buffer, size, "|");
	if (flags & RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU)
		winpr_str_append("RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU", buffer, size, "|");
	if (flags & RNS_UD_CS_SUPPORT_NETCHAR_AUTODETECT)
		winpr_str_append("RNS_UD_CS_SUPPORT_NETCHAR_AUTODETECT", buffer, size, "|");
	if (flags & RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL)
		winpr_str_append("RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL", buffer, size, "|");
	if (flags & RNS_UD_CS_SUPPORT_DYNAMIC_TIME_ZONE)
		winpr_str_append("RNS_UD_CS_SUPPORT_DYNAMIC_TIME_ZONE", buffer, size, "|");
	if (flags & RNS_UD_CS_SUPPORT_HEARTBEAT_PDU)
		winpr_str_append("RNS_UD_CS_SUPPORT_HEARTBEAT_PDU", buffer, size, "|");
	if (flags & RNS_UD_CS_SUPPORT_SKIP_CHANNELJOIN)
		winpr_str_append("RNS_UD_CS_SUPPORT_SKIP_CHANNELJOIN", buffer, size, "|");

	if (unknown != 0)
	{
		(void)_snprintf(msg, sizeof(msg), "RNS_UD_CS_UNKNOWN[0x%08" PRIx32 "]", unknown);
		winpr_str_append(msg, buffer, size, "|");
	}
	(void)_snprintf(msg, sizeof(msg), "[0x%08" PRIx32 "]", flags);
	winpr_str_append(msg, buffer, size, "|");
	return buffer;
}

static DWORD rdp_version_common(wLog* log, DWORD serverVersion, DWORD clientVersion)
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
		case RDP_VERSION_10_8:
		case RDP_VERSION_10_9:
		case RDP_VERSION_10_10:
		case RDP_VERSION_10_11:
		case RDP_VERSION_10_12:
			return version;

		default:
			WLog_Print(log, WLOG_ERROR,
			           "Invalid client [%" PRIu32 "] and server [%" PRIu32 "] versions",
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

static const BYTE h221_cs_key[4] = { 'D', 'u', 'c', 'a' };
static const BYTE h221_sc_key[4] = { 'M', 'c', 'D', 'n' };

/**
 * Read a GCC Conference Create Request.
 * msdn{cc240836}
 *
 * @param s stream
 * @param mcs The MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_read_conference_create_request(wStream* s, rdpMcs* mcs)
{
	UINT16 length = 0;
	BYTE choice = 0;
	BYTE number = 0;
	BYTE selection = 0;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);
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

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, length))
		return FALSE;

	if (!gcc_read_client_data_blocks(s, mcs, length))
		return FALSE;

	return TRUE;
}

/**
 * Write a GCC Conference Create Request.
 * msdn{cc240836}
 *
 * @param s stream
 * @param userData client data blocks
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_write_conference_create_request(wStream* s, wStream* userData)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(userData);
	/* ConnectData */
	if (!per_write_choice(s, 0)) /* From Key select object (0) of type OBJECT_IDENTIFIER */
		return FALSE;
	if (!per_write_object_identifier(s, t124_02_98_oid)) /* ITU-T T.124 (02/98) OBJECT_IDENTIFIER */
		return FALSE;
	/* ConnectData::connectPDU (OCTET_STRING) */
	const size_t pos = Stream_GetPosition(userData);
	WINPR_ASSERT(pos <= UINT16_MAX - 14);
	if (!per_write_length(s, (UINT16)pos + 14)) /* connectPDU length */
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
	const size_t upos = Stream_GetPosition(userData);
	WINPR_ASSERT(upos <= UINT16_MAX);
	return per_write_octet_string(s, Stream_Buffer(userData), (UINT16)upos,
	                              0); /* array of client data blocks */
}

BOOL gcc_read_conference_create_response(wStream* s, rdpMcs* mcs)
{
	UINT16 length = 0;
	UINT32 tag = 0;
	UINT16 nodeID = 0;
	BYTE result = 0;
	BYTE choice = 0;
	BYTE number = 0;
	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);
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
		WLog_Print(mcs->log, WLOG_ERROR,
		           "gcc_read_conference_create_response: gcc_read_server_data_blocks failed");
		return FALSE;
	}

	return TRUE;
}

BOOL gcc_write_conference_create_response(wStream* s, wStream* userData)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(userData);
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
	const size_t pos = Stream_GetPosition(userData);
	WINPR_ASSERT(pos <= UINT16_MAX);
	return per_write_octet_string(s, Stream_Buffer(userData), (UINT16)pos,
	                              0); /* array of server data blocks */
}

static BOOL gcc_read_client_unused1_data(wStream* s)
{
	return Stream_SafeSeek(s, 2);
}

BOOL gcc_read_client_data_blocks(wStream* s, rdpMcs* mcs, UINT16 length)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	BOOL gotMultitransport = FALSE;

	while (length > 0)
	{
		wStream sbuffer = { 0 };
		UINT16 type = 0;
		UINT16 blockLength = 0;

		if (!gcc_read_user_data_header(mcs->log, s, &type, &blockLength))
			return FALSE;

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, (size_t)(blockLength - 4)))
			return FALSE;

		wStream* sub = Stream_StaticConstInit(&sbuffer, Stream_Pointer(s), blockLength - 4);
		WINPR_ASSERT(sub);

		Stream_Seek(s, blockLength - 4);

		{
			char buffer[64] = { 0 };
			WLog_Print(mcs->log, WLOG_TRACE, "Processing block %s",
			           gcc_block_type_string(type, buffer, sizeof(buffer)));
		}
		switch (type)
		{
			case CS_CORE:
				if (!gcc_read_client_core_data(sub, mcs))
					return FALSE;

				break;

			case CS_SECURITY:
				if (!gcc_read_client_security_data(sub, mcs))
					return FALSE;

				break;

			case CS_NET:
				if (!gcc_read_client_network_data(sub, mcs))
					return FALSE;

				break;

			case CS_CLUSTER:
				if (!gcc_read_client_cluster_data(sub, mcs))
					return FALSE;

				break;

			case CS_MONITOR:
				if (!gcc_read_client_monitor_data(sub, mcs))
					return FALSE;

				break;

			case CS_MCS_MSGCHANNEL:
				if (!gcc_read_client_message_channel_data(sub, mcs))
					return FALSE;

				break;

			case CS_MONITOR_EX:
				if (!gcc_read_client_monitor_extended_data(sub, mcs))
					return FALSE;

				break;

			case CS_UNUSED1:
				if (!gcc_read_client_unused1_data(sub))
					return FALSE;

				break;

			case 0xC009:
			case CS_MULTITRANSPORT:
				gotMultitransport = TRUE;
				if (!gcc_read_client_multitransport_channel_data(sub, mcs))
					return FALSE;

				break;

			default:
				WLog_Print(mcs->log, WLOG_ERROR, "Unknown GCC client data block: 0x%04" PRIX16 "",
				           type);
				winpr_HexLogDump(mcs->log, WLOG_TRACE, Stream_Pointer(sub),
				                 Stream_GetRemainingLength(sub));
				break;
		}

		const size_t rem = Stream_GetRemainingLength(sub);
		if (rem > 0)
		{
			char buffer[128] = { 0 };
			const size_t total = Stream_Length(sub);
			WLog_Print(mcs->log, WLOG_ERROR,
			           "Error parsing GCC client data block %s: Actual Offset: %" PRIuz
			           " Expected Offset: %" PRIuz,
			           gcc_block_type_string(type, buffer, sizeof(buffer)), total - rem, total);
		}

		if (blockLength > length)
		{
			char buffer[128] = { 0 };
			WLog_Print(mcs->log, WLOG_ERROR,
			           "Error parsing GCC client data block %s: got blockLength 0x%04" PRIx16
			           ", but only 0x%04" PRIx16 "remaining",
			           gcc_block_type_string(type, buffer, sizeof(buffer)), blockLength, length);
			length = 0;
		}
		else
			length -= blockLength;
	}

	if (!gotMultitransport)
	{
		rdpSettings* settings = mcs_get_settings(mcs);
		if (!freerdp_settings_set_bool(settings, FreeRDP_SupportMultitransport, FALSE))
			return FALSE;
		if (!freerdp_settings_set_uint32(settings, FreeRDP_MultitransportFlags, 0))
			return FALSE;
	}
	return TRUE;
}

BOOL gcc_write_client_data_blocks(wStream* s, const rdpMcs* mcs)
{
	const rdpSettings* settings = mcs_get_const_settings(mcs);

	WINPR_ASSERT(s);
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
			WLog_Print(mcs->log, WLOG_ERROR,
			           "WARNING: true multi monitor support was not advertised by server!");

			if (settings->ForceMultimon)
			{
				WLog_Print(mcs->log, WLOG_ERROR,
				           "Sending multi monitor information anyway (may break connectivity!)");
				if (!gcc_write_client_monitor_data(s, mcs) ||
				    !gcc_write_client_monitor_extended_data(s, mcs))
					return FALSE;
			}
			else
			{
				WLog_Print(mcs->log, WLOG_ERROR,
				           "Use /multimon:force to force sending multi monitor information");
			}
		}
	}
	return TRUE;
}

char* gcc_block_type_string(UINT16 type, char* buffer, size_t size)
{
	switch (type)
	{
		case CS_CORE:
			(void)_snprintf(buffer, size, "CS_CORE [0x%04" PRIx16 "]", type);
			break;
		case CS_SECURITY:
			(void)_snprintf(buffer, size, "CS_SECURITY [0x%04" PRIx16 "]", type);
			break;
		case CS_NET:
			(void)_snprintf(buffer, size, "CS_NET [0x%04" PRIx16 "]", type);
			break;
		case CS_CLUSTER:
			(void)_snprintf(buffer, size, "CS_CLUSTER [0x%04" PRIx16 "]", type);
			break;
		case CS_MONITOR:
			(void)_snprintf(buffer, size, "CS_MONITOR [0x%04" PRIx16 "]", type);
			break;
		case CS_MCS_MSGCHANNEL:
			(void)_snprintf(buffer, size, "CS_MONITOR [0x%04" PRIx16 "]", type);
			break;
		case CS_MONITOR_EX:
			(void)_snprintf(buffer, size, "CS_MONITOR_EX [0x%04" PRIx16 "]", type);
			break;
		case CS_UNUSED1:
			(void)_snprintf(buffer, size, "CS_UNUSED1 [0x%04" PRIx16 "]", type);
			break;
		case CS_MULTITRANSPORT:
			(void)_snprintf(buffer, size, "CS_MONITOR_EX [0x%04" PRIx16 "]", type);
			break;
		case SC_CORE:
			(void)_snprintf(buffer, size, "SC_CORE [0x%04" PRIx16 "]", type);
			break;
		case SC_SECURITY:
			(void)_snprintf(buffer, size, "SC_SECURITY [0x%04" PRIx16 "]", type);
			break;
		case SC_NET:
			(void)_snprintf(buffer, size, "SC_NET [0x%04" PRIx16 "]", type);
			break;
		case SC_MCS_MSGCHANNEL:
			(void)_snprintf(buffer, size, "SC_MCS_MSGCHANNEL [0x%04" PRIx16 "]", type);
			break;
		case SC_MULTITRANSPORT:
			(void)_snprintf(buffer, size, "SC_MULTITRANSPORT [0x%04" PRIx16 "]", type);
			break;
		default:
			(void)_snprintf(buffer, size, "UNKNOWN [0x%04" PRIx16 "]", type);
			break;
	}
	return buffer;
}

BOOL gcc_read_server_data_blocks(wStream* s, rdpMcs* mcs, UINT16 length)
{
	UINT16 type = 0;
	UINT16 offset = 0;
	UINT16 blockLength = 0;
	BYTE* holdp = NULL;

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	while (offset < length)
	{
		char buffer[64] = { 0 };
		size_t rest = 0;
		wStream subbuffer;
		wStream* sub = NULL;

		if (!gcc_read_user_data_header(mcs->log, s, &type, &blockLength))
		{
			WLog_Print(mcs->log, WLOG_ERROR,
			           "gcc_read_server_data_blocks: gcc_read_user_data_header failed");
			return FALSE;
		}
		holdp = Stream_Pointer(s);
		sub = Stream_StaticInit(&subbuffer, holdp, blockLength - 4);
		if (!Stream_SafeSeek(s, blockLength - 4))
		{
			WLog_Print(mcs->log, WLOG_ERROR, "gcc_read_server_data_blocks: stream too short");
			return FALSE;
		}
		offset += blockLength;

		switch (type)
		{
			case SC_CORE:
				if (!gcc_read_server_core_data(sub, mcs))
				{
					WLog_Print(mcs->log, WLOG_ERROR,
					           "gcc_read_server_data_blocks: gcc_read_server_core_data failed");
					return FALSE;
				}

				break;

			case SC_SECURITY:
				if (!gcc_read_server_security_data(sub, mcs))
					return FALSE;
				break;

			case SC_NET:
				if (!gcc_read_server_network_data(sub, mcs))
				{
					WLog_Print(mcs->log, WLOG_ERROR,
					           "gcc_read_server_data_blocks: gcc_read_server_network_data failed");
					return FALSE;
				}

				break;

			case SC_MCS_MSGCHANNEL:
				if (!gcc_read_server_message_channel_data(sub, mcs))
				{
					WLog_Print(
					    mcs->log, WLOG_ERROR,
					    "gcc_read_server_data_blocks: gcc_read_server_message_channel_data failed");
					return FALSE;
				}

				break;

			case SC_MULTITRANSPORT:
				if (!gcc_read_server_multitransport_channel_data(sub, mcs))
				{
					WLog_Print(mcs->log, WLOG_ERROR,
					           "gcc_read_server_data_blocks: "
					           "gcc_read_server_multitransport_channel_data failed");
					return FALSE;
				}

				break;

			default:
				WLog_Print(mcs->log, WLOG_ERROR, "gcc_read_server_data_blocks: ignoring type=%s",
				           gcc_block_type_string(type, buffer, sizeof(buffer)));
				winpr_HexLogDump(mcs->log, WLOG_TRACE, Stream_Pointer(sub),
				                 Stream_GetRemainingLength(sub));
				break;
		}

		rest = Stream_GetRemainingLength(sub);
		if (rest > 0)
		{
			WLog_Print(mcs->log, WLOG_WARN,
			           "gcc_read_server_data_blocks: ignoring %" PRIuz " bytes with type=%s", rest,
			           gcc_block_type_string(type, buffer, sizeof(buffer)));
		}
	}

	return TRUE;
}

BOOL gcc_write_server_data_blocks(wStream* s, rdpMcs* mcs)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	if (!gcc_write_server_core_data(s, mcs) ||          /* serverCoreData */
	    !gcc_write_server_network_data(s, mcs) ||       /* serverNetworkData */
	    !gcc_write_server_security_data(s, mcs) ||      /* serverSecurityData */
	    !gcc_write_server_message_channel_data(s, mcs)) /* serverMessageChannelData */
		return FALSE;

	const rdpSettings* settings = mcs_get_const_settings(mcs);
	WINPR_ASSERT(settings);

	if (settings->SupportMultitransport && (settings->MultitransportFlags != 0))
		/* serverMultitransportChannelData */
		return gcc_write_server_multitransport_channel_data(s, mcs);

	return TRUE;
}

BOOL gcc_read_user_data_header(wLog* log, wStream* s, UINT16* type, UINT16* length)
{
	WINPR_ASSERT(s);
	if (!Stream_CheckAndLogRequiredLengthWLog(log, s, 4))
		return FALSE;

	Stream_Read_UINT16(s, *type);   /* type */
	Stream_Read_UINT16(s, *length); /* length */

	if ((*length < 4) || (!Stream_CheckAndLogRequiredLengthWLog(log, s, (size_t)(*length - 4))))
		return FALSE;

	return TRUE;
}

/**
 * Write a user data header (TS_UD_HEADER).
 * msdn{cc240509}
 *
 * @param s stream
 * @param type data block type
 * @param length data block length
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_write_user_data_header(wStream* s, UINT16 type, UINT16 length)
{

	WINPR_ASSERT(s);
	if (!Stream_EnsureRemainingCapacity(s, 4 + length))
		return FALSE;
	Stream_Write_UINT16(s, type);   /* type */
	Stream_Write_UINT16(s, length); /* length */
	return TRUE;
}

static UINT32 filterAndLogEarlyServerCapabilityFlags(wLog* log, UINT32 flags)
{
	const UINT32 mask =
	    (RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V1 | RNS_UD_SC_DYNAMIC_DST_SUPPORTED |
	     RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V2 | RNS_UD_SC_SKIP_CHANNELJOIN_SUPPORTED);
	const UINT32 filtered = flags & mask;
	const UINT32 unknown = flags & (~mask);
	if (unknown != 0)
	{
		char buffer[256] = { 0 };
		WLog_Print(log, WLOG_WARN,
		           "TS_UD_SC_CORE::EarlyCapabilityFlags [0x%08" PRIx32 " & 0x%08" PRIx32
		           " --> 0x%08" PRIx32 "] filtering %s, feature not implemented",
		           flags, ~mask, unknown,
		           rdp_early_server_caps_string(unknown, buffer, sizeof(buffer)));
	}
	return filtered;
}

static UINT32 earlyServerCapsFromSettings(wLog* log, const rdpSettings* settings)
{
	UINT32 EarlyCapabilityFlags = 0;

	if (settings->SupportEdgeActionV1)
		EarlyCapabilityFlags |= RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V1;
	if (settings->SupportDynamicTimeZone)
		EarlyCapabilityFlags |= RNS_UD_SC_DYNAMIC_DST_SUPPORTED;
	if (settings->SupportEdgeActionV2)
		EarlyCapabilityFlags |= RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V2;
	if (settings->SupportSkipChannelJoin)
		EarlyCapabilityFlags |= RNS_UD_SC_SKIP_CHANNELJOIN_SUPPORTED;

	return filterAndLogEarlyServerCapabilityFlags(log, EarlyCapabilityFlags);
}

static UINT16 filterAndLogEarlyClientCapabilityFlags(wLog* log, UINT32 flags)
{
	const UINT32 mask =
	    (RNS_UD_CS_SUPPORT_ERRINFO_PDU | RNS_UD_CS_WANT_32BPP_SESSION |
	     RNS_UD_CS_SUPPORT_STATUSINFO_PDU | RNS_UD_CS_STRONG_ASYMMETRIC_KEYS |
	     RNS_UD_CS_RELATIVE_MOUSE_INPUT | RNS_UD_CS_VALID_CONNECTION_TYPE |
	     RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU | RNS_UD_CS_SUPPORT_NETCHAR_AUTODETECT |
	     RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL | RNS_UD_CS_SUPPORT_DYNAMIC_TIME_ZONE |
	     RNS_UD_CS_SUPPORT_HEARTBEAT_PDU | RNS_UD_CS_SUPPORT_SKIP_CHANNELJOIN);
	const UINT32 filtered = flags & mask;
	const UINT32 unknown = flags & ~mask;
	if (unknown != 0)
	{
		char buffer[256] = { 0 };
		WLog_Print(log, WLOG_WARN,
		           "(TS_UD_CS_CORE)::EarlyCapabilityFlags [0x%08" PRIx32 " & 0x%08" PRIx32
		           " --> 0x%08" PRIx32 "] filtering %s, feature not implemented",
		           flags, ~mask, unknown,
		           rdp_early_client_caps_string(unknown, buffer, sizeof(buffer)));
	}

	WINPR_ASSERT(filtered <= UINT16_MAX);
	return (UINT16)filtered;
}

static UINT16 earlyClientCapsFromSettings(wLog* log, const rdpSettings* settings)
{
	UINT32 earlyCapabilityFlags = 0;

	WINPR_ASSERT(settings);
	if (settings->SupportErrorInfoPdu)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_ERRINFO_PDU;

	if (freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth) == 32)
		earlyCapabilityFlags |= RNS_UD_CS_WANT_32BPP_SESSION;

	if (settings->SupportStatusInfoPdu)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_STATUSINFO_PDU;

	if (settings->ConnectionType)
		earlyCapabilityFlags |= RNS_UD_CS_VALID_CONNECTION_TYPE;

	if (settings->SupportMonitorLayoutPdu)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU;

	if (freerdp_settings_get_bool(settings, FreeRDP_NetworkAutoDetect))
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_NETCHAR_AUTODETECT;

	if (settings->SupportGraphicsPipeline)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL;

	if (settings->SupportDynamicTimeZone)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_DYNAMIC_TIME_ZONE;

	if (settings->SupportHeartbeatPdu)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_HEARTBEAT_PDU;

	if (settings->SupportAsymetricKeys)
		earlyCapabilityFlags |= RNS_UD_CS_STRONG_ASYMMETRIC_KEYS;

	if (settings->HasRelativeMouseEvent)
		earlyCapabilityFlags |= RNS_UD_CS_RELATIVE_MOUSE_INPUT;

	if (settings->SupportSkipChannelJoin)
		earlyCapabilityFlags |= RNS_UD_CS_SUPPORT_SKIP_CHANNELJOIN;

	return filterAndLogEarlyClientCapabilityFlags(log, earlyCapabilityFlags);
}

static BOOL updateEarlyClientCaps(wLog* log, rdpSettings* settings, UINT32 earlyCapabilityFlags,
                                  UINT32 connectionType)
{
	WINPR_ASSERT(settings);

	if (settings->SupportErrorInfoPdu)
		settings->SupportErrorInfoPdu =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_ERRINFO_PDU) ? TRUE : FALSE;

	/* RNS_UD_CS_WANT_32BPP_SESSION is already handled in gcc_read_client_core_data:
	 *
	 * it is evaluated in combination with highColorDepth and the server side
	 * settings to determine the session color depth to use.
	 */

	if (settings->SupportStatusInfoPdu)
		settings->SupportStatusInfoPdu =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_STATUSINFO_PDU) ? TRUE : FALSE;

	if (settings->SupportAsymetricKeys)
		settings->SupportAsymetricKeys =
		    (earlyCapabilityFlags & RNS_UD_CS_STRONG_ASYMMETRIC_KEYS) ? TRUE : FALSE;

	if (settings->HasRelativeMouseEvent)
	{
		/* [MS-RDPBCGR] 2.2.7.1.5 Pointer Capability Set (TS_POINTER_CAPABILITYSET)
		 * the flag must be ignored if the RDP version is < 0x00080011 */
		if (settings->RdpVersion >= RDP_VERSION_10_12)
		{
			settings->HasRelativeMouseEvent =
			    (earlyCapabilityFlags & RNS_UD_CS_RELATIVE_MOUSE_INPUT) ? TRUE : FALSE;
		}
		else
			settings->HasRelativeMouseEvent = FALSE;
	}

	if (settings->NetworkAutoDetect)
		settings->NetworkAutoDetect =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_NETCHAR_AUTODETECT) ? TRUE : FALSE;

	if (settings->SupportSkipChannelJoin)
		settings->SupportSkipChannelJoin =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_SKIP_CHANNELJOIN) ? TRUE : FALSE;

	if (settings->SupportMonitorLayoutPdu)
		settings->SupportMonitorLayoutPdu =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_MONITOR_LAYOUT_PDU) ? TRUE : FALSE;

	if (settings->SupportHeartbeatPdu)
		settings->SupportHeartbeatPdu =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_HEARTBEAT_PDU) ? TRUE : FALSE;

	if (settings->SupportGraphicsPipeline)
		settings->SupportGraphicsPipeline =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_DYNVC_GFX_PROTOCOL) ? TRUE : FALSE;

	if (settings->SupportDynamicTimeZone)
		settings->SupportDynamicTimeZone =
		    (earlyCapabilityFlags & RNS_UD_CS_SUPPORT_DYNAMIC_TIME_ZONE) ? TRUE : FALSE;

	if ((earlyCapabilityFlags & RNS_UD_CS_VALID_CONNECTION_TYPE) == 0)
		connectionType = 0;
	settings->ConnectionType = connectionType;

	filterAndLogEarlyClientCapabilityFlags(log, earlyCapabilityFlags);
	return TRUE;
}

static BOOL updateEarlyServerCaps(wLog* log, rdpSettings* settings, UINT32 earlyCapabilityFlags,
                                  WINPR_ATTR_UNUSED UINT32 connectionType)
{
	WINPR_ASSERT(settings);

	settings->SupportEdgeActionV1 =
	    settings->SupportEdgeActionV1 &&
	            (earlyCapabilityFlags & RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V1)
	        ? TRUE
	        : FALSE;
	settings->SupportDynamicTimeZone =
	    settings->SupportDynamicTimeZone && (earlyCapabilityFlags & RNS_UD_SC_DYNAMIC_DST_SUPPORTED)
	        ? TRUE
	        : FALSE;
	settings->SupportEdgeActionV2 =
	    settings->SupportEdgeActionV2 &&
	            (earlyCapabilityFlags & RNS_UD_SC_EDGE_ACTIONS_SUPPORTED_V2)
	        ? TRUE
	        : FALSE;
	settings->SupportSkipChannelJoin =
	    settings->SupportSkipChannelJoin &&
	            (earlyCapabilityFlags & RNS_UD_SC_SKIP_CHANNELJOIN_SUPPORTED)
	        ? TRUE
	        : FALSE;

	filterAndLogEarlyServerCapabilityFlags(log, earlyCapabilityFlags);
	return TRUE;
}

/**
 * Read a client core data block (TS_UD_CS_CORE).
 * msdn{cc240510}
 * @param s stream
 * @param mcs The MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_read_client_core_data(wStream* s, rdpMcs* mcs)
{
	char buffer[2048] = { 0 };
	char strbuffer[130] = { 0 };
	UINT32 version = 0;
	BYTE connectionType = 0;
	UINT32 clientColorDepth = 0;
	UINT16 colorDepth = 0;
	UINT16 postBeta2ColorDepth = 0;
	UINT16 highColorDepth = 0;
	UINT32 serverSelectedProtocol = 0;
	rdpSettings* settings = mcs_get_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	/* Length of all required fields, until imeFileName */
	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 128))
		return FALSE;

	Stream_Read_UINT32(s, version); /* version (4 bytes) */
	settings->RdpVersion = rdp_version_common(mcs->log, version, settings->RdpVersion);
	Stream_Read_UINT16(s, settings->DesktopWidth);  /* DesktopWidth (2 bytes) */
	Stream_Read_UINT16(s, settings->DesktopHeight); /* DesktopHeight (2 bytes) */
	Stream_Read_UINT16(s, colorDepth);              /* ColorDepth (2 bytes) */
	Stream_Seek_UINT16(s); /* SASSequence (Secure Access Sequence) (2 bytes) */
	Stream_Read_UINT32(s, settings->KeyboardLayout); /* KeyboardLayout (4 bytes) */
	Stream_Read_UINT32(s, settings->ClientBuild);    /* ClientBuild (4 bytes) */

	/* clientName (32 bytes, null-terminated unicode, truncated to 15 characters) */
	if (Stream_Read_UTF16_String_As_UTF8_Buffer(s, 32 / sizeof(WCHAR), strbuffer,
	                                            ARRAYSIZE(strbuffer)) < 0)
	{
		WLog_Print(mcs->log, WLOG_ERROR, "failed to convert client host name");
		return FALSE;
	}

	if (!freerdp_settings_set_string(settings, FreeRDP_ClientHostname, strbuffer))
		return FALSE;

	Stream_Read_UINT32(s, settings->KeyboardType);        /* KeyboardType (4 bytes) */
	Stream_Read_UINT32(s, settings->KeyboardSubType);     /* KeyboardSubType (4 bytes) */
	Stream_Read_UINT32(s, settings->KeyboardFunctionKey); /* KeyboardFunctionKey (4 bytes) */
	Stream_Seek(s, 64);                                   /* imeFileName (64 bytes) */

	/**
	 * The following fields are all optional. If one field is present, all of the preceding
	 * fields MUST also be present. If one field is not present, all of the subsequent fields
	 * MUST NOT be present.
	 * We must check the bytes left before reading each field.
	 */

	do
	{
		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 2))
			break;

		Stream_Read_UINT16(s, postBeta2ColorDepth); /* postBeta2ColorDepth (2 bytes) */

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 2))
			break;

		const UINT16 clientProductId = Stream_Get_UINT16(s); /* clientProductID (2 bytes) */

		/* [MS-RDPBCGR] 2.2.1.3.2 Client Core Data (TS_UD_CS_CORE)::clientProductId (optional)
		 * should be initialized to 1
		 */
		if (clientProductId != 1)
		{
			WLog_Print(mcs->log, WLOG_WARN,
			           "[MS-RDPBCGR] 2.2.1.3.2 Client Core Data (TS_UD_CS_CORE)::clientProductId "
			           "(optional) expected 1, got %" PRIu32,
			           clientProductId);
		}

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
			break;

		const UINT32 serialNumber = Stream_Get_UINT32(s); /* serialNumber (4 bytes) */

		/* [MS-RDPBCGR] 2.2.1.3.2 Client Core Data (TS_UD_CS_CORE)::serialNumber (optional)
		 * should be initialized to 0
		 */
		if (serialNumber != 0)
		{
			WLog_Print(mcs->log, WLOG_WARN,
			           "[MS-RDPBCGR] 2.2.1.3.2 Client Core Data (TS_UD_CS_CORE)::serialNumber "
			           "(optional) expected 0, got %" PRIu32,
			           serialNumber);
		}

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 2))
			break;

		Stream_Read_UINT16(s, highColorDepth); /* highColorDepth (2 bytes) */

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 2))
			break;

		Stream_Read_UINT16(s, settings->SupportedColorDepths); /* supportedColorDepths (2 bytes) */

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 2))
			break;

		Stream_Read_UINT16(s, settings->EarlyCapabilityFlags); /* earlyCapabilityFlags (2 bytes) */

		/* clientDigProductId (64 bytes): Contains a value that uniquely identifies the client */
		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 64))
			break;

		if (Stream_Read_UTF16_String_As_UTF8_Buffer(s, 64 / sizeof(WCHAR), strbuffer,
		                                            ARRAYSIZE(strbuffer)) < 0)
		{
			WLog_Print(mcs->log, WLOG_ERROR, "failed to convert the client product identifier");
			return FALSE;
		}

		if (!freerdp_settings_set_string(settings, FreeRDP_ClientProductId, strbuffer))
			return FALSE;

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 1))
			break;

		Stream_Read_UINT8(s, connectionType); /* connectionType (1 byte) */

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 1))
			break;

		Stream_Seek_UINT8(s); /* pad1octet (1 byte) */

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
			break;

		Stream_Read_UINT32(s, serverSelectedProtocol); /* serverSelectedProtocol (4 bytes) */

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
			break;

		Stream_Read_UINT32(s, settings->DesktopPhysicalWidth); /* desktopPhysicalWidth (4 bytes) */

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
			break;

		Stream_Read_UINT32(s,
		                   settings->DesktopPhysicalHeight); /* desktopPhysicalHeight (4 bytes) */

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 2))
			break;

		Stream_Read_UINT16(s, settings->DesktopOrientation); /* desktopOrientation (2 bytes) */

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
			break;

		Stream_Read_UINT32(s, settings->DesktopScaleFactor); /* desktopScaleFactor (4 bytes) */

		if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
			break;

		Stream_Read_UINT32(s, settings->DeviceScaleFactor); /* deviceScaleFactor (4 bytes) */

		if (freerdp_settings_get_bool(settings, FreeRDP_TransportDumpReplay))
			settings->SelectedProtocol = serverSelectedProtocol;
		else if (settings->SelectedProtocol != serverSelectedProtocol)
			return FALSE;
	} while (0);

	if (highColorDepth > 0)
	{
		if (settings->EarlyCapabilityFlags & RNS_UD_CS_WANT_32BPP_SESSION)
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
	if ((clientColorDepth < freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth)) ||
	    !settings->ServerMode)
	{
		if (!freerdp_settings_set_uint32(settings, FreeRDP_ColorDepth, clientColorDepth))
			return FALSE;
	}

	WLog_Print(
	    mcs->log, WLOG_DEBUG, "Received EarlyCapabilityFlags=%s",
	    rdp_early_client_caps_string(settings->EarlyCapabilityFlags, buffer, sizeof(buffer)));

	return updateEarlyClientCaps(mcs->log, settings, settings->EarlyCapabilityFlags,
	                             connectionType);
}

/**
 * Write a client core data block (TS_UD_CS_CORE).
 * msdn{cc240510}
 * @param s The stream to write to
 * @param mcs The MSC instance to get the data from
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_write_client_core_data(wStream* s, const rdpMcs* mcs)
{
	char buffer[2048] = { 0 };
	char dbuffer[2048] = { 0 };
	BYTE connectionType = 0;
	HIGH_COLOR_DEPTH highColorDepth = HIGH_COLOR_4BPP;

	UINT16 earlyCapabilityFlags = 0;
	const rdpSettings* settings = mcs_get_const_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	const UINT16 SupportedColorDepths =
	    freerdp_settings_get_uint16(settings, FreeRDP_SupportedColorDepths);
	const UINT32 ColorDepth = freerdp_settings_get_uint32(settings, FreeRDP_ColorDepth);

	if (!gcc_write_user_data_header(s, CS_CORE, 234))
		return FALSE;

	Stream_Write_UINT32(s, settings->RdpVersion); /* Version */
	Stream_Write_UINT16(
	    s, WINPR_ASSERTING_INT_CAST(uint16_t, settings->DesktopWidth)); /* DesktopWidth */
	Stream_Write_UINT16(
	    s, WINPR_ASSERTING_INT_CAST(uint16_t, settings->DesktopHeight)); /* DesktopHeight */
	Stream_Write_UINT16(s,
	                    RNS_UD_COLOR_8BPP); /* ColorDepth, ignored because of postBeta2ColorDepth */
	Stream_Write_UINT16(s, RNS_UD_SAS_DEL); /* SASSequence (Secure Access Sequence) */
	Stream_Write_UINT32(s, settings->KeyboardLayout); /* KeyboardLayout */
	Stream_Write_UINT32(s, settings->ClientBuild);    /* ClientBuild */

	if (!Stream_EnsureRemainingCapacity(s, 32 + 12 + 64 + 8))
		return FALSE;

	/* clientName (32 bytes, null-terminated unicode, truncated to 15 characters) */
	size_t clientNameLength = 0;
	WCHAR* clientName = ConvertUtf8ToWCharAlloc(settings->ClientHostname, &clientNameLength);
	if (clientNameLength >= 16)
	{
		clientNameLength = 16;
		clientName[clientNameLength - 1] = 0;
	}

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
	highColorDepth = ColorDepthToHighColor(ColorDepth);
	earlyCapabilityFlags = earlyClientCapsFromSettings(mcs->log, settings);

	WINPR_ASSERT(settings->ConnectionType <= UINT8_MAX);
	connectionType = (UINT8)settings->ConnectionType;

	if (!Stream_EnsureRemainingCapacity(s, 6))
		return FALSE;

	WLog_Print(
	    mcs->log, WLOG_DEBUG,
	    "Sending highColorDepth=%s, supportedColorDepths=%s, earlyCapabilityFlags=%s",
	    HighColorToString(highColorDepth),
	    freerdp_supported_color_depths_string(SupportedColorDepths, dbuffer, sizeof(dbuffer)),
	    rdp_early_client_caps_string(earlyCapabilityFlags, buffer, sizeof(buffer)));
	Stream_Write_UINT16(s, WINPR_ASSERTING_INT_CAST(uint16_t, highColorDepth)); /* highColorDepth */
	Stream_Write_UINT16(s, SupportedColorDepths); /* supportedColorDepths */
	Stream_Write_UINT16(s, earlyCapabilityFlags); /* earlyCapabilityFlags */

	if (!Stream_EnsureRemainingCapacity(s, 64 + 24))
		return FALSE;

	/* clientDigProductId (64 bytes, assume WCHAR, not \0 terminated */
	const char* str = freerdp_settings_get_string(settings, FreeRDP_ClientProductId);
	if (str)
	{
		if (Stream_Write_UTF16_String_From_UTF8(s, 32, str, strnlen(str, 32), TRUE) < 0)
			return FALSE;
	}
	else
		Stream_Zero(s, 32 * sizeof(WCHAR));

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
	UINT32 serverVersion = 0;
	rdpSettings* settings = mcs_get_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, serverVersion); /* version */
	settings->RdpVersion = rdp_version_common(mcs->log, serverVersion, settings->RdpVersion);

	if (Stream_GetRemainingLength(s) >= 4)
	{
		Stream_Read_UINT32(s, settings->RequestedProtocols); /* clientRequestedProtocols */
	}

	if (Stream_GetRemainingLength(s) >= 4)
	{
		char buffer[2048] = { 0 };

		Stream_Read_UINT32(s, settings->EarlyCapabilityFlags); /* earlyCapabilityFlags */
		WLog_Print(
		    mcs->log, WLOG_DEBUG, "Received EarlyCapabilityFlags=%s",
		    rdp_early_client_caps_string(settings->EarlyCapabilityFlags, buffer, sizeof(buffer)));
	}

	return updateEarlyServerCaps(mcs->log, settings, settings->EarlyCapabilityFlags,
	                             settings->ConnectionType);
}

/* TODO: This function modifies rdpMcs
 * TODO:  Split this out of this function
 */
BOOL gcc_write_server_core_data(wStream* s, rdpMcs* mcs)
{
	const rdpSettings* settings = mcs_get_const_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (!gcc_write_user_data_header(s, SC_CORE, 16))
		return FALSE;

	const UINT32 EarlyCapabilityFlags = earlyServerCapsFromSettings(mcs->log, settings);
	Stream_Write_UINT32(s, settings->RdpVersion);         /* version (4 bytes) */
	Stream_Write_UINT32(s, settings->RequestedProtocols); /* clientRequestedProtocols (4 bytes) */
	Stream_Write_UINT32(s, EarlyCapabilityFlags);         /* earlyCapabilityFlags (4 bytes) */
	return TRUE;
}

/**
 * Read a client security data block (TS_UD_CS_SEC).
 * msdn{cc240511}
 * @param s stream
 * @param mcs MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_read_client_security_data(wStream* s, rdpMcs* mcs)
{
	rdpSettings* settings = mcs_get_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 8))
		return FALSE;

	if (settings->UseRdpSecurityLayer)
	{
		Stream_Read_UINT32(s, settings->EncryptionMethods); /* encryptionMethods */

		if (settings->EncryptionMethods == ENCRYPTION_METHOD_NONE)
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
 * Write a client security data block (TS_UD_CS_SEC).
 * msdn{cc240511}
 * @param s stream
 * @param mcs The MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_write_client_security_data(wStream* s, const rdpMcs* mcs)
{
	const rdpSettings* settings = mcs_get_const_settings(mcs);

	WINPR_ASSERT(s);
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
	BOOL validCryptoConfig = FALSE;
	UINT32 EncryptionMethod = 0;
	UINT32 EncryptionLevel = 0;
	rdpSettings* settings = mcs_get_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, EncryptionMethod); /* encryptionMethod */
	Stream_Read_UINT32(s, EncryptionLevel);  /* encryptionLevel */

	/* Only accept valid/known encryption methods */
	switch (EncryptionMethod)
	{
		case ENCRYPTION_METHOD_NONE:
			WLog_Print(mcs->log, WLOG_DEBUG, "Server rdp encryption method: NONE");
			break;

		case ENCRYPTION_METHOD_40BIT:
			WLog_Print(mcs->log, WLOG_DEBUG, "Server rdp encryption method: 40BIT");
			break;

		case ENCRYPTION_METHOD_56BIT:
			WLog_Print(mcs->log, WLOG_DEBUG, "Server rdp encryption method: 56BIT");
			break;

		case ENCRYPTION_METHOD_128BIT:
			WLog_Print(mcs->log, WLOG_DEBUG, "Server rdp encryption method: 128BIT");
			break;

		case ENCRYPTION_METHOD_FIPS:
			WLog_Print(mcs->log, WLOG_DEBUG, "Server rdp encryption method: FIPS");
			break;

		default:
			WLog_Print(mcs->log, WLOG_ERROR, "Received unknown encryption method %08" PRIX32 "",
			           EncryptionMethod);
			return FALSE;
	}

	if (settings->UseRdpSecurityLayer && !(settings->EncryptionMethods & EncryptionMethod))
	{
		WLog_Print(mcs->log, WLOG_WARN,
		           "Server uses non-advertised encryption method 0x%08" PRIX32 "",
		           EncryptionMethod);
		/* FIXME: Should we return FALSE; in this case ?? */
	}

	settings->EncryptionMethods = EncryptionMethod;
	settings->EncryptionLevel = EncryptionLevel;
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
			WLog_Print(mcs->log, WLOG_ERROR, "Received unknown encryption level 0x%08" PRIX32 "",
			           settings->EncryptionLevel);
	}

	if (!validCryptoConfig)
	{
		WLog_Print(mcs->log, WLOG_ERROR,
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

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, settings->ServerRandomLength);      /* serverRandomLen */
	Stream_Read_UINT32(s, settings->ServerCertificateLength); /* serverCertLen */

	if ((settings->ServerRandomLength == 0) || (settings->ServerCertificateLength == 0))
	{
		WLog_Print(mcs->log, WLOG_ERROR,
		           "Invalid ServerRandom (length=%" PRIu32 ") or ServerCertificate (length=%" PRIu32
		           ")",
		           settings->ServerRandomLength, settings->ServerCertificateLength);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, settings->ServerRandomLength))
		return FALSE;

	/* serverRandom */
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerRandom, NULL,
	                                      settings->ServerRandomLength))
		goto fail;

	Stream_Read(s, settings->ServerRandom, settings->ServerRandomLength);

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, settings->ServerCertificateLength))
		goto fail;

	/* serverCertificate */
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerCertificate, NULL,
	                                      settings->ServerCertificateLength))
		goto fail;

	Stream_Read(s, settings->ServerCertificate, settings->ServerCertificateLength);

	{
		const BYTE* data = settings->ServerCertificate;
		const uint32_t length = settings->ServerCertificateLength;

		if (!freerdp_certificate_read_server_cert(settings->RdpServerCertificate, data, length))
			goto fail;
	}
	return TRUE;
fail:
	(void)freerdp_settings_set_pointer_len(settings, FreeRDP_ServerRandom, NULL, 0);
	(void)freerdp_settings_set_pointer_len(settings, FreeRDP_ServerCertificate, NULL, 0);
	return FALSE;
}

static BOOL gcc_update_server_random(rdpSettings* settings)
{
	const size_t length = 32;
	WINPR_ASSERT(settings);
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ServerRandom, NULL, length))
		return FALSE;
	BYTE* data = freerdp_settings_get_pointer_writable(settings, FreeRDP_ServerRandom);
	if (!data)
		return FALSE;
	winpr_RAND(data, length);
	return TRUE;
}

/* TODO: This function does manipulate data in rdpMcs
 * TODO: Split this out of this function
 */
BOOL gcc_write_server_security_data(wStream* s, rdpMcs* mcs)
{
	if (!gcc_update_server_random(mcs_get_settings(mcs)))
		return FALSE;

	const rdpSettings* settings = mcs_get_const_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	const size_t posHeader = Stream_GetPosition(s);
	if (!gcc_write_user_data_header(s, SC_SECURITY, 12))
		return FALSE;

	Stream_Write_UINT32(s, settings->EncryptionMethods); /* encryptionMethod */
	Stream_Write_UINT32(s, settings->EncryptionLevel);   /* encryptionLevel */

	if (settings->EncryptionMethods == ENCRYPTION_METHOD_NONE)
		return TRUE;

	if (!Stream_EnsureRemainingCapacity(s, sizeof(UINT32) + settings->ServerRandomLength))
		return FALSE;
	Stream_Write_UINT32(s, settings->ServerRandomLength); /* serverRandomLen */
	const size_t posCertLen = Stream_GetPosition(s);
	Stream_Seek_UINT32(s); /* serverCertLen */
	Stream_Write(s, settings->ServerRandom, settings->ServerRandomLength);

	const SSIZE_T len = freerdp_certificate_write_server_cert(
	    settings->RdpServerCertificate, CERT_TEMPORARILY_ISSUED | CERT_CHAIN_VERSION_1, s);
	if (len < 0)
		return FALSE;
	const size_t end = Stream_GetPosition(s);

	WINPR_ASSERT(end >= posHeader);
	const size_t diff = end - posHeader;
	WINPR_ASSERT(diff <= UINT16_MAX);
	Stream_SetPosition(s, posHeader);
	if (!gcc_write_user_data_header(s, SC_SECURITY, (UINT16)diff))
		return FALSE;
	Stream_SetPosition(s, posCertLen);
	WINPR_ASSERT(len <= UINT32_MAX);
	Stream_Write_UINT32(s, (UINT32)len);
	Stream_SetPosition(s, end);
	return TRUE;
}

/**
 * Read a client network data block (TS_UD_CS_NET).
 * msdn{cc240512}
 *
 * @param s stream
 * @param mcs The MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_read_client_network_data(wStream* s, rdpMcs* mcs)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, mcs->channelCount); /* channelCount */

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(mcs->log, s, mcs->channelCount, 12ull))
		return FALSE;

	if (mcs->channelCount > CHANNEL_MAX_COUNT)
	{
		WLog_Print(mcs->log, WLOG_ERROR, "rdpMcs::channelCount %" PRIu32 " > maximum %d",
		           mcs->channelCount, CHANNEL_MAX_COUNT);
		return FALSE;
	}

	/* channelDefArray */
	for (UINT32 i = 0; i < mcs->channelCount; i++)
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
			WLog_Print(
			    mcs->log, WLOG_ERROR,
			    "protocol violation: received a static channel name with missing null-termination");
			return FALSE;
		}

		Stream_Read_UINT32(s, channel->options); /* options (4 bytes) */
		channel->ChannelId = mcs->baseChannelId++;
	}

	return TRUE;
}

/**
 * Write a client network data block (TS_UD_CS_NET).
 * msdn{cc240512}
 * @param s stream
 * @param mcs The MCS to use
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_write_client_network_data(wStream* s, const rdpMcs* mcs)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);
	if (mcs->channelCount > 0)
	{
		const size_t length = mcs->channelCount * 12 + 8;
		WINPR_ASSERT(length <= UINT16_MAX);
		if (!gcc_write_user_data_header(s, CS_NET, (UINT16)length))
			return FALSE;
		Stream_Write_UINT32(s, mcs->channelCount); /* channelCount */

		/* channelDefArray */
		for (UINT32 i = 0; i < mcs->channelCount; i++)
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
	UINT16 channelId = 0;
	UINT32 parsedChannelCount = 0;
	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);
	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
		return FALSE;

	mcs->IOChannelId = Stream_Get_UINT16(s);            /* MCSChannelId */
	const uint16_t channelCount = Stream_Get_UINT16(s); /* channelCount */
	parsedChannelCount = channelCount;

	if (channelCount != mcs->channelCount)
	{
		WLog_Print(mcs->log, WLOG_ERROR, "requested %" PRIu32 " channels, got %" PRIu16 " instead",
		           mcs->channelCount, channelCount);

		/* we ensure that the response is not bigger than the request */

		mcs->channelCount = channelCount;
	}

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(mcs->log, s, channelCount, 2ull))
		return FALSE;

	if (mcs->channelMaxCount < parsedChannelCount)
	{
		WLog_Print(mcs->log, WLOG_ERROR,
		           "requested %" PRIu32 " channels > channelMaxCount %" PRIu16, mcs->channelCount,
		           mcs->channelMaxCount);
		return FALSE;
	}

	for (UINT32 i = 0; i < parsedChannelCount; i++)
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
	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);
	const size_t payloadLen = 8 + mcs->channelCount * 2 + (mcs->channelCount % 2 == 1 ? 2 : 0);

	WINPR_ASSERT(payloadLen <= UINT16_MAX);
	if (!gcc_write_user_data_header(s, SC_NET, (UINT16)payloadLen))
		return FALSE;

	Stream_Write_UINT16(s, MCS_GLOBAL_CHANNEL_ID); /* MCSChannelId */
	Stream_Write_UINT16(s,
	                    WINPR_ASSERTING_INT_CAST(uint16_t, mcs->channelCount)); /* channelCount */

	for (UINT32 i = 0; i < mcs->channelCount; i++)
	{
		const rdpMcsChannel* channel = &mcs->channels[i];
		Stream_Write_UINT16(s, channel->ChannelId);
	}

	if (mcs->channelCount % 2 == 1)
		Stream_Write_UINT16(s, 0);

	return TRUE;
}

/**
 * Read a client cluster data block (TS_UD_CS_CLUSTER).
 * msdn{cc240514}
 * @param s stream
 * @param mcs The MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_read_client_cluster_data(wStream* s, rdpMcs* mcs)
{
	char buffer[128] = { 0 };
	UINT32 redirectedSessionId = 0;
	rdpSettings* settings = mcs_get_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, settings->ClusterInfoFlags); /* flags */
	Stream_Read_UINT32(s, redirectedSessionId);        /* redirectedSessionId */

	WLog_Print(mcs->log, WLOG_TRACE, "read ClusterInfoFlags=%s, RedirectedSessionId=0x%08" PRIx32,
	           rdp_cluster_info_flags_to_string(settings->ClusterInfoFlags, buffer, sizeof(buffer)),
	           redirectedSessionId);
	if (settings->ClusterInfoFlags & REDIRECTED_SESSIONID_FIELD_VALID)
		settings->RedirectedSessionId = redirectedSessionId;

	settings->ConsoleSession =
	    (settings->ClusterInfoFlags & REDIRECTED_SESSIONID_FIELD_VALID) ? TRUE : FALSE;
	settings->RedirectSmartCards =
	    (settings->ClusterInfoFlags & REDIRECTED_SMARTCARD) ? TRUE : FALSE;

	if (Stream_GetRemainingLength(s) > 0)
	{
		/* The old Microsoft Mac RDP client can send a pad here */
		Stream_Seek(s, Stream_GetRemainingLength(s));
	}

	return TRUE;
}

/**
 * Write a client cluster data block (TS_UD_CS_CLUSTER).
 * msdn{cc240514}
 * @param s stream
 * @param mcs The MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_write_client_cluster_data(wStream* s, const rdpMcs* mcs)
{
	char buffer[128] = { 0 };
	UINT32 flags = 0;
	const rdpSettings* settings = mcs_get_const_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (!gcc_write_user_data_header(s, CS_CLUSTER, 12))
		return FALSE;
	flags = settings->ClusterInfoFlags;

	if (settings->ConsoleSession || settings->RedirectedSessionId)
		flags |= REDIRECTED_SESSIONID_FIELD_VALID;

	if (settings->RedirectSmartCards && settings->SmartcardLogon)
		flags |= REDIRECTED_SMARTCARD;

	if (flags & REDIRECTION_SUPPORTED)
	{
		/* REDIRECTION_VERSION6 requires multitransport enabled.
		 * if we run without that use REDIRECTION_VERSION5 */
		if (freerdp_settings_get_bool(settings, FreeRDP_SupportMultitransport))
			flags |= (REDIRECTION_VERSION6 << 2);
		else
			flags |= (REDIRECTION_VERSION5 << 2);
	}

	WLog_Print(mcs->log, WLOG_TRACE, "write ClusterInfoFlags=%s, RedirectedSessionId=0x%08" PRIx32,
	           rdp_cluster_info_flags_to_string(flags, buffer, sizeof(buffer)),
	           settings->RedirectedSessionId);
	Stream_Write_UINT32(s, flags);                         /* flags */
	Stream_Write_UINT32(s, settings->RedirectedSessionId); /* redirectedSessionID */
	return TRUE;
}

/**
 * Read a client monitor data block (TS_UD_CS_MONITOR).
 * msdn{dd305336}
 * @param s stream
 * @param mcs The MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_read_client_monitor_data(wStream* s, rdpMcs* mcs)
{
	UINT32 monitorCount = 0;
	rdpSettings* settings = mcs_get_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 8))
		return FALSE;

	Stream_Read_UINT32(s, settings->MonitorFlags); /* flags */
	Stream_Read_UINT32(s, monitorCount);           /* monitorCount */

	/* 2.2.1.3.6 Client Monitor Data -
	 * monitorCount (4 bytes): A 32-bit, unsigned integer. The number of display
	 * monitor definitions in the monitorDefArray field (the maximum allowed is 16).
	 */
	if (monitorCount > 16)
	{
		WLog_Print(mcs->log, WLOG_ERROR, "announced monitors(%" PRIu32 ") exceed the 16 limit",
		           monitorCount);
		return FALSE;
	}

	if (monitorCount > settings->MonitorDefArraySize)
	{
		WLog_Print(mcs->log, WLOG_ERROR,
		           "too many announced monitors(%" PRIu32 "), clamping to %" PRIu32 "",
		           monitorCount, settings->MonitorDefArraySize);
		monitorCount = settings->MonitorDefArraySize;
	}

	if (!Stream_CheckAndLogRequiredLengthOfSizeWLog(mcs->log, s, monitorCount, 20))
		return FALSE;

	settings->MonitorCount = monitorCount;

	for (UINT32 index = 0; index < monitorCount; index++)
	{
		rdpMonitor* current = &settings->MonitorDefArray[index];

		const INT32 left = Stream_Get_INT32(s);    /* left */
		const INT32 top = Stream_Get_INT32(s);     /* top */
		const INT32 right = Stream_Get_INT32(s);   /* right */
		const INT32 bottom = Stream_Get_INT32(s);  /* bottom */
		const UINT32 flags = Stream_Get_UINT32(s); /* flags */

		if ((left > right) || (top > bottom))
		{
			WLog_Print(mcs->log, WLOG_ERROR, "rdpMonitor::rect %dx%d-%dx%d invalid", left, top,
			           right, bottom);
			return FALSE;
		}

		const INT64 w = 1ll * right - left;
		const INT64 h = 1ll * bottom - top;
		if ((w >= INT32_MAX) || (h >= INT32_MAX) || (w < 0) || (h < 0))
		{
			WLog_Print(mcs->log, WLOG_ERROR,
			           "rdpMonitor::width/height %" PRId64 "/%" PRId64 " invalid", w, h);
			return FALSE;
		}

		current->x = left;
		current->y = top;
		current->width = WINPR_ASSERTING_INT_CAST(int32_t, w + 1);
		current->height = WINPR_ASSERTING_INT_CAST(int32_t, h + 1);
		current->is_primary = (flags & MONITOR_PRIMARY) ? TRUE : FALSE;
	}

	return TRUE;
}

/**
 * Write a client monitor data block (TS_UD_CS_MONITOR).
 * msdn{dd305336}
 * @param s stream
 * @param mcs The MCS to use
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_write_client_monitor_data(wStream* s, const rdpMcs* mcs)
{
	INT32 baseX = 0;
	INT32 baseY = 0;
	const rdpSettings* settings = mcs_get_const_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	WLog_Print(mcs->log, WLOG_DEBUG, "MonitorCount=%" PRIu32, settings->MonitorCount);
	if (settings->MonitorCount > 1)
	{
		const size_t len = (20 * settings->MonitorCount) + 12;
		WINPR_ASSERT(len <= UINT16_MAX);
		const UINT16 length = (UINT16)len;
		if (!gcc_write_user_data_header(s, CS_MONITOR, length))
			return FALSE;
		Stream_Write_UINT32(s, settings->MonitorFlags); /* flags */
		Stream_Write_UINT32(s, settings->MonitorCount); /* monitorCount */

		/* first pass to get the primary monitor coordinates (it is supposed to be
		 * in (0,0) */
		for (UINT32 i = 0; i < settings->MonitorCount; i++)
		{
			const rdpMonitor* current = &settings->MonitorDefArray[i];
			if (current->is_primary)
			{
				baseX = current->x;
				baseY = current->y;
				break;
			}
		}

		for (UINT32 i = 0; i < settings->MonitorCount; i++)
		{
			const rdpMonitor* current = &settings->MonitorDefArray[i];
			const INT32 left = current->x - baseX;
			const INT32 top = current->y - baseY;
			const INT32 right = left + current->width - 1;
			const INT32 bottom = top + current->height - 1;
			const UINT32 flags = current->is_primary ? MONITOR_PRIMARY : 0;
			WLog_Print(mcs->log, WLOG_DEBUG,
			           "Monitor[%" PRIu32 "]: top=%" PRId32 ", left=%" PRId32 ", bottom=%" PRId32
			           ", right=%" PRId32 ", flags=%" PRIu32,
			           i, top, left, bottom, right, flags);
			Stream_Write_INT32(s, left);   /* left */
			Stream_Write_INT32(s, top);    /* top */
			Stream_Write_INT32(s, right);  /* right */
			Stream_Write_INT32(s, bottom); /* bottom */
			Stream_Write_UINT32(s, flags); /* flags */
		}
	}
	WLog_Print(mcs->log, WLOG_DEBUG, "FINISHED");
	return TRUE;
}

BOOL gcc_read_client_monitor_extended_data(wStream* s, rdpMcs* mcs)
{
	UINT32 monitorCount = 0;
	UINT32 monitorAttributeSize = 0;
	rdpSettings* settings = mcs_get_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 12))
		return FALSE;

	Stream_Read_UINT32(s, settings->MonitorAttributeFlags); /* flags */
	Stream_Read_UINT32(s, monitorAttributeSize);            /* monitorAttributeSize */
	Stream_Read_UINT32(s, monitorCount);                    /* monitorCount */

	if (monitorAttributeSize != 20)
	{
		WLog_Print(mcs->log, WLOG_ERROR,
		           "TS_UD_CS_MONITOR_EX::monitorAttributeSize %" PRIu32 " != 20",
		           monitorAttributeSize);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredCapacityOfSizeWLog(mcs->log, s, monitorCount,
	                                                  monitorAttributeSize))
		return FALSE;

	if (settings->MonitorCount != monitorCount)
	{
		WLog_Print(mcs->log, WLOG_ERROR,
		           "(TS_UD_CS_MONITOR_EX)::monitorCount %" PRIu32 " != expected %" PRIu32,
		           monitorCount, settings->MonitorCount);
		return FALSE;
	}

	settings->HasMonitorAttributes = TRUE;

	for (UINT32 index = 0; index < monitorCount; index++)
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
	const rdpSettings* settings = mcs_get_const_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (settings->HasMonitorAttributes)
	{
		const size_t length = (20 * settings->MonitorCount) + 16;
		WINPR_ASSERT(length <= UINT16_MAX);
		if (!gcc_write_user_data_header(s, CS_MONITOR_EX, (UINT16)length))
			return FALSE;
		Stream_Write_UINT32(s, settings->MonitorAttributeFlags); /* flags */
		Stream_Write_UINT32(s, 20);                              /* monitorAttributeSize */
		Stream_Write_UINT32(s, settings->MonitorCount);          /* monitorCount */

		for (UINT32 i = 0; i < settings->MonitorCount; i++)
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
 * Read a client message channel data block (TS_UD_CS_MCS_MSGCHANNEL).
 * msdn{jj217627}
 * @param s stream
 * @param mcs The MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_read_client_message_channel_data(wStream* s, rdpMcs* mcs)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, mcs->flags);
	mcs->messageChannelId = mcs->baseChannelId++;
	return TRUE;
}

/**
 * Write a client message channel data block (TS_UD_CS_MCS_MSGCHANNEL).
 * msdn{jj217627}
 * @param s stream
 * @param mcs The MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_write_client_message_channel_data(wStream* s, const rdpMcs* mcs)
{
	const rdpSettings* settings = mcs_get_const_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);
	WINPR_ASSERT(settings);
	if (freerdp_settings_get_bool(settings, FreeRDP_NetworkAutoDetect) ||
	    settings->SupportHeartbeatPdu || settings->SupportMultitransport)
	{
		if (!gcc_write_user_data_header(s, CS_MCS_MSGCHANNEL, 8))
			return FALSE;
		Stream_Write_UINT32(s, mcs->flags); /* flags */
	}
	return TRUE;
}

BOOL gcc_read_server_message_channel_data(wStream* s, rdpMcs* mcs)
{
	UINT16 MCSChannelId = 0;
	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);
	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, MCSChannelId); /* MCSChannelId */
	/* Save the MCS message channel id */
	mcs->messageChannelId = MCSChannelId;
	return TRUE;
}

BOOL gcc_write_server_message_channel_data(wStream* s, const rdpMcs* mcs)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(mcs);
	if (mcs->messageChannelId == 0)
		return TRUE;

	if (!gcc_write_user_data_header(s, SC_MCS_MSGCHANNEL, 6))
		return FALSE;

	Stream_Write_UINT16(s, mcs->messageChannelId); /* mcsChannelId (2 bytes) */
	return TRUE;
}

/**
 * Read a client multitransport channel data block (TS_UD_CS_MULTITRANSPORT).
 * msdn{jj217498}
 * @param s stream
 * @param mcs The MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_read_client_multitransport_channel_data(wStream* s, rdpMcs* mcs)
{
	rdpSettings* settings = mcs_get_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
		return FALSE;

	UINT32 remoteFlags = 0;
	Stream_Read_UINT32(s, remoteFlags);
	settings->MultitransportFlags &= remoteFlags; /* merge local and remote flags */
	return TRUE;
}

/**
 * Write a client multitransport channel data block (TS_UD_CS_MULTITRANSPORT).
 * msdn{jj217498}
 *
 * @param s stream
 * @param mcs The MCS instance
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL gcc_write_client_multitransport_channel_data(wStream* s, const rdpMcs* mcs)
{
	const rdpSettings* settings = mcs_get_const_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);
	if (!gcc_write_user_data_header(s, CS_MULTITRANSPORT, 8))
		return FALSE;
	Stream_Write_UINT32(s, settings->MultitransportFlags); /* flags */
	return TRUE;
}

BOOL gcc_read_server_multitransport_channel_data(wStream* s, rdpMcs* mcs)
{
	rdpSettings* settings = mcs_get_settings(mcs);
	UINT32 remoteFlags = 0;

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);
	if (!Stream_CheckAndLogRequiredLengthWLog(mcs->log, s, 4))
		return FALSE;

	Stream_Read_UINT32(s, remoteFlags);
	settings->MultitransportFlags &= remoteFlags; /* merge with client setting */
	return TRUE;
}

BOOL gcc_write_server_multitransport_channel_data(wStream* s, const rdpMcs* mcs)
{
	const rdpSettings* settings = mcs_get_const_settings(mcs);

	WINPR_ASSERT(s);
	WINPR_ASSERT(settings);

	if (!gcc_write_user_data_header(s, SC_MULTITRANSPORT, 8))
		return FALSE;

	Stream_Write_UINT32(s, settings->MultitransportFlags); /* flags (4 bytes) */
	return TRUE;
}
