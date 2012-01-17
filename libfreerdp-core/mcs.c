/**
 * FreeRDP: A Remote Desktop Protocol Client
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

static const uint8 callingDomainSelector[1] = "\x01";
static const uint8 calledDomainSelector[1] = "\x01";

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

boolean mcs_read_domain_mcspdu_header(STREAM* s, enum DomainMCSPDU* domainMCSPDU, uint16* length)
{
	uint8 choice;
	enum DomainMCSPDU MCSPDU;

	*length = tpkt_read_header(s);

	if (tpdu_read_data(s) == 0)
		return false;

	MCSPDU = *domainMCSPDU;
	per_read_choice(s, &choice);
	*domainMCSPDU = (choice >> 2);

	if (*domainMCSPDU != MCSPDU)
		return false;

	return true;
}

/**
 * Write a DomainMCSPDU header.
 * @param s stream
 * @param domainMCSPDU DomainMCSPDU type
 * @param length TPKT length
 */

void mcs_write_domain_mcspdu_header(STREAM* s, enum DomainMCSPDU domainMCSPDU, uint16 length, uint8 options)
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
		uint32 maxChannelIds, uint32 maxUserIds, uint32 maxTokenIds, uint32 maxMCSPDUsize)
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

boolean mcs_read_domain_parameters(STREAM* s, DomainParameters* domainParameters)
{
	int length;
	ber_read_sequence_tag(s, &length);
	ber_read_integer(s, &(domainParameters->maxChannelIds));
	ber_read_integer(s, &(domainParameters->maxUserIds));
	ber_read_integer(s, &(domainParameters->maxTokenIds));
	ber_read_integer(s, &(domainParameters->numPriorities));
	ber_read_integer(s, &(domainParameters->minThroughput));
	ber_read_integer(s, &(domainParameters->maxHeight));
	ber_read_integer(s, &(domainParameters->maxMCSPDUsize));
	ber_read_integer(s, &(domainParameters->protocolVersion));

	return true;
}

/**
 * Write MCS Domain Parameters.
 * @param s stream
 * @param domainParameters domain parameters
 */

void mcs_write_domain_parameters(STREAM* s, DomainParameters* domainParameters)
{
	int length;
	STREAM* tmps;

	tmps = stream_new(stream_get_size(s));
	ber_write_integer(tmps, domainParameters->maxChannelIds);
	ber_write_integer(tmps, domainParameters->maxUserIds);
	ber_write_integer(tmps, domainParameters->maxTokenIds);
	ber_write_integer(tmps, domainParameters->numPriorities);
	ber_write_integer(tmps, domainParameters->minThroughput);
	ber_write_integer(tmps, domainParameters->maxHeight);
	ber_write_integer(tmps, domainParameters->maxMCSPDUsize);
	ber_write_integer(tmps, domainParameters->protocolVersion);

	length = stream_get_length(tmps);
	ber_write_sequence_tag(s, length);
	stream_write(s, stream_get_head(tmps), length);
	stream_free(tmps);
}

/**
 * Print MCS Domain Parameters.
 * @param domainParameters domain parameters
 */

void mcs_print_domain_parameters(DomainParameters* domainParameters)
{
	printf("DomainParameters {\n");
	printf("\tmaxChannelIds:%d\n", domainParameters->maxChannelIds);
	printf("\tmaxUserIds:%d\n", domainParameters->maxUserIds);
	printf("\tmaxTokenIds:%d\n", domainParameters->maxTokenIds);
	printf("\tnumPriorities:%d\n", domainParameters->numPriorities);
	printf("\tminThroughput:%d\n", domainParameters->minThroughput);
	printf("\tmaxHeight:%d\n", domainParameters->maxHeight);
	printf("\tmaxMCSPDUsize:%d\n", domainParameters->maxMCSPDUsize);
	printf("\tprotocolVersion:%d\n", domainParameters->protocolVersion);
	printf("}\n");
}

/**
 * Read an MCS Connect Initial PDU.\n
 * @msdn{cc240508}
 * @param mcs MCS module
 * @param s stream
 */

