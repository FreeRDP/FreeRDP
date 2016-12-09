/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Auto-Detect PDUs
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
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

#include <winpr/crypto.h>

#include "autodetect.h"

#define RDP_RTT_REQUEST_TYPE_CONTINUOUS  0x0001
#define RDP_RTT_REQUEST_TYPE_CONNECTTIME 0x1001

#define RDP_RTT_RESPONSE_TYPE 0x0000

#define RDP_BW_START_REQUEST_TYPE_CONTINUOUS  0x0014
#define RDP_BW_START_REQUEST_TYPE_TUNNEL      0x0114
#define RDP_BW_START_REQUEST_TYPE_CONNECTTIME 0x1014
#define RDP_BW_PAYLOAD_REQUEST_TYPE           0x0002
#define RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME  0x002B
#define RDP_BW_STOP_REQUEST_TYPE_CONTINUOUS   0x0429
#define RDP_BW_STOP_REQUEST_TYPE_TUNNEL       0x0629

#define RDP_BW_RESULTS_RESPONSE_TYPE_CONNECTTIME 0x0003
#define RDP_BW_RESULTS_RESPONSE_TYPE_CONTINUOUS  0x000B

#define RDP_NETCHAR_SYNC_RESPONSE_TYPE 0x0018

typedef struct
{
	UINT8 headerLength;
	UINT8 headerTypeId;
	UINT16 sequenceNumber;
	UINT16 requestType;
} AUTODETECT_REQ_PDU;

typedef struct
{
	UINT8 headerLength;
	UINT8 headerTypeId;
	UINT16 sequenceNumber;
	UINT16 responseType;
} AUTODETECT_RSP_PDU;

static BOOL autodetect_send_rtt_measure_request(rdpContext* context, UINT16 sequenceNumber, UINT16 requestType)
{
	wStream* s;

	s = rdp_message_channel_pdu_init(context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending RTT Measure Request PDU");

	Stream_Write_UINT8(s, 0x06); /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, requestType); /* requestType (2 bytes) */

	context->rdp->autodetect->rttMeasureStartTime = GetTickCountPrecise();

	return rdp_send_message_channel_pdu(context->rdp, s, SEC_AUTODETECT_REQ);
}

static BOOL autodetect_send_continuous_rtt_measure_request(rdpContext* context, UINT16 sequenceNumber)
{
	return autodetect_send_rtt_measure_request(context, sequenceNumber, RDP_RTT_REQUEST_TYPE_CONTINUOUS);
}

BOOL autodetect_send_connecttime_rtt_measure_request(rdpContext* context, UINT16 sequenceNumber)
{
	return autodetect_send_rtt_measure_request(context, sequenceNumber, RDP_RTT_REQUEST_TYPE_CONNECTTIME);
}

static BOOL autodetect_send_rtt_measure_response(rdpRdp* rdp, UINT16 sequenceNumber)
{
	wStream* s;

	/* Send the response PDU to the server */

	s = rdp_message_channel_pdu_init(rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending RTT Measure Response PDU");

	Stream_Write_UINT8(s, 0x06); /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_RESPONSE); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, RDP_RTT_RESPONSE_TYPE); /* responseType (1 byte) */

	return rdp_send_message_channel_pdu(rdp, s, SEC_AUTODETECT_RSP);
}

static BOOL autodetect_send_bandwidth_measure_start(rdpContext* context, UINT16 sequenceNumber, UINT16 requestType)
{
	wStream* s;

	s = rdp_message_channel_pdu_init(context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending Bandwidth Measure Start PDU");

	Stream_Write_UINT8(s, 0x06); /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, requestType); /* requestType (2 bytes) */

	return rdp_send_message_channel_pdu(context->rdp, s, SEC_AUTODETECT_REQ);
}

static BOOL autodetect_send_continuous_bandwidth_measure_start(rdpContext* context, UINT16 sequenceNumber)
{
	return autodetect_send_bandwidth_measure_start(context, sequenceNumber, RDP_BW_START_REQUEST_TYPE_CONTINUOUS);
}

