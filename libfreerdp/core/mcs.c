/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * T.125 Multipoint Communication Service (MCS) Protocol
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2017 Armin Novak <armin.novak@thincast.com>
 * Copyright 2017 Thincast Technologies GmbH
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
#include <winpr/assert.h>
#include <freerdp/log.h>

#include "gcc.h"

#include "mcs.h"
#include "tpdu.h"
#include "tpkt.h"
#include "client.h"
#include "connection.h"

#define TAG FREERDP_TAG("core")

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

const char* mcs_domain_pdu_string(DomainMCSPDU pdu)
{
	switch (pdu)
	{
		case DomainMCSPDU_PlumbDomainIndication:
			return "DomainMCSPDU_PlumbDomainIndication";
		case DomainMCSPDU_ErectDomainRequest:
			return "DomainMCSPDU_ErectDomainRequest";
		case DomainMCSPDU_MergeChannelsRequest:
			return "DomainMCSPDU_MergeChannelsRequest";
		case DomainMCSPDU_MergeChannelsConfirm:
			return "DomainMCSPDU_MergeChannelsConfirm";
		case DomainMCSPDU_PurgeChannelsIndication:
			return "DomainMCSPDU_PurgeChannelsIndication";
		case DomainMCSPDU_MergeTokensRequest:
			return "DomainMCSPDU_MergeTokensRequest";
		case DomainMCSPDU_MergeTokensConfirm:
			return "DomainMCSPDU_MergeTokensConfirm";
		case DomainMCSPDU_PurgeTokensIndication:
			return "DomainMCSPDU_PurgeTokensIndication";
		case DomainMCSPDU_DisconnectProviderUltimatum:
			return "DomainMCSPDU_DisconnectProviderUltimatum";
		case DomainMCSPDU_RejectMCSPDUUltimatum:
			return "DomainMCSPDU_RejectMCSPDUUltimatum";
		case DomainMCSPDU_AttachUserRequest:
			return "DomainMCSPDU_AttachUserRequest";
		case DomainMCSPDU_AttachUserConfirm:
			return "DomainMCSPDU_AttachUserConfirm";
		case DomainMCSPDU_DetachUserRequest:
			return "DomainMCSPDU_DetachUserRequest";
		case DomainMCSPDU_DetachUserIndication:
			return "DomainMCSPDU_DetachUserIndication";
		case DomainMCSPDU_ChannelJoinRequest:
			return "DomainMCSPDU_ChannelJoinRequest";
		case DomainMCSPDU_ChannelJoinConfirm:
			return "DomainMCSPDU_ChannelJoinConfirm";
		case DomainMCSPDU_ChannelLeaveRequest:
			return "DomainMCSPDU_ChannelLeaveRequest";
		case DomainMCSPDU_ChannelConveneRequest:
			return "DomainMCSPDU_ChannelConveneRequest";
		case DomainMCSPDU_ChannelConveneConfirm:
			return "DomainMCSPDU_ChannelConveneConfirm";
		case DomainMCSPDU_ChannelDisbandRequest:
			return "DomainMCSPDU_ChannelDisbandRequest";
		case DomainMCSPDU_ChannelDisbandIndication:
			return "DomainMCSPDU_ChannelDisbandIndication";
		case DomainMCSPDU_ChannelAdmitRequest:
			return "DomainMCSPDU_ChannelAdmitRequest";
		case DomainMCSPDU_ChannelAdmitIndication:
			return "DomainMCSPDU_ChannelAdmitIndication";
		case DomainMCSPDU_ChannelExpelRequest:
			return "DomainMCSPDU_ChannelExpelRequest";
		case DomainMCSPDU_ChannelExpelIndication:
			return "DomainMCSPDU_ChannelExpelIndication";
		case DomainMCSPDU_SendDataRequest:
			return "DomainMCSPDU_SendDataRequest";
		case DomainMCSPDU_SendDataIndication:
			return "DomainMCSPDU_SendDataIndication";
		case DomainMCSPDU_UniformSendDataRequest:
			return "DomainMCSPDU_UniformSendDataRequest";
		case DomainMCSPDU_UniformSendDataIndication:
			return "DomainMCSPDU_UniformSendDataIndication";
		case DomainMCSPDU_TokenGrabRequest:
			return "DomainMCSPDU_TokenGrabRequest";
		case DomainMCSPDU_TokenGrabConfirm:
			return "DomainMCSPDU_TokenGrabConfirm";
		case DomainMCSPDU_TokenInhibitRequest:
			return "DomainMCSPDU_TokenInhibitRequest";
		case DomainMCSPDU_TokenInhibitConfirm:
			return "DomainMCSPDU_TokenInhibitConfirm";
		case DomainMCSPDU_TokenGiveRequest:
			return "DomainMCSPDU_TokenGiveRequest";
		case DomainMCSPDU_TokenGiveIndication:
			return "DomainMCSPDU_TokenGiveIndication";
		case DomainMCSPDU_TokenGiveResponse:
			return "DomainMCSPDU_TokenGiveResponse";
		case DomainMCSPDU_TokenGiveConfirm:
			return "DomainMCSPDU_TokenGiveConfirm";
		case DomainMCSPDU_TokenPleaseRequest:
			return "DomainMCSPDU_TokenPleaseRequest";
		case DomainMCSPDU_TokenPleaseConfirm:
			return "DomainMCSPDU_TokenPleaseConfirm";
		case DomainMCSPDU_TokenReleaseRequest:
			return "DomainMCSPDU_TokenReleaseRequest";
		case DomainMCSPDU_TokenReleaseConfirm:
			return "DomainMCSPDU_TokenReleaseConfirm";
		case DomainMCSPDU_TokenTestRequest:
			return "DomainMCSPDU_TokenTestRequest";
		case DomainMCSPDU_TokenTestConfirm:
			return "DomainMCSPDU_TokenTestConfirm";
		case DomainMCSPDU_enum_length:
			return "DomainMCSPDU_enum_length";
		default:
			return "DomainMCSPDU_UNKNOWN";
	}
}

