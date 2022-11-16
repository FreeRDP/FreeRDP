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

#include <freerdp/config.h>

#include <winpr/crypto.h>
#include <winpr/assert.h>

#include "autodetect.h"

#define TYPE_ID_AUTODETECT_REQUEST 0x00
#define TYPE_ID_AUTODETECT_RESPONSE 0x01

#define RDP_RTT_REQUEST_TYPE_CONTINUOUS 0x0001
#define RDP_RTT_REQUEST_TYPE_CONNECTTIME 0x1001

#define RDP_RTT_RESPONSE_TYPE 0x0000

#define RDP_BW_START_REQUEST_TYPE_CONTINUOUS 0x0014
#define RDP_BW_START_REQUEST_TYPE_TUNNEL 0x0114
#define RDP_BW_START_REQUEST_TYPE_CONNECTTIME 0x1014
#define RDP_BW_PAYLOAD_REQUEST_TYPE 0x0002
#define RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME 0x002B
#define RDP_BW_STOP_REQUEST_TYPE_CONTINUOUS 0x0429
#define RDP_BW_STOP_REQUEST_TYPE_TUNNEL 0x0629

#define RDP_BW_RESULTS_RESPONSE_TYPE_CONNECTTIME 0x0003
#define RDP_BW_RESULTS_RESPONSE_TYPE_CONTINUOUS 0x000B

#define RDP_NETCHAR_SYNC_RESPONSE_TYPE 0x0018

#define RDP_NETCHAR_RESULTS_0x0840 0x0840U
#define RDP_NETCHAR_RESULTS_0x0880 0x0880U
#define RDP_NETCHAR_RESULTS_0x08C0 0x08C0U

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

static const char* autodetect_request_type_to_string(UINT32 requestType)
{
	switch (requestType)
	{
		case RDP_RTT_RESPONSE_TYPE:
			return "RDP_RTT_RESPONSE_TYPE";
		case RDP_BW_RESULTS_RESPONSE_TYPE_CONNECTTIME:
			return "RDP_BW_RESULTS_RESPONSE_TYPE_CONNECTTIME";
		case RDP_BW_RESULTS_RESPONSE_TYPE_CONTINUOUS:
			return "RDP_BW_RESULTS_RESPONSE_TYPE_CONTINUOUS";
		case RDP_RTT_REQUEST_TYPE_CONTINUOUS:
			return "RDP_RTT_REQUEST_TYPE_CONTINUOUS";
		case RDP_RTT_REQUEST_TYPE_CONNECTTIME:
			return "RDP_RTT_REQUEST_TYPE_CONNECTTIME";
		case RDP_BW_START_REQUEST_TYPE_CONTINUOUS:
			return "RDP_BW_START_REQUEST_TYPE_CONTINUOUS";
		case RDP_BW_START_REQUEST_TYPE_TUNNEL:
			return "RDP_BW_START_REQUEST_TYPE_TUNNEL";
		case RDP_BW_START_REQUEST_TYPE_CONNECTTIME:
			return "RDP_BW_START_REQUEST_TYPE_CONNECTTIME";
		case RDP_BW_PAYLOAD_REQUEST_TYPE:
			return "RDP_BW_PAYLOAD_REQUEST_TYPE";
		case RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME:
			return "RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME";
		case RDP_BW_STOP_REQUEST_TYPE_CONTINUOUS:
			return "RDP_BW_STOP_REQUEST_TYPE_CONTINUOUS";
		case RDP_BW_STOP_REQUEST_TYPE_TUNNEL:
			return "RDP_BW_STOP_REQUEST_TYPE_TUNNEL";
		case RDP_NETCHAR_RESULTS_0x0840:
			return "RDP_NETCHAR_RESULTS_0x0840";
		case RDP_NETCHAR_RESULTS_0x0880:
			return "RDP_NETCHAR_RESULTS_0x0880";
		case RDP_NETCHAR_RESULTS_0x08C0:
			return "RDP_NETCHAR_RESULTS_0x08C0";
		default:
			return "UNKNOWN";
	}
}

static BOOL autodetect_send_rtt_measure_request(rdpAutoDetect* autodetect,
                                                RDP_TRANSPORT_TYPE transport, UINT16 sequenceNumber,
                                                UINT16 requestType)
{
	wStream* s;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(autodetect->context);
	s = rdp_message_channel_pdu_init(autodetect->context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending RTT Measure Request PDU");
	Stream_Write_UINT8(s, 0x06);                       /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber);            /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, requestType);               /* requestType (2 bytes) */
	autodetect->rttMeasureStartTime = GetTickCount64();
	return rdp_send_message_channel_pdu(autodetect->context->rdp, s, SEC_AUTODETECT_REQ);
}

static BOOL autodetect_send_continuous_rtt_measure_request(rdpAutoDetect* autodetect,
                                                           RDP_TRANSPORT_TYPE transport,
                                                           UINT16 sequenceNumber)
{
	return autodetect_send_rtt_measure_request(autodetect, transport, sequenceNumber,
	                                           RDP_RTT_REQUEST_TYPE_CONTINUOUS);
}

