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

#include <freerdp/config.h>

#define WITH_DEBUG_HEARTBEAT

#include "heartbeat.h"

state_run_t rdp_recv_heartbeat_packet(rdpRdp* rdp, wStream* s)
{
	BYTE reserved;
	BYTE period;
	BYTE count1;
	BYTE count2;
	BOOL rc;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->context);
	WINPR_ASSERT(s);

	if (!Stream_CheckAndLogRequiredLength(AUTODETECT_TAG, s, 4))
		return STATE_RUN_FAILED;

	Stream_Read_UINT8(s, reserved); /* reserved (1 byte) */
	Stream_Read_UINT8(s, period);   /* period (1 byte) */
	Stream_Read_UINT8(s, count1);   /* count1 (1 byte) */
	Stream_Read_UINT8(s, count2);   /* count2 (1 byte) */

	WLog_DBG(HEARTBEAT_TAG,
	         "received Heartbeat PDU -> period=%" PRIu8 ", count1=%" PRIu8 ", count2=%" PRIu8 "",
	         period, count1, count2);

	rc = IFCALLRESULT(TRUE, rdp->heartbeat->ServerHeartbeat, rdp->context->instance, period, count1,
	                  count2);
	if (!rc)
	{
		WLog_ERR(HEARTBEAT_TAG, "heartbeat->ServerHeartbeat callback failed!");
		return STATE_RUN_FAILED;
	}

	return STATE_RUN_SUCCESS;
}

BOOL freerdp_heartbeat_send_heartbeat_pdu(freerdp_peer* peer, BYTE period, BYTE count1, BYTE count2)
{
	rdpRdp* rdp = peer->context->rdp;
	wStream* s = rdp_message_channel_pdu_init(rdp);

	if (!s)
		return FALSE;

	Stream_Seek_UINT8(s);          /* reserved (1 byte) */
	Stream_Write_UINT8(s, period); /* period (1 byte) */
	Stream_Write_UINT8(s, count1); /* count1 (1 byte) */
	Stream_Write_UINT8(s, count2); /* count2 (1 byte) */

	WLog_DBG(HEARTBEAT_TAG,
	         "sending Heartbeat PDU -> period=%" PRIu8 ", count1=%" PRIu8 ", count2=%" PRIu8 "",
	         period, count1, count2);

	if (!rdp_send_message_channel_pdu(rdp, s, SEC_HEARTBEAT))
		return FALSE;

	return TRUE;
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