static BOOL mcs_merge_domain_parameters(DomainParameters* targetParameters,
                                        DomainParameters* minimumParameters,
                                        DomainParameters* maximumParameters,
                                        DomainParameters* pOutParameters);

static BOOL mcs_write_connect_initial(wStream* s, rdpMcs* mcs, wStream* userData);
static BOOL mcs_write_connect_response(wStream* s, rdpMcs* mcs, wStream* userData);
static BOOL mcs_read_domain_mcspdu_header(wStream* s, DomainMCSPDU domainMCSPDU, UINT16* length,
                                          DomainMCSPDU* actual);

static int mcs_initialize_client_channels(rdpMcs* mcs, const rdpSettings* settings)
{
	UINT32 index;

	if (!mcs || !settings)
		return -1;

	mcs->channelCount = freerdp_settings_get_uint32(settings, FreeRDP_ChannelCount);

	if (mcs->channelCount > mcs->channelMaxCount)
		mcs->channelCount = mcs->channelMaxCount;

	ZeroMemory(mcs->channels, sizeof(rdpMcsChannel) * mcs->channelMaxCount);

	for (index = 0; index < mcs->channelCount; index++)
	{
		const CHANNEL_DEF* defchannel =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_ChannelDefArray, index);
		rdpMcsChannel* cur = &mcs->channels[index];
		WINPR_ASSERT(defchannel);
		CopyMemory(cur->Name, defchannel->name, CHANNEL_NAME_LEN);
		cur->options = defchannel->options;
	}

	return 0;
}

/**
 * Read a DomainMCSPDU header.
 * @param s stream
 * @param domainMCSPDU DomainMCSPDU type
 * @param length TPKT length
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL mcs_read_domain_mcspdu_header(wStream* s, DomainMCSPDU domainMCSPDU, UINT16* length,
                                   DomainMCSPDU* actual)
{
	UINT16 li;
	BYTE choice;
	DomainMCSPDU MCSPDU;

	if (actual)
		*actual = DomainMCSPDU_invalid;

	WINPR_ASSERT(s);
	WINPR_ASSERT(domainMCSPDU);
	WINPR_ASSERT(length);

	if (!tpkt_read_header(s, length))
		return FALSE;

	if (!tpdu_read_data(s, &li, *length))
		return FALSE;

	if (!per_read_choice(s, &choice))
		return FALSE;

	MCSPDU = (choice >> 2);
	if (actual)
		*actual = MCSPDU;

	if (domainMCSPDU != MCSPDU)
	{
		WLog_ERR(TAG, "Expected MCS %s, got %s", mcs_domain_pdu_string(domainMCSPDU),
		         mcs_domain_pdu_string(MCSPDU));
		return FALSE;
	}

	return TRUE;
}

/**
 * Write a DomainMCSPDU header.
 * @param s stream
 * @param domainMCSPDU DomainMCSPDU type
 * @param length TPKT length
 */

BOOL mcs_write_domain_mcspdu_header(wStream* s, DomainMCSPDU domainMCSPDU, UINT16 length,
                                    BYTE options)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT((options & ~0x03) == 0);
	WINPR_ASSERT((domainMCSPDU & ~0x3F) == 0);

	if (!tpkt_write_header(s, length))
		return FALSE;
	if (!tpdu_write_data(s))
		return FALSE;
	return per_write_choice(s, (BYTE)((domainMCSPDU << 2) | options));
}

/**
 * Initialize MCS Domain Parameters.
 * @param domainParameters domain parameters
 * @param maxChannelIds max channel ids
 * @param maxUserIds max user ids
 * @param maxTokenIds max token ids
 * @param maxMCSPDUsize max MCS PDU size
 */