BOOL autodetect_send_connecttime_rtt_measure_request(rdpAutoDetect* autodetect,
                                                     RDP_TRANSPORT_TYPE transport,
                                                     UINT16 sequenceNumber)
{
	return autodetect_send_rtt_measure_request(autodetect, transport, sequenceNumber,
	                                           RDP_RTT_REQUEST_TYPE_CONNECTTIME);
}

static BOOL autodetect_send_rtt_measure_response(rdpAutoDetect* autodetect, UINT16 sequenceNumber)
{
	wStream* s;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(autodetect->context);

	/* Send the response PDU to the server */
	s = rdp_message_channel_pdu_init(autodetect->context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending RTT Measure Response PDU (seqNumber=0x%" PRIx16 ")",
	         sequenceNumber);
	Stream_Write_UINT8(s, 0x06);                        /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_RESPONSE); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber);             /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, RDP_RTT_RESPONSE_TYPE);      /* responseType (1 byte) */
	return rdp_send_message_channel_pdu(autodetect->context->rdp, s, SEC_AUTODETECT_RSP);
}

static BOOL autodetect_send_bandwidth_measure_start(rdpAutoDetect* autodetect,
                                                    RDP_TRANSPORT_TYPE transport,
                                                    UINT16 sequenceNumber, UINT16 requestType)
{
	wStream* s;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(autodetect->context);

	s = rdp_message_channel_pdu_init(autodetect->context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending Bandwidth Measure Start PDU(seqNumber=%" PRIu16 ")",
	         sequenceNumber);
	Stream_Write_UINT8(s, 0x06);                       /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber);            /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, requestType);               /* requestType (2 bytes) */
	return rdp_send_message_channel_pdu(autodetect->context->rdp, s, SEC_AUTODETECT_REQ);
}

static BOOL autodetect_send_continuous_bandwidth_measure_start(rdpAutoDetect* autodetect,
                                                               RDP_TRANSPORT_TYPE transport,
                                                               UINT16 sequenceNumber)
{
	return autodetect_send_bandwidth_measure_start(autodetect, transport, sequenceNumber,
	                                               RDP_BW_START_REQUEST_TYPE_CONTINUOUS);
}

BOOL autodetect_send_connecttime_bandwidth_measure_start(rdpAutoDetect* autodetect,
                                                         RDP_TRANSPORT_TYPE transport,
                                                         UINT16 sequenceNumber)
{
	return autodetect_send_bandwidth_measure_start(autodetect, transport, sequenceNumber,
	                                               RDP_BW_START_REQUEST_TYPE_CONNECTTIME);
}

BOOL autodetect_send_bandwidth_measure_payload(rdpAutoDetect* autodetect,
                                               RDP_TRANSPORT_TYPE transport, UINT16 payloadLength,
                                               UINT16 sequenceNumber)
{
	wStream* s;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(autodetect->context);

	s = rdp_message_channel_pdu_init(autodetect->context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending Bandwidth Measure Payload PDU -> payloadLength=%" PRIu16 "",
	         payloadLength);
	/* 4-bytes aligned */
	payloadLength &= ~3;

	if (!Stream_EnsureRemainingCapacity(s, 8 + payloadLength))
	{
		WLog_ERR(AUTODETECT_TAG, "Failed to ensure %" PRIuz " bytes in stream",
		         8ull + payloadLength);
		Stream_Release(s);
		return FALSE;
	}

	Stream_Write_UINT8(s, 0x08);                         /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST);   /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber);              /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, RDP_BW_PAYLOAD_REQUEST_TYPE); /* requestType (2 bytes) */
	Stream_Write_UINT16(s, payloadLength);               /* payloadLength (2 bytes) */
	/* Random data (better measurement in case the line is compressed) */
	winpr_RAND(Stream_Pointer(s), payloadLength);
	Stream_Seek(s, payloadLength);
	return rdp_send_message_channel_pdu(autodetect->context->rdp, s, SEC_AUTODETECT_REQ);
}

static BOOL autodetect_send_bandwidth_measure_stop(rdpAutoDetect* autodetect,
                                                   RDP_TRANSPORT_TYPE transport,
                                                   UINT16 payloadLength, UINT16 sequenceNumber,
                                                   UINT16 requestType)
{
	wStream* s;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(autodetect->context);

	s = rdp_message_channel_pdu_init(autodetect->context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending Bandwidth Measure Stop PDU -> payloadLength=%" PRIu16 "",
	         payloadLength);
	/* 4-bytes aligned */
	payloadLength &= ~3;
	Stream_Write_UINT8(s, requestType == RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME
	                          ? 0x08
	                          : 0x06);                 /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST); /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber);            /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, requestType);               /* requestType (2 bytes) */

	if (requestType == RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME)
	{
		Stream_Write_UINT16(s, payloadLength); /* payloadLength (2 bytes) */

		if (payloadLength > 0)
		{
			if (!Stream_EnsureRemainingCapacity(s, payloadLength))
			{
				WLog_ERR(AUTODETECT_TAG, "Failed to ensure %" PRIuz " bytes in stream",
				         payloadLength);
				Stream_Release(s);
				return FALSE;
			}

			/* Random data (better measurement in case the line is compressed) */
			winpr_RAND(Stream_Pointer(s), payloadLength);
			Stream_Seek(s, payloadLength);
		}
	}

	return rdp_send_message_channel_pdu(autodetect->context->rdp, s, SEC_AUTODETECT_REQ);
}

