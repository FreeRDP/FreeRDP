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

#ifndef __MCS_H
#define __MCS_H

#include "ber.h"
#include "transport.h"

#include <freerdp/types.h>
#include <freerdp/utils/stream.h>

#define MCS_BASE_CHANNEL_ID	1001
#define MCS_GLOBAL_CHANNEL_ID	1003

enum MCS_Result
{
	MCS_Result_successful = 0,
	MCS_Result_domain_merging = 1,
	MCS_Result_domain_not_hierarchical = 2,
	MCS_Result_no_such_channel = 3,
	MCS_Result_no_such_domain = 4,
	MCS_Result_no_such_user = 5,
	MCS_Result_not_admitted = 6,
	MCS_Result_other_user_id = 7,
	MCS_Result_parameters_unacceptable = 8,
	MCS_Result_token_not_available = 9,
	MCS_Result_token_not_possessed = 10,
	MCS_Result_too_many_channels = 11,
	MCS_Result_too_many_tokens = 12,
	MCS_Result_too_many_users = 13,
	MCS_Result_unspecified_failure = 14,
	MCS_Result_user_rejected = 15,
	MCS_Result_enum_length = 16
};

enum DomainMCSPDU
{
	DomainMCSPDU_PlumbDomainIndication = 0,
	DomainMCSPDU_ErectDomainRequest = 1,
	DomainMCSPDU_MergeChannelsRequest = 2,
	DomainMCSPDU_MergeChannelsConfirm = 3,
	DomainMCSPDU_PurgeChannelsIndication = 4,
	DomainMCSPDU_MergeTokensRequest = 5,
	DomainMCSPDU_MergeTokensConfirm = 6,
	DomainMCSPDU_PurgeTokensIndication = 7,
	DomainMCSPDU_DisconnectProviderUltimatum = 8,
	DomainMCSPDU_RejectMCSPDUUltimatum = 9,
	DomainMCSPDU_AttachUserRequest = 10,
	DomainMCSPDU_AttachUserConfirm = 11,
	DomainMCSPDU_DetachUserRequest = 12,
	DomainMCSPDU_DetachUserIndication = 13,
	DomainMCSPDU_ChannelJoinRequest = 14,
	DomainMCSPDU_ChannelJoinConfirm = 15,
	DomainMCSPDU_ChannelLeaveRequest = 16,
	DomainMCSPDU_ChannelConveneRequest = 17,
	DomainMCSPDU_ChannelConveneConfirm = 18,
	DomainMCSPDU_ChannelDisbandRequest = 19,
	DomainMCSPDU_ChannelDisbandIndication = 20,
	DomainMCSPDU_ChannelAdmitRequest = 21,
	DomainMCSPDU_ChannelAdmitIndication = 22,
	DomainMCSPDU_ChannelExpelRequest = 23,
	DomainMCSPDU_ChannelExpelIndication = 24,
	DomainMCSPDU_SendDataRequest = 25,
	DomainMCSPDU_SendDataIndication = 26,
	DomainMCSPDU_UniformSendDataRequest = 27,
	DomainMCSPDU_UniformSendDataIndication = 28,
	DomainMCSPDU_TokenGrabRequest = 29,
	DomainMCSPDU_TokenGrabConfirm = 30,
	DomainMCSPDU_TokenInhibitRequest = 31,
	DomainMCSPDU_TokenInhibitConfirm = 32,
	DomainMCSPDU_TokenGiveRequest = 33,
	DomainMCSPDU_TokenGiveIndication = 34,
	DomainMCSPDU_TokenGiveResponse = 35,
	DomainMCSPDU_TokenGiveConfirm = 36,
	DomainMCSPDU_TokenPleaseRequest = 37,
	DomainMCSPDU_TokenPleaseConfirm = 38,
	DomainMCSPDU_TokenReleaseRequest = 39,
	DomainMCSPDU_TokenReleaseConfirm = 40,
	DomainMCSPDU_TokenTestRequest = 41,
	DomainMCSPDU_TokenTestConfirm = 42,
	DomainMCSPDU_enum_length = 43
};

typedef struct
{
	uint32 maxChannelIds;
	uint32 maxUserIds;
	uint32 maxTokenIds;
	uint32 numPriorities;
	uint32 minThroughput;
	uint32 maxHeight;
	uint32 maxMCSPDUsize;
	uint32 protocolVersion;
} DomainParameters;

struct rdp_mcs
{
	uint16 user_id;
	struct rdp_transport* transport;
	DomainParameters domainParameters;
	DomainParameters targetParameters;
	DomainParameters minimumParameters;
	DomainParameters maximumParameters;

	boolean user_channel_joined;
	boolean global_channel_joined;
};
typedef struct rdp_mcs rdpMcs;

#define MCS_SEND_DATA_HEADER_MAX_LENGTH		8

#define MCS_TYPE_CONNECT_INITIAL		0x65
#define MCS_TYPE_CONNECT_RESPONSE		0x66

void mcs_write_connect_initial(STREAM* s, rdpMcs* mcs, STREAM* user_data);
void mcs_write_connect_response(STREAM* s, rdpMcs* mcs, STREAM* user_data);

boolean mcs_recv_connect_initial(rdpMcs* mcs, STREAM* s);
boolean mcs_send_connect_initial(rdpMcs* mcs);
boolean mcs_recv_connect_response(rdpMcs* mcs, STREAM* s);
boolean mcs_send_connect_response(rdpMcs* mcs);
boolean mcs_recv_erect_domain_request(rdpMcs* mcs, STREAM* s);
boolean mcs_send_erect_domain_request(rdpMcs* mcs);
boolean mcs_recv_attach_user_request(rdpMcs* mcs, STREAM* s);
boolean mcs_send_attach_user_request(rdpMcs* mcs);
boolean mcs_recv_attach_user_confirm(rdpMcs* mcs, STREAM* s);
boolean mcs_send_attach_user_confirm(rdpMcs* mcs);
boolean mcs_recv_channel_join_request(rdpMcs* mcs, STREAM* s, uint16* channel_id);
boolean mcs_send_channel_join_request(rdpMcs* mcs, uint16 channel_id);
boolean mcs_recv_channel_join_confirm(rdpMcs* mcs, STREAM* s, uint16* channel_id);
boolean mcs_send_channel_join_confirm(rdpMcs* mcs, uint16 channel_id);
boolean mcs_send_disconnect_provider_ultimatum(rdpMcs* mcs);
boolean mcs_read_domain_mcspdu_header(STREAM* s, enum DomainMCSPDU* domainMCSPDU, uint16* length);
void mcs_write_domain_mcspdu_header(STREAM* s, enum DomainMCSPDU domainMCSPDU, uint16 length, uint8 options);

rdpMcs* mcs_new(rdpTransport* transport);
void mcs_free(rdpMcs* mcs);

#endif /* __MCS_H */
