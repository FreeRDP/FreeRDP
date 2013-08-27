/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * T.125 Multipoint Communication Service (MCS) Protocol
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

#include "mcs.h"
#include "tpdu.h"
#include "tpkt.h"

/**
 * T.125 MCS is defined in:
 *
 * http://www.itu.int/rec/T-REC-T.125-199802-I/
 * ITU-T T.125 Multipoint Communication Service Protocol Specification
 */

/**
 * Connect-Initial ::= [APPLICATION 101] IMPLICIT SEQUENCE
 * {
 * 	callingDomainSelector		OCTET_STRING,
 * 	calledDomainSelector		OCTET_STRING,
 * 	upwardFlag			BOOLEAN,
 * 	targetParameters		DomainParameters,
 * 	minimumParameters		DomainParameters,
 * 	maximumParameters		DomainParameters,
 * 	userData			OCTET_STRING
 * }
 *
 * DomainParameters ::= SEQUENCE
 * {
 * 	maxChannelIds			INTEGER (0..MAX),
 * 	maxUserIds			INTEGER (0..MAX),
 * 	maxTokenIds			INTEGER (0..MAX),
 * 	numPriorities			INTEGER (0..MAX),
 * 	minThroughput			INTEGER (0..MAX),
 * 	maxHeight			INTEGER (0..MAX),
 * 	maxMCSPDUsize			INTEGER (0..MAX),
 * 	protocolVersion			INTEGER (0..MAX)
 * }
 *
 * Connect-Response ::= [APPLICATION 102] IMPLICIT SEQUENCE
 * {
 * 	result				Result,
 * 	calledConnectId			INTEGER (0..MAX),
 * 	domainParameters		DomainParameters,
 * 	userData			OCTET_STRING
 * }
 *
 * Result ::= ENUMERATED
 * {
 * 	rt-successful			(0),
 * 	rt-domain-merging		(1),
 * 	rt-domain-not-hierarchical	(2),
 * 	rt-no-such-channel		(3),
 * 	rt-no-such-domain		(4),
 * 	rt-no-such-user			(5),
 * 	rt-not-admitted			(6),
 * 	rt-other-user-id		(7),
 * 	rt-parameters-unacceptable	(8),
 * 	rt-token-not-available		(9),
 * 	rt-token-not-possessed		(10),
 * 	rt-too-many-channels		(11),
 * 	rt-too-many-tokens		(12),
 * 	rt-too-many-users		(13),
 * 	rt-unspecified-failure		(14),
 * 	rt-user-rejected		(15)
 * }
 *
 * ErectDomainRequest ::= [APPLICATION 1] IMPLICIT SEQUENCE
 * {
 * 	subHeight			INTEGER (0..MAX),
 * 	subInterval			INTEGER (0..MAX)
 * }
 *
 * AttachUserRequest ::= [APPPLICATION 10] IMPLICIT SEQUENCE
 * {
 * }
 *
 * AttachUserConfirm ::= [APPLICATION 11] IMPLICIT SEQUENCE
 * {
 * 	result				Result,
 * 	initiator			UserId OPTIONAL
 * }
 *
 * ChannelJoinRequest ::= [APPLICATION 14] IMPLICIT SEQUENCE
 * {
 * 	initiator			UserId,
 * 	channelId			ChannelId
 * }
 *
 * ChannelJoinConfirm ::= [APPLICATION 15] IMPLICIT SEQUENCE
 * {
 * 	result				Result,
 * 	initiator			UserId,
 * 	requested			ChannelId,
 * 	channelId			ChannelId OPTIONAL
 * }
 *
 * SendDataRequest ::= [APPLICATION 25] IMPLICIT SEQUENCE
 * {
 * 	initiator			UserId,
 * 	channelId			ChannelId,
 * 	dataPriority			DataPriority,
 * 	segmentation			Segmentation,
 * 	userData			OCTET_STRING
 * }
 *
 * DataPriority ::= CHOICE
 * {
 * 	top				NULL,
 * 	high				NULL,
 * 	medium				NULL,
 * 	low				NULL,
 * 	...
 * }
 *
 * Segmentation ::= BIT_STRING
 * {
 * 	begin				(0),
 * 	end				(1)
 * } (SIZE(2))
 *
 * SendDataIndication ::= SEQUENCE
 * {
 * 	initiator			UserId,
 * 	channelId			ChannelId,
 * 	reliability			BOOLEAN,
 * 	domainReferenceID		INTEGER (0..65535) OPTIONAL,
 * 	dataPriority			DataPriority,
 * 	segmentation			Segmentation,
 * 	userData			OCTET_STRING,
 * 	totalDataSize			INTEGER OPTIONAL,
 * 	nonStandard			SEQUENCE OF NonStandardParameter OPTIONAL,
 * 	...
 * }
 *
 */