static BOOL mcs_init_domain_parameters(DomainParameters* domainParameters, UINT32 maxChannelIds,
                                       UINT32 maxUserIds, UINT32 maxTokenIds, UINT32 maxMCSPDUsize)
{
	if (!domainParameters)
		return FALSE;

	domainParameters->maxChannelIds = maxChannelIds;
	domainParameters->maxUserIds = maxUserIds;
	domainParameters->maxTokenIds = maxTokenIds;
	domainParameters->maxMCSPDUsize = maxMCSPDUsize;
	domainParameters->numPriorities = 1;
	domainParameters->minThroughput = 0;
	domainParameters->maxHeight = 1;
	domainParameters->protocolVersion = 2;
	return TRUE;
}

/**
 * Read MCS Domain Parameters.
 * @param s stream
 * @param domainParameters domain parameters
 */

static BOOL mcs_read_domain_parameters(wStream* s, DomainParameters* domainParameters)
{
	size_t length;

	if (!s || !domainParameters)
		return FALSE;

	return ber_read_sequence_tag(s, &length) &&
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

static BOOL mcs_write_domain_parameters(wStream* s, DomainParameters* domainParameters)
{
	size_t length;
	wStream* tmps;

	if (!s || !domainParameters)
		return FALSE;

	tmps = Stream_New(NULL, Stream_Capacity(s));

	if (!tmps)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

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
	return TRUE;
}

#ifdef DEBUG_MCS
/**
 * Print MCS Domain Parameters.
 * @param domainParameters domain parameters
 */

static void mcs_print_domain_parameters(DomainParameters* domainParameters)
{
	WLog_INFO(TAG, "DomainParameters {");

	if (domainParameters)
	{
		WLog_INFO(TAG, "\tmaxChannelIds:%" PRIu32 "", domainParameters->maxChannelIds);
		WLog_INFO(TAG, "\tmaxUserIds:%" PRIu32 "", domainParameters->maxUserIds);
		WLog_INFO(TAG, "\tmaxTokenIds:%" PRIu32 "", domainParameters->maxTokenIds);
		WLog_INFO(TAG, "\tnumPriorities:%" PRIu32 "", domainParameters->numPriorities);
		WLog_INFO(TAG, "\tminThroughput:%" PRIu32 "", domainParameters->minThroughput);
		WLog_INFO(TAG, "\tmaxHeight:%" PRIu32 "", domainParameters->maxHeight);
		WLog_INFO(TAG, "\tmaxMCSPDUsize:%" PRIu32 "", domainParameters->maxMCSPDUsize);
		WLog_INFO(TAG, "\tprotocolVersion:%" PRIu32 "", domainParameters->protocolVersion);
	}
	else
		WLog_INFO(TAG, "\tdomainParameters=%p", domainParameters);

	WLog_INFO(TAG, "}");
}
#endif

/**
 * Merge MCS Domain Parameters.
 * @param targetParameters target parameters
 * @param minimumParameters minimum parameters
 * @param maximumParameters maximum parameters
 * @param pOutParameters output parameters
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL mcs_merge_domain_parameters(DomainParameters* targetParameters,
                                 DomainParameters* minimumParameters,
                                 DomainParameters* maximumParameters,
                                 DomainParameters* pOutParameters)
{
	/* maxChannelIds */
	if (!targetParameters || !minimumParameters || !maximumParameters || !pOutParameters)
		return FALSE;

	if (targetParameters->maxChannelIds >= 4)
	{
		pOutParameters->maxChannelIds = targetParameters->maxChannelIds;
	}
	else if (maximumParameters->maxChannelIds >= 4)
	{
		pOutParameters->maxChannelIds = 4;
	}
	else
	{
		return FALSE;
	}

	/* maxUserIds */

	if (targetParameters->maxUserIds >= 3)
	{
		pOutParameters->maxUserIds = targetParameters->maxUserIds;
	}
	else if (maximumParameters->maxUserIds >= 3)
	{
		pOutParameters->maxUserIds = 3;
	}
	else
	{
		return FALSE;
	}

	/* maxTokenIds */
	pOutParameters->maxTokenIds = targetParameters->maxTokenIds;

	/* numPriorities */

	if (minimumParameters->numPriorities <= 1)
	{
		pOutParameters->numPriorities = 1;
	}
	else
	{
		return FALSE;
	}

	/* minThroughput */
	pOutParameters->minThroughput = targetParameters->minThroughput;

	/* maxHeight */

	if ((targetParameters->maxHeight == 1) || (minimumParameters->maxHeight <= 1))
	{
		pOutParameters->maxHeight = 1;
	}
	else
	{
		return FALSE;
	}

	/* maxMCSPDUsize */

	if (targetParameters->maxMCSPDUsize >= 1024)
	{
		if (targetParameters->maxMCSPDUsize <= 65528)
		{
			pOutParameters->maxMCSPDUsize = targetParameters->maxMCSPDUsize;
		}
		else if ((minimumParameters->maxMCSPDUsize >= 124) &&
		         (minimumParameters->maxMCSPDUsize <= 65528))
		{
			pOutParameters->maxMCSPDUsize = 65528;
		}
		else
		{
			return FALSE;
		}
	}
	else
	{
		if (maximumParameters->maxMCSPDUsize >= 124)
		{
			pOutParameters->maxMCSPDUsize = maximumParameters->maxMCSPDUsize;
		}
		else
		{
			return FALSE;
		}
	}

	/* protocolVersion */

	if ((targetParameters->protocolVersion == 2) ||
	    ((minimumParameters->protocolVersion <= 2) && (maximumParameters->protocolVersion >= 2)))
	{
		pOutParameters->protocolVersion = 2;
	}
	else
	{
		return FALSE;
	}

	return TRUE;
}