static BOOL autodetect_send_continuous_bandwidth_measure_stop(rdpAutoDetect* autodetect,
                                                              RDP_TRANSPORT_TYPE transport,
                                                              UINT16 sequenceNumber)
{
	return autodetect_send_bandwidth_measure_stop(autodetect, transport, 0, sequenceNumber,
	                                              RDP_BW_STOP_REQUEST_TYPE_CONTINUOUS);
}

BOOL autodetect_send_connecttime_bandwidth_measure_stop(rdpAutoDetect* autodetect,
                                                        RDP_TRANSPORT_TYPE transport,
                                                        UINT16 payloadLength, UINT16 sequenceNumber)
{
	return autodetect_send_bandwidth_measure_stop(
	    autodetect, transport, payloadLength, sequenceNumber, RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME);
}

static BOOL autodetect_send_bandwidth_measure_results(rdpAutoDetect* autodetect,
                                                      RDP_TRANSPORT_TYPE transport,
                                                      UINT16 responseType, UINT16 sequenceNumber)
{
	BOOL success = TRUE;
	wStream* s;
	UINT64 timeDelta = GetTickCount64();

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(autodetect->context);

	/* Compute the total time */
	if (autodetect->bandwidthMeasureStartTime > timeDelta)
	{
		WLog_WARN(AUTODETECT_TAG,
		          "Invalid bandwidthMeasureStartTime %" PRIu64 " > current %" PRIu64
		          ", trimming to 0",
		          autodetect->bandwidthMeasureStartTime, timeDelta);
		timeDelta = 0;
	}
	else
		timeDelta -= autodetect->bandwidthMeasureStartTime;

	/* Send the result PDU to the server */
	s = rdp_message_channel_pdu_init(autodetect->context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG,
	         "sending Bandwidth Measure Results PDU -> timeDelta=%" PRIu64 ", byteCount=%" PRIu32
	         "",
	         timeDelta, autodetect->bandwidthMeasureByteCount);

	Stream_Write_UINT8(s, 0x0E);                                   /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_RESPONSE);            /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber);                        /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, responseType);                          /* responseType (1 byte) */
	Stream_Write_UINT32(s, (UINT32)MIN(timeDelta, UINT32_MAX));    /* timeDelta (4 bytes) */
	Stream_Write_UINT32(s, autodetect->bandwidthMeasureByteCount); /* byteCount (4 bytes) */
	IFCALLRET(autodetect->ClientBandwidthMeasureResult, success, autodetect, transport,
	          responseType, sequenceNumber, (UINT32)MIN(timeDelta, UINT32_MAX),
	          autodetect->bandwidthMeasureByteCount);

	if (!success)
	{
		WLog_ERR(AUTODETECT_TAG, "ClientBandwidthMeasureResult failed");
		return FALSE;
	}

	return rdp_send_message_channel_pdu(autodetect->context->rdp, s, SEC_AUTODETECT_RSP);
}

static BOOL autodetect_send_netchar_result(rdpAutoDetect* autodetect, RDP_TRANSPORT_TYPE transport,
                                           UINT16 sequenceNumber)
{
	wStream* s;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(autodetect->context);

	s = rdp_message_channel_pdu_init(autodetect->context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG, "sending Bandwidth Network Characteristics Result PDU");

	if (autodetect->netCharBandwidth > 0)
	{
		Stream_Write_UINT8(s, 0x12);                           /* headerLength (1 byte) */
		Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST);     /* headerTypeId (1 byte) */
		Stream_Write_UINT16(s, sequenceNumber);                /* sequenceNumber (2 bytes) */
		Stream_Write_UINT16(s, RDP_NETCHAR_RESULTS_0x08C0);    /* requestType (2 bytes) */
		Stream_Write_UINT32(s, autodetect->netCharBaseRTT);    /* baseRTT (4 bytes) */
		Stream_Write_UINT32(s, autodetect->netCharBandwidth);  /* bandwidth (4 bytes) */
		Stream_Write_UINT32(s, autodetect->netCharAverageRTT); /* averageRTT (4 bytes) */
	}
	else
	{
		Stream_Write_UINT8(s, 0x0E);                           /* headerLength (1 byte) */
		Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_REQUEST);     /* headerTypeId (1 byte) */
		Stream_Write_UINT16(s, sequenceNumber);                /* sequenceNumber (2 bytes) */
		Stream_Write_UINT16(s, RDP_NETCHAR_RESULTS_0x0840);    /* requestType (2 bytes) */
		Stream_Write_UINT32(s, autodetect->netCharBaseRTT);    /* baseRTT (4 bytes) */
		Stream_Write_UINT32(s, autodetect->netCharAverageRTT); /* averageRTT (4 bytes) */
	}

	return rdp_send_message_channel_pdu(autodetect->context->rdp, s, SEC_AUTODETECT_REQ);
}