static const BYTE callingDomainSelector[1] = "\x01";
static const BYTE calledDomainSelector[1] = "\x01";

/*
static const char* const mcs_result_enumerated[] =
{
		"rt-successful",
		"rt-domain-merging",
		"rt-domain-not-hierarchical",
		"rt-no-such-channel",
		"rt-no-such-domain",
		"rt-no-such-user",
		"rt-not-admitted",
		"rt-other-user-id",
		"rt-parameters-unacceptable",
		"rt-token-not-available",
		"rt-token-not-possessed",
		"rt-too-many-channels",
		"rt-too-many-tokens",
		"rt-too-many-users",
		"rt-unspecified-failure",
		"rt-user-rejected"
};
*/

/**
 * Read a DomainMCSPDU header.
 * @param s stream
 * @param domainMCSPDU DomainMCSPDU type
 * @param length TPKT length
 * @return
 */

BOOL mcs_read_domain_mcspdu_header(wStream* s, enum DomainMCSPDU* domainMCSPDU, UINT16* length)
{
	UINT16 li;
	BYTE choice;
	enum DomainMCSPDU MCSPDU;

	*length = tpkt_read_header(s);

	if (!tpdu_read_data(s, &li))
		return FALSE;

	MCSPDU = *domainMCSPDU;

	if (!per_read_choice(s, &choice))
		return FALSE;

	*domainMCSPDU = (choice >> 2);

	if (*domainMCSPDU != MCSPDU)
		return FALSE;

	return TRUE;
}

/**
 * Write a DomainMCSPDU header.
 * @param s stream
 * @param domainMCSPDU DomainMCSPDU type
 * @param length TPKT length
 */

void mcs_write_domain_mcspdu_header(wStream* s, enum DomainMCSPDU domainMCSPDU, UINT16 length, BYTE options)
{
	tpkt_write_header(s, length);
	tpdu_write_data(s);
	per_write_choice(s, (domainMCSPDU << 2) | options);
}

/**
 * Initialize MCS Domain Parameters.
 * @param domainParameters domain parameters
 * @param maxChannelIds max channel ids
 * @param maxUserIds max user ids
 * @param maxTokenIds max token ids
 * @param maxMCSPDUsize max MCS PDU size
 */

static void mcs_init_domain_parameters(DomainParameters* domainParameters,
		UINT32 maxChannelIds, UINT32 maxUserIds, UINT32 maxTokenIds, UINT32 maxMCSPDUsize)
{
	domainParameters->maxChannelIds = maxChannelIds;
	domainParameters->maxUserIds = maxUserIds;
	domainParameters->maxTokenIds = maxTokenIds;
	domainParameters->maxMCSPDUsize = maxMCSPDUsize;

	domainParameters->numPriorities = 1;
	domainParameters->minThroughput = 0;
	domainParameters->maxHeight = 1;
	domainParameters->protocolVersion = 2;
}

/**
 * Read MCS Domain Parameters.
 * @param s stream
 * @param domainParameters domain parameters
 */