/**
 * Read an MCS Connect Initial PDU.
 * msdn{cc240508}
 * @param mcs MCS module
 * @param s stream
 */

BOOL mcs_recv_connect_initial(rdpMcs* mcs, wStream* s)
{
	UINT16 li;
	size_t length;
	BOOL upwardFlag;
	UINT16 tlength;

	WINPR_ASSERT(mcs);
	WINPR_ASSERT(s);

	if (!tpkt_read_header(s, &tlength))
		return FALSE;

	if (!tpdu_read_data(s, &li, tlength))
		return FALSE;

	if (!ber_read_application_tag(s, MCS_TYPE_CONNECT_INITIAL, &length))
		return FALSE;

	/* callingDomainSelector (OCTET_STRING) */
	if (!ber_read_octet_string_tag(s, &length) ||
	    (!Stream_CheckAndLogRequiredLength(TAG, s, length)))
		return FALSE;

	Stream_Seek(s, length);

	/* calledDomainSelector (OCTET_STRING) */
	if (!ber_read_octet_string_tag(s, &length) ||
	    (!Stream_CheckAndLogRequiredLength(TAG, s, length)))
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

	if (!ber_read_octet_string_tag(s, &length) ||
	    (!Stream_CheckAndLogRequiredLength(TAG, s, length)))
		return FALSE;

	if (!gcc_read_conference_create_request(s, mcs))
		return FALSE;

	if (!mcs_merge_domain_parameters(&mcs->targetParameters, &mcs->minimumParameters,
	                                 &mcs->maximumParameters, &mcs->domainParameters))
		return FALSE;

	return tpkt_ensure_stream_consumed(s, tlength);
}

/**
 * Write an MCS Connect Initial PDU.
 * msdn{cc240508}
 * @param s stream
 * @param mcs MCS module
 * @param userData GCC Conference Create Request
 */

BOOL mcs_write_connect_initial(wStream* s, rdpMcs* mcs, wStream* userData)
{
	size_t length;
	wStream* tmps;
	BOOL ret = FALSE;

	if (!s || !mcs || !userData)
		return FALSE;

	tmps = Stream_New(NULL, Stream_Capacity(s));

	if (!tmps)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	/* callingDomainSelector (OCTET_STRING) */
	ber_write_octet_string(tmps, callingDomainSelector, sizeof(callingDomainSelector));
	/* calledDomainSelector (OCTET_STRING) */
	ber_write_octet_string(tmps, calledDomainSelector, sizeof(calledDomainSelector));
	/* upwardFlag (BOOLEAN) */
	ber_write_BOOL(tmps, TRUE);

	/* targetParameters (DomainParameters) */
	if (!mcs_write_domain_parameters(tmps, &mcs->targetParameters))
		goto out;

	/* minimumParameters (DomainParameters) */
	if (!mcs_write_domain_parameters(tmps, &mcs->minimumParameters))
		goto out;

	/* maximumParameters (DomainParameters) */
	if (!mcs_write_domain_parameters(tmps, &mcs->maximumParameters))
		goto out;

	/* userData (OCTET_STRING) */
	ber_write_octet_string(tmps, Stream_Buffer(userData), Stream_GetPosition(userData));
	length = Stream_GetPosition(tmps);
	/* Connect-Initial (APPLICATION 101, IMPLICIT SEQUENCE) */
	ber_write_application_tag(s, MCS_TYPE_CONNECT_INITIAL, length);
	Stream_Write(s, Stream_Buffer(tmps), length);
	ret = TRUE;
out:
	Stream_Free(tmps, TRUE);
	return ret;
}