BOOL autodetect_send_connecttime_bandwidth_measure_start(rdpContext* context, UINT16 sequenceNumber)
{
	return autodetect_send_bandwidth_measure_start(context, sequenceNumber, RDP_BW_START_REQUEST_TYPE_CONNECTTIME);
}

BOOL autodetect_send_bandwidth_measure_payload(rdpContext* context, UINT16 payloadLength, UINT16 sequenceNumber)
{
	wStream* s;
	UCHAR *buffer = NULL;
	BOOL bResult = FALSE;

	s = rdp_message_channel_pdu_init(context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending Bandwidth Measure Payload PDU -> payloadLength=%u", payloadLength);

	/* 4-bytes aligned */
	payloadLength &= ~3;

	if (!Stream_EnsureRemainingCapacity(s, 8 + payloadLength))
	{
		Stream_Release(s);
		return FALSE;
	}
	Stream_Write_UINT8(s, 0x08); /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, RDP_BW_PAYLOAD_REQUEST_TYPE); /* requestType (2 bytes) */
	Stream_Write_UINT16(s, payloadLength); /* payloadLength (2 bytes) */

	/* Random data (better measurement in case the line is compressed) */
	buffer = (UCHAR *)malloc(payloadLength);
	if (NULL == buffer)
	{
		Stream_Release(s);
		return FALSE;
	}

	winpr_RAND(buffer, payloadLength);
	Stream_Write(s, buffer, payloadLength);

	bResult = rdp_send_message_channel_pdu(context->rdp, s, SEC_AUTODETECT_REQ);
	if (!bResult)
	{
		Stream_Release(s);
	}
	free(buffer);

	return bResult;
}

static BOOL autodetect_send_bandwidth_measure_stop(rdpContext* context, UINT16 payloadLength, UINT16 sequenceNumber, UINT16 requestType)
{
	wStream* s;
	UCHAR *buffer = NULL;
	BOOL bResult = FALSE;

	s = rdp_message_channel_pdu_init(context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending Bandwidth Measure Stop PDU -> payloadLength=%u", payloadLength);

	/* 4-bytes aligned */
	payloadLength &= ~3;

	Stream_Write_UINT8(s, requestType == RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME ? 0x08 : 0x06); /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, requestType); /* requestType (2 bytes) */
	if (requestType == RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME)
	{
		Stream_Write_UINT16(s, payloadLength); /* payloadLength (2 bytes) */
		if (payloadLength > 0)
		{
			if (!Stream_EnsureRemainingCapacity(s, payloadLength))
			{
				Stream_Release(s);
				return FALSE;
			}

			/* Random data (better measurement in case the line is compressed) */
			buffer = malloc(payloadLength);
			if (NULL == buffer)
			{
				Stream_Release(s);
				return FALSE;
			}

			winpr_RAND(buffer, payloadLength);
			Stream_Write(s, buffer, payloadLength);
		}
	}

	bResult = rdp_send_message_channel_pdu(context->rdp, s, SEC_AUTODETECT_REQ);
	if (!bResult)
	{
		Stream_Release(s);
	}
	free(buffer);

	return bResult;
}

static BOOL autodetect_send_continuous_bandwidth_measure_stop(rdpContext* context, UINT16 sequenceNumber)
{
	return autodetect_send_bandwidth_measure_stop(context, 0, sequenceNumber, RDP_BW_STOP_REQUEST_TYPE_CONTINUOUS);
}

BOOL autodetect_send_connecttime_bandwidth_measure_stop(rdpContext* context, UINT16 payloadLength, UINT16 sequenceNumber)
{
	return autodetect_send_bandwidth_measure_stop(context, payloadLength, sequenceNumber, RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME);
}