BOOL mcs_read_domain_parameters(wStream* s, DomainParameters* domainParameters)
{
	int length;

	return
		ber_read_sequence_tag(s, &length) &&
		ber_read_integer(s, &(domainParameters->maxChannelIds)) &&
		ber_read_integer(s, &(domainParameters->maxUserIds)) &&
		ber_read_integer(s, &(domainParameters->maxTokenIds)) &&
		ber_read_integer(s, &(domainParameters->numPriorities)) &&
		ber_read_integer(s, &(domainParameters->minThroughput)) &&
		ber_read_integer(s, &(domainParameters->maxHeight)) &&
		ber_read_integer(s, &(domainParameters->maxMCSPDUsize)) &&
		ber_read_integer(s, &(domainParameters->protocolVersion));
}

/**
 * Write MCS Domain Parameters.
 * @param s stream
 * @param domainParameters domain parameters
 */

void mcs_write_domain_parameters(wStream* s, DomainParameters* domainParameters)
{
	int length;
	wStream* tmps;

	tmps = Stream_New(NULL, Stream_Capacity(s));
	ber_write_integer(tmps, domainParameters->maxChannelIds);
	ber_write_integer(tmps, domainParameters->maxUserIds);
	ber_write_integer(tmps, domainParameters->maxTokenIds);
	ber_write_integer(tmps, domainParameters->numPriorities);
	ber_write_integer(tmps, domainParameters->minThroughput);
	ber_write_integer(tmps, domainParameters->maxHeight);
	ber_write_integer(tmps, domainParameters->maxMCSPDUsize);
	ber_write_integer(tmps, domainParameters->protocolVersion);

	length = Stream_GetPosition(tmps);
	ber_write_sequence_tag(s, length);
	Stream_Write(s, Stream_Buffer(tmps), length);
	Stream_Free(tmps, TRUE);
}

/**
 * Print MCS Domain Parameters.
 * @param domainParameters domain parameters
 */

void mcs_print_domain_parameters(DomainParameters* domainParameters)
{
	fprintf(stderr, "DomainParameters {\n");
	fprintf(stderr, "\tmaxChannelIds:%d\n", domainParameters->maxChannelIds);
	fprintf(stderr, "\tmaxUserIds:%d\n", domainParameters->maxUserIds);
	fprintf(stderr, "\tmaxTokenIds:%d\n", domainParameters->maxTokenIds);
	fprintf(stderr, "\tnumPriorities:%d\n", domainParameters->numPriorities);
	fprintf(stderr, "\tminThroughput:%d\n", domainParameters->minThroughput);
	fprintf(stderr, "\tmaxHeight:%d\n", domainParameters->maxHeight);
	fprintf(stderr, "\tmaxMCSPDUsize:%d\n", domainParameters->maxMCSPDUsize);
	fprintf(stderr, "\tprotocolVersion:%d\n", domainParameters->protocolVersion);
	fprintf(stderr, "}\n");
}

/**
 * Read an MCS Connect Initial PDU.\n
 * @msdn{cc240508}
 * @param mcs MCS module
 * @param s stream
 */

