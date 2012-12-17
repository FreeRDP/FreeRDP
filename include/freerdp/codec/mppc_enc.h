/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Implements Microsoft Point to Point Compression (MPPC) protocol
 *
 * Copyright 2012 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
 * Copyright 2012 Jay Sorg <jay.sorg@gmail.com>
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

#ifndef FREERDP_CODEC_MPPC_ENCODER_H
#define FREERDP_CODEC_MPPC_ENCODER_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#define PROTO_RDP_40 1
#define PROTO_RDP_50 2

struct rdp_mppc_enc
{
	int   protocol_type;    /* PROTO_RDP_40, PROTO_RDP_50 etc */
	char* historyBuffer;    /* contains uncompressed data */
	char* outputBuffer;     /* contains compressed data */
	char* outputBufferPlus;
	int   historyOffset;    /* next free slot in historyBuffer */
	int   buf_len;          /* length of historyBuffer, protocol dependant */
	int   bytes_in_opb;     /* compressed bytes available in outputBuffer */
	int   flags;            /* PACKET_COMPRESSED, PACKET_AT_FRONT, PACKET_FLUSHED etc */
	int   flagsHold;
	int   first_pkt;        /* this is the first pkt passing through enc */
	UINT16* hash_table;
};

FREERDP_API BOOL compress_rdp(struct rdp_mppc_enc* enc, BYTE* srcData, int len);
FREERDP_API BOOL compress_rdp_4(struct rdp_mppc_enc* enc, BYTE* srcData, int len);
FREERDP_API BOOL compress_rdp_5(struct rdp_mppc_enc* enc, BYTE* srcData, int len);
FREERDP_API struct rdp_mppc_enc* mppc_enc_new(int protocol_type);
FREERDP_API void mppc_enc_free(struct rdp_mppc_enc* enc);

#endif /* FREERDP_CODEC_MPPC_ENCODER_H */