/**
 * Write an MCS Connect Response PDU.
 * msdn{cc240508}
 * @param s stream
 * @param mcs MCS module
 * @param userData GCC Conference Create Response
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL mcs_write_connect_response(wStream* s, rdpMcs* mcs, wStream* userData)
{
	size_t length;
	wStream* tmps;
	BOOL ret = FALSE;

	if (!s || !mcs || !userData)
		return FALSE;

	tmps = Stream_New(NULL, Stream_Capacity(s));

	if (!tmps)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	ber_write_enumerated(tmps, 0, MCS_Result_enum_length);
	ber_write_integer(tmps, 0); /* calledConnectId */

	if (!mcs_write_domain_parameters(tmps, &(mcs->domainParameters)))
		goto out;

	/* userData (OCTET_STRING) */
	ber_write_octet_string(tmps, Stream_Buffer(userData), Stream_GetPosition(userData));
	length = Stream_GetPosition(tmps);
	ber_write_application_tag(s, MCS_TYPE_CONNECT_RESPONSE, length);
	Stream_Write(s, Stream_Buffer(tmps), length);
	ret = TRUE;
out:
	Stream_Free(tmps, TRUE);
	return ret;
}

/**
 * Send MCS Connect Initial.
 * msdn{cc240508}
 * @param mcs mcs module
 */

static BOOL mcs_send_connect_initial(rdpMcs* mcs)
{
	int status = -1;
	size_t length;
	wStream* s = NULL;
	size_t bm, em;
	wStream* gcc_CCrq = NULL;
	wStream* client_data = NULL;
	rdpContext* context;

	if (!mcs)
		return FALSE;

	context = transport_get_context(mcs->transport);
	WINPR_ASSERT(context);

	mcs_initialize_client_channels(mcs, context->settings);
	client_data = Stream_New(NULL, 512);

	if (!client_data)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	if (!gcc_write_client_data_blocks(client_data, mcs))
		goto out;
	gcc_CCrq = Stream_New(NULL, 1024);

	if (!gcc_CCrq)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto out;
	}

	if (!gcc_write_conference_create_request(gcc_CCrq, client_data))
		goto out;
	length = Stream_GetPosition(gcc_CCrq) + 7;
	s = Stream_New(NULL, 1024 + length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto out;
	}

	bm = Stream_GetPosition(s);
	Stream_Seek(s, 7);

	if (!mcs_write_connect_initial(s, mcs, gcc_CCrq))
	{
		WLog_ERR(TAG, "mcs_write_connect_initial failed!");
		goto out;
	}

	em = Stream_GetPosition(s);
	length = (em - bm);
	if (length > UINT16_MAX)
		goto out;
	Stream_SetPosition(s, bm);
	if (!tpkt_write_header(s, (UINT16)length))
		goto out;
	if (!tpdu_write_data(s))
		goto out;
	Stream_SetPosition(s, em);
	Stream_SealLength(s);
	status = transport_write(mcs->transport, s);
out:
	Stream_Free(s, TRUE);
	Stream_Free(gcc_CCrq, TRUE);
	Stream_Free(client_data, TRUE);
	return (status < 0 ? FALSE : TRUE);
}

/**
 * Read MCS Connect Response.
 * msdn{cc240501}
 * @param mcs mcs module
 */

BOOL mcs_recv_connect_response(rdpMcs* mcs, wStream* s)
{
	size_t length;
	UINT16 tlength;
	BYTE result;
	UINT16 li;
	UINT32 calledConnectId;

	if (!mcs || !s)
		return FALSE;

	if (!tpkt_read_header(s, &tlength))
		return FALSE;

	if (!tpdu_read_data(s, &li, tlength))
		return FALSE;

	if (!ber_read_application_tag(s, MCS_TYPE_CONNECT_RESPONSE, &length) ||
	    !ber_read_enumerated(s, &result, MCS_Result_enum_length) ||
	    !ber_read_integer(s, &calledConnectId) ||
	    !mcs_read_domain_parameters(s, &(mcs->domainParameters)) ||
	    !ber_read_octet_string_tag(s, &length))
	{
		return FALSE;
	}

	if (!gcc_read_conference_create_response(s, mcs))
	{
		WLog_ERR(TAG, "gcc_read_conference_create_response failed");
		return FALSE;
	}

	return tpkt_ensure_stream_consumed(s, tlength);
}

/**
 * Send MCS Connect Response.
 * msdn{cc240501}
 * @param mcs mcs module
 */