boolean mcs_recv_connect_initial(rdpMcs* mcs, STREAM* s)
{
	int length;
	boolean upwardFlag;

	tpkt_read_header(s);

	if (tpdu_read_data(s) == 0)
		return false;

	if (!ber_read_application_tag(s, MCS_TYPE_CONNECT_INITIAL, &length))
		return false;

	/* callingDomainSelector (OCTET_STRING) */
	if (!ber_read_octet_string(s, &length))
		return false;
	stream_seek(s, length);

	/* calledDomainSelector (OCTET_STRING) */
	if (!ber_read_octet_string(s, &length))
		return false;
	stream_seek(s, length);

	/* upwardFlag (BOOLEAN) */
	if (!ber_read_boolean(s, &upwardFlag))
		return false;

	/* targetParameters (DomainParameters) */
	mcs_read_domain_parameters(s, &mcs->targetParameters);

	/* minimumParameters (DomainParameters) */
	mcs_read_domain_parameters(s, &mcs->minimumParameters);

	/* maximumParameters (DomainParameters) */
	mcs_read_domain_parameters(s, &mcs->maximumParameters);

	if (!ber_read_octet_string(s, &length))
		return false;

	if (!gcc_read_conference_create_request(s, mcs->transport->settings))
		return false;

	return true;
}

/**
 * Write an MCS Connect Initial PDU.\n
 * @msdn{cc240508}
 * @param s stream
 * @param mcs MCS module
 * @param user_data GCC Conference Create Request
 */

void mcs_write_connect_initial(STREAM* s, rdpMcs* mcs, STREAM* user_data)
{
	int length;
	STREAM* tmps;

	tmps = stream_new(stream_get_size(s));

	/* callingDomainSelector (OCTET_STRING) */
	ber_write_octet_string(tmps, callingDomainSelector, sizeof(callingDomainSelector));

	/* calledDomainSelector (OCTET_STRING) */
	ber_write_octet_string(tmps, calledDomainSelector, sizeof(calledDomainSelector));

	/* upwardFlag (BOOLEAN) */
	ber_write_boolean(tmps, true);

	/* targetParameters (DomainParameters) */
	mcs_write_domain_parameters(tmps, &mcs->targetParameters);

	/* minimumParameters (DomainParameters) */
	mcs_write_domain_parameters(tmps, &mcs->minimumParameters);

	/* maximumParameters (DomainParameters) */
	mcs_write_domain_parameters(tmps, &mcs->maximumParameters);

	/* userData (OCTET_STRING) */
	ber_write_octet_string(tmps, user_data->data, stream_get_length(user_data));

	length = stream_get_length(tmps);
	/* Connect-Initial (APPLICATION 101, IMPLICIT SEQUENCE) */
	ber_write_application_tag(s, MCS_TYPE_CONNECT_INITIAL, length);
	stream_write(s, stream_get_head(tmps), length);
	stream_free(tmps);
}

/**
 * Write an MCS Connect Response PDU.\n
 * @msdn{cc240508}
 * @param s stream
 * @param mcs MCS module
 * @param user_data GCC Conference Create Response
 */

void mcs_write_connect_response(STREAM* s, rdpMcs* mcs, STREAM* user_data)
{
	int length;
	STREAM* tmps;

	tmps = stream_new(stream_get_size(s));
	ber_write_enumerated(tmps, 0, MCS_Result_enum_length);
	ber_write_integer(tmps, 0); /* calledConnectId */
	mcs->domainParameters = mcs->targetParameters;
	mcs_write_domain_parameters(tmps, &(mcs->domainParameters));
	/* userData (OCTET_STRING) */
	ber_write_octet_string(tmps, user_data->data, stream_get_length(user_data));

	length = stream_get_length(tmps);
	ber_write_application_tag(s, MCS_TYPE_CONNECT_RESPONSE, length);
	stream_write(s, stream_get_head(tmps), length);
	stream_free(tmps);
}

/**
 * Send MCS Connect Initial.\n
 * @msdn{cc240508}
 * @param mcs mcs module
 */

boolean mcs_send_connect_initial(rdpMcs* mcs)
{
	STREAM* s;
	int length;
	uint8 *bm, *em;
	STREAM* gcc_CCrq;
	STREAM* client_data;
	int status;

	client_data = stream_new(512);
	gcc_write_client_data_blocks(client_data, mcs->transport->settings);

	gcc_CCrq = stream_new(512);
	gcc_write_conference_create_request(gcc_CCrq, client_data);
	length = stream_get_length(gcc_CCrq) + 7;

	s = transport_send_stream_init(mcs->transport, 1024);
	stream_get_mark(s, bm);
	stream_seek(s, 7);

	mcs_write_connect_initial(s, mcs, gcc_CCrq);
	stream_get_mark(s, em);
	length = (em - bm);
	stream_set_mark(s, bm);

	tpkt_write_header(s, length);
	tpdu_write_data(s);
	stream_set_mark(s, em);

	status = transport_write(mcs->transport, s);

	stream_free(gcc_CCrq);
	stream_free(client_data);

	return (status < 0 ? false : true);
}

