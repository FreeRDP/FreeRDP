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

typedef struct rdp_autodetect rdpAutoDetect;

typedef BOOL (*pRTTMeasureRequest)(rdpContext* context, UINT16 sequenceNumber);
typedef BOOL (*pRTTMeasureResponse)(rdpContext* context, UINT16 sequenceNumber);
typedef BOOL (*pBandwidthMeasureStart)(rdpContext* context, UINT16 sequenceNumber);
typedef BOOL (*pBandwidthMeasureStop)(rdpContext* context, UINT16 sequenceNumber);
typedef BOOL (*pBandwidthMeasureResults)(rdpContext* context, UINT16 sequenceNumber);
typedef BOOL (*pNetworkCharacteristicsResult)(rdpContext* context, UINT16 sequenceNumber);
typedef BOOL (*pClientBandwidthMeasureResult)(rdpContext* context, rdpAutoDetect* data);

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
	ALIGN64 UINT32 netCharBandwidth;      /* 5 */
	ALIGN64 UINT32 netCharBaseRTT;        /* 6 */
	ALIGN64 UINT32 netCharAverageRTT;     /* 7 */
	ALIGN64 BOOL bandwidthMeasureStarted; /* 8 */
	UINT64 paddingA[16 - 9];              /* 9 */

	ALIGN64 pRTTMeasureRequest RTTMeasureRequest;                       /* 16 */
	ALIGN64 pRTTMeasureResponse RTTMeasureResponse;                     /* 17 */
	ALIGN64 pBandwidthMeasureStart BandwidthMeasureStart;               /* 18 */
	ALIGN64 pBandwidthMeasureStop BandwidthMeasureStop;                 /* 19 */
	ALIGN64 pBandwidthMeasureResults BandwidthMeasureResults;           /* 20 */
	ALIGN64 pNetworkCharacteristicsResult NetworkCharacteristicsResult; /* 21 */
	ALIGN64 pClientBandwidthMeasureResult ClientBandwidthMeasureResult; /* 22 */
	UINT64 paddingB[32 - 23];                                           /* 23 */
};

#endif /* FREERDP_AUTODETECT_H */
