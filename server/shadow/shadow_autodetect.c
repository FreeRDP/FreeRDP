/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2026 Cong Zhang <congzhangzh@gmail.com>
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

#include <winpr/assert.h>
#include <winpr/sysinfo.h>

#include <freerdp/freerdp.h>
#include <freerdp/autodetect.h>
#include <freerdp/log.h>

#include "shadow.h"
#include "shadow_autodetect.h"

#define TAG SERVER_TAG("shadow.autodetect")

/* an RTT Measure Request (6 bytes) and a Network Characteristics Result (18 bytes) this often */
#define SHADOW_AUTODETECT_RTT_INTERVAL_MS 2000
/* an unanswered RTT Measure Request is given up after this long */
#define SHADOW_AUTODETECT_RTT_GIVEUP_MS 10000
/* the connect-time burst: this many Bandwidth Measure Payload messages and the Stop with one
 * more payload of this size, 32 KiB in all, before the licensing */
#define SHADOW_AUTODETECT_BW_PAYLOADS 3
#define SHADOW_AUTODETECT_BW_PAYLOAD_SIZE 8192

typedef enum
{
	SHADOW_AUTODETECT_PHASE_NONE,
	SHADOW_AUTODETECT_PHASE_CONNECT_RTT,
	SHADOW_AUTODETECT_PHASE_CONNECT_BW,
	SHADOW_AUTODETECT_PHASE_CONTINUOUS
} SHADOW_AUTODETECT_PHASE;

typedef struct
{
	rdpShadowClient* client;
	SHADOW_AUTODETECT_PHASE phase;
	UINT16 sequenceNumber;

	BOOL rttPending;
	UINT16 rttSequenceNumber;
	UINT64 rttSentNs;
	UINT64 rttSentMs;
	UINT64 nextRttMs;

	UINT64 rttSamples;
	UINT64 baseRttUs;
	UINT64 averageRttUs;

	BOOL bandwidthDone;
	UINT32 bandwidthKbps;
} ShadowAutoDetect;

static ShadowAutoDetect* shadow_autodetect_get(rdpAutoDetect* autodetect)
{
	WINPR_ASSERT(autodetect);
	return (ShadowAutoDetect*)autodetect->custom;
}

static BOOL shadow_autodetect_enabled(const ShadowAutoDetect* ad)
{
	WINPR_ASSERT(ad);
	WINPR_ASSERT(ad->client);
	return freerdp_settings_get_bool(ad->client->context.settings, FreeRDP_NetworkAutoDetect);
}

/* the Result carries milliseconds: anything measured is at least 1 ms, so that a fast link
 * does not read as not measured */
static UINT32 shadow_autodetect_us_to_ms(UINT64 us)
{
	const UINT64 ms = (us + 999) / 1000;
	return (UINT32)MIN(MAX(ms, 1), UINT32_MAX);
}

static BOOL shadow_autodetect_send_rtt(ShadowAutoDetect* ad, rdpAutoDetect* autodetect)
{
	WINPR_ASSERT(ad);
	WINPR_ASSERT(autodetect);

	if (!autodetect->RTTMeasureRequest)
		return FALSE;

	ad->rttSequenceNumber = ad->sequenceNumber++;
	ad->rttPending = TRUE;
	ad->rttSentMs = GetTickCount64();
	ad->rttSentNs = winpr_GetTickCount64NS();
	return autodetect->RTTMeasureRequest(autodetect, RDP_TRANSPORT_TCP, ad->rttSequenceNumber);
}

