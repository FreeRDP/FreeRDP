/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Implements Microsoft Point to Point Compression (MPPC) protocol
 *
 * Copyright 2011 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
 * Copyright Jiten Pathy
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

#ifndef FREERDP_CODEC_MPPC_DECODER_H
#define FREERDP_CODEC_MPPC_DECODER_H

#include <freerdp/api.h>
#include <freerdp/types.h>

/* Compression Types */
#define PACKET_COMPRESSED		0x20
#define PACKET_AT_FRONT			0x40
#define PACKET_FLUSHED			0x80
#define PACKET_COMPR_TYPE_8K		0x00
#define PACKET_COMPR_TYPE_64K		0x01
#define PACKET_COMPR_TYPE_RDP6		0x02
#define PACKET_COMPR_TYPE_RDP61		0x03
#define CompressionTypeMask		0x0F

#define RDP6_HISTORY_BUF_SIZE		65536
#define RDP6_OFFSET_CACHE_SIZE		8

struct rdp_mppc_dec
{
	BYTE* history_buf;
	UINT16* offset_cache;
	BYTE* history_buf_end;
	BYTE* history_ptr;
};

FREERDP_API int decompress_rdp(struct rdp_mppc_dec* dec, BYTE* cbuf, int len, int ctype, UINT32* roff, UINT32* rlen);
FREERDP_API int decompress_rdp_4(struct rdp_mppc_dec* dec, BYTE* cbuf, int len, int ctype, UINT32* roff, UINT32* rlen);
FREERDP_API int decompress_rdp_5(struct rdp_mppc_dec* dec, BYTE* cbuf, int len, int ctype, UINT32* roff, UINT32* rlen);
FREERDP_API int decompress_rdp_6(struct rdp_mppc_dec* dec, BYTE* cbuf, int len, int ctype, UINT32* roff, UINT32* rlen);
FREERDP_API int decompress_rdp_61(struct rdp_mppc_dec* dec, BYTE* cbuf, int len, int ctype, UINT32* roff, UINT32* rlen);
FREERDP_API struct rdp_mppc_dec* mppc_dec_new(void);
FREERDP_API void mppc_dec_free(struct rdp_mppc_dec* dec);

#endif /* FREERDP_CODEC_MPPC_DECODER_H */
