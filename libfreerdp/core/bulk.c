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
	UINT32 size;
	UINT32 roff = 0;
	UINT32 rlen = 0;
	UINT32 type = flags & 0x0F;

	switch (type)
	{
		case PACKET_COMPR_TYPE_8K:
			size = SrcSize;
			mppc_set_compression_level(bulk->mppcRecv, 0);
			mppc_decompress(bulk->mppcRecv, pSrcData, ppDstData, &size, flags);
			*pDstSize = size;
			status = 1;
			break;

		case PACKET_COMPR_TYPE_64K:
			size = SrcSize;
			mppc_set_compression_level(bulk->mppcRecv, 1);
			mppc_decompress(bulk->mppcRecv, pSrcData, ppDstData, &size, flags);
			*pDstSize = size;
			status = 1;
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
	UINT32 size;
	UINT32 flags;
	int status = -1;

	size = SrcSize;
	flags = mppc_compress(bulk->mppcSend, pSrcData, bulk->OutputBuffer, &size);

	*pFlags = flags;
	*pDstSize = size;
	*ppDstData = bulk->OutputBuffer;
	status = 1;

	return status;
}

void bulk_reset(rdpBulk* bulk)
{
	mppc_context_reset(bulk->mppcSend);
	mppc_context_reset(bulk->mppcRecv);

	mppc_dec_free(bulk->mppc_dec);
	bulk->mppc_dec = mppc_dec_new();
}

rdpBulk* bulk_new(rdpContext* context)
{
	rdpBulk* bulk;

	bulk = (rdpBulk*) calloc(1, sizeof(rdpBulk));

	if (bulk)
	{
		bulk->context = context;

		bulk->mppcSend = mppc_context_new(1, TRUE);
		bulk->mppcRecv = mppc_context_new(1, FALSE);

		bulk->mppc_dec = mppc_dec_new();

		bulk->CompressionLevel = context->settings->CompressionLevel;
	}

	return bulk;
}

void bulk_free(rdpBulk* bulk)
{
	if (bulk)
	{
		mppc_context_free(bulk->mppcSend);
		mppc_context_free(bulk->mppcRecv);

		mppc_dec_free(bulk->mppc_dec);

		free(bulk);
	}
}
