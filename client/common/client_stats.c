/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Runtime statistics of a client session
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
#include <winpr/crt.h>

#include <freerdp/autodetect.h>
#include <freerdp/client.h>
#include <freerdp/freerdp.h>

#if defined(CHANNEL_RDPGFX_CLIENT)
#include <freerdp/client/rdpgfx.h>
#include <freerdp/gdi/gdi.h>
#endif

/* Nothing is counted here: this reads what the libraries count anyway, so no path that carries
 * data gets slower. */
enum
{
	STATS_BYTES_RECEIVED,
	STATS_BYTES_SENT,
	STATS_PACKETS_RECEIVED,
	STATS_PACKETS_SENT,
	STATS_BANDWIDTH_KBPS,
	STATS_RTT_BASE_MS,
	STATS_RTT_AVERAGE_MS,
	STATS_NETWORK_MAX
};

static const char* const networkNames[] = { "NETWORK_BYTES_RECEIVED",   "NETWORK_BYTES_SENT",
	                                        "NETWORK_PACKETS_RECEIVED", "NETWORK_PACKETS_SENT",
	                                        "NETWORK_BANDWIDTH_KBPS",   "NETWORK_RTT_BASE_MS",
	                                        "NETWORK_RTT_AVERAGE_MS" };

static uint64_t network_value(const rdpContext* context, size_t index)
{
	if (index <= STATS_PACKETS_SENT)
	{
		UINT64 values[4] = WINPR_C_ARRAY_INIT;
		if (!freerdp_get_stats(context->rdp, &values[STATS_BYTES_RECEIVED],
		                       &values[STATS_BYTES_SENT], &values[STATS_PACKETS_RECEIVED],
		                       &values[STATS_PACKETS_SENT]))
			return 0;
		return values[index];
	}

	const rdpAutoDetect* autodetect = context->autodetect;
	if (!autodetect)
		return 0;

	switch (index)
	{
		case STATS_BANDWIDTH_KBPS:
			return autodetect->netCharBandwidth;
		case STATS_RTT_BASE_MS:
			return autodetect->netCharBaseRTT;
		case STATS_RTT_AVERAGE_MS:
			return autodetect->netCharAverageRTT;
		default:
			return 0;
	}
}

size_t freerdp_client_stats_max_index(void)
{
	/* in a function: without C11 this is an assert(), which a file scope does not take */
	WINPR_STATIC_ASSERT(ARRAYSIZE(networkNames) == STATS_NETWORK_MAX);

#if defined(CHANNEL_RDPGFX_CLIENT)
	return STATS_NETWORK_MAX + rdpgfx_stats_max_index();
#else
	return STATS_NETWORK_MAX;
#endif
}

const char* freerdp_client_stats_name_for_index(size_t index)
{
	if (index < STATS_NETWORK_MAX)
		return networkNames[index];

#if defined(CHANNEL_RDPGFX_CLIENT)
	if (index < freerdp_client_stats_max_index())
		return rdpgfx_stats_name_for_index(index - STATS_NETWORK_MAX);
#endif
	return "FREERDP_CLIENT_STATS_UNUSED";
}

uint64_t freerdp_client_stats_value_for_index(const rdpContext* context, size_t index)
{
	if (!context)
		return 0;

	if (index < STATS_NETWORK_MAX)
		return network_value(context, index);

#if defined(CHANNEL_RDPGFX_CLIENT)
	if ((index < freerdp_client_stats_max_index()) && context->gdi)
	{
		/* once: another thread can clear it at any time */
		RdpgfxClientContext* gfx = context->gdi->gfx;
		if (gfx)
			return rdpgfx_stats_value_for_index(gfx, index - STATS_NETWORK_MAX);
	}
#endif
	return 0;
}