BOOL mcs_recv_connect_initial(rdpMcs* mcs, wStream* s)
{
	UINT16	li;
	int length;
	BOOL upwardFlag;

	tpkt_read_header(s);

	if (!tpdu_read_data(s, &li))
		return FALSE;

	if (!ber_read_application_tag(s, MCS_TYPE_CONNECT_INITIAL, &length))
		return FALSE;

	/* callingDomainSelector (OCTET_STRING) */
	if (!ber_read_octet_string_tag(s, &length) || Stream_GetRemainingLength(s) < length)
		return FALSE;
	Stream_Seek(s, length);

	/* calledDomainSelector (OCTET_STRING) */
	if (!ber_read_octet_string_tag(s, &length) || Stream_GetRemainingLength(s) < length)
		return FALSE;
	Stream_Seek(s, length);

	/* upwardFlag (BOOLEAN) */
	if (!ber_read_BOOL(s, &upwardFlag))
		return FALSE;

	/* targetParameters (DomainParameters) */
	if (!mcs_read_domain_parameters(s, &mcs->targetParameters))
		return FALSE;

	/* minimumParameters (DomainParameters) */
	if (!mcs_read_domain_parameters(s, &mcs->minimumParameters))
		return FALSE;

	/* maximumParameters (DomainParameters) */
	if (!mcs_read_domain_parameters(s, &mcs->maximumParameters))
		return FALSE;

	if (!ber_read_octet_string_tag(s, &length) || Stream_GetRemainingLength(s) < length)
		return FALSE;

	if (!gcc_read_conference_create_request(s, mcs->transport->settings))
		return FALSE;

	return TRUE;
}

/**
 * Write an MCS Connect Initial PDU.\n
 * @msdn{cc240508}
 * @param s stream
 * @param mcs MCS module
 * @param user_data GCC Conference Create Request
 */

void mcs_write_connect_initial(wStream* s, rdpMcs* mcs, wStream* user_data)
{
	int length;
	wStream* tmps;

	tmps = Stream_New(NULL, Stream_Capacity(s));

	/* callingDomainSelector (OCTET_STRING) */
	ber_write_octet_string(tmps, callingDomainSelector, sizeof(callingDomainSelector));

	/* calledDomainSelector (OCTET_STRING) */
	ber_write_octet_string(tmps, calledDomainSelector, sizeof(calledDomainSelector));

	/* upwardFlag (BOOLEAN) */
	ber_write_BOOL(tmps, TRUE);

	/* targetParameters (DomainParameters) */
	mcs_write_domain_parameters(tmps, &mcs->targetParameters);

	/* minimumParameters (DomainParameters) */
	mcs_write_domain_parameters(tmps, &mcs->minimumParameters);

	/* maximumParameters (DomainParameters) */
	mcs_write_domain_parameters(tmps, &mcs->maximumParameters);

	/* userData (OCTET_STRING) */
	ber_write_octet_string(tmps, user_data->buffer, Stream_GetPosition(user_data));

	length = Stream_GetPosition(tmps);
	/* Connect-Initial (APPLICATION 101, IMPLICIT SEQUENCE) */
	ber_write_application_tag(s, MCS_TYPE_CONNECT_INITIAL, length);
	Stream_Write(s, Stream_Buffer(tmps), length);
	Stream_Free(tmps, TRUE);
}

/**
 * Write an MCS Connect Response PDU.\n
 * @msdn{cc240508}
 * @param s stream
 * @param mcs MCS module
 * @param user_data GCC Conference Create Response
 */

void mcs_write_connect_response(wStream* s, rdpMcs* mcs, wStream* user_data)
{
	int length;
	wStream* tmps;

	tmps = Stream_New(NULL, Stream_Capacity(s));
	ber_write_enumerated(tmps, 0, MCS_Result_enum_length);
	ber_write_integer(tmps, 0); /* calledConnectId */
	mcs->domainParameters = mcs->targetParameters;
	mcs_write_domain_parameters(tmps, &(mcs->domainParameters));
	/* userData (OCTET_STRING) */
	ber_write_octet_string(tmps, user_data->buffer, Stream_GetPosition(user_data));

	length = Stream_GetPosition(tmps);
	ber_write_application_tag(s, MCS_TYPE_CONNECT_RESPONSE, length);
	Stream_Write(s, Stream_Buffer(tmps), length);
	Stream_Free(tmps, TRUE);
}

/**
 * Send MCS Connect Initial.\n
 * @msdn{cc240508}
 * @param mcs mcs module
 */

