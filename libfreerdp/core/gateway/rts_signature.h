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

#ifndef FREERDP_CORE_RTS_SIGNATURE_H
#define FREERDP_CORE_RTS_SIGNATURE_H

typedef struct rts_pdu_signature RtsPduSignature;
typedef struct _RTS_PDU_SIGNATURE_ENTRY RTS_PDU_SIGNATURE_ENTRY;

#include "rts.h"

#include <winpr/wtypes.h>

struct rts_pdu_signature
{
	UINT16 Flags;
	UINT16 NumberOfCommands;
	UINT32 CommandTypes[8];
};

struct _RTS_PDU_SIGNATURE_ENTRY
{
	UINT32 SignatureId;
	BOOL SignatureClient;
	RtsPduSignature* Signature;
	const char* PduName;
};

/* RTS PDU Signature IDs */

#define RTS_PDU_CONN_A					0x10000000
#define RTS_PDU_CONN_A1					(RTS_PDU_CONN_A	| 0x00000001)
#define RTS_PDU_CONN_A2					(RTS_PDU_CONN_A	| 0x00000002)
#define RTS_PDU_CONN_A3					(RTS_PDU_CONN_A	| 0x00000003)

#define RTS_PDU_CONN_B					0x20000000
#define RTS_PDU_CONN_B1					(RTS_PDU_CONN_B | 0x00000001)
#define RTS_PDU_CONN_B2					(RTS_PDU_CONN_B | 0x00000002)
#define RTS_PDU_CONN_B3					(RTS_PDU_CONN_B | 0x00000003)

#define RTS_PDU_CONN_C					0x40000000
#define RTS_PDU_CONN_C1					(RTS_PDU_CONN_C | 0x00000001)
#define RTS_PDU_CONN_C2					(RTS_PDU_CONN_C | 0x00000002)

#define RTS_PDU_IN_R1_A					0x01000000
#define RTS_PDU_IN_R1_A1				(RTS_PDU_IN_R1_A | 0x00000001)
#define RTS_PDU_IN_R1_A2				(RTS_PDU_IN_R1_A | 0x00000002)
#define RTS_PDU_IN_R1_A3				(RTS_PDU_IN_R1_A | 0x00000003)
#define RTS_PDU_IN_R1_A4				(RTS_PDU_IN_R1_A | 0x00000004)
#define RTS_PDU_IN_R1_A5				(RTS_PDU_IN_R1_A | 0x00000005)
#define RTS_PDU_IN_R1_A6				(RTS_PDU_IN_R1_A | 0x00000006)

#define RTS_PDU_IN_R1_B					0x02000000
#define RTS_PDU_IN_R1_B1				(RTS_PDU_IN_R1_B | 0x00000001)
#define RTS_PDU_IN_R1_B2				(RTS_PDU_IN_R1_B | 0x00000002)

#define RTS_PDU_IN_R2_A					0x04000000
#define RTS_PDU_IN_R2_A1				(RTS_PDU_IN_R2_A | 0x00000001)
#define RTS_PDU_IN_R2_A2				(RTS_PDU_IN_R2_A | 0x00000002)
#define RTS_PDU_IN_R2_A3				(RTS_PDU_IN_R2_A | 0x00000003)
#define RTS_PDU_IN_R2_A4				(RTS_PDU_IN_R2_A | 0x00000004)
#define RTS_PDU_IN_R2_A5				(RTS_PDU_IN_R2_A | 0x00000005)

#define RTS_PDU_OUT_R1_A				0x00100000
#define RTS_PDU_OUT_R1_A1				(RTS_PDU_OUT_R1_A | 0x00000001)
#define RTS_PDU_OUT_R1_A2				(RTS_PDU_OUT_R1_A | 0x00000002)
#define RTS_PDU_OUT_R1_A3				(RTS_PDU_OUT_R1_A | 0x00000003)
#define RTS_PDU_OUT_R1_A4				(RTS_PDU_OUT_R1_A | 0x00000004)
#define RTS_PDU_OUT_R1_A5				(RTS_PDU_OUT_R1_A | 0x00000005)
#define RTS_PDU_OUT_R1_A6				(RTS_PDU_OUT_R1_A | 0x00000006)
#define RTS_PDU_OUT_R1_A7				(RTS_PDU_OUT_R1_A | 0x00000007)
#define RTS_PDU_OUT_R1_A8				(RTS_PDU_OUT_R1_A | 0x00000008)
#define RTS_PDU_OUT_R1_A9				(RTS_PDU_OUT_R1_A | 0x00000009)
#define RTS_PDU_OUT_R1_A10				(RTS_PDU_OUT_R1_A | 0x0000000A)
#define RTS_PDU_OUT_R1_A11				(RTS_PDU_OUT_R1_A | 0x0000000B)

#define RTS_PDU_OUT_R2_A				0x00200000
#define RTS_PDU_OUT_R2_A1				(RTS_PDU_OUT_R2_A | 0x00000001)
#define RTS_PDU_OUT_R2_A2				(RTS_PDU_OUT_R2_A | 0x00000002)
#define RTS_PDU_OUT_R2_A3				(RTS_PDU_OUT_R2_A | 0x00000003)
#define RTS_PDU_OUT_R2_A4				(RTS_PDU_OUT_R2_A | 0x00000004)
#define RTS_PDU_OUT_R2_A5				(RTS_PDU_OUT_R2_A | 0x00000005)
#define RTS_PDU_OUT_R2_A6				(RTS_PDU_OUT_R2_A | 0x00000006)
#define RTS_PDU_OUT_R2_A7				(RTS_PDU_OUT_R2_A | 0x00000007)
#define RTS_PDU_OUT_R2_A8				(RTS_PDU_OUT_R2_A | 0x00000008)

