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

uint8 callingDomainSelector[1] = "\x01";
uint8 calledDomainSelector[1] = "\x01";

uint8 mcs_result_enumerated[16][32] =
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

/**
 * MCS Connection Sequence.
 * @param mcs mcs module
 * @return
 */

boolean mcs_connect(rdpMcs* mcs)
{
	int i;
	uint16 channelId;
	rdpSettings* settings;

	settings = mcs->transport->settings;

	mcs_send_connect_initial(mcs);
	mcs_recv_connect_response(mcs);

	mcs_send_erect_domain_request(mcs);
	mcs_send_attach_user_request(mcs);
	mcs_recv_attach_user_confirm(mcs);

	mcs_send_channel_join_request(mcs, mcs->user_id);
	mcs_recv_channel_join_confirm(mcs);

	mcs_send_channel_join_request(mcs, MCS_GLOBAL_CHANNEL_ID);
	mcs_recv_channel_join_confirm(mcs);

	for (i = 0; i < settings->num_channels; i++)
	{
		channelId = settings->channels[i].chan_id;
		mcs_send_channel_join_request(mcs, channelId);
		mcs_recv_channel_join_confirm(mcs);
	}

	return True;
}

/**
 * Read a DomainMCSPDU header.
 * @param s stream
 * @param domainMCSPDU DomainMCSPDU type
 * @param length TPKT length
 * @return
 */

boolean mcs_read_domain_mcspdu_header(STREAM* s, enum DomainMCSPDU* domainMCSPDU, int* length)
{
	uint8 choice;
	enum DomainMCSPDU MCSPDU;

	*length = tpkt_read_header(s);

	if (tpdu_read_data(s) == 0)
		return False;

	MCSPDU = *domainMCSPDU;
	per_read_choice(s, &choice);
	*domainMCSPDU = (choice >> 2);

	if (*domainMCSPDU != MCSPDU)
		return False;

	return True;
}

/**
 * Write a DomainMCSPDU header.
 * @param s stream
 * @param domainMCSPDU DomainMCSPDU type
 * @param length TPKT length
 */

void mcs_write_domain_mcspdu_header(STREAM* s, enum DomainMCSPDU domainMCSPDU, int length, uint8 options)
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

void mcs_read_domain_parameters(STREAM* s, DomainParameters* domainParameters)
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
}

/**
 * Write MCS Domain Parameters.
 * @param s stream
 * @param domainParameters domain parameters
 */