BOOL mcs_send_connect_initial(rdpMcs* mcs)
{
	int status;
	int length;
	wStream* s;
	int bm, em;
	wStream* gcc_CCrq;
	wStream* client_data;

	client_data = Stream_New(NULL, 512);
	gcc_write_client_data_blocks(client_data, mcs->transport->settings);

	gcc_CCrq = Stream_New(NULL, 512);
	gcc_write_conference_create_request(gcc_CCrq, client_data);
	length = Stream_GetPosition(gcc_CCrq) + 7;

	s = Stream_New(NULL, 1024 + length);

	bm = Stream_GetPosition(s);
	Stream_Seek(s, 7);

	mcs_write_connect_initial(s, mcs, gcc_CCrq);
	em = Stream_GetPosition(s);
	length = (em - bm);
	Stream_SetPosition(s, bm);

	tpkt_write_header(s, length);
	tpdu_write_data(s);
	Stream_SetPosition(s, em);
	Stream_SealLength(s);

	status = transport_write(mcs->transport, s);

	Stream_Free(s, TRUE);
	Stream_Free(gcc_CCrq, TRUE);
	Stream_Free(client_data, TRUE);

	return (status < 0 ? FALSE : TRUE);
}

/**
 * Read MCS Connect Response.\n
 * @msdn{cc240501}
 * @param mcs mcs module
 */

BOOL mcs_recv_connect_response(rdpMcs* mcs, wStream* s)
{
	int length;
	BYTE result;
	UINT16 li;
	UINT32 calledConnectId;

	tpkt_read_header(s);

	if (!tpdu_read_data(s, &li))
		return FALSE;

	if (!ber_read_application_tag(s, MCS_TYPE_CONNECT_RESPONSE, &length) ||
		!ber_read_enumerated(s, &result, MCS_Result_enum_length) ||
		!ber_read_integer(s, &calledConnectId) ||
		!mcs_read_domain_parameters(s, &(mcs->domainParameters)) ||
		!ber_read_octet_string_tag(s, &length))
	{
		return FALSE;
	}

	if (!gcc_read_conference_create_response(s, mcs->transport->settings))
	{
		fprintf(stderr, "mcs_recv_connect_response: gcc_read_conference_create_response failed\n");
		return FALSE;
	}

	return TRUE;
}

/**
 * Send MCS Connect Response.\n
 * @msdn{cc240501}
 * @param mcs mcs module
 */

BOOL mcs_send_connect_response(rdpMcs* mcs)
{
	int length;
	int status;
	wStream* s;
	int bm, em;
	wStream* gcc_CCrsp;
	wStream* server_data;

	server_data = Stream_New(NULL, 512);
	gcc_write_server_data_blocks(server_data, mcs->transport->settings);

	gcc_CCrsp = Stream_New(NULL, 512);
	gcc_write_conference_create_response(gcc_CCrsp, server_data);
	length = Stream_GetPosition(gcc_CCrsp) + 7;

	s = Stream_New(NULL, length + 1024);

	bm = Stream_GetPosition(s);
	Stream_Seek(s, 7);

	mcs_write_connect_response(s, mcs, gcc_CCrsp);
	em = Stream_GetPosition(s);
	length = (em - bm);
	Stream_SetPosition(s, bm);

	tpkt_write_header(s, length);
	tpdu_write_data(s);
	Stream_SetPosition(s, em);
	Stream_SealLength(s);

	status = transport_write(mcs->transport, s);

	Stream_Free(s, TRUE);
	Stream_Free(gcc_CCrsp, TRUE);
	Stream_Free(server_data, TRUE);

	return (status < 0) ? FALSE : TRUE;
}

/**
 * Read MCS Erect Domain Request.\n
 * @msdn{cc240523}
 * @param mcs
 * @param s stream
 */

BOOL mcs_recv_erect_domain_request(rdpMcs* mcs, wStream* s)
{
	UINT16 length;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = DomainMCSPDU_ErectDomainRequest;

	return mcs_read_domain_mcspdu_header(s, &MCSPDU, &length);
}