#define RTS_PDU_OUT_R2_B				0x00400000
#define RTS_PDU_OUT_R2_B1				(RTS_PDU_OUT_R2_B | 0x00000001)
#define RTS_PDU_OUT_R2_B2				(RTS_PDU_OUT_R2_B | 0x00000002)
#define RTS_PDU_OUT_R2_B3				(RTS_PDU_OUT_R2_B | 0x00000003)

#define RTS_PDU_OUT_R2_C				0x00800000
#define RTS_PDU_OUT_R2_C1				(RTS_PDU_OUT_R2_C | 0x00000001)

#define RTS_PDU_OUT_OF_SEQUENCE				0x00010000
#define RTS_PDU_KEEP_ALIVE				(RTS_PDU_OUT_OF_SEQUENCE | 0x00000001)
#define RTS_PDU_PING_TRAFFIC_SENT_NOTIFY		(RTS_PDU_OUT_OF_SEQUENCE | 0x00000002)
#define RTS_PDU_ECHO					(RTS_PDU_OUT_OF_SEQUENCE | 0x00000003)
#define RTS_PDU_PING					(RTS_PDU_OUT_OF_SEQUENCE | 0x00000004)
#define RTS_PDU_FLOW_CONTROL_ACK			(RTS_PDU_OUT_OF_SEQUENCE | 0x00000005)
#define RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION	(RTS_PDU_OUT_OF_SEQUENCE | 0x00000006)

extern RtsPduSignature RTS_PDU_CONN_A1_SIGNATURE;
extern RtsPduSignature RTS_PDU_CONN_A2_SIGNATURE;
extern RtsPduSignature RTS_PDU_CONN_A3_SIGNATURE;

extern RtsPduSignature RTS_PDU_CONN_B1_SIGNATURE;
extern RtsPduSignature RTS_PDU_CONN_B2_SIGNATURE;
extern RtsPduSignature RTS_PDU_CONN_B3_SIGNATURE;

extern RtsPduSignature RTS_PDU_CONN_C1_SIGNATURE;
extern RtsPduSignature RTS_PDU_CONN_C2_SIGNATURE;

extern RtsPduSignature RTS_PDU_IN_R1_A1_SIGNATURE;
extern RtsPduSignature RTS_PDU_IN_R1_A2_SIGNATURE;
extern RtsPduSignature RTS_PDU_IN_R1_A3_SIGNATURE;
extern RtsPduSignature RTS_PDU_IN_R1_A4_SIGNATURE;
extern RtsPduSignature RTS_PDU_IN_R1_A5_SIGNATURE;
extern RtsPduSignature RTS_PDU_IN_R1_A6_SIGNATURE;

extern RtsPduSignature RTS_PDU_IN_R1_B1_SIGNATURE;
extern RtsPduSignature RTS_PDU_IN_R1_B2_SIGNATURE;

extern RtsPduSignature RTS_PDU_IN_R2_A1_SIGNATURE;
extern RtsPduSignature RTS_PDU_IN_R2_A2_SIGNATURE;
extern RtsPduSignature RTS_PDU_IN_R2_A3_SIGNATURE;
extern RtsPduSignature RTS_PDU_IN_R2_A4_SIGNATURE;
extern RtsPduSignature RTS_PDU_IN_R2_A5_SIGNATURE;

extern RtsPduSignature RTS_PDU_OUT_R1_A1_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R1_A2_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R1_A3_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R1_A4_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R1_A5_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R1_A6_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R1_A7_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R1_A8_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R1_A9_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R1_A10_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R1_A11_SIGNATURE;

extern RtsPduSignature RTS_PDU_OUT_R2_A1_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R2_A2_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R2_A3_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R2_A4_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R2_A5_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R2_A6_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R2_A7_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R2_A8_SIGNATURE;

extern RtsPduSignature RTS_PDU_OUT_R2_B1_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R2_B2_SIGNATURE;
extern RtsPduSignature RTS_PDU_OUT_R2_B3_SIGNATURE;

extern RtsPduSignature RTS_PDU_OUT_R2_C1_SIGNATURE;

extern RtsPduSignature RTS_PDU_KEEP_ALIVE_SIGNATURE;
extern RtsPduSignature RTS_PDU_PING_TRAFFIC_SENT_NOTIFY_SIGNATURE;
extern RtsPduSignature RTS_PDU_ECHO_SIGNATURE;
extern RtsPduSignature RTS_PDU_PING_SIGNATURE;
extern RtsPduSignature RTS_PDU_FLOW_CONTROL_ACK_SIGNATURE;
extern RtsPduSignature RTS_PDU_FLOW_CONTROL_ACK_WITH_DESTINATION_SIGNATURE;

BOOL rts_match_pdu_signature(rdpRpc* rpc, RtsPduSignature* signature, rpcconn_rts_hdr_t* rts);
int rts_extract_pdu_signature(rdpRpc* rpc, RtsPduSignature* signature, rpcconn_rts_hdr_t* rts);
UINT32 rts_identify_pdu_signature(rdpRpc* rpc, RtsPduSignature* signature, RTS_PDU_SIGNATURE_ENTRY** entry);
int rts_print_pdu_signature(rdpRpc* rpc, RtsPduSignature* signature);

#endif
