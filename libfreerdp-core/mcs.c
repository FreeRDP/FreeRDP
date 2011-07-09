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
 * Initialize MCS Domain Parameters.
 * @param domainParameters domain parameters
 * @param maxChannelIds max channel ids
 * @param maxUserIds max user ids
 * @param maxTokenIds max token ids
 * @param maxMCSPDUsize max MCS PDU size
 */

static void mcs_init_domain_parameters(DOMAIN_PARAMETERS* domainParameters,
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

static void mcs_read_domain_parameters(STREAM* s, DOMAIN_PARAMETERS* domainParameters)
{
	int length;
	ber_read_sequence_of_tag(s, &length);
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

static void mcs_write_domain_parameters(STREAM* s, DOMAIN_PARAMETERS* domainParameters)
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

	ber_write_sequence_of_tag(s, length);
	stream_set_mark(s, em);
}

static void mcs_print_domain_parameters(DOMAIN_PARAMETERS* domainParameters)
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
 * Write an MCS Connect Initial PDU.
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

void mcs_send_connect_initial(rdpMcs* mcs)
{
	STREAM* s;
	int length;
	uint8 *bm, *em;
	STREAM* gcc_CCrq;
	STREAM* client_data;

	client_data = stream_new(512);
	gcc_write_client_core_data(client_data, mcs->transport->settings);
	gcc_write_client_cluster_data(client_data, mcs->transport->settings);
	gcc_write_client_security_data(client_data, mcs->transport->settings);
	gcc_write_client_network_data(client_data, mcs->transport->settings);
	gcc_write_client_monitor_data(client_data, mcs->transport->settings);

	gcc_CCrq = stream_new(512);
	gcc_write_conference_create_request(gcc_CCrq, client_data);
	length = stream_get_length(gcc_CCrq) + 7;

	s = stream_new(512);
	stream_get_mark(s, bm);
	stream_seek(s, 7);

	mcs_write_connect_initial(s, mcs, gcc_CCrq);
	stream_get_mark(s, em);
	length = (em - bm);
	stream_set_mark(s, bm);

	tpkt_write_header(s, length);
	tpdu_write_data(s, length - 5);
	stream_set_mark(s, em);

	tls_write(mcs->transport->tls, s->data, stream_get_length(s));
}

void mcs_recv_connect_response(rdpMcs* mcs)
{
	STREAM* s;
	int length;
	uint8 result;
	uint32 calledConnectId;

	s = stream_new(1024);
	tls_read(mcs->transport->tls, s->data, s->size);

	tpkt_read_header(s);
	tpdu_read_data(s);

	ber_read_application_tag(s, MCS_TYPE_CONNECT_RESPONSE, &length);
	ber_read_enumerated(s, &result, 15);
	ber_read_integer(s, &calledConnectId);

	printf("MCS Connect-Response Result: %s\n", mcs_result_enumerated[result]);

	mcs_read_domain_parameters(s, &(mcs->domainParameters));
	mcs_print_domain_parameters(&(mcs->domainParameters));

	ber_read_octet_string(s, &length);

	gcc_read_conference_create_response(s);
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