void mcs_write_domain_parameters(STREAM* s, DomainParameters* domainParameters)
{
	int length;
	uint8 *bm, *em;

	stream_get_mark(s, bm);
	stream_seek(s, 2);

	ber_write_integer(s, domainParameters->maxChannelIds);
	ber_write_integer(s, domainParameters->maxUserIds);
	ber_write_integer(s, domainParameters->maxTokenIds);
	ber_write_integer(s, domainParameters->numPriorities);
	ber_write_integer(s, domainParameters->minThroughput);
	ber_write_integer(s, domainParameters->maxHeight);
	ber_write_integer(s, domainParameters->maxMCSPDUsize);
	ber_write_integer(s, domainParameters->protocolVersion);

	stream_get_mark(s, em);
	length = (em - bm) - 2;
	stream_set_mark(s, bm);

	ber_write_sequence_tag(s, length);
	stream_set_mark(s, em);
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

boolean mcs_read_connect_initial(rdpMcs* mcs, STREAM* s)
{
	int length;
	boolean upwardFlag;

	tpkt_read_header(s);

	if (tpdu_read_data(s) == 0)
		return False;

	if (!ber_read_application_tag(s, MCS_TYPE_CONNECT_INITIAL, &length))
		return False;

	/* callingDomainSelector (OCTET_STRING) */
	if (!ber_read_octet_string(s, &length))
		return False;
	stream_seek(s, length);

	/* calledDomainSelector (OCTET_STRING) */
	if (!ber_read_octet_string(s, &length))
		return False;
	stream_seek(s, length);

	/* upwardFlag (BOOLEAN) */
	if (!ber_read_boolean(s, &upwardFlag))
		return False;

	/* targetParameters (DomainParameters) */
	mcs_read_domain_parameters(s, &mcs->targetParameters);

	/* minimumParameters (DomainParameters) */
	mcs_read_domain_parameters(s, &mcs->minimumParameters);

	/* maximumParameters (DomainParameters) */
	mcs_read_domain_parameters(s, &mcs->maximumParameters);

	if (!ber_read_octet_string(s, &length))
		return False;

	if (!gcc_read_conference_create_request(s, mcs->transport->settings))
		return False;

	return True;
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
	uint8 *bm, *em;

	int gcc_CCrq_length = stream_get_length(user_data);

	stream_get_mark(s, bm);
	stream_seek(s, 5);

	/* callingDomainSelector (OCTET_STRING) */
	ber_write_octet_string(s, callingDomainSelector, sizeof(callingDomainSelector));

	/* calledDomainSelector (OCTET_STRING) */
	ber_write_octet_string(s, calledDomainSelector, sizeof(calledDomainSelector));

	/* upwardFlag (BOOLEAN) */
	ber_write_boolean(s, True);

	/* targetParameters (DomainParameters) */
	mcs_write_domain_parameters(s, &mcs->targetParameters);

	/* minimumParameters (DomainParameters) */
	mcs_write_domain_parameters(s, &mcs->minimumParameters);

	/* maximumParameters (DomainParameters) */
	mcs_write_domain_parameters(s, &mcs->maximumParameters);

	/* userData (OCTET_STRING) */
	ber_write_octet_string(s, user_data->data, gcc_CCrq_length);

	stream_get_mark(s, em);
	length = (em - bm) - 5;
	stream_set_mark(s, bm);

	/* Connect-Initial (APPLICATION 101, IMPLICIT SEQUENCE) */
	ber_write_application_tag(s, MCS_TYPE_CONNECT_INITIAL, length);
	stream_set_mark(s, em);
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
	uint8 *bm, *em;

	int gcc_CCrsp_length = stream_get_length(user_data);

	stream_get_mark(s, bm);
	stream_seek(s, 3);

	ber_write_enumerated(s, 0, MCS_Result_enum_length);
	ber_write_integer(s, 0); /* calledConnectId */

	mcs->domainParameters = mcs->targetParameters;
	mcs_write_domain_parameters(s, &(mcs->domainParameters));

	/* userData (OCTET_STRING) */
	ber_write_octet_string(s, user_data->data, gcc_CCrsp_length);

	stream_get_mark(s, em);
	length = (em - bm) - 3;
	stream_set_mark(s, bm);

	ber_write_application_tag(s, MCS_TYPE_CONNECT_RESPONSE, length);
	stream_set_mark(s, em);
}

/**
 * Send MCS Connect Initial.\n
 * @msdn{cc240508}
 * @param mcs mcs module
 */

void mcs_send_connect_initial(rdpMcs* mcs)
{
	STREAM* s;
	int length;
	uint8 *bm, *em;
	STREAM* gcc_CCrq;
	STREAM* client_data;

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

	transport_write(mcs->transport, s);

	stream_free(gcc_CCrq);
	stream_free(client_data);
}

/**
 * Receive MCS Connect Response.\n
 * @msdn{cc240501}
 * @param mcs mcs module
 */

void mcs_recv_connect_response(rdpMcs* mcs)
{
	STREAM* s;
	int length;
	uint8 result;
	uint32 calledConnectId;

	s = transport_recv_stream_init(mcs->transport, 1024);
	transport_read(mcs->transport, s);

	tpkt_read_header(s);

	if (tpdu_read_data(s) == 0)
		return;

	ber_read_application_tag(s, MCS_TYPE_CONNECT_RESPONSE, &length);
	ber_read_enumerated(s, &result, MCS_Result_enum_length);
	ber_read_integer(s, &calledConnectId);

	mcs_read_domain_parameters(s, &(mcs->domainParameters));

	ber_read_octet_string(s, &length);

	gcc_read_conference_create_response(s, mcs->transport->settings);
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

	return True;
}

/**
 * Read MCS Erect Domain Request.\n
 * @msdn{cc240523}
 * @param mcs
 * @param s stream
 */

boolean mcs_read_erect_domain_request(rdpMcs* mcs, STREAM* s)
{
	int length;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = DomainMCSPDU_ErectDomainRequest;
	if (!mcs_read_domain_mcspdu_header(s, &MCSPDU, &length))
		return False;

	return True;
}

/**
 * Send MCS Erect Domain Request.\n
 * @msdn{cc240523}
 * @param mcs
 */

void mcs_send_erect_domain_request(rdpMcs* mcs)
{
	STREAM* s;
	int length = 12;
	s = transport_send_stream_init(mcs->transport, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_ErectDomainRequest, length, 0);

	per_write_integer(s, 0); /* subHeight (INTEGER) */
	per_write_integer(s, 0); /* subInterval (INTEGER) */

	transport_write(mcs->transport, s);
}

/**
 * Read MCS Attach User Request.\n
 * @msdn{cc240524}
 * @param mcs mcs module
 * @param s stream
 */

boolean mcs_read_attach_user_request(rdpMcs* mcs, STREAM* s)
{
	int length;
	enum DomainMCSPDU MCSPDU;

	MCSPDU = DomainMCSPDU_AttachUserRequest;
	if (!mcs_read_domain_mcspdu_header(s, &MCSPDU, &length))
		return False;

	return True;
}

/**
 * Send MCS Attach User Request.\n
 * @msdn{cc240524}
 * @param mcs mcs module
 */

void mcs_send_attach_user_request(rdpMcs* mcs)
{
	STREAM* s;
	int length = 8;
	s = transport_send_stream_init(mcs->transport, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_AttachUserRequest, length, 0);

	transport_write(mcs->transport, s);
}

/**
 * Receive MCS Attach User Confirm.\n
 * @msdn{cc240525}
 * @param mcs mcs module
 */

void mcs_recv_attach_user_confirm(rdpMcs* mcs)
{
	STREAM* s;
	int length;
	uint8 result;
	enum DomainMCSPDU MCSPDU;

	s = transport_recv_stream_init(mcs->transport, 32);
	transport_read(mcs->transport, s);

	MCSPDU = DomainMCSPDU_AttachUserConfirm;
	mcs_read_domain_mcspdu_header(s, &MCSPDU, &length);

	per_read_enumerated(s, &result, MCS_Result_enum_length); /* result */
	per_read_integer16(s, &(mcs->user_id), MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
}

/**
 * Send MCS Attach User Confirm.\n
 * @msdn{cc240525}
 * @param mcs mcs module
 */

boolean mcs_send_attach_user_confirm(rdpMcs* mcs)
{
	STREAM* s;
	int length = 11;
	
	s = transport_send_stream_init(mcs->transport, length);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_AttachUserConfirm, length, 2);

	per_write_enumerated(s, 0, MCS_Result_enum_length); /* result */
	mcs->user_id = 1002;
	per_write_integer16(s, mcs->user_id, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */

	transport_write(mcs->transport, s);

	return True;
}

/**
 * Send MCS Channel Join Request.\n
 * @msdn{cc240526}
 * @param mcs mcs module
 * @param channel_id channel id
 */

void mcs_send_channel_join_request(rdpMcs* mcs, uint16 channel_id)
{
	STREAM* s;
	int length = 12;
	s = transport_send_stream_init(mcs->transport, 12);

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_ChannelJoinRequest, length, 0);

	per_write_integer16(s, mcs->user_id, MCS_BASE_CHANNEL_ID);
	per_write_integer16(s, channel_id, 0);

	transport_write(mcs->transport, s);
}

/**
 * Receive MCS Channel Join Confirm.\n
 * @msdn{cc240527}
 * @param mcs mcs module
 */

void mcs_recv_channel_join_confirm(rdpMcs* mcs)
{
	STREAM* s;
	int length;
	uint8 result;
	uint16 initiator;
	uint16 requested;
	uint16 channelId;
	enum DomainMCSPDU MCSPDU;

	s = transport_recv_stream_init(mcs->transport, 32);
	transport_read(mcs->transport, s);

	MCSPDU = DomainMCSPDU_ChannelJoinConfirm;
	mcs_read_domain_mcspdu_header(s, &MCSPDU, &length);

	per_read_enumerated(s, &result, MCS_Result_enum_length); /* result */
	per_read_integer16(s, &initiator, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
	per_read_integer16(s, &requested, 0); /* requested (ChannelId) */
	per_read_integer16(s, &channelId, 0); /* channelId */
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
