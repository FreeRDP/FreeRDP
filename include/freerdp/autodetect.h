/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Auto-Detect PDUs
 *
 * Copyright 2014 Dell Software <Mike.McDonald@software.dell.com>
 * Copyright 2014 Vic Lee
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

#ifndef FREERDP_AUTODETECT_H
#define FREERDP_AUTODETECT_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum
	{
		FREERDP_AUTODETECT_STATE_INITIAL,
		FREERDP_AUTODETECT_STATE_REQUEST,
		FREERDP_AUTODETECT_STATE_RESPONSE,
		FREERDP_AUTODETECT_STATE_COMPLETE,
		FREERDP_AUTODETECT_STATE_FAIL
	} FREERDP_AUTODETECT_STATE;

	typedef enum
	{
		RDP_NETCHAR_RESERVED = 0x0000U,
		/* The baseRTT and averageRTT fields are valid */
		RDP_NETCHAR_RESULT_TYPE_BASE_RTT_AVG_RTT = 0x0840U,
		/* The bandwidth and averageRTT fields are valid */
		RDP_NETCHAR_RESULT_TYPE_BW_AVG_RTT = 0x0880U,
		/* The baseRTT, bandwidth and averageRTT fields are valid */
		RDP_NETCHAR_RESULT_TYPE_BASE_RTT_BW_AVG_RTT = 0x08C0U
	} RDP_NETCHAR_RESULT_TYPE;

	typedef enum
	{
		RDP_BW_RESULTS_RESPONSE_TYPE_CONNECTTIME = 0x0003,
		RDP_BW_RESULTS_RESPONSE_TYPE_CONTINUOUS = 0x000B
	} RDP_BW_RESULTS_RESPONSE_TYPE;

	typedef struct rdp_autodetect rdpAutoDetect;
	typedef struct rdp_network_characteristics_result rdpNetworkCharacteristicsResult;

	typedef BOOL (*pRTTMeasureRequest)(rdpAutoDetect* autodetect, RDP_TRANSPORT_TYPE transport,
	                                   UINT16 sequenceNumber);
	typedef BOOL (*pRTTMeasureResponse)(rdpAutoDetect* autodetect, RDP_TRANSPORT_TYPE transport,
	                                    UINT16 sequenceNumber);
	typedef BOOL (*pBandwidthMeasureStart)(rdpAutoDetect* autodetect, RDP_TRANSPORT_TYPE transport,
	                                       UINT16 sequenceNumber);
	typedef BOOL (*pBandwidthMeasurePayload)(rdpAutoDetect* autodetect,
	                                         RDP_TRANSPORT_TYPE transport, UINT16 sequenceNumber,
	                                         UINT16 payloadLength);
	typedef BOOL (*pBandwidthMeasureStop)(rdpAutoDetect* autodetect, RDP_TRANSPORT_TYPE transport,
	                                      UINT16 sequenceNumber, UINT16 payloadLength);
	typedef BOOL (*pBandwidthMeasureResults)(rdpAutoDetect* autodetect,
	                                         RDP_TRANSPORT_TYPE transport, UINT16 sequenceNumber,
	                                         UINT16 responseType, UINT32 timeDelta,
	                                         UINT32 byteCount);
	typedef BOOL (*pNetworkCharacteristicsResult)(rdpAutoDetect* autodetect,
	                                              RDP_TRANSPORT_TYPE transport,
	                                              UINT16 sequenceNumber,
	                                              const rdpNetworkCharacteristicsResult* result);
	typedef BOOL (*pClientBandwidthMeasureResult)(rdpAutoDetect* autodetect,
	                                              RDP_TRANSPORT_TYPE transport, UINT16 responseType,
	                                              UINT16 sequenceNumber, UINT32 timeDelta,
	                                              UINT32 byteCount);
	typedef BOOL (*pNetworkCharacteristicsSync)(rdpAutoDetect* autodetect,
	                                            RDP_TRANSPORT_TYPE transport, UINT16 sequenceNumber,
	                                            UINT32 bandwidth, UINT32 rtt);
	typedef BOOL (*pRxTxReceived)(rdpAutoDetect* autodetect, RDP_TRANSPORT_TYPE transport,
	                              UINT16 requestType, UINT16 sequenceNumber);
	typedef FREERDP_AUTODETECT_STATE (*pOnConnectTimeAutoDetect)(rdpAutoDetect* autodetect);

	struct rdp_network_characteristics_result
	{
		/* Specifies, which fields are valid */
		RDP_NETCHAR_RESULT_TYPE type;

		/* Lowest detected round-trip time in milliseconds. */
		UINT32 baseRTT;
		/* Current average round-trip time in milliseconds. */
		UINT32 averageRTT;
		/* Current bandwidth in kilobits per second. */
		UINT32 bandwidth;
	};

	struct rdp_autodetect
	{
		ALIGN64 rdpContext* context; /* 0 */
		/* RTT measurement */
		ALIGN64 UINT64 rttMeasureStartTime; /* 1 */
		/* Bandwidth measurement */
		ALIGN64 UINT64 bandwidthMeasureStartTime; /* 2 */
		ALIGN64 UINT64 bandwidthMeasureTimeDelta; /* 3 */
		ALIGN64 UINT32 bandwidthMeasureByteCount; /* 4 */
		/* Network characteristics (as reported by server) */
		ALIGN64 UINT32 netCharBandwidth;        /* 5 */
		ALIGN64 UINT32 netCharBaseRTT;          /* 6 */
		ALIGN64 UINT32 netCharAverageRTT;       /* 7 */
		ALIGN64 BOOL bandwidthMeasureStarted;   /* 8 */
		ALIGN64 FREERDP_AUTODETECT_STATE state; /* 9 */
		ALIGN64 void* custom;                   /* 10 */
		ALIGN64 wLog* log;                      /* 11 */
		UINT64 paddingA[16 - 12];               /* 12 */

		ALIGN64 pRTTMeasureRequest RTTMeasureRequest;                       /* 16 */
		ALIGN64 pRTTMeasureResponse RTTMeasureResponse;                     /* 17 */
		ALIGN64 pBandwidthMeasureStart BandwidthMeasureStart;               /* 18 */
		ALIGN64 pBandwidthMeasurePayload BandwidthMeasurePayload;           /* 19 */
		ALIGN64 pBandwidthMeasureStop BandwidthMeasureStop;                 /* 20 */
		ALIGN64 pBandwidthMeasureResults BandwidthMeasureResults;           /* 21 */
		ALIGN64 pNetworkCharacteristicsResult NetworkCharacteristicsResult; /* 22 */
		ALIGN64 pClientBandwidthMeasureResult ClientBandwidthMeasureResult; /* 23 */
		ALIGN64 pNetworkCharacteristicsSync NetworkCharacteristicsSync;     /* 24 */
		ALIGN64 pRxTxReceived RequestReceived;                              /* 25 */
		ALIGN64 pRxTxReceived ResponseReceived;                             /* 26 */
		ALIGN64 pOnConnectTimeAutoDetect OnConnectTimeAutoDetectBegin;      /* 27 */
		ALIGN64 pOnConnectTimeAutoDetect OnConnectTimeAutoDetectProgress;   /* 28 */
		UINT64 paddingB[32 - 29];                                           /* 29 */
	};
	FREERDP_API rdpAutoDetect* autodetect_get(rdpContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_AUTODETECT_H */