BOOL mcs_send_connect_response(rdpMcs* mcs)
{
	size_t length;
	int status = -1;
	wStream* s = NULL;
	size_t bm, em;
	wStream* gcc_CCrsp = NULL;
	wStream* server_data = NULL;

	if (!mcs)
		return FALSE;

	server_data = Stream_New(NULL, 512);

	if (!server_data)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	if (!gcc_write_server_data_blocks(server_data, mcs))
		goto out;

	gcc_CCrsp = Stream_New(NULL, 512 + Stream_Capacity(server_data));

	if (!gcc_CCrsp)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto out;
	}

	if (!gcc_write_conference_create_response(gcc_CCrsp, server_data))
		goto out;
	length = Stream_GetPosition(gcc_CCrsp) + 7;
	s = Stream_New(NULL, length + 1024);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		goto out;
	}

	bm = Stream_GetPosition(s);
	Stream_Seek(s, 7);

	if (!mcs_write_connect_response(s, mcs, gcc_CCrsp))
		goto out;

	em = Stream_GetPosition(s);
	length = (em - bm);
	if (length > UINT16_MAX)
		goto out;
	Stream_SetPosition(s, bm);
	if (!tpkt_write_header(s, (UINT16)length))
		goto out;
	if (!tpdu_write_data(s))
		goto out;
	Stream_SetPosition(s, em);
	Stream_SealLength(s);
	status = transport_write(mcs->transport, s);
out:
	Stream_Free(s, TRUE);
	Stream_Free(gcc_CCrsp, TRUE);
	Stream_Free(server_data, TRUE);
	return (status < 0) ? FALSE : TRUE;
}

/**
 * Read MCS Erect Domain Request.
 * msdn{cc240523}
 * @param mcs MCS module to use
 * @param s stream
 */

BOOL mcs_recv_erect_domain_request(rdpMcs* mcs, wStream* s)
{
	UINT16 length;
	UINT32 subHeight;
	UINT32 subInterval;

	WINPR_ASSERT(mcs);
	WINPR_ASSERT(s);

	if (!mcs_read_domain_mcspdu_header(s, DomainMCSPDU_ErectDomainRequest, &length, NULL))
		return FALSE;

	if (!per_read_integer(s, &subHeight)) /* subHeight (INTEGER) */
		return FALSE;

	if (!per_read_integer(s, &subInterval)) /* subInterval (INTEGER) */
		return FALSE;

	return tpkt_ensure_stream_consumed(s, length);
}

/**
 * Send MCS Erect Domain Request.
 * msdn{cc240523}
 * @param mcs MCS module to use
 */

BOOL mcs_send_erect_domain_request(rdpMcs* mcs)
{
	wStream* s;
	int status;
	UINT16 length = 12;

	if (!mcs)
		return FALSE;

	s = Stream_New(NULL, length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_ErectDomainRequest, length, 0);
	per_write_integer(s, 0); /* subHeight (INTEGER) */
	per_write_integer(s, 0); /* subInterval (INTEGER) */
	Stream_SealLength(s);
	status = transport_write(mcs->transport, s);
	Stream_Free(s, TRUE);
	return (status < 0) ? FALSE : TRUE;
}

/**
 * Read MCS Attach User Request.
 * msdn{cc240524}
 * @param mcs mcs module
 * @param s stream
 */

BOOL mcs_recv_attach_user_request(rdpMcs* mcs, wStream* s)
{
	UINT16 length;

	if (!mcs || !s)
		return FALSE;

	if (!mcs_read_domain_mcspdu_header(s, DomainMCSPDU_AttachUserRequest, &length, NULL))
		return FALSE;
	return tpkt_ensure_stream_consumed(s, length);
}

/**
 * Send MCS Attach User Request.
 * msdn{cc240524}
 * @param mcs mcs module
 */

BOOL mcs_send_attach_user_request(rdpMcs* mcs)
{
	wStream* s;
	int status;
	UINT16 length = 8;

	if (!mcs)
		return FALSE;

	s = Stream_New(NULL, length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_AttachUserRequest, length, 0);
	Stream_SealLength(s);
	status = transport_write(mcs->transport, s);
	Stream_Free(s, TRUE);
	return (status < 0) ? FALSE : TRUE;
}

/**
 * Read MCS Attach User Confirm.
 * msdn{cc240525}
 * @param mcs mcs module
 */

BOOL mcs_recv_attach_user_confirm(rdpMcs* mcs, wStream* s)
{
	BYTE result;
	UINT16 length;

	if (!mcs || !s)
		return FALSE;

	if (!mcs_read_domain_mcspdu_header(s, DomainMCSPDU_AttachUserConfirm, &length, NULL))
		return FALSE;
	if (!per_read_enumerated(s, &result, MCS_Result_enum_length)) /* result */
		return FALSE;
	if (!per_read_integer16(s, &(mcs->userId), MCS_BASE_CHANNEL_ID)) /* initiator (UserId) */
		return FALSE;
	return tpkt_ensure_stream_consumed(s, length);
}

/**
 * Send MCS Attach User Confirm.
 * msdn{cc240525}
 * @param mcs mcs module
 */