static BOOL autodetect_send_netchar_sync(rdpAutoDetect* autodetect, RDP_TRANSPORT_TYPE transport,
                                         UINT16 sequenceNumber)
{
	wStream* s;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(autodetect->context);
	WINPR_ASSERT(autodetect->context->rdp);

	/* Send the response PDU to the server */
	s = rdp_message_channel_pdu_init(autodetect->context->rdp);

	if (!s)
		return FALSE;

	WLog_VRB(AUTODETECT_TAG,
	         "sending Network Characteristics Sync PDU -> bandwidth=%" PRIu32 ", rtt=%" PRIu32 "",
	         autodetect->netCharBandwidth, autodetect->netCharAverageRTT);
	Stream_Write_UINT8(s, 0x0E);                            /* headerLength (1 byte) */
	Stream_Write_UINT8(s, TYPE_ID_AUTODETECT_RESPONSE);     /* headerTypeId (1 byte) */
	Stream_Write_UINT16(s, sequenceNumber);                 /* sequenceNumber (2 bytes) */
	Stream_Write_UINT16(s, RDP_NETCHAR_SYNC_RESPONSE_TYPE); /* responseType (1 byte) */
	Stream_Write_UINT32(s, autodetect->netCharBandwidth);   /* bandwidth (4 bytes) */
	Stream_Write_UINT32(s, autodetect->netCharAverageRTT);  /* rtt (4 bytes) */
	return rdp_send_message_channel_pdu(autodetect->context->rdp, s, SEC_AUTODETECT_RSP);
}

static BOOL autodetect_recv_rtt_measure_request(rdpAutoDetect* autodetect,
                                                RDP_TRANSPORT_TYPE transport, wStream* s,
                                                const AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(s);
	WINPR_ASSERT(autodetectReqPdu);

	if (autodetectReqPdu->headerLength != 0x06)
	{
		WLog_ERR(AUTODETECT_TAG, "autodetectReqPdu->headerLength != 0x06 [0x%02" PRIx8 "]",
		         autodetectReqPdu->headerLength);
		return FALSE;
	}

	WLog_VRB(AUTODETECT_TAG, "received RTT Measure Request PDU");
	/* Send a response to the server */
	return autodetect_send_rtt_measure_response(autodetect, autodetectReqPdu->sequenceNumber);
}

static BOOL autodetect_recv_rtt_measure_response(rdpAutoDetect* autodetect,
                                                 RDP_TRANSPORT_TYPE transport, wStream* s,
                                                 const AUTODETECT_RSP_PDU* autodetectRspPdu)
{
	BOOL success = TRUE;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(autodetectRspPdu);

	if (autodetectRspPdu->headerLength != 0x06)
	{
		WLog_ERR(AUTODETECT_TAG, "autodetectRspPdu->headerLength != 0x06 [0x%02" PRIx8 "]",
		         autodetectRspPdu->headerLength);
		return FALSE;
	}

	WLog_VRB(AUTODETECT_TAG, "received RTT Measure Response PDU");
	autodetect->netCharAverageRTT =
	    (UINT32)MIN(GetTickCount64() - autodetect->rttMeasureStartTime, UINT32_MAX);

	if (autodetect->netCharBaseRTT == 0 ||
	    autodetect->netCharBaseRTT > autodetect->netCharAverageRTT)
		autodetect->netCharBaseRTT = autodetect->netCharAverageRTT;

	IFCALLRET(autodetect->RTTMeasureResponse, success, autodetect, transport,
	          autodetectRspPdu->sequenceNumber);
	if (!success)
		WLog_WARN(AUTODETECT_TAG, "RTTMeasureResponse failed");
	return success;
}

static BOOL autodetect_recv_bandwidth_measure_start(rdpAutoDetect* autodetect,
                                                    RDP_TRANSPORT_TYPE transport, wStream* s,
                                                    const AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(s);
	WINPR_ASSERT(autodetectReqPdu);

	if (autodetectReqPdu->headerLength != 0x06)
	{
		WLog_ERR(AUTODETECT_TAG, "autodetectReqPdu->headerLength != 0x06 [0x%02" PRIx8 "]",
		         autodetectReqPdu->headerLength);
		return FALSE;
	}

	WLog_VRB(AUTODETECT_TAG, "received Bandwidth Measure Start PDU - time=%" PRIu64 "",
	         GetTickCount64());
	/* Initialize bandwidth measurement parameters */
	autodetect->bandwidthMeasureStartTime = GetTickCount64();
	autodetect->bandwidthMeasureByteCount = 0;

	/* Continuous Auto-Detection: mark the start of the measurement */
	if (autodetectReqPdu->requestType == RDP_BW_START_REQUEST_TYPE_CONTINUOUS)
	{
		autodetect->bandwidthMeasureStarted = TRUE;
	}

	return TRUE;
}