static BOOL shadow_autodetect_send_bandwidth_burst(ShadowAutoDetect* ad, rdpAutoDetect* autodetect)
{
	WINPR_ASSERT(ad);
	WINPR_ASSERT(autodetect);

	if (!autodetect->BandwidthMeasureStart || !autodetect->BandwidthMeasurePayload ||
	    !autodetect->BandwidthMeasureStop)
		return FALSE;

	const UINT16 sequenceNumber = ad->sequenceNumber++;
	if (!autodetect->BandwidthMeasureStart(autodetect, RDP_TRANSPORT_TCP, sequenceNumber))
		return FALSE;
	for (size_t x = 0; x < SHADOW_AUTODETECT_BW_PAYLOADS; x++)
	{
		if (!autodetect->BandwidthMeasurePayload(autodetect, RDP_TRANSPORT_TCP, sequenceNumber,
		                                         SHADOW_AUTODETECT_BW_PAYLOAD_SIZE))
			return FALSE;
	}
	return autodetect->BandwidthMeasureStop(autodetect, RDP_TRANSPORT_TCP, sequenceNumber,
	                                        SHADOW_AUTODETECT_BW_PAYLOAD_SIZE);
}

static BOOL shadow_autodetect_send_result(ShadowAutoDetect* ad, rdpAutoDetect* autodetect)
{
	WINPR_ASSERT(ad);
	WINPR_ASSERT(autodetect);

	if (!autodetect->NetworkCharacteristicsResult || (ad->rttSamples == 0))
		return TRUE;

	rdpNetworkCharacteristicsResult result = { .type = RDP_NETCHAR_RESULT_TYPE_BASE_RTT_AVG_RTT,
		                                       .baseRTT = shadow_autodetect_us_to_ms(ad->baseRttUs),
		                                       .averageRTT =
		                                           shadow_autodetect_us_to_ms(ad->averageRttUs),
		                                       .bandwidth = 0 };
	if (ad->bandwidthKbps > 0)
	{
		result.type = RDP_NETCHAR_RESULT_TYPE_BASE_RTT_BW_AVG_RTT;
		result.bandwidth = ad->bandwidthKbps;
	}

	WLog_DBG(TAG,
	         "Network Characteristics Result: base RTT %" PRIu32 " ms, average RTT %" PRIu32
	         " ms, bandwidth %" PRIu32 " kbit/s",
	         result.baseRTT, result.averageRTT, result.bandwidth);
	return autodetect->NetworkCharacteristicsResult(autodetect, RDP_TRANSPORT_TCP,
	                                                ad->sequenceNumber++, &result);
}

static BOOL shadow_autodetect_rtt_response(rdpAutoDetect* autodetect,
                                           WINPR_ATTR_UNUSED RDP_TRANSPORT_TYPE transport,
                                           UINT16 sequenceNumber)
{
	ShadowAutoDetect* ad = shadow_autodetect_get(autodetect);
	if (!ad || !ad->rttPending || (sequenceNumber != ad->rttSequenceNumber))
		return TRUE;

	ad->rttPending = FALSE;
	const UINT64 now = winpr_GetTickCount64NS();
	const UINT64 sample = (now > ad->rttSentNs) ? (now - ad->rttSentNs) / 1000 : 0;

	/* the lowest one seen, and a running average as TCP's smoothed RTT (RFC 6298, 1/8) */
	if ((ad->rttSamples == 0) || (sample < ad->baseRttUs))
		ad->baseRttUs = sample;
	if (ad->rttSamples == 0)
		ad->averageRttUs = sample;
	else
		ad->averageRttUs = (ad->averageRttUs * 7 + sample) / 8;
	ad->rttSamples++;

	WLog_DBG(TAG, "RTT %" PRIu64 " us (base %" PRIu64 " us, average %" PRIu64 " us)", sample,
	         ad->baseRttUs, ad->averageRttUs);

	/* at connect time the progress callback sends the next step */
	if (ad->phase == SHADOW_AUTODETECT_PHASE_CONTINUOUS)
		return shadow_autodetect_send_result(ad, autodetect);
	return TRUE;
}

