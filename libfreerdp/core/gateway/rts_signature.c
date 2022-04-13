/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Request To Send (RTS) PDU Signatures
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/assert.h>
#include <winpr/stream.h>

#include <freerdp/log.h>

#include "rts_signature.h"

#define TAG FREERDP_TAG("core.gateway.rts")

const RtsPduSignature RTS_PDU_CONN_A1_SIGNATURE = {
	RTS_FLAG_NONE,
	4,
	{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_RECEIVE_WINDOW_SIZE, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_CONN_A2_SIGNATURE = { RTS_FLAG_OUT_CHANNEL,
	                                                5,
	                                                { RTS_CMD_VERSION, RTS_CMD_COOKIE,
	                                                  RTS_CMD_COOKIE, RTS_CMD_CHANNEL_LIFETIME,
	                                                  RTS_CMD_RECEIVE_WINDOW_SIZE, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_CONN_A3_SIGNATURE = {
	RTS_FLAG_NONE, 1, { RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0, 0, 0, 0 }
};

const RtsPduSignature RTS_PDU_CONN_B1_SIGNATURE = {
	RTS_FLAG_NONE,
	6,
	{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_CHANNEL_LIFETIME,
	  RTS_CMD_CLIENT_KEEPALIVE, RTS_CMD_ASSOCIATION_GROUP_ID, 0, 0 }
};
const RtsPduSignature RTS_PDU_CONN_B2_SIGNATURE = {
	RTS_FLAG_IN_CHANNEL,
	7,
	{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_RECEIVE_WINDOW_SIZE,
	  RTS_CMD_CONNECTION_TIMEOUT, RTS_CMD_ASSOCIATION_GROUP_ID, RTS_CMD_CLIENT_ADDRESS, 0 }
};
const RtsPduSignature RTS_PDU_CONN_B3_SIGNATURE = {
	RTS_FLAG_NONE, 2, { RTS_CMD_RECEIVE_WINDOW_SIZE, RTS_CMD_VERSION, 0, 0, 0, 0, 0, 0 }
};

const RtsPduSignature RTS_PDU_CONN_C1_SIGNATURE = { RTS_FLAG_NONE,
	                                                3,
	                                                { RTS_CMD_VERSION, RTS_CMD_RECEIVE_WINDOW_SIZE,
	                                                  RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_CONN_C2_SIGNATURE = { RTS_FLAG_NONE,
	                                                3,
	                                                { RTS_CMD_VERSION, RTS_CMD_RECEIVE_WINDOW_SIZE,
	                                                  RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0, 0 } };

const RtsPduSignature RTS_PDU_IN_R1_A1_SIGNATURE = {
	RTS_FLAG_RECYCLE_CHANNEL,
	4,
	{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_COOKIE, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_IN_R1_A2_SIGNATURE = {
	RTS_FLAG_NONE,
	4,
	{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_RECEIVE_WINDOW_SIZE,
	  RTS_CMD_CONNECTION_TIMEOUT, 0, 0 }
};
const RtsPduSignature RTS_PDU_IN_R1_A3_SIGNATURE = { RTS_FLAG_NONE,
	                                                 4,
	                                                 { RTS_CMD_DESTINATION, RTS_CMD_VERSION,
	                                                   RTS_CMD_RECEIVE_WINDOW_SIZE,
	                                                   RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_IN_R1_A4_SIGNATURE = { RTS_FLAG_NONE,
	                                                 4,
	                                                 { RTS_CMD_DESTINATION, RTS_CMD_VERSION,
	                                                   RTS_CMD_RECEIVE_WINDOW_SIZE,
	                                                   RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_IN_R1_A5_SIGNATURE = { RTS_FLAG_NONE,
	                                                 1,
	                                                 { RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_IN_R1_A6_SIGNATURE = { RTS_FLAG_NONE,
	                                                 1,
	                                                 { RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0, 0 } };

const RtsPduSignature RTS_PDU_IN_R1_B1_SIGNATURE = { RTS_FLAG_NONE,
	                                                 1,
	                                                 { RTS_CMD_EMPTY, 0, 0, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_IN_R1_B2_SIGNATURE = {
	RTS_FLAG_NONE, 1, { RTS_CMD_RECEIVE_WINDOW_SIZE, 0, 0, 0, 0, 0, 0, 0 }
};

const RtsPduSignature RTS_PDU_IN_R2_A1_SIGNATURE = {
	RTS_FLAG_RECYCLE_CHANNEL,
	4,
	{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_COOKIE, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_IN_R2_A2_SIGNATURE = { RTS_FLAG_NONE,
	                                                 1,
	                                                 { RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_IN_R2_A3_SIGNATURE = { RTS_FLAG_NONE,
	                                                 1,
	                                                 { RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_IN_R2_A4_SIGNATURE = { RTS_FLAG_NONE,
	                                                 1,
	                                                 { RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_IN_R2_A5_SIGNATURE = { RTS_FLAG_NONE,
	                                                 1,
	                                                 { RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0, 0 } };

const RtsPduSignature RTS_PDU_OUT_R1_A1_SIGNATURE = {
	RTS_FLAG_RECYCLE_CHANNEL, 1, { RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R1_A2_SIGNATURE = {
	RTS_FLAG_RECYCLE_CHANNEL, 1, { RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R1_A3_SIGNATURE = { RTS_FLAG_RECYCLE_CHANNEL,
	                                                  5,
	                                                  { RTS_CMD_VERSION, RTS_CMD_COOKIE,
	                                                    RTS_CMD_COOKIE, RTS_CMD_COOKIE,
	                                                    RTS_CMD_RECEIVE_WINDOW_SIZE, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_OUT_R1_A4_SIGNATURE = {
	RTS_FLAG_RECYCLE_CHANNEL | RTS_FLAG_OUT_CHANNEL,
	7,
	{ RTS_CMD_VERSION, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_COOKIE, RTS_CMD_CHANNEL_LIFETIME,
	  RTS_CMD_RECEIVE_WINDOW_SIZE, RTS_CMD_CONNECTION_TIMEOUT, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R1_A5_SIGNATURE = {
	RTS_FLAG_OUT_CHANNEL,
	3,
	{ RTS_CMD_DESTINATION, RTS_CMD_VERSION, RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R1_A6_SIGNATURE = {
	RTS_FLAG_OUT_CHANNEL,
	3,
	{ RTS_CMD_DESTINATION, RTS_CMD_VERSION, RTS_CMD_CONNECTION_TIMEOUT, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R1_A7_SIGNATURE = {
	RTS_FLAG_OUT_CHANNEL, 2, { RTS_CMD_DESTINATION, RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R1_A8_SIGNATURE = {
	RTS_FLAG_OUT_CHANNEL, 2, { RTS_CMD_DESTINATION, RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R1_A9_SIGNATURE = { RTS_FLAG_NONE,
	                                                  1,
	                                                  { RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_OUT_R1_A10_SIGNATURE = { RTS_FLAG_NONE,
	                                                   1,
	                                                   { RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_OUT_R1_A11_SIGNATURE = { RTS_FLAG_NONE,
	                                                   1,
	                                                   { RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0, 0 } };

const RtsPduSignature RTS_PDU_OUT_R2_A1_SIGNATURE = {
	RTS_FLAG_RECYCLE_CHANNEL, 1, { RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R2_A2_SIGNATURE = {
	RTS_FLAG_RECYCLE_CHANNEL, 1, { RTS_CMD_DESTINATION, 0, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R2_A3_SIGNATURE = { RTS_FLAG_RECYCLE_CHANNEL,
	                                                  5,
	                                                  { RTS_CMD_VERSION, RTS_CMD_COOKIE,
	                                                    RTS_CMD_COOKIE, RTS_CMD_COOKIE,
	                                                    RTS_CMD_RECEIVE_WINDOW_SIZE, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_OUT_R2_A4_SIGNATURE = { RTS_FLAG_NONE,
	                                                  1,
	                                                  { RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_OUT_R2_A5_SIGNATURE = {
	RTS_FLAG_NONE, 2, { RTS_CMD_DESTINATION, RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R2_A6_SIGNATURE = {
	RTS_FLAG_NONE, 2, { RTS_CMD_DESTINATION, RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R2_A7_SIGNATURE = {
	RTS_FLAG_NONE, 3, { RTS_CMD_DESTINATION, RTS_CMD_COOKIE, RTS_CMD_VERSION, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R2_A8_SIGNATURE = {
	RTS_FLAG_OUT_CHANNEL, 2, { RTS_CMD_DESTINATION, RTS_CMD_COOKIE, 0, 0, 0, 0, 0, 0 }
};

const RtsPduSignature RTS_PDU_OUT_R2_B1_SIGNATURE = { RTS_FLAG_NONE,
	                                                  1,
	                                                  { RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_OUT_R2_B2_SIGNATURE = {
	RTS_FLAG_NONE, 1, { RTS_CMD_NEGATIVE_ANCE, 0, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_OUT_R2_B3_SIGNATURE = { RTS_FLAG_EOF,
	                                                  1,
	                                                  { RTS_CMD_ANCE, 0, 0, 0, 0, 0, 0, 0 } };

const RtsPduSignature RTS_PDU_OUT_R2_C1_SIGNATURE = { RTS_FLAG_PING,
	                                                  1,
	                                                  { 0, 0, 0, 0, 0, 0, 0, 0 } };

const RtsPduSignature RTS_PDU_KEEP_ALIVE_SIGNATURE = {
	RTS_FLAG_OTHER_CMD, 1, { RTS_CMD_CLIENT_KEEPALIVE, 0, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_PING_TRAFFIC_SENT_NOTIFY_SIGNATURE = {
	RTS_FLAG_OTHER_CMD, 1, { RTS_CMD_PING_TRAFFIC_SENT_NOTIFY, 0, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_ECHO_SIGNATURE = { RTS_FLAG_ECHO, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_PING_SIGNATURE = { RTS_FLAG_PING, 0, { 0, 0, 0, 0, 0, 0, 0, 0 } };
const RtsPduSignature RTS_PDU_FLOW_CONTROL_ACK_SIGNATURE = {
	RTS_FLAG_OTHER_CMD, 1, { RTS_CMD_FLOW_CONTROL_ACK, 0, 0, 0, 0, 0, 0, 0 }
};
const RtsPduSignature RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION_SIGNATURE = {
	RTS_FLAG_OTHER_CMD, 2, { RTS_CMD_DESTINATION, RTS_CMD_FLOW_CONTROL_ACK, 0, 0, 0, 0, 0, 0 }
};

static const RTS_PDU_SIGNATURE_ENTRY RTS_PDU_SIGNATURE_TABLE[] = {
	{ RTS_PDU_CONN_A1, FALSE, &RTS_PDU_CONN_A1_SIGNATURE, "CONN/A1" },
	{ RTS_PDU_CONN_A2, FALSE, &RTS_PDU_CONN_A2_SIGNATURE, "CONN/A2" },
	{ RTS_PDU_CONN_A3, TRUE, &RTS_PDU_CONN_A3_SIGNATURE, "CONN/A3" },

	{ RTS_PDU_CONN_B1, FALSE, &RTS_PDU_CONN_B1_SIGNATURE, "CONN/B1" },
	{ RTS_PDU_CONN_B2, FALSE, &RTS_PDU_CONN_B2_SIGNATURE, "CONN/B2" },
	{ RTS_PDU_CONN_B3, FALSE, &RTS_PDU_CONN_B3_SIGNATURE, "CONN/B3" },

	{ RTS_PDU_CONN_C1, FALSE, &RTS_PDU_CONN_C1_SIGNATURE, "CONN/C1" },
	{ RTS_PDU_CONN_C2, TRUE, &RTS_PDU_CONN_C2_SIGNATURE, "CONN/C2" },

	{ RTS_PDU_IN_R1_A1, FALSE, &RTS_PDU_IN_R1_A1_SIGNATURE, "IN_R1/A1" },
	{ RTS_PDU_IN_R1_A2, FALSE, &RTS_PDU_IN_R1_A2_SIGNATURE, "IN_R1/A2" },
	{ RTS_PDU_IN_R1_A3, FALSE, &RTS_PDU_IN_R1_A3_SIGNATURE, "IN_R1/A3" },
	{ RTS_PDU_IN_R1_A4, TRUE, &RTS_PDU_IN_R1_A4_SIGNATURE, "IN_R1/A4" },
	{ RTS_PDU_IN_R1_A5, TRUE, &RTS_PDU_IN_R1_A5_SIGNATURE, "IN_R1/A5" },
	{ RTS_PDU_IN_R1_A6, FALSE, &RTS_PDU_IN_R1_A6_SIGNATURE, "IN_R1/A6" },

	{ RTS_PDU_IN_R1_B1, FALSE, &RTS_PDU_IN_R1_B1_SIGNATURE, "IN_R1/B1" },
	{ RTS_PDU_IN_R1_B2, FALSE, &RTS_PDU_IN_R1_B2_SIGNATURE, "IN_R1/B2" },

	{ RTS_PDU_IN_R2_A1, FALSE, &RTS_PDU_IN_R2_A1_SIGNATURE, "IN_R2/A1" },
	{ RTS_PDU_IN_R2_A2, FALSE, &RTS_PDU_IN_R2_A2_SIGNATURE, "IN_R2/A2" },
	{ RTS_PDU_IN_R2_A3, FALSE, &RTS_PDU_IN_R2_A3_SIGNATURE, "IN_R2/A3" },
	{ RTS_PDU_IN_R2_A4, TRUE, &RTS_PDU_IN_R2_A4_SIGNATURE, "IN_R2/A4" },
	{ RTS_PDU_IN_R2_A5, FALSE, &RTS_PDU_IN_R2_A5_SIGNATURE, "IN_R2/A5" },

	{ RTS_PDU_OUT_R1_A1, FALSE, &RTS_PDU_OUT_R1_A1_SIGNATURE, "OUT_R1/A1" },
	{ RTS_PDU_OUT_R1_A2, TRUE, &RTS_PDU_OUT_R1_A2_SIGNATURE, "OUT_R1/A2" },
	{ RTS_PDU_OUT_R1_A3, FALSE, &RTS_PDU_OUT_R1_A3_SIGNATURE, "OUT_R1/A3" },
	{ RTS_PDU_OUT_R1_A4, FALSE, &RTS_PDU_OUT_R1_A4_SIGNATURE, "OUT_R1/A4" },
	{ RTS_PDU_OUT_R1_A5, FALSE, &RTS_PDU_OUT_R1_A5_SIGNATURE, "OUT_R1/A5" },
	{ RTS_PDU_OUT_R1_A6, TRUE, &RTS_PDU_OUT_R1_A6_SIGNATURE, "OUT_R1/A6" },
	{ RTS_PDU_OUT_R1_A7, FALSE, &RTS_PDU_OUT_R1_A7_SIGNATURE, "OUT_R1/A7" },
	{ RTS_PDU_OUT_R1_A8, FALSE, &RTS_PDU_OUT_R1_A8_SIGNATURE, "OUT_R1/A8" },
	{ RTS_PDU_OUT_R1_A9, FALSE, &RTS_PDU_OUT_R1_A9_SIGNATURE, "OUT_R1/A9" },
	{ RTS_PDU_OUT_R1_A10, TRUE, &RTS_PDU_OUT_R1_A10_SIGNATURE, "OUT_R1/A10" },
	{ RTS_PDU_OUT_R1_A11, FALSE, &RTS_PDU_OUT_R1_A11_SIGNATURE, "OUT_R1/A11" },

	{ RTS_PDU_OUT_R2_A1, FALSE, &RTS_PDU_OUT_R2_A1_SIGNATURE, "OUT_R2/A1" },
	{ RTS_PDU_OUT_R2_A2, TRUE, &RTS_PDU_OUT_R2_A2_SIGNATURE, "OUT_R2/A2" },
	{ RTS_PDU_OUT_R2_A3, FALSE, &RTS_PDU_OUT_R2_A3_SIGNATURE, "OUT_R2/A3" },
	{ RTS_PDU_OUT_R2_A4, FALSE, &RTS_PDU_OUT_R2_A4_SIGNATURE, "OUT_R2/A4" },
	{ RTS_PDU_OUT_R2_A5, FALSE, &RTS_PDU_OUT_R2_A5_SIGNATURE, "OUT_R2/A5" },
	{ RTS_PDU_OUT_R2_A6, TRUE, &RTS_PDU_OUT_R2_A6_SIGNATURE, "OUT_R2/A6" },
	{ RTS_PDU_OUT_R2_A7, FALSE, &RTS_PDU_OUT_R2_A7_SIGNATURE, "OUT_R2/A7" },
	{ RTS_PDU_OUT_R2_A8, FALSE, &RTS_PDU_OUT_R2_A8_SIGNATURE, "OUT_R2/A8" },

	{ RTS_PDU_OUT_R2_B1, FALSE, &RTS_PDU_OUT_R2_B1_SIGNATURE, "OUT_R2/B1" },
	{ RTS_PDU_OUT_R2_B2, FALSE, &RTS_PDU_OUT_R2_B2_SIGNATURE, "OUT_R2/B2" },
	{ RTS_PDU_OUT_R2_B3, TRUE, &RTS_PDU_OUT_R2_B3_SIGNATURE, "OUT_R2/B3" },

	{ RTS_PDU_OUT_R2_C1, FALSE, &RTS_PDU_OUT_R2_C1_SIGNATURE, "OUT_R2/C1" },

	{ RTS_PDU_KEEP_ALIVE, TRUE, &RTS_PDU_KEEP_ALIVE_SIGNATURE, "Keep-Alive" },
	{ RTS_PDU_PING_TRAFFIC_SENT_NOTIFY, TRUE, &RTS_PDU_PING_TRAFFIC_SENT_NOTIFY_SIGNATURE,
	  "Ping Traffic Sent Notify" },
	{ RTS_PDU_ECHO, TRUE, &RTS_PDU_ECHO_SIGNATURE, "Echo" },
	{ RTS_PDU_PING, TRUE, &RTS_PDU_PING_SIGNATURE, "Ping" },
	{ RTS_PDU_FLOW_CONTROL_ACK, TRUE, &RTS_PDU_FLOW_CONTROL_ACK_SIGNATURE, "FlowControlAck" },
	{ RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION, TRUE,
	  &RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION_SIGNATURE, "FlowControlAckWithDestination" }
};

BOOL rts_match_pdu_signature(const RtsPduSignature* signature, wStream* src,
                             const rpcconn_hdr_t* header)
{
	RtsPduSignature extracted = { 0 };

	WINPR_ASSERT(signature);
	WINPR_ASSERT(src);

	if (!rts_extract_pdu_signature(&extracted, src, header))
		return FALSE;

	return memcmp(signature, &extracted, sizeof(extracted)) == 0;
}

BOOL rts_extract_pdu_signature(RtsPduSignature* signature, wStream* src,
                               const rpcconn_hdr_t* header)
{
	BOOL rc = FALSE;
	UINT16 i;
	wStream tmp;
	rpcconn_hdr_t rheader = { 0 };
	const rpcconn_rts_hdr_t* rts;

	WINPR_ASSERT(signature);
	WINPR_ASSERT(src);

	Stream_StaticInit(&tmp, Stream_Pointer(src), Stream_GetRemainingLength(src));
	if (!header)
	{
		if (!rts_read_pdu_header(&tmp, &rheader))
			goto fail;
		header = &rheader;
	}
	rts = &header->rts;
	if (rts->header.frag_length < sizeof(rpcconn_rts_hdr_t))
		goto fail;

	signature->Flags = rts->Flags;
	signature->NumberOfCommands = rts->NumberOfCommands;

	for (i = 0; i < rts->NumberOfCommands; i++)
	{
		UINT32 CommandType;
		size_t CommandLength;

		if (!Stream_CheckAndLogRequiredLength(TAG, &tmp, 4))
			goto fail;

		Stream_Read_UINT32(&tmp, CommandType); /* CommandType (4 bytes) */

		/* We only need this for comparison against known command types */
		if (i < ARRAYSIZE(signature->CommandTypes))
			signature->CommandTypes[i] = CommandType;

		if (!rts_command_length(CommandType, &tmp, &CommandLength))
			goto fail;
		if (!Stream_SafeSeek(&tmp, CommandLength))
			goto fail;
	}

	rc = TRUE;
fail:
	rts_free_pdu_header(&rheader, FALSE);
	Stream_Free(&tmp, FALSE);
	return rc;
}

UINT32 rts_identify_pdu_signature(const RtsPduSignature* signature,
                                  const RTS_PDU_SIGNATURE_ENTRY** entry)
{
	size_t i, j;

	if (entry)
		*entry = NULL;

	for (i = 0; i < ARRAYSIZE(RTS_PDU_SIGNATURE_TABLE); i++)
	{
		const RTS_PDU_SIGNATURE_ENTRY* current = &RTS_PDU_SIGNATURE_TABLE[i];
		const RtsPduSignature* pSignature = current->Signature;

		if (!current->SignatureClient)
			continue;

		if (signature->Flags != pSignature->Flags)
			continue;

		if (signature->NumberOfCommands != pSignature->NumberOfCommands)
			continue;

		for (j = 0; j < signature->NumberOfCommands; j++)
		{
			if (signature->CommandTypes[j] != pSignature->CommandTypes[j])
				continue;
		}

		if (entry)
			*entry = current;

		return current->SignatureId;
	}

	return 0;
}

BOOL rts_print_pdu_signature(const RtsPduSignature* signature)
{
	UINT32 SignatureId;
	const RTS_PDU_SIGNATURE_ENTRY* entry;

	if (!signature)
		return FALSE;

	WLog_INFO(TAG, "RTS PDU Signature: Flags: 0x%04" PRIX16 " NumberOfCommands: %" PRIu16 "",
	          signature->Flags, signature->NumberOfCommands);
	SignatureId = rts_identify_pdu_signature(signature, &entry);

	if (SignatureId)
		WLog_ERR(TAG, "Identified %s RTS PDU", entry->PduName);

	return TRUE;
}