static BOOL autodetect_recv_bandwidth_measure_payload(rdpAutoDetect* autodetect,
                                                      RDP_TRANSPORT_TYPE transport, wStream* s,
                                                      const AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	UINT16 payloadLength;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(s);
	WINPR_ASSERT(autodetectReqPdu);

	if (autodetectReqPdu->headerLength != 0x08)
	{
		WLog_ERR(AUTODETECT_TAG, "autodetectReqPdu->headerLength != 0x08 [0x%02" PRIx8 "]",
		         autodetectReqPdu->headerLength);
		return FALSE;
	}

	if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, 2))
		return FALSE;

	Stream_Read_UINT16(s, payloadLength); /* payloadLength (2 bytes) */
	if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, payloadLength))
		return FALSE;
	Stream_Seek(s, payloadLength);

	WLog_DBG(AUTODETECT_TAG, "received Bandwidth Measure Payload PDU -> payloadLength=%" PRIu16 "",
	         payloadLength);
	/* Add the payload length to the bandwidth measurement parameters */
	autodetect->bandwidthMeasureByteCount += payloadLength;
	return TRUE;
}

static BOOL autodetect_recv_bandwidth_measure_stop(rdpAutoDetect* autodetect,
                                                   RDP_TRANSPORT_TYPE transport, wStream* s,
                                                   const AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	UINT16 payloadLength;
	UINT16 responseType;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(s);
	WINPR_ASSERT(autodetectReqPdu);

	if (autodetectReqPdu->requestType == RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME)
	{
		if (autodetectReqPdu->headerLength != 0x08)
		{
			WLog_ERR(AUTODETECT_TAG, "autodetectReqPdu->headerLength != 0x08 [0x%02" PRIx8 "]",
			         autodetectReqPdu->headerLength);
			return FALSE;
		}

		if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, 2))
			return FALSE;

		Stream_Read_UINT16(s, payloadLength); /* payloadLength (2 bytes) */
	}
	else
	{
		if (autodetectReqPdu->headerLength != 0x06)
		{
			WLog_ERR(AUTODETECT_TAG, "autodetectReqPdu->headerLength != 0x06 [0x%02" PRIx8 "]",
			         autodetectReqPdu->headerLength);
			return FALSE;
		}

		payloadLength = 0;
	}

	if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, payloadLength))
		return FALSE;
	Stream_Seek(s, payloadLength);

	WLog_VRB(AUTODETECT_TAG, "received Bandwidth Measure Stop PDU -> payloadLength=%" PRIu16 "",
	         payloadLength);
	/* Add the payload length to the bandwidth measurement parameters */
	autodetect->bandwidthMeasureByteCount += payloadLength;

	/* Continuous Auto-Detection: mark the stop of the measurement */
	if (autodetectReqPdu->requestType == RDP_BW_STOP_REQUEST_TYPE_CONTINUOUS)
	{
		autodetect->bandwidthMeasureStarted = FALSE;
	}

	/* Send a response the server */
	responseType = autodetectReqPdu->requestType == RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME
	                   ? RDP_BW_RESULTS_RESPONSE_TYPE_CONNECTTIME
	                   : RDP_BW_RESULTS_RESPONSE_TYPE_CONTINUOUS;
	IFCALL(autodetect->BandwidthMeasureResults, autodetect, transport, responseType,
	       autodetectReqPdu->sequenceNumber);
	return autodetect_send_bandwidth_measure_results(autodetect, transport, responseType,
	                                                 autodetectReqPdu->sequenceNumber);
}

static BOOL autodetect_recv_bandwidth_measure_results(rdpAutoDetect* autodetect,
                                                      RDP_TRANSPORT_TYPE transport, wStream* s,
                                                      const AUTODETECT_RSP_PDU* autodetectRspPdu)
{
	BOOL success = TRUE;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(s);
	WINPR_ASSERT(autodetectRspPdu);

	if (autodetectRspPdu->headerLength != 0x0E)
	{
		WLog_ERR(AUTODETECT_TAG, "autodetectRspPdu->headerLength != 0x0E [0x%02" PRIx8 "]",
		         autodetectRspPdu->headerLength);
		return FALSE;
	}

	WLog_VRB(AUTODETECT_TAG, "received Bandwidth Measure Results PDU");
	if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, 8))
		return FALSE;
	Stream_Read_UINT32(s, autodetect->bandwidthMeasureTimeDelta); /* timeDelta (4 bytes) */
	Stream_Read_UINT32(s, autodetect->bandwidthMeasureByteCount); /* byteCount (4 bytes) */

	if (autodetect->bandwidthMeasureTimeDelta > 0)
		autodetect->netCharBandwidth = (UINT32)MIN(autodetect->bandwidthMeasureByteCount * 8ULL /
		                                               autodetect->bandwidthMeasureTimeDelta,
		                                           UINT32_MAX);
	else
		autodetect->netCharBandwidth = 0;

	IFCALLRET(autodetect->BandwidthMeasureResults, success, autodetect, transport,
	          autodetectRspPdu->responseType, autodetectRspPdu->sequenceNumber);
	if (!success)
		WLog_WARN(AUTODETECT_TAG, "BandwidthMeasureResults failed");
	return success;
}