static BOOL autodetect_send_bandwidth_measure_results(rdpRdp* rdp, UINT16 responseType, UINT16 sequenceNumber)
{
	BOOL success = TRUE;
	wStream* s;
	UINT32 timeDelta;

	/* Compute the total time */
	timeDelta = GetTickCountPrecise() - rdp->autodetect->bandwidthMeasureStartTime;

	/* Send the result PDU to the server */

	s = rdp_message_channel_pdu_init(rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending Bandwidth Measure Results PDU -> timeDelta=%u, byteCount=%u", timeDelta, rdp->autodetect->bandwidthMeasureByteCount);

	Stream_Write_UINT8(s, 0x0E); /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_RESPONSE); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, responseType); /* responseType (1 byte) */
	Stream_Write_UINT32(s, timeDelta); /* timeDelta (4 bytes) */
	Stream_Write_UINT32(s, rdp->autodetect->bandwidthMeasureByteCount); /* byteCount (4 bytes) */

	IFCALLRET(rdp->autodetect->ClientBandwidthMeasureResult, success,
						rdp->context, rdp->autodetect);

	if (!success)
		return FALSE;

	return rdp_send_message_channel_pdu(rdp, s, SEC_AUTODETECT_RSP);
}

static BOOL autodetect_send_netchar_result(rdpContext* context, UINT16 sequenceNumber)
{
	wStream* s;

	s = rdp_message_channel_pdu_init(context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending Bandwidth Network Characteristics Result PDU");

	if (context->rdp->autodetect->netCharBandwidth > 0)
	{
		Stream_Write_UINT8(s, 0x12); /* headerLength (1 byte) */
		Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST); /* headerTypeId (1 byte) */
		Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
		Stream_Write_UINT16(s, 0x08C0); /* requestType (2 bytes) */
		Stream_Write_UINT32(s, context->rdp->autodetect->netCharBaseRTT); /* baseRTT (4 bytes) */
		Stream_Write_UINT32(s, context->rdp->autodetect->netCharBandwidth); /* bandwidth (4 bytes) */
		Stream_Write_UINT32(s, context->rdp->autodetect->netCharAverageRTT); /* averageRTT (4 bytes) */
	}
	else
	{
		Stream_Write_UINT8(s, 0x0E); /* headerLength (1 byte) */
		Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST); /* headerTypeId (1 byte) */
		Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
		Stream_Write_UINT16(s, 0x0840); /* requestType (2 bytes) */
		Stream_Write_UINT32(s, context->rdp->autodetect->netCharBaseRTT); /* baseRTT (4 bytes) */
		Stream_Write_UINT32(s, context->rdp->autodetect->netCharAverageRTT); /* averageRTT (4 bytes) */
	}

	return rdp_send_message_channel_pdu(context->rdp, s, SEC_AUTODETECT_REQ);
}

BOOL autodetect_send_netchar_sync(rdpRdp* rdp, UINT16 sequenceNumber)
{
	wStream* s;

	/* Send the response PDU to the server */

	s = rdp_message_channel_pdu_init(rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending Network Characteristics Sync PDU -> bandwidth=%u, rtt=%u", rdp->autodetect->netCharBandwidth, rdp->autodetect->netCharAverageRTT);

	Stream_Write_UINT8(s, 0x0E); /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_RESPONSE); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, RDP_NETCHAR_SYNC_RESPONSE_TYPE); /* responseType (1 byte) */
	Stream_Write_UINT32(s, rdp->autodetect->netCharBandwidth); /* bandwidth (4 bytes) */
	Stream_Write_UINT32(s, rdp->autodetect->netCharAverageRTT); /* rtt (4 bytes) */

	return rdp_send_message_channel_pdu(rdp, s, SEC_AUTODETECT_RSP);
}

static BOOL autodetect_recv_rtt_measure_request(rdpRdp* rdp, wStream* s, AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	if (autodetectReqPdu->headerLength != 0x06)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "received RTT Measure Request PDU");

	/* Send a response to the server */
	return autodetect_send_rtt_measure_response(rdp, autodetectReqPdu->sequenceNumber);
}

static BOOL autodetect_recv_rtt_measure_response(rdpRdp* rdp, wStream* s, AUTODETECT_RSP_PDU* autodetectRspPdu)
{
	BOOL success = TRUE;

	if (autodetectRspPdu->headerLength != 0x06)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "received RTT Measure Response PDU");

	rdp->autodetect->netCharAverageRTT = GetTickCountPrecise() - rdp->autodetect->rttMeasureStartTime;
	if (rdp->autodetect->netCharBaseRTT == 0 || rdp->autodetect->netCharBaseRTT > rdp->autodetect->netCharAverageRTT)
		rdp->autodetect->netCharBaseRTT = rdp->autodetect->netCharAverageRTT;

	IFCALLRET(rdp->autodetect->RTTMeasureResponse, success, rdp->context, autodetectRspPdu->sequenceNumber);

	return success;
}

