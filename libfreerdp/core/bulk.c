/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Bulk Compression
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "bulk.h"

int bulk_decompress(rdpBulk* bulk, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags)
{
	int status = -1;
	UINT32 roff = 0;
	UINT32 rlen = 0;
	UINT32 type = flags & 0x0F;

	switch (type)
	{
		case PACKET_COMPR_TYPE_8K:
			status = decompress_rdp_4(bulk->mppc_dec, pSrcData, SrcSize, flags, &roff, &rlen);
			*ppDstData = (bulk->mppc_dec->history_buf + roff);
			*pDstSize = rlen;
			break;

		case PACKET_COMPR_TYPE_64K:
			status = decompress_rdp_5(bulk->mppc_dec, pSrcData, SrcSize, flags, &roff, &rlen);
			*ppDstData = (bulk->mppc_dec->history_buf + roff);
			*pDstSize = rlen;
			break;

		case PACKET_COMPR_TYPE_RDP6:
			status = decompress_rdp_6(bulk->mppc_dec, pSrcData, SrcSize, flags, &roff, &rlen);
			*ppDstData = (bulk->mppc_dec->history_buf + roff);
			*pDstSize = rlen;
			break;

		case PACKET_COMPR_TYPE_RDP61:
			status = -1;
			break;

		case PACKET_COMPR_TYPE_RDP8:
			status = -1;
			break;
	}

	return status;
}

int bulk_compress(rdpBulk* bulk, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
{
	int status = -1;

	if (compress_rdp(bulk->mppc_enc, pSrcData, SrcSize))
	{
		*pFlags = (UINT32) bulk->mppc_enc->flags;
		*pDstSize = (UINT32) bulk->mppc_enc->bytes_in_opb;
		*ppDstData = (BYTE*) bulk->mppc_enc->outputBuffer;
		status = 0;
	}

	return status;
}

void bulk_reset(rdpBulk* bulk)
{
	mppc_dec_free(bulk->mppc_dec);
	mppc_enc_free(bulk->mppc_enc);
	bulk->mppc_dec = mppc_dec_new();
	bulk->mppc_enc = mppc_enc_new(PROTO_RDP_50);
}

rdpBulk* bulk_new(rdpContext* context)
{
	rdpBulk* bulk;

	bulk = (rdpBulk*) calloc(1, sizeof(rdpBulk));

	if (bulk)
	{
		bulk->context = context;

		bulk->mppc_dec = mppc_dec_new();
		bulk->mppc_enc = mppc_enc_new(PROTO_RDP_50);
	}

	return bulk;
}

void bulk_free(rdpBulk* bulk)
{
	if (bulk)
	{
		mppc_dec_free(bulk->mppc_dec);
		mppc_enc_free(bulk->mppc_enc);
		free(bulk);
	}
}