static BOOL autodetect_recv_netchar_result(rdpAutoDetect* autodetect, RDP_TRANSPORT_TYPE transport,
                                           wStream* s, const AUTODETECT_RSP_PDU* autodetectRspPdu)
{
	BOOL success = TRUE;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(s);
	WINPR_ASSERT(autodetectRspPdu);

	switch (autodetectRspPdu->responseType)
	{
		case RDP_NETCHAR_SYNC_RESPONSE_TYPE:
			/* bandwidth and averageRTT fields are present (baseRTT field is not) */
			if (autodetectRspPdu->headerLength != 0x0E)
			{
				WLog_ERR(AUTODETECT_TAG, "autodetectRspPdu->headerLength != 0x0E [0x%02" PRIx8 "]",
				         autodetectRspPdu->headerLength);
				return FALSE;
			}
			if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, 8))
				return FALSE;

			Stream_Read_UINT32(s, autodetect->netCharBandwidth);  /* bandwidth (4 bytes) */
			Stream_Read_UINT32(s, autodetect->netCharAverageRTT); /* averageRTT (4 bytes) */
			break;
		default:
			break;
	}

	WLog_VRB(AUTODETECT_TAG,
	         "received Network Characteristics Result PDU -> baseRTT=%" PRIu32
	         ", bandwidth=%" PRIu32 ", averageRTT=%" PRIu32 "",
	         autodetect->netCharBaseRTT, autodetect->netCharBandwidth,
	         autodetect->netCharAverageRTT);

	IFCALLRET(autodetect->NetworkCharacteristicsResult, success, autodetect, transport,
	          autodetectRspPdu->sequenceNumber);
	if (!success)
		WLog_WARN(AUTODETECT_TAG, "NetworkCharacteristicsResult failed");
	return success;
}

static BOOL autodetect_recv_netchar_request(rdpAutoDetect* autodetect, RDP_TRANSPORT_TYPE transport,
                                            wStream* s, const AUTODETECT_REQ_PDU* autodetectReqPdu)
{
	BOOL success = TRUE;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(s);
	WINPR_ASSERT(autodetectReqPdu);

	switch (autodetectReqPdu->requestType)
	{
		case RDP_NETCHAR_RESULTS_0x0840:

			/* baseRTT and averageRTT fields are present (bandwidth field is not) */
			if (autodetectReqPdu->headerLength != 0x0E)
			{
				WLog_ERR(AUTODETECT_TAG, "autodetectReqPdu->headerLength != 0x0E [0x%02" PRIx8 "]",
				         autodetectReqPdu->headerLength);
				return FALSE;
			}
			if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, 8))
				return FALSE;

			Stream_Read_UINT32(s, autodetect->netCharBaseRTT);    /* baseRTT (4 bytes) */
			Stream_Read_UINT32(s, autodetect->netCharAverageRTT); /* averageRTT (4 bytes) */
			break;

		case RDP_NETCHAR_RESULTS_0x0880:

			/* bandwidth and averageRTT fields are present (baseRTT field is not) */
			if (autodetectReqPdu->headerLength != 0x0E)
			{
				WLog_ERR(AUTODETECT_TAG, "autodetectReqPdu->headerLength != 0x0E [0x%02" PRIx8 "]",
				         autodetectReqPdu->headerLength);
				return FALSE;
			}
			if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, 8))
				return FALSE;

			Stream_Read_UINT32(s, autodetect->netCharBandwidth);  /* bandwidth (4 bytes) */
			Stream_Read_UINT32(s, autodetect->netCharAverageRTT); /* averageRTT (4 bytes) */
			break;

		case RDP_NETCHAR_RESULTS_0x08C0:

			/* baseRTT, bandwidth, and averageRTT fields are present */
			if (autodetectReqPdu->headerLength != 0x12)
			{
				WLog_ERR(AUTODETECT_TAG, "autodetectReqPdu->headerLength != 0x012 [0x%02" PRIx8 "]",
				         autodetectReqPdu->headerLength);
				return FALSE;
			}
			if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, 12))
				return FALSE;

			Stream_Read_UINT32(s, autodetect->netCharBaseRTT);    /* baseRTT (4 bytes) */
			Stream_Read_UINT32(s, autodetect->netCharBandwidth);  /* bandwidth (4 bytes) */
			Stream_Read_UINT32(s, autodetect->netCharAverageRTT); /* averageRTT (4 bytes) */
			break;

		default:
			break;
	}

	WLog_VRB(AUTODETECT_TAG,
	         "received Network Characteristics Result PDU -> baseRTT=%" PRIu32
	         ", bandwidth=%" PRIu32 ", averageRTT=%" PRIu32 "",
	         autodetect->netCharBaseRTT, autodetect->netCharBandwidth,
	         autodetect->netCharAverageRTT);

	IFCALLRET(autodetect->NetworkCharacteristicsResult, success, autodetect, transport,
	          autodetectReqPdu->sequenceNumber);
	if (!success)
		WLog_WARN(AUTODETECT_TAG, "NetworkCharacteristicsResult failed");
	return success;
}