static BOOL autodetect_recv_bandwidth_measure_start(rdpRdp* rdp, wStream* s, AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	if (autodetectReqPdu->headerLength != 0x06)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "received Bandwidth Measure Start PDU - time=%lu", GetTickCountPrecise());

	/* Initialize bandwidth measurement parameters */
	rdp->autodetect->bandwidthMeasureStartTime = GetTickCountPrecise();
	rdp->autodetect->bandwidthMeasureByteCount = 0;

	/* Continuous Auto-Detection: mark the start of the measurement */
	if (autodetectReqPdu->requestType == RDP_BW_START_REQUEST_TYPE_CONTINUOUS)
	{
		rdp->autodetect->bandwidthMeasureStarted = TRUE;
	}

	return TRUE;
}

static BOOL autodetect_recv_bandwidth_measure_payload(rdpRdp* rdp, wStream* s, AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	UINT16 payloadLength;

	if (autodetectReqPdu->headerLength != 0x08)
		return FALSE;

	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, payloadLength); /* payloadLength (2 bytes) */

	WLog_DBG(AUTODETECT_TAG, "received Bandwidth Measure Payload PDU -> payloadLength=%u", payloadLength);

	/* Add the payload length to the bandwidth measurement parameters */
	rdp->autodetect->bandwidthMeasureByteCount += payloadLength;

	return TRUE;
}

static BOOL autodetect_recv_bandwidth_measure_stop(rdpRdp* rdp, wStream* s, AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	UINT16 payloadLength;
	UINT16 responseType;

	if (autodetectReqPdu->requestType == RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME)
	{
		if (autodetectReqPdu->headerLength != 0x08)
			return FALSE;

		if (Stream_GetRemainingLength(s) < 2)
			return FALSE;

		Stream_Read_UINT16(s, payloadLength); /* payloadLength (2 bytes) */
	}
	else
	{
		if (autodetectReqPdu->headerLength != 0x06)
			return FALSE;

		payloadLength = 0;
	}

	WLog_VRB(AUTODETECT_TAG, "received Bandwidth Measure Stop PDU -> payloadLength=%u", payloadLength);

	/* Add the payload length to the bandwidth measurement parameters */
	rdp->autodetect->bandwidthMeasureByteCount += payloadLength;

	/* Continuous Auto-Detection: mark the stop of the measurement */
	if (autodetectReqPdu->requestType == RDP_BW_STOP_REQUEST_TYPE_CONTINUOUS)
	{
		rdp->autodetect->bandwidthMeasureStarted = FALSE;
	}

	/* Send a response the server */
	responseType = autodetectReqPdu->requestType == RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME ?
		RDP_BW_RESULTS_RESPONSE_TYPE_CONNECTTIME : RDP_BW_RESULTS_RESPONSE_TYPE_CONTINUOUS;

	return autodetect_send_bandwidth_measure_results(rdp, responseType, autodetectReqPdu->sequenceNumber);
}

static BOOL autodetect_recv_bandwidth_measure_results(rdpRdp* rdp, wStream* s, AUTODETECT_RSP_PDU* autodetectRspPdu)
{
	BOOL success = TRUE;

	if (autodetectRspPdu->headerLength != 0x0E)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "received Bandwidth Measure Results PDU");

	Stream_Read_UINT32(s, rdp->autodetect->bandwidthMeasureTimeDelta); /* timeDelta (4 bytes) */
	Stream_Read_UINT32(s, rdp->autodetect->bandwidthMeasureByteCount); /* byteCount (4 bytes) */

	if (rdp->autodetect->bandwidthMeasureTimeDelta > 0)
		rdp->autodetect->netCharBandwidth = rdp->autodetect->bandwidthMeasureByteCount * 8 / rdp->autodetect->bandwidthMeasureTimeDelta;
	else
		rdp->autodetect->netCharBandwidth = 0;

	IFCALLRET(rdp->autodetect->BandwidthMeasureResults, success, rdp->context, autodetectRspPdu->sequenceNumber);

	return success;
}