/**
 * Send MCS Erect Domain Request.\n
 * @msdn{cc240523}
 * @param mcs
 */

BOOL mcs_send_erect_domain_request(rdpMcs* mcs)
{
	wStream* s;
	int status;
	UINT16 length = 12;

	s = Stream_New(NULL, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_ErectDomainRequest, length, 0);

	per_write_integer(s, 0); /* subHeight (INTEGER) */
	per_write_integer(s, 0); /* subInterval (INTEGER) */

	Stream_SealLength(s);

	status = transport_write(mcs->transport, s);

	Stream_Free(s, TRUE);

	return (status < 0) ? FALSE : TRUE;
}

/**
 * Read MCS Attach User Request.\n
 * @msdn{cc240524}
 * @param mcs mcs module
 * @param s stream
 */

BOOL mcs_recv_attach_user_request(rdpMcs* mcs, wStream* s)
{
	UINT16 length;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = DomainMCSPDU_AttachUserRequest;

	return mcs_read_domain_mcspdu_header(s, &MCSPDU, &length);
}

/**
 * Send MCS Attach User Request.\n
 * @msdn{cc240524}
 * @param mcs mcs module
 */

BOOL mcs_send_attach_user_request(rdpMcs* mcs)
{
	wStream* s;
	int status;
	UINT16 length = 8;

	s = Stream_New(NULL, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_AttachUserRequest, length, 0);

	Stream_SealLength(s);

	status = transport_write(mcs->transport, s);

	Stream_Free(s, TRUE);

	return (status < 0) ? FALSE : TRUE;
}

/**
 * Read MCS Attach User Confirm.\n
 * @msdn{cc240525}
 * @param mcs mcs module
 */

