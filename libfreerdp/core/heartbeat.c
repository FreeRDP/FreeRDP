/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Heartbeat PDUs
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

#define WITH_DEBUG_HEARTBEAT

#include "heartbeat.h"

int rdp_recv_heartbeat_packet(rdpRdp* rdp, wStream* s)
{
	BYTE reserved;
	BYTE period;
	BYTE count1;
	BYTE count2;

	if (Stream_GetRemainingLength(s) < 4)
		return -1;

	Stream_Read_UINT8(s, reserved); /* reserved (1 byte) */
	Stream_Read_UINT8(s, period);   /* period (1 byte) */
	Stream_Read_UINT8(s, count1);   /* count1 (1 byte) */
	Stream_Read_UINT8(s, count2);   /* count2 (1 byte) */

	WLog_DBG(HEARTBEAT_TAG,
	         "received Heartbeat PDU -> period=%" PRIu8 ", count1=%" PRIu8 ", count2=%" PRIu8 "",
	         period, count1, count2);

	return 0;
}

rdpHeartbeat* heartbeat_new(void)
{
	rdpHeartbeat* heartbeat = (rdpHeartbeat*)calloc(1, sizeof(rdpHeartbeat));

	if (heartbeat)
	{
	}

	return heartbeat;
}

void heartbeat_free(rdpHeartbeat* heartbeat)
{
	free(heartbeat);
}