BOOL mcs_send_attach_user_confirm(rdpMcs* mcs)
{
	wStream* s;
	int status;
	UINT16 length = 11;

	if (!mcs)
		return FALSE;

	s = Stream_New(NULL, length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	mcs->userId = mcs->baseChannelId++;
	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_AttachUserConfirm, length, 2);
	per_write_enumerated(s, 0, MCS_Result_enum_length);       /* result */
	per_write_integer16(s, mcs->userId, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
	Stream_SealLength(s);
	status = transport_write(mcs->transport, s);
	Stream_Free(s, TRUE);
	return (status < 0) ? FALSE : TRUE;
}

/**
 * Read MCS Channel Join Request.
 * msdn{cc240526}
 * @param mcs mcs module
 * @param s stream
 */

BOOL mcs_recv_channel_join_request(rdpMcs* mcs, const rdpSettings* settings, wStream* s,
                                   UINT16* channelId)
{
	UINT16 length;
	UINT16 userId;

	if (!mcs || !s || !channelId)
		return FALSE;

	if (!mcs_read_domain_mcspdu_header(s, DomainMCSPDU_ChannelJoinRequest, &length, NULL))
		return FALSE;

	if (!per_read_integer16(s, &userId, MCS_BASE_CHANNEL_ID))
		return FALSE;
	if (userId != mcs->userId)
	{
		if (freerdp_settings_get_bool(settings, FreeRDP_TransportDumpReplay))
			mcs->userId = userId;
		else
			return FALSE;
	}
	if (!per_read_integer16(s, channelId, 0))
		return FALSE;

	return tpkt_ensure_stream_consumed(s, length);
}

/**
 * Send MCS Channel Join Request.
 * msdn{cc240526}
 *
 * @param mcs mcs module
 * @param channelId channel id
 *
 * @return \b TRUE for success, \b FALSE otherwise
 */

BOOL mcs_send_channel_join_request(rdpMcs* mcs, UINT16 channelId)
{
	wStream* s;
	int status;
	UINT16 length = 12;

	if (!mcs)
		return FALSE;

	s = Stream_New(NULL, length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_ChannelJoinRequest, length, 0);
	per_write_integer16(s, mcs->userId, MCS_BASE_CHANNEL_ID);
	per_write_integer16(s, channelId, 0);
	Stream_SealLength(s);
	status = transport_write(mcs->transport, s);
	Stream_Free(s, TRUE);
	return (status < 0) ? FALSE : TRUE;
}

/**
 * Read MCS Channel Join Confirm.
 * msdn{cc240527}
 * @param mcs mcs module
 */

BOOL mcs_recv_channel_join_confirm(rdpMcs* mcs, wStream* s, UINT16* channelId)
{
	UINT16 length;
	BYTE result;
	UINT16 initiator;
	UINT16 requested;

	if (!mcs || !s || !channelId)
		return FALSE;

	if (!mcs_read_domain_mcspdu_header(s, DomainMCSPDU_ChannelJoinConfirm, &length, NULL))
		return FALSE;

	if (!per_read_enumerated(s, &result, MCS_Result_enum_length)) /* result */
		return FALSE;
	if (!per_read_integer16(s, &initiator, MCS_BASE_CHANNEL_ID)) /* initiator (UserId) */
		return FALSE;
	if (!per_read_integer16(s, &requested, 0)) /* requested (ChannelId) */
		return FALSE;
	if (!per_read_integer16(s, channelId, 0)) /* channelId */
		return FALSE;
	return tpkt_ensure_stream_consumed(s, length);
}

/**
 * Send MCS Channel Join Confirm.
 * msdn{cc240527}
 * @param mcs mcs module
 */

BOOL mcs_send_channel_join_confirm(rdpMcs* mcs, UINT16 channelId)
{
	wStream* s;
	int status = -1;
	UINT16 length = 15;

	if (!mcs)
		return FALSE;

	s = Stream_New(NULL, length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	if (!mcs_write_domain_mcspdu_header(s, DomainMCSPDU_ChannelJoinConfirm, length, 2))
		goto fail;
	if (!per_write_enumerated(s, 0, MCS_Result_enum_length)) /* result */
		goto fail;
	if (!per_write_integer16(s, mcs->userId, MCS_BASE_CHANNEL_ID)) /* initiator (UserId) */
		goto fail;
	if (!per_write_integer16(s, channelId, 0)) /* requested (ChannelId) */
		goto fail;
	if (!per_write_integer16(s, channelId, 0)) /* channelId */
		goto fail;
	Stream_SealLength(s);
	status = transport_write(mcs->transport, s);
fail:
	Stream_Free(s, TRUE);
	return (status < 0) ? FALSE : TRUE;
}

/**
 * Receive MCS Disconnect Provider Ultimatum PDU.
 * @param mcs mcs module
 */

BOOL mcs_recv_disconnect_provider_ultimatum(rdpMcs* mcs, wStream* s, int* reason)
{
	BYTE b1, b2;

	WINPR_ASSERT(mcs);
	WINPR_ASSERT(s);
	WINPR_ASSERT(reason);

	/*
	 * http://msdn.microsoft.com/en-us/library/cc240872.aspx:
	 *
	 * PER encoded (ALIGNED variant of BASIC-PER) PDU contents:
	 * 21 80
	 *
	 * 0x21:
	 * 0 - --\
	 * 0 -   |
	 * 1 -   | CHOICE: From DomainMCSPDU select disconnectProviderUltimatum (8)
	 * 0 -   | of type DisconnectProviderUltimatum
	 * 0 -   |
	 * 0 - --/
	 * 0 - --\
	 * 1 -   |
	 *       | DisconnectProviderUltimatum::reason = rn-user-requested (3)
	 * 0x80: |
	 * 1 - --/
	 * 0 - padding
	 * 0 - padding
	 * 0 - padding
	 * 0 - padding
	 * 0 - padding
	 * 0 - padding
	 * 0 - padding
	 */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 1))
		return FALSE;

	Stream_Rewind_UINT8(s);
	Stream_Read_UINT8(s, b1);
	Stream_Read_UINT8(s, b2);
	*reason = ((b1 & 0x01) << 1) | (b2 >> 7);
	return TRUE;
}