static BOOL shadow_autodetect_bandwidth_results(rdpAutoDetect* autodetect,
                                                WINPR_ATTR_UNUSED RDP_TRANSPORT_TYPE transport,
                                                WINPR_ATTR_UNUSED UINT16 sequenceNumber,
                                                UINT16 responseType, UINT32 timeDelta,
                                                UINT32 byteCount)
{
	ShadowAutoDetect* ad = shadow_autodetect_get(autodetect);
	if (!ad || (responseType != RDP_BW_RESULTS_RESPONSE_TYPE_CONNECTTIME))
		return TRUE;

	/* bits per millisecond are kbit/s; a burst that came in less than a millisecond is counted
	 * as 1 ms, so the bandwidth is then at least the value shown (262 Mbit/s for 32 KiB) */
	const UINT64 kbps = (8ull * byteCount) / MAX(timeDelta, 1);
	ad->bandwidthKbps = (UINT32)MIN(MAX(kbps, 1), UINT32_MAX);
	ad->bandwidthDone = TRUE;
	WLog_INFO(TAG, "bandwidth %" PRIu32 " kbit/s (%" PRIu32 " bytes in %" PRIu32 " ms)",
	          ad->bandwidthKbps, byteCount, timeDelta);
	return TRUE;
}

/* an auto-reconnect may answer the burst with the values the client had before */
static BOOL shadow_autodetect_netchar_sync(rdpAutoDetect* autodetect,
                                           WINPR_ATTR_UNUSED RDP_TRANSPORT_TYPE transport,
                                           WINPR_ATTR_UNUSED UINT16 sequenceNumber,
                                           UINT32 bandwidth, UINT32 rtt)
{
	ShadowAutoDetect* ad = shadow_autodetect_get(autodetect);
	if (!ad)
		return TRUE;

	ad->bandwidthKbps = bandwidth;
	ad->bandwidthDone = TRUE;
	if ((ad->rttSamples == 0) && (rtt > 0))
	{
		ad->baseRttUs = ad->averageRttUs = 1000ull * rtt;
		ad->rttSamples = 1;
	}
	WLog_INFO(TAG, "bandwidth %" PRIu32 " kbit/s, RTT %" PRIu32 " ms (from the client)", bandwidth,
	          rtt);
	return TRUE;
}

static FREERDP_AUTODETECT_STATE shadow_autodetect_connect_time_begin(rdpAutoDetect* autodetect)
{
	ShadowAutoDetect* ad = shadow_autodetect_get(autodetect);
	WINPR_ASSERT(ad);

	ad->phase = SHADOW_AUTODETECT_PHASE_CONNECT_RTT;
	if (!shadow_autodetect_send_rtt(ad, autodetect))
		return FREERDP_AUTODETECT_STATE_FAIL;
	return FREERDP_AUTODETECT_STATE_REQUEST;
}

static FREERDP_AUTODETECT_STATE shadow_autodetect_connect_time_progress(rdpAutoDetect* autodetect)
{
	ShadowAutoDetect* ad = shadow_autodetect_get(autodetect);
	WINPR_ASSERT(ad);

	if (autodetect->state == FREERDP_AUTODETECT_STATE_FAIL)
		return FREERDP_AUTODETECT_STATE_FAIL;

	switch (ad->phase)
	{
		case SHADOW_AUTODETECT_PHASE_CONNECT_RTT:
			if (ad->rttPending)
				return FREERDP_AUTODETECT_STATE_REQUEST;
			ad->phase = SHADOW_AUTODETECT_PHASE_CONNECT_BW;
			if (!shadow_autodetect_send_bandwidth_burst(ad, autodetect))
				return FREERDP_AUTODETECT_STATE_FAIL;
			return FREERDP_AUTODETECT_STATE_REQUEST;

		case SHADOW_AUTODETECT_PHASE_CONNECT_BW:
			if (!ad->bandwidthDone)
				return FREERDP_AUTODETECT_STATE_REQUEST;
			if (!shadow_autodetect_send_result(ad, autodetect))
				return FREERDP_AUTODETECT_STATE_FAIL;
			ad->phase = SHADOW_AUTODETECT_PHASE_CONTINUOUS;
			ad->nextRttMs = GetTickCount64() + SHADOW_AUTODETECT_RTT_INTERVAL_MS;
			return FREERDP_AUTODETECT_STATE_COMPLETE;

		default:
			return FREERDP_AUTODETECT_STATE_COMPLETE;
	}
}