/**
 * Read MCS Connect Response.\n
 * @msdn{cc240501}
 * @param mcs mcs module
 */

boolean mcs_recv_connect_response(rdpMcs* mcs, STREAM* s)
{
	int length;
	uint8 result;
	uint32 calledConnectId;

	tpkt_read_header(s);

	if (tpdu_read_data(s) == 0)
		return false;

	ber_read_application_tag(s, MCS_TYPE_CONNECT_RESPONSE, &length);
	ber_read_enumerated(s, &result, MCS_Result_enum_length);
	ber_read_integer(s, &calledConnectId);

	if (!mcs_read_domain_parameters(s, &(mcs->domainParameters)))
		return false;

	ber_read_octet_string(s, &length);

	if (!gcc_read_conference_create_response(s, mcs->transport->settings))
	{
		printf("mcs_recv_connect_response: gcc_read_conference_create_response failed\n");
		return false;
	}

	return true;
}

/**
 * Send MCS Connect Response.\n
 * @msdn{cc240501}
 * @param mcs mcs module
 */

boolean mcs_send_connect_response(rdpMcs* mcs)
{
	STREAM* s;
	int length;
	uint8 *bm, *em;
	STREAM* gcc_CCrsp;
	STREAM* server_data;

	server_data = stream_new(512);
	gcc_write_server_data_blocks(server_data, mcs->transport->settings);

	gcc_CCrsp = stream_new(512);
	gcc_write_conference_create_response(gcc_CCrsp, server_data);
	length = stream_get_length(gcc_CCrsp) + 7;

	s = transport_send_stream_init(mcs->transport, 1024);
	stream_get_mark(s, bm);
	stream_seek(s, 7);

	mcs_write_connect_response(s, mcs, gcc_CCrsp);
	stream_get_mark(s, em);
	length = (em - bm);
	stream_set_mark(s, bm);

	tpkt_write_header(s, length);
	tpdu_write_data(s);
	stream_set_mark(s, em);

	transport_write(mcs->transport, s);

	stream_free(gcc_CCrsp);
	stream_free(server_data);

	return true;
}

/**
 * Read MCS Erect Domain Request.\n
 * @msdn{cc240523}
 * @param mcs
 * @param s stream
 */

boolean mcs_recv_erect_domain_request(rdpMcs* mcs, STREAM* s)
{
	uint16 length;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = DomainMCSPDU_ErectDomainRequest;
	if (!mcs_read_domain_mcspdu_header(s, &MCSPDU, &length))
		return false;

	return true;
}

/**
 * Send MCS Erect Domain Request.\n
 * @msdn{cc240523}
 * @param mcs
 */

boolean mcs_send_erect_domain_request(rdpMcs* mcs)
{
	STREAM* s;
	uint16 length = 12;
	s = transport_send_stream_init(mcs->transport, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_ErectDomainRequest, length, 0);

	per_write_integer(s, 0); /* subHeight (INTEGER) */
	per_write_integer(s, 0); /* subInterval (INTEGER) */

	if (transport_write(mcs->transport, s) < 0)
		return false;

	return true;
}

/**
 * Read MCS Attach User Request.\n
 * @msdn{cc240524}
 * @param mcs mcs module
 * @param s stream
 */

boolean mcs_recv_attach_user_request(rdpMcs* mcs, STREAM* s)
{
	uint16 length;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = DomainMCSPDU_AttachUserRequest;
	if (!mcs_read_domain_mcspdu_header(s, &MCSPDU, &length))
		return false;

	return true;
}

/**
 * Send MCS Attach User Request.\n
 * @msdn{cc240524}
 * @param mcs mcs module
 */

boolean mcs_send_attach_user_request(rdpMcs* mcs)
{
	STREAM* s;
	uint16 length = 8;
	s = transport_send_stream_init(mcs->transport, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_AttachUserRequest, length, 0);

	if (transport_write(mcs->transport, s) < 0)
		return false;

	return true;
}

/**
 * Read MCS Attach User Confirm.\n
 * @msdn{cc240525}
 * @param mcs mcs module
 */

boolean mcs_recv_attach_user_confirm(rdpMcs* mcs, STREAM* s)
{
	uint16 length;
	uint8 result;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = DomainMCSPDU_AttachUserConfirm;
	if (!mcs_read_domain_mcspdu_header(s, &MCSPDU, &length))
		return false;

	per_read_enumerated(s, &result, MCS_Result_enum_length); /* result */
	per_read_integer16(s, &(mcs->user_id), MCS_BASE_CHANNEL_ID); /* initiator (UserId) */

	return true;
}