static BOOL autodetect_recv_netchar_result(rdpRdp* rdp, wStream* s, AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	BOOL success = TRUE;

	switch (autodetectReqPdu->requestType)
	{
	case 0x0840:
		/* baseRTT and averageRTT fields are present (bandwidth field is not) */
		if ((autodetectReqPdu->headerLength != 0x0E) || (Stream_GetRemainingLength(s) < 8))
			return FALSE;
		Stream_Read_UINT32(s, rdp->autodetect->netCharBaseRTT); /* baseRTT (4 bytes) */
		Stream_Read_UINT32(s, rdp->autodetect->netCharAverageRTT); /* averageRTT (4 bytes) */
		break;

	case 0x0880:
		/* bandwidth and averageRTT fields are present (baseRTT field is not) */
		if ((autodetectReqPdu->headerLength != 0x0E) || (Stream_GetRemainingLength(s) < 8))
			return FALSE;
		Stream_Read_UINT32(s, rdp->autodetect->netCharBandwidth); /* bandwidth (4 bytes) */
		Stream_Read_UINT32(s, rdp->autodetect->netCharAverageRTT); /* averageRTT (4 bytes) */
		break;

	case 0x08C0:
		/* baseRTT, bandwidth, and averageRTT fields are present */
		if ((autodetectReqPdu->headerLength != 0x12) || (Stream_GetRemainingLength(s) < 12))
			return FALSE;
		Stream_Read_UINT32(s, rdp->autodetect->netCharBaseRTT); /* baseRTT (4 bytes) */
		Stream_Read_UINT32(s, rdp->autodetect->netCharBandwidth); /* bandwidth (4 bytes) */
		Stream_Read_UINT32(s, rdp->autodetect->netCharAverageRTT); /* averageRTT (4 bytes) */
		break;
	}

	WLog_VRB(AUTODETECT_TAG, "received Network Characteristics Result PDU -> baseRTT=%u, bandwidth=%u, averageRTT=%u", rdp->autodetect->netCharBaseRTT, rdp->autodetect->netCharBandwidth, rdp->autodetect->netCharAverageRTT);

	IFCALLRET(rdp->autodetect->NetworkCharacteristicsResult, success, rdp->context, autodetectReqPdu->sequenceNumber);
 
	return success;
}