state_run_t autodetect_recv_request_packet(rdpAutoDetect* autodetect, RDP_TRANSPORT_TYPE transport,
                                           wStream* s)
{
	AUTODETECT_REQ_PDU autodetectReqPdu = { 0 };
	const rdpSettings* settings;
	BOOL success = FALSE;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(autodetect->context);

	settings = autodetect->context->settings;
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, 6))
		return STATE_RUN_FAILED;

	Stream_Read_UINT8(s, autodetectReqPdu.headerLength);    /* headerLength (1 byte) */
	Stream_Read_UINT8(s, autodetectReqPdu.headerTypeId);    /* headerTypeId (1 byte) */
	Stream_Read_UINT16(s, autodetectReqPdu.sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Read_UINT16(s, autodetectReqPdu.requestType);    /* requestType (2 bytes) */
	WLog_VRB(AUTODETECT_TAG,
	         "rdp_recv_autodetect_request_packet: headerLength=%" PRIu8 ", headerTypeId=%" PRIu8
	         ", sequenceNumber=%" PRIu16 ", requestType=%04" PRIx16 "",
	         autodetectReqPdu.headerLength, autodetectReqPdu.headerTypeId,
	         autodetectReqPdu.sequenceNumber, autodetectReqPdu.requestType);

	if (!freerdp_settings_get_bool(settings, FreeRDP_NetworkAutoDetect))
	{
		WLog_WARN(AUTODETECT_TAG,
		          "Received a [MS-RDPBCGR] 2.2.14.1.1 RTT Measure Request (RDP_RTT_REQUEST) [%s] "
		          "message but support was not enabled",
		          autodetect_request_type_to_string(autodetectReqPdu.requestType));
	}

	if (autodetectReqPdu.headerTypeId != TYPE_ID_AUTODETECT_REQUEST)
	{
		WLog_ERR(AUTODETECT_TAG,
		         "Received a [MS-RDPBCGR] 2.2.14.1.1 RTT Measure Request (RDP_RTT_REQUEST) [%s] "
		         "message with invalid headerTypeId=0x%04" PRIx16,
		         autodetect_request_type_to_string(autodetectReqPdu.requestType),
		         autodetectReqPdu.headerTypeId);
		goto fail;
	}

	IFCALL(autodetect->RequestReceived, autodetect, transport, autodetectReqPdu.requestType,
	       autodetectReqPdu.sequenceNumber);
	switch (autodetectReqPdu.requestType)
	{
		case RDP_RTT_REQUEST_TYPE_CONTINUOUS:
		case RDP_RTT_REQUEST_TYPE_CONNECTTIME:
			/* RTT Measure Request (RDP_RTT_REQUEST) - MS-RDPBCGR 2.2.14.1.1 */
			success =
			    autodetect_recv_rtt_measure_request(autodetect, transport, s, &autodetectReqPdu);
			break;

		case RDP_BW_START_REQUEST_TYPE_CONTINUOUS:
		case RDP_BW_START_REQUEST_TYPE_TUNNEL:
		case RDP_BW_START_REQUEST_TYPE_CONNECTTIME:
			/* Bandwidth Measure Start (RDP_BW_START) - MS-RDPBCGR 2.2.14.1.2 */
			success = autodetect_recv_bandwidth_measure_start(autodetect, transport, s,
			                                                  &autodetectReqPdu);
			break;

		case RDP_BW_PAYLOAD_REQUEST_TYPE:
			/* Bandwidth Measure Payload (RDP_BW_PAYLOAD) - MS-RDPBCGR 2.2.14.1.3 */
			success = autodetect_recv_bandwidth_measure_payload(autodetect, transport, s,
			                                                    &autodetectReqPdu);
			break;

		case RDP_BW_STOP_REQUEST_TYPE_CONNECTTIME:
		case RDP_BW_STOP_REQUEST_TYPE_CONTINUOUS:
		case RDP_BW_STOP_REQUEST_TYPE_TUNNEL:
			/* Bandwidth Measure Stop (RDP_BW_STOP) - MS-RDPBCGR 2.2.14.1.4 */
			success =
			    autodetect_recv_bandwidth_measure_stop(autodetect, transport, s, &autodetectReqPdu);
			break;

		case RDP_NETCHAR_RESULTS_0x0840:
		case RDP_NETCHAR_RESULTS_0x0880:
		case RDP_NETCHAR_RESULTS_0x08C0:
			/* Network Characteristics Result (RDP_NETCHAR_RESULT) - MS-RDPBCGR 2.2.14.1.5 */
			success = autodetect_recv_netchar_request(autodetect, transport, s, &autodetectReqPdu);
			break;

		default:
			break;
	}

fail:
	if (success)
		autodetect->state = AUTODETECT_STATE_REQUEST;
	else
		autodetect->state = AUTODETECT_STATE_FAIL;
	return success ? STATE_RUN_SUCCESS : STATE_RUN_FAILED;
}