/**
 * Send MCS Attach User Confirm.\n
 * @msdn{cc240525}
 * @param mcs mcs module
 */

boolean mcs_send_attach_user_confirm(rdpMcs* mcs)
{
	STREAM* s;
	uint16 length = 11;
	
	s = transport_send_stream_init(mcs->transport, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_AttachUserConfirm, length, 2);

	per_write_enumerated(s, 0, MCS_Result_enum_length); /* result */
	mcs->user_id = 1002;
	per_write_integer16(s, mcs->user_id, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */

	transport_write(mcs->transport, s);

	return true;
}

/**
 * Read MCS Channel Join Request.\n
 * @msdn{cc240526}
 * @param mcs mcs module
 * @param s stream
 */

boolean mcs_recv_channel_join_request(rdpMcs* mcs, STREAM* s, uint16* channel_id)
{
	uint16 length;
	enum DomainMCSPDU MCSPDU;
	uint16 user_id;

	MCSPDU = DomainMCSPDU_ChannelJoinRequest;
	if (!mcs_read_domain_mcspdu_header(s, &MCSPDU, &length))
		return false;

	if (!per_read_integer16(s, &user_id, MCS_BASE_CHANNEL_ID))
		return false;
	if (user_id != mcs->user_id)
		return false;
	if (!per_read_integer16(s, channel_id, 0))
		return false;

	return true;
}

/**
 * Send MCS Channel Join Request.\n
 * @msdn{cc240526}
 * @param mcs mcs module
 * @param channel_id channel id
 */

boolean mcs_send_channel_join_request(rdpMcs* mcs, uint16 channel_id)
{
	STREAM* s;
	uint16 length = 12;
	s = transport_send_stream_init(mcs->transport, 12);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_ChannelJoinRequest, length, 0);

	per_write_integer16(s, mcs->user_id, MCS_BASE_CHANNEL_ID);
	per_write_integer16(s, channel_id, 0);

	if (transport_write(mcs->transport, s) < 0)
		return false;

	return true;
}

/**
 * Read MCS Channel Join Confirm.\n
 * @msdn{cc240527}
 * @param mcs mcs module
 */

boolean mcs_recv_channel_join_confirm(rdpMcs* mcs, STREAM* s, uint16* channel_id)
{
	uint16 length;
	uint8 result;
	uint16 initiator;
	uint16 requested;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = DomainMCSPDU_ChannelJoinConfirm;
	if (!mcs_read_domain_mcspdu_header(s, &MCSPDU, &length))
		return false;

	per_read_enumerated(s, &result, MCS_Result_enum_length); /* result */
	per_read_integer16(s, &initiator, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
	per_read_integer16(s, &requested, 0); /* requested (ChannelId) */
	per_read_integer16(s, channel_id, 0); /* channelId */

	return true;
}

/**
 * Send MCS Channel Join Confirm.\n
 * @msdn{cc240527}
 * @param mcs mcs module
 */

boolean mcs_send_channel_join_confirm(rdpMcs* mcs, uint16 channel_id)
{
	STREAM* s;
	uint16 length = 15;
	s = transport_send_stream_init(mcs->transport, 15);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_ChannelJoinConfirm, length, 2);

	per_write_enumerated(s, 0, MCS_Result_enum_length); /* result */
	per_write_integer16(s, mcs->user_id, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
	per_write_integer16(s, channel_id, 0); /* requested (ChannelId) */
	per_write_integer16(s, channel_id, 0); /* channelId */

	transport_write(mcs->transport, s);

	return true;
}

/**
 * Send MCS Disconnect Provider Ultimatum PDU.\n
 * @param mcs mcs module
 */

boolean mcs_send_disconnect_provider_ultimatum(rdpMcs* mcs)
{
	STREAM* s;
	uint16 length = 9;
	s = transport_send_stream_init(mcs->transport, 9);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_DisconnectProviderUltimatum, length, 1);

	per_write_enumerated(s, 0, 0); /* reason */

	transport_write(mcs->transport, s);

	return true;
}

/**
 * Instantiate new MCS module.
 * @param transport transport
 * @return new MCS module
 */

rdpMcs* mcs_new(rdpTransport* transport)
{
	rdpMcs* mcs;

	mcs = (rdpMcs*) xzalloc(sizeof(rdpMcs));

	if (mcs != NULL)
	{
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
		xfree(mcs);
	}
}