BOOL shadow_client_autodetect_init(rdpShadowClient* client)
{
	WINPR_ASSERT(client);

	rdpAutoDetect* autodetect = client->context.autodetect;
	WINPR_ASSERT(autodetect);
	WINPR_ASSERT(!autodetect->custom);

	ShadowAutoDetect* ad = calloc(1, sizeof(ShadowAutoDetect));
	if (!ad)
		return FALSE;
	ad->client = client;
	autodetect->custom = ad;

	autodetect->RTTMeasureResponse = shadow_autodetect_rtt_response;
	autodetect->BandwidthMeasureResults = shadow_autodetect_bandwidth_results;
	autodetect->NetworkCharacteristicsSync = shadow_autodetect_netchar_sync;
	autodetect->OnConnectTimeAutoDetectBegin = shadow_autodetect_connect_time_begin;
	autodetect->OnConnectTimeAutoDetectProgress = shadow_autodetect_connect_time_progress;
	return TRUE;
}

void shadow_client_autodetect_uninit(rdpShadowClient* client)
{
	if (!client)
		return;

	rdpAutoDetect* autodetect = client->context.autodetect;
	if (!autodetect)
		return;

	ShadowAutoDetect* ad = shadow_autodetect_get(autodetect);
	if (!ad)
		return;
	if (ad->rttSamples > 0)
		WLog_DBG(TAG, "%" PRIu64 " RTT measurements", ad->rttSamples);
	autodetect->custom = nullptr;
	free(ad);
}

DWORD shadow_client_autodetect_timeout(rdpShadowClient* client)
{
	WINPR_ASSERT(client);

	rdpAutoDetect* autodetect = client->context.autodetect;
	if (!autodetect)
		return INFINITE;

	ShadowAutoDetect* ad = shadow_autodetect_get(autodetect);
	if (!ad || (ad->phase != SHADOW_AUTODETECT_PHASE_CONTINUOUS) || !shadow_autodetect_enabled(ad))
		return INFINITE;

	const UINT64 now = GetTickCount64();
	if (now >= ad->nextRttMs)
		return 0;
	return (DWORD)MIN(ad->nextRttMs - now, SHADOW_AUTODETECT_RTT_GIVEUP_MS);
}

BOOL shadow_client_autodetect_poll(rdpShadowClient* client)
{
	WINPR_ASSERT(client);

	rdpAutoDetect* autodetect = client->context.autodetect;
	if (!autodetect)
		return TRUE;

	ShadowAutoDetect* ad = shadow_autodetect_get(autodetect);
	if (!ad || (ad->phase != SHADOW_AUTODETECT_PHASE_CONTINUOUS) || !shadow_autodetect_enabled(ad))
		return TRUE;

	const UINT64 now = GetTickCount64();
	if (now < ad->nextRttMs)
		return TRUE;

	/* not while a resize reactivates the session: try again an interval later, so that the
	 * timeout does not stay 0 and the client thread does not spin until it is active */
	if (freerdp_get_state(&client->context) != CONNECTION_STATE_ACTIVE)
	{
		ad->nextRttMs = now + SHADOW_AUTODETECT_RTT_INTERVAL_MS;
		return TRUE;
	}

	if (ad->rttPending && (now - ad->rttSentMs < SHADOW_AUTODETECT_RTT_GIVEUP_MS))
	{
		ad->nextRttMs = ad->rttSentMs + SHADOW_AUTODETECT_RTT_GIVEUP_MS;
		return TRUE;
	}

	ad->nextRttMs = now + SHADOW_AUTODETECT_RTT_INTERVAL_MS;
	return shadow_autodetect_send_rtt(ad, autodetect);
}