state_run_t autodetect_recv_response_packet(rdpAutoDetect* autodetect, RDP_TRANSPORT_TYPE transport,
                                            wStream* s)
{
	AUTODETECT_RSP_PDU autodetectRspPdu = { 0 };
	const rdpSettings* settings;
	BOOL success = FALSE;

	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(autodetect->context);
	WINPR_ASSERT(s);

	settings = autodetect->context->settings;
	WINPR_ASSERT(settings);

	if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, 6))
		goto fail;

	Stream_Read_UINT8(s, autodetectRspPdu.headerLength);    /* headerLength (1 byte) */
	Stream_Read_UINT8(s, autodetectRspPdu.headerTypeId);    /* headerTypeId (1 byte) */
	Stream_Read_UINT16(s, autodetectRspPdu.sequenceNumber); /* sequenceNumber (2 bytes) */
	Stream_Read_UINT16(s, autodetectRspPdu.responseType);   /* responseType (2 bytes) */
	WLog_VRB(AUTODETECT_TAG,
	         "rdp_recv_autodetect_response_packet: headerLength=%" PRIu8 ", headerTypeId=%" PRIu8
	         ", sequenceNumber=%" PRIu16 ", requestType=%04" PRIx16 "",
	         autodetectRspPdu.headerLength, autodetectRspPdu.headerTypeId,
	         autodetectRspPdu.sequenceNumber, autodetectRspPdu.responseType);

	if (!freerdp_settings_get_bool(settings, FreeRDP_NetworkAutoDetect))
	{
		WLog_WARN(AUTODETECT_TAG,
		          "Received a [MS-RDPBCGR] 2.2.14.2.1 RTT Measure Response (RDP_RTT_RESPONSE) [%s] "
		          "message but support was not enabled",
		          autodetect_request_type_to_string(autodetectRspPdu.responseType));
	}

	if (autodetectRspPdu.headerTypeId != TYPE_ID_AUTODETECT_RESPONSE)
	{
		WLog_ERR(AUTODETECT_TAG,
		         "Received a [MS-RDPBCGR] 2.2.14.2.1 RTT Measure Response (RDP_RTT_RESPONSE) [%s] "
		         "message with invalid headerTypeId=0x%04" PRIx16,
		         autodetect_request_type_to_string(autodetectRspPdu.responseType),
		         autodetectRspPdu.headerTypeId);
		goto fail;
	}

	IFCALL(autodetect->ResponseReceived, autodetect, transport, autodetectRspPdu.responseType,
	       autodetectRspPdu.sequenceNumber);
	switch (autodetectRspPdu.responseType)
	{
		case RDP_RTT_RESPONSE_TYPE:
			/* RTT Measure Response (RDP_RTT_RESPONSE) - MS-RDPBCGR 2.2.14.2.1 */
			success =
			    autodetect_recv_rtt_measure_response(autodetect, transport, s, &autodetectRspPdu);
			break;

		case RDP_BW_RESULTS_RESPONSE_TYPE_CONNECTTIME:
		case RDP_BW_RESULTS_RESPONSE_TYPE_CONTINUOUS:
			/* Bandwidth Measure Results (RDP_BW_RESULTS) - MS-RDPBCGR 2.2.14.2.2 */
			success = autodetect_recv_bandwidth_measure_results(autodetect, transport, s,
			                                                    &autodetectRspPdu);
			break;

		case RDP_NETCHAR_SYNC_RESPONSE_TYPE:
			success = autodetect_recv_netchar_result(autodetect, transport, s, &autodetectRspPdu);
			break;

		default:
			break;
	}

fail:
	if (success)
	{
		if (autodetectRspPdu.responseType == RDP_BW_RESULTS_RESPONSE_TYPE_CONNECTTIME)
			autodetect->state = AUTODETECT_STATE_COMPLETE;
		else
			autodetect->state = AUTODETECT_STATE_RESPONSE;
	}
	else
		autodetect->state = AUTODETECT_STATE_FAIL;

	return success ? STATE_RUN_SUCCESS : STATE_RUN_FAILED;
}

rdpAutoDetect* autodetect_new(rdpContext* context)
{
	rdpAutoDetect* autoDetect = (rdpAutoDetect*)calloc(1, sizeof(rdpAutoDetect));
	if (!autoDetect)
		return NULL;
	autoDetect->context = context;

	return autoDetect;
}

void autodetect_free(rdpAutoDetect* autoDetect)
{
	free(autoDetect);
}

void autodetect_register_server_callbacks(rdpAutoDetect* autodetect)
{
	WINPR_ASSERT(autodetect);

	autodetect->RTTMeasureRequest = autodetect_send_continuous_rtt_measure_request;
	autodetect->BandwidthMeasureStart = autodetect_send_continuous_bandwidth_measure_start;
	autodetect->BandwidthMeasureStop = autodetect_send_continuous_bandwidth_measure_stop;
	autodetect->NetworkCharacteristicsResult = autodetect_send_netchar_result;
}

AUTODETECT_STATE autodetect_get_state(rdpAutoDetect* autodetect)
{
	WINPR_ASSERT(autodetect);
	return autodetect->state;
}

rdpAutoDetect* autodetect_get(rdpContext* context)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->rdp);
	return context->rdp->autodetect;
}