BOOL mcs_recv_attach_user_confirm(rdpMcs* mcs, wStream* s)
{
	UINT16 length;
	BYTE result;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = DomainMCSPDU_AttachUserConfirm;

	return
		mcs_read_domain_mcspdu_header(s, &MCSPDU, &length) &&
		per_read_enumerated(s, &result, MCS_Result_enum_length) && /* result */
		per_read_integer16(s, &(mcs->user_id), MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
}

/**
 * Send MCS Attach User Confirm.\n
 * @msdn{cc240525}
 * @param mcs mcs module
 */

BOOL mcs_send_attach_user_confirm(rdpMcs* mcs)
{
	wStream* s;
	int status;
	UINT16 length = 11;
	
	s = Stream_New(NULL, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_AttachUserConfirm, length, 2);

	per_write_enumerated(s, 0, MCS_Result_enum_length); /* result */
	mcs->user_id = MCS_GLOBAL_CHANNEL_ID + 1 + mcs->transport->settings->ChannelCount;
	per_write_integer16(s, mcs->user_id, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */

	Stream_SealLength(s);

	status = transport_write(mcs->transport, s);

	Stream_Free(s, TRUE);

	return (status < 0) ? FALSE : TRUE;
}

/**
 * Read MCS Channel Join Request.\n
 * @msdn{cc240526}
 * @param mcs mcs module
 * @param s stream
 */

BOOL mcs_recv_channel_join_request(rdpMcs* mcs, wStream* s, UINT16* channel_id)
{
	UINT16 length;
	enum DomainMCSPDU MCSPDU;
	UINT16 user_id;

	MCSPDU = DomainMCSPDU_ChannelJoinRequest;

	return
		mcs_read_domain_mcspdu_header(s, &MCSPDU, &length) &&
		per_read_integer16(s, &user_id, MCS_BASE_CHANNEL_ID) &&
		(user_id == mcs->user_id) &&
		per_read_integer16(s, channel_id, 0);
}

/**
 * Send MCS Channel Join Request.\n
 * @msdn{cc240526}
 * @param mcs mcs module
 * @param channel_id channel id
 */

BOOL mcs_send_channel_join_request(rdpMcs* mcs, UINT16 channel_id)
{
	wStream* s;
	int status;
	UINT16 length = 12;

	s = Stream_New(NULL, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_ChannelJoinRequest, length, 0);

	per_write_integer16(s, mcs->user_id, MCS_BASE_CHANNEL_ID);
	per_write_integer16(s, channel_id, 0);

	Stream_SealLength(s);

	status = transport_write(mcs->transport, s);

	Stream_Free(s, TRUE);

	return (status < 0) ? FALSE : TRUE;
}

/**
 * Read MCS Channel Join Confirm.\n
 * @msdn{cc240527}
 * @param mcs mcs module
 */

BOOL mcs_recv_channel_join_confirm(rdpMcs* mcs, wStream* s, UINT16* channel_id)
{
	BOOL status;
	UINT16 length;
	BYTE result;
	UINT16 initiator;
	UINT16 requested;
	enum DomainMCSPDU MCSPDU;

	status = TRUE;
	MCSPDU = DomainMCSPDU_ChannelJoinConfirm;

	status &= mcs_read_domain_mcspdu_header(s, &MCSPDU, &length);
	status &= per_read_enumerated(s, &result, MCS_Result_enum_length); /* result */
	status &= per_read_integer16(s, &initiator, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
	status &= per_read_integer16(s, &requested, 0); /* requested (ChannelId) */
	status &= per_read_integer16(s, channel_id, 0); /* channelId */

	return status;
}

/**
 * Send MCS Channel Join Confirm.\n
 * @msdn{cc240527}
 * @param mcs mcs module
 */

BOOL mcs_send_channel_join_confirm(rdpMcs* mcs, UINT16 channel_id)
{
	wStream* s;
	int status;
	UINT16 length = 15;

	s = Stream_New(NULL, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_ChannelJoinConfirm, length, 2);

	per_write_enumerated(s, 0, MCS_Result_enum_length); /* result */
	per_write_integer16(s, mcs->user_id, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
	per_write_integer16(s, channel_id, 0); /* requested (ChannelId) */
	per_write_integer16(s, channel_id, 0); /* channelId */

	Stream_SealLength(s);

	status = transport_write(mcs->transport, s);

	Stream_Free(s, TRUE);

	return (status < 0) ? FALSE : TRUE;
}

/**
 * Send MCS Disconnect Provider Ultimatum PDU.\n
 * @param mcs mcs module
 */

BOOL mcs_send_disconnect_provider_ultimatum(rdpMcs* mcs)
{
	wStream* s;
	int status;
	UINT16 length = 9;

	s = Stream_New(NULL, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_DisconnectProviderUltimatum, length, 1);

	per_write_enumerated(s, 0, 0); /* reason */

	status = transport_write(mcs->transport, s);

	Stream_Free(s, TRUE);

	return (status < 0) ? FALSE : TRUE;
}

/**
 * Instantiate new MCS module.
 * @param transport transport
 * @return new MCS module
 */

rdpMcs* mcs_new(rdpTransport* transport)
{
	rdpMcs* mcs;

	mcs = (rdpMcs*) malloc(sizeof(rdpMcs));

	if (mcs != NULL)
	{
		ZeroMemory(mcs, sizeof(rdpMcs));

		mcs->transport = transport;
		mcs_init_domain_parameters(&mcs->targetParameters, 34, 2, 0, 0xFFFF);
		mcs_init_domain_parameters(&mcs->minimumParameters, 1, 1, 1, 0x420);
		mcs_init_domain_parameters(&mcs->maximumParameters, 0xFFFF, 0xFC17, 0xFFFF, 0xFFFF);
	}

	return mcs;
}

/**
 * Free MCS module.
 * @param mcs MCS module to be freed
 */

void mcs_free(rdpMcs* mcs)
{
	if (mcs != NULL)
	{
		free(mcs);
	}
}