int rdp_recv_autodetect_request_packet(rdpRdp* rdp, wStream* s)
{
	AUTODETECT_REQ_PDU autodetectReqPdu;
	BOOL success = FALSE;
	
	if (Stream_GetRemainingLength(s) < 6)
		return -1;

	Stream_Read_UINT8(s, autodetectReqPdu.headerLength); /* headerLength (1 byte) */
	Stream_Read_UINT8(s, autodetectReqPdu.headerTypeId); /* headerTypeId (1 byte) */
	Stream_Read_UINT16(s, autodetectReqPdu.sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Read_UINT16(s, autodetectReqPdu.requestType); /* requestType (2 bytes) */

	WLog_VRB(AUTODETECT_TAG,
		"rdp_recv_autodetect_request_packet: headerLength=%u, headerTypeId=%u, sequenceNumber=%u, requestType=%04x",
		autodetectReqPdu.headerLength, autodetectReqPdu.headerTypeId,
		autodetectReqPdu.sequenceNumber, autodetectReqPdu.requestType);

	if (autodetectReqPdu.headerTypeId != TYPE_ID_AUTODETECT_REQUEST)
		return -1;

	switch (autodetectReqPdu.requestType)
	{
	case RDP_RTT_REQUEST_TYPE_CONTINUOUS:
	case RDP_RTT_REQUEST_TYPE_CONNECTTIME:
		/* RTT Measure Request (RDP_RTT_REQUEST) - MS-RDPBCGR 2.2.14.1.1 */
		success = autodetect_recv_rtt_measure_request(rdp, s, &autodetectReqPdu);
		break;

	case RDP_BW_START_REQUEST_TYPE_CONTINUOUS:
	case RDP_BW_START_REQUEST_TYPE_TUNNEL:
	case RDP_BW_START_REQUEST_TYPE_CONNECTTIME:
		/* Bandwidth Measure Start (RDP_BW_START) - MS-RDPBCGR 2.2.14.1.2 */
		success = autodetect_recv_bandwidth_measure_start(rdp, s, &autodetectReqPdu);
		break;

	case RDP_BW_PAYLOAD_REQUEST_TYPE:
		/* Bandwidth Measure Payload (RDP_BW_PAYLOAD) - MS-RDPBCGR 2.2.14.1.3 */
		success = autodetect_recv_bandwidth_measure_payload(rdp, s, &autodetectReqPdu);
		break;

	case RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME:
	case RDP_BW_STOP_REQUEST_TYPE_CONTINUOUS:
	case RDP_BW_STOP_REQUEST_TYPE_TUNNEL:
		/* Bandwidth Measure Stop (RDP_BW_STOP) - MS-RDPBCGR 2.2.14.1.4 */
		success = autodetect_recv_bandwidth_measure_stop(rdp, s, &autodetectReqPdu);
		break;

	case 0x0840:
	case 0x0880:
	case 0x08C0:
		/* Network Characteristics Result (RDP_NETCHAR_RESULT) - MS-RDPBCGR 2.2.14.1.5 */
		success = autodetect_recv_netchar_result(rdp, s, &autodetectReqPdu);
		break;

	default:
		break;
	}

	return success ? 0 : -1;
}

int rdp_recv_autodetect_response_packet(rdpRdp* rdp, wStream* s)
{
	AUTODETECT_RSP_PDU autodetectRspPdu;
	BOOL success = FALSE;

	if (Stream_GetRemainingLength(s) < 6)
		return -1;

	Stream_Read_UINT8(s, autodetectRspPdu.headerLength); /* headerLength (1 byte) */
	Stream_Read_UINT8(s, autodetectRspPdu.headerTypeId); /* headerTypeId (1 byte) */
	Stream_Read_UINT16(s, autodetectRspPdu.sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Read_UINT16(s, autodetectRspPdu.responseType); /* responseType (2 bytes) */

	WLog_VRB(AUTODETECT_TAG,
		"rdp_recv_autodetect_response_packet: headerLength=%u, headerTypeId=%u, sequenceNumber=%u, requestType=%04x",
		autodetectRspPdu.headerLength, autodetectRspPdu.headerTypeId,
		autodetectRspPdu.sequenceNumber, autodetectRspPdu.responseType);

	if (autodetectRspPdu.headerTypeId != TYPE_ID_AUTODETECT_RESPONSE)
		return -1;

	switch (autodetectRspPdu.responseType)
	{
	case RDP_RTT_RESPONSE_TYPE:
		/* RTT Measure Response (RDP_RTT_RESPONSE) - MS-RDPBCGR 2.2.14.2.1 */
		success = autodetect_recv_rtt_measure_response(rdp, s, &autodetectRspPdu);
		break;

	case RDP_BW_RESULTS_RESPONSE_TYPE_CONNECTTIME:
	case RDP_BW_RESULTS_RESPONSE_TYPE_CONTINUOUS:
		/* Bandwidth Measure Results (RDP_BW_RESULTS) - MS-RDPBCGR 2.2.14.2.2 */
		success = autodetect_recv_bandwidth_measure_results(rdp, s, &autodetectRspPdu);
		break;

	default:
		break;
	}

	return success ? 0 : -1;
}

rdpAutoDetect* autodetect_new(void)
{
	rdpAutoDetect* autoDetect = (rdpAutoDetect*) calloc(1, sizeof(rdpAutoDetect));

	if (autoDetect)
	{

	}
	
	return autoDetect;
}

void autodetect_free(rdpAutoDetect* autoDetect)
{
	free(autoDetect);
}

void autodetect_register_server_callbacks(rdpAutoDetect* autodetect)
{
	autodetect->RTTMeasureRequest = autodetect_send_continuous_rtt_measure_request;
	autodetect->BandwidthMeasureStart = autodetect_send_continuous_bandwidth_measure_start;
	autodetect->BandwidthMeasureStop = autodetect_send_continuous_bandwidth_measure_stop;
	autodetect->NetworkCharacteristicsResult = autodetect_send_netchar_result;
}
