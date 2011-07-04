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

/* http://msdn.microsoft.com/en-us/library/cc240836/ */

/*
 * OID = 0.0.20.124.0.1
 * { itu-t(0) recommendation(0) t(20) t124(124) version(0) 1 }
 * v.1 of ITU-T Recommendation T.124 (Feb 1998): "Generic Conference Control"
 */
uint8 t124_02_98_oid[6] = { 0, 0, 20, 124, 0, 1 };

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
	per_write_octet_string(s, user_data->buffer, stream_get_length(user_data), 0); /* array of client data blocks */
}