/**
 * Send MCS Disconnect Provider Ultimatum PDU.
 * @param mcs mcs module
 */

BOOL mcs_send_disconnect_provider_ultimatum(rdpMcs* mcs)
{
	wStream* s;
	int status;
	UINT16 length = 9;

	WINPR_ASSERT(mcs);

	s = Stream_New(NULL, length);

	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return FALSE;
	}

	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_DisconnectProviderUltimatum, length, 1);
	per_write_enumerated(s, 0x80, 0);
	status = transport_write(mcs->transport, s);
	Stream_Free(s, TRUE);
	return (status < 0) ? FALSE : TRUE;
}

BOOL mcs_client_begin(rdpMcs* mcs)
{
	rdpContext* context;

	if (!mcs || !mcs->transport)
		return FALSE;

	context = transport_get_context(mcs->transport);

	if (!context)
		return FALSE;

	/* First transition state, we need this to trigger session recording */
	if (!mcs_send_connect_initial(mcs))
	{
		freerdp_set_last_error_if_not(context, FREERDP_ERROR_MCS_CONNECT_INITIAL_ERROR);

		WLog_ERR(TAG, "Error: unable to send MCS Connect Initial");
		return FALSE;
	}

	return TRUE;
}

/**
 * Instantiate new MCS module.
 * @param transport transport
 * @return new MCS module
 */

rdpMcs* mcs_new(rdpTransport* transport)
{
	rdpMcs* mcs;

	mcs = (rdpMcs*)calloc(1, sizeof(rdpMcs));

	if (!mcs)
		return NULL;

	mcs->transport = transport;
	mcs_init_domain_parameters(&mcs->targetParameters, 34, 2, 0, 0xFFFF);
	mcs_init_domain_parameters(&mcs->minimumParameters, 1, 1, 1, 0x420);
	mcs_init_domain_parameters(&mcs->maximumParameters, 0xFFFF, 0xFC17, 0xFFFF, 0xFFFF);
	mcs_init_domain_parameters(&mcs->domainParameters, 0, 0, 0, 0xFFFF);
	mcs->channelCount = 0;
	mcs->channelMaxCount = CHANNEL_MAX_COUNT;
	mcs->baseChannelId = MCS_GLOBAL_CHANNEL_ID + 1;
	mcs->channels = (rdpMcsChannel*)calloc(mcs->channelMaxCount, sizeof(rdpMcsChannel));

	if (!mcs->channels)
		goto out_free;

	return mcs;
out_free:
	free(mcs);
	return NULL;
}

/**
 * Free MCS module.
 * @param mcs MCS module to be freed
 */

void mcs_free(rdpMcs* mcs)
{
	if (mcs)
	{
		free(mcs->channels);
		free(mcs);
	}
}

BOOL mcs_server_apply_to_settings(const rdpMcs* mcs, rdpSettings* settings)
{
	UINT32 x;

	WINPR_ASSERT(mcs);
	WINPR_ASSERT(settings);

	if (!freerdp_settings_set_uint32(settings, FreeRDP_ChannelCount, mcs->channelCount))
		return FALSE;
	if (!freerdp_settings_set_pointer_len(settings, FreeRDP_ChannelDefArray, NULL,
	                                      mcs->channelCount))
		return FALSE;

	for (x = 0; x < mcs->channelCount; x++)
	{
		const rdpMcsChannel* current = &mcs->channels[x];
		CHANNEL_DEF def = { 0 };
		def.options = current->options;
		memcpy(def.name, current->Name, sizeof(def.name));
		if (!freerdp_settings_set_pointer_array(settings, FreeRDP_ChannelDefArray, x, &def))
			return FALSE;
	}

	return TRUE;
}
