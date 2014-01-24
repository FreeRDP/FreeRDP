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

#define WITH_DEBUG_AUTODETECT

#include "autodetect.h"

typedef struct
{
	UINT8 headerLength;
	UINT8 headerTypeId;
	UINT16 sequenceNumber;
	UINT16 requestType;
} AUTODETECT_REQ_PDU;

static BOOL autodetect_send_rtt_measure_response(rdpRdp* rdp, UINT16 sequenceNumber)
{
	wStream* s;

	/* Send the response PDU to the server */
	s = rdp_message_channel_pdu_init(rdp);
	if (s == NULL) return FALSE;

	DEBUG_AUTODETECT("sending RTT Measure Response PDU");

	Stream_Write_UINT8(s, 0x06); /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_RESPONSE); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, 0x0000); /* responseType (1 byte) */

	return rdp_send_message_channel_pdu(rdp, s, SEC_AUTODETECT_RSP);
}

static BOOL autodetect_send_bandwidth_measure_results(rdpRdp* rdp, UINT16 responseType, UINT16 sequenceNumber)
{
	UINT32 timeDelta;
	wStream* s;
	
	/* Compute the total time */
	timeDelta = GetTickCount() - rdp->autodetect->bandwidthMeasureStartTime;
	
	/* Send the result PDU to the server */
	s = rdp_message_channel_pdu_init(rdp);
	if (s == NULL) return FALSE;

	DEBUG_AUTODETECT("sending Bandwidth Measure Results PDU -> timeDelta=%u, byteCount=%u", timeDelta, rdp->autodetect->bandwidthMeasureByteCount);

	Stream_Write_UINT8(s, 0x0E); /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_RESPONSE); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, responseType); /* responseType (1 byte) */
	Stream_Write_UINT32(s, timeDelta); /* timeDelta (4 bytes) */
	Stream_Write_UINT32(s, rdp->autodetect->bandwidthMeasureByteCount); /* byteCount (4 bytes) */

	return rdp_send_message_channel_pdu(rdp, s, SEC_AUTODETECT_RSP);
}

static BOOL autodetect_send_netchar_sync(rdpRdp* rdp, UINT16 sequenceNumber)
{
	wStream* s;
	
	/* Send the response PDU to the server */
	s = rdp_message_channel_pdu_init(rdp);
	if (s == NULL) return FALSE;

	DEBUG_AUTODETECT("sending Network Characteristics Sync PDU -> bandwidth=%u, rtt=%u", rdp->autodetect->netCharBandwidth, rdp->autodetect->netCharAverageRTT);

	Stream_Write_UINT8(s, 0x0E); /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_RESPONSE); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, 0x0018); /* responseType (1 byte) */
	Stream_Write_UINT32(s, rdp->autodetect->netCharBandwidth); /* bandwidth (4 bytes) */
	Stream_Write_UINT32(s, rdp->autodetect->netCharAverageRTT); /* rtt (4 bytes) */

	return rdp_send_message_channel_pdu(rdp, s, SEC_AUTODETECT_RSP);
}

static BOOL autodetect_recv_rtt_measure_request(rdpRdp* rdp, wStream* s, AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	if (autodetectReqPdu->headerLength != 0x06)
		return FALSE;

	DEBUG_AUTODETECT("received RTT Measure Request PDU");

	/* Send a response to the server */
	return autodetect_send_rtt_measure_response(rdp, autodetectReqPdu->sequenceNumber);
}

static BOOL autodetect_recv_bandwidth_measure_start(rdpRdp* rdp, wStream* s, AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	if (autodetectReqPdu->headerLength != 0x06)
		return FALSE;

	DEBUG_AUTODETECT("received Bandwidth Measure Start PDU");

	/* Initialize bandwidth measurement parameters */
	rdp->autodetect->bandwidthMeasureStartTime = GetTickCount();
	rdp->autodetect->bandwidthMeasureByteCount = 0;

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

	DEBUG_AUTODETECT("received Bandwidth Measure Payload PDU -> payloadLength=%u", payloadLength);

	/* Add the payload length to the bandwidth measurement parameters */
	rdp->autodetect->bandwidthMeasureByteCount += payloadLength;

	return TRUE;
}

static BOOL autodetect_recv_bandwidth_measure_stop(rdpRdp* rdp, wStream* s, AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	UINT16 payloadLength;
	UINT16 responseType;

	if (autodetectReqPdu->requestType == 0x002B)
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

	DEBUG_AUTODETECT("received Bandwidth Measure Stop PDU -> payloadLength=%u", payloadLength);

	/* Add the payload length to the bandwidth measurement parameters */
	rdp->autodetect->bandwidthMeasureByteCount += payloadLength;

	/* Send a response the server */
	responseType = autodetectReqPdu->requestType == 0x002B ? 0x0003 : 0x000B;

	return autodetect_send_bandwidth_measure_results(rdp, responseType, autodetectReqPdu->sequenceNumber);
}

static BOOL autodetect_recv_netchar_result(rdpRdp* rdp, wStream* s, AUTODETECT_REQ_PDU* autodetectReqPdu)
{
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

	DEBUG_AUTODETECT("received Network Characteristics Result PDU -> baseRTT=%u, bandwidth=%u, averageRTT=%u", rdp->autodetect->netCharBaseRTT, rdp->autodetect->netCharBandwidth, rdp->autodetect->netCharAverageRTT);
 
	return TRUE;
}

int rdp_recv_autodetect_packet(rdpRdp* rdp, wStream* s)
{
	AUTODETECT_REQ_PDU autodetectReqPdu;
	BOOL success = FALSE;
	
	if (Stream_GetRemainingLength(s) < 6)
		return -1;

	Stream_Read_UINT8(s, autodetectReqPdu.headerLength); /* headerLength (1 byte) */
	Stream_Read_UINT8(s, autodetectReqPdu.headerTypeId); /* headerTypeId (1 byte) */
	Stream_Read_UINT16(s, autodetectReqPdu.sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Read_UINT16(s, autodetectReqPdu.requestType); /* requestType (2 bytes) */

	if (autodetectReqPdu.headerTypeId != TYPE_ID_AUTODETECT_REQUEST)
		return -1;

	switch (autodetectReqPdu.requestType)
	{
	case 0x0001:
	case 0x1001:
		/* RTT Measure Request (RDP_RTT_REQUEST) - MS-RDPBCGR 2.2.14.1.1 */
		success = autodetect_recv_rtt_measure_request(rdp, s, &autodetectReqPdu);
		break;

	case 0x0014:
	case 0x0114:
	case 0x1014:
		/* Bandwidth Measure Start (RDP_BW_START) - MS-RDPBCGR 2.2.14.1.2 */
		success = autodetect_recv_bandwidth_measure_start(rdp, s, &autodetectReqPdu);
		break;

	case 0x0002:
		/* Bandwidth Measure Payload (RDP_BW_PAYLOAD) - MS-RDPBCGR 2.2.14.1.3 */
		success = autodetect_recv_bandwidth_measure_payload(rdp, s, &autodetectReqPdu);
		break;

	case 0x002B:
	case 0x0429:
	case 0x0629:
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

rdpAutoDetect* autodetect_new(void)
{
	rdpAutoDetect* autodetect = (rdpAutoDetect*)malloc(sizeof(rdpAutoDetect));
	if (autodetect)
	{
		memset(autodetect, 0, sizeof(rdpAutoDetect));
	}
	
	return autodetect;
}

void autodetect_free(rdpAutoDetect* autodetect)
{
	free(autodetect);
}
