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

//#define WITH_BULK_DEBUG		1

const char* bulk_get_compression_flags_string(UINT32 flags)
{
	flags &= BULK_COMPRESSION_FLAGS_MASK;

	if (flags == 0)
		return "PACKET_UNCOMPRESSED";
	else if (flags == PACKET_COMPRESSED)
		return "PACKET_COMPRESSED";
	else if (flags == PACKET_AT_FRONT)
		return "PACKET_AT_FRONT";
	else if (flags == PACKET_FLUSHED)
		return "PACKET_FLUSHED";
	else if (flags == (PACKET_COMPRESSED | PACKET_AT_FRONT))
		return "PACKET_COMPRESSED | PACKET_AT_FRONT";
	else if (flags == (PACKET_COMPRESSED | PACKET_FLUSHED))
		return "PACKET_COMPRESSED | PACKET_FLUSHED";
	else if (flags == (PACKET_AT_FRONT | PACKET_FLUSHED))
		return "PACKET_AT_FRONT | PACKET_FLUSHED";
	else if (flags == (PACKET_COMPRESSED | PACKET_AT_FRONT | PACKET_FLUSHED))
		return "PACKET_COMPRESSED | PACKET_AT_FRONT | PACKET_FLUSHED";

	return "PACKET_UNKNOWN";
}

UINT32 bulk_compression_level(rdpBulk* bulk)
{
	rdpSettings* settings = bulk->context->settings;
	bulk->CompressionLevel = (settings->CompressionLevel >= 2) ? 2 : settings->CompressionLevel;
	return bulk->CompressionLevel;
}

UINT32 bulk_compression_max_size(rdpBulk* bulk)
{
	bulk_compression_level(bulk);
	bulk->CompressionMaxSize = (bulk->CompressionLevel < 1) ? 8192 : 65536;
	return bulk->CompressionMaxSize;
}

int bulk_compress_validate(rdpBulk* bulk, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
{
	int status;
	BYTE* _pSrcData = NULL;
	BYTE* _pDstData = NULL;
	UINT32 _SrcSize = 0;
	UINT32 _DstSize = 0;
	UINT32 _Flags = 0;

	_pSrcData = *ppDstData;
	_SrcSize = *pDstSize;
	_Flags = *pFlags | bulk->CompressionLevel;

	status = bulk_decompress(bulk, _pSrcData, _SrcSize, &_pDstData, &_DstSize, _Flags);

	if (status < 0)
	{
		printf("compression/decompression failure\n");
		return status;
	}

	if (_DstSize != SrcSize)
	{
		printf("compression/decompression size mismatch: Actual: %d, Expected: %d\n", _DstSize, SrcSize);
		return -1;
	}

	if (memcmp(_pDstData, pSrcData, SrcSize) != 0)
	{
		printf("compression/decompression input/output mismatch! flags: 0x%04X\n", _Flags);

#if 1
		printf("Actual:\n");
		winpr_HexDump(_pDstData, SrcSize);

		printf("Expected:\n");
		winpr_HexDump(pSrcData, SrcSize);
#endif

		return -1;
	}

	return status;
}

int bulk_decompress(rdpBulk* bulk, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags)
{
	UINT32 type;
	int status = -1;
	UINT32 CompressedBytes;
	UINT32 UncompressedBytes;

	bulk_compression_max_size(bulk);
	type = flags & BULK_COMPRESSION_TYPE_MASK;

	if (flags & BULK_COMPRESSION_FLAGS_MASK)
	{
		switch (type)
		{
			case PACKET_COMPR_TYPE_8K:
				mppc_set_compression_level(bulk->mppcRecv, 0);
				status = mppc_decompress(bulk->mppcRecv, pSrcData, SrcSize, ppDstData, pDstSize, flags);
				break;

			case PACKET_COMPR_TYPE_64K:
				mppc_set_compression_level(bulk->mppcRecv, 1);
				status = mppc_decompress(bulk->mppcRecv, pSrcData, SrcSize, ppDstData, pDstSize, flags);
				break;

			case PACKET_COMPR_TYPE_RDP6:
				status = ncrush_decompress(bulk->ncrushRecv, pSrcData, SrcSize, ppDstData, pDstSize, flags);
				break;

			case PACKET_COMPR_TYPE_RDP61:
				status = xcrush_decompress(bulk->xcrushRecv, pSrcData, SrcSize, ppDstData, pDstSize, flags);
				break;

			case PACKET_COMPR_TYPE_RDP8:
				status = -1;
				break;
		}
	}
	else
	{
		*ppDstData = pSrcData;
		*pDstSize = SrcSize;
		status = 0;
	}

	if (status >= 0)
	{
		CompressedBytes = SrcSize;
		UncompressedBytes = *pDstSize;

		bulk->TotalUncompressedBytes += UncompressedBytes;
		bulk->TotalCompressedBytes += CompressedBytes;

#ifdef WITH_BULK_DEBUG
		{
			double CompressionRatio;
			double TotalCompressionRatio;

			CompressionRatio = ((double) CompressedBytes) / ((double) UncompressedBytes);
			TotalCompressionRatio = ((double) bulk->TotalCompressedBytes) / ((double) bulk->TotalUncompressedBytes);

			printf("Decompress Type: %d Flags: %s (0x%04X) Compression Ratio: %f (%d / %d), Total: %f (%d / %d)\n",
					type, bulk_get_compression_flags_string(flags), flags,
					CompressionRatio, CompressedBytes, UncompressedBytes,
					TotalCompressionRatio, bulk->TotalCompressedBytes, bulk->TotalUncompressedBytes);
		}
#endif
	}
	else
	{
		fprintf(stderr, "Decompression failure!\n");
	}

	return status;
}

int bulk_compress(rdpBulk* bulk, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
{
	int status = -1;
	UINT32 CompressedBytes;
	UINT32 UncompressedBytes;

	if ((SrcSize <= 50) || (SrcSize >= 16384))
	{
		*ppDstData = pSrcData;
		*pDstSize = SrcSize;
		return 0;
	}

	*ppDstData = bulk->OutputBuffer;
	*pDstSize = sizeof(bulk->OutputBuffer);

	bulk_compression_level(bulk);
	bulk_compression_max_size(bulk);
	
	if (bulk->CompressionLevel < PACKET_COMPR_TYPE_RDP6)
	{
		mppc_set_compression_level(bulk->mppcSend, bulk->CompressionLevel);
		status = mppc_compress(bulk->mppcSend, pSrcData, SrcSize, ppDstData, pDstSize, pFlags);
	}
	else
	{
		status = ncrush_compress(bulk->ncrushSend, pSrcData, SrcSize, ppDstData, pDstSize, pFlags);
	}
	
	if (status >= 0)
	{
		CompressedBytes = *pDstSize;
		UncompressedBytes = SrcSize;

		bulk->TotalUncompressedBytes += UncompressedBytes;
		bulk->TotalCompressedBytes += CompressedBytes;

#ifdef WITH_BULK_DEBUG
		{
			UINT32 type;
			double CompressionRatio;
			double TotalCompressionRatio;

			type = bulk->CompressionLevel;

			CompressionRatio = ((double) CompressedBytes) / ((double) UncompressedBytes);
			TotalCompressionRatio = ((double) bulk->TotalCompressedBytes) / ((double) bulk->TotalUncompressedBytes);

			printf("Compress Type: %d Flags: %s (0x%04X) Compression Ratio: %f (%d / %d), Total: %f (%d / %d)\n",
					type, bulk_get_compression_flags_string(*pFlags), *pFlags,
					CompressionRatio, CompressedBytes, UncompressedBytes,
					TotalCompressionRatio, bulk->TotalCompressedBytes, bulk->TotalUncompressedBytes);
		}
#endif
	}

#if 0
	if (bulk_compress_validate(bulk, pSrcData, SrcSize, ppDstData, pDstSize, pFlags) < 0)
		status = -1;
#endif

	return status;
}

void bulk_reset(rdpBulk* bulk)
{
	mppc_context_reset(bulk->mppcSend);
	mppc_context_reset(bulk->mppcRecv);

	ncrush_context_reset(bulk->ncrushRecv);
	ncrush_context_reset(bulk->ncrushSend);

	xcrush_context_reset(bulk->xcrushRecv);
	xcrush_context_reset(bulk->xcrushSend);
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

		bulk->ncrushRecv = ncrush_context_new(FALSE);
		bulk->ncrushSend = ncrush_context_new(TRUE);

		bulk->xcrushRecv = xcrush_context_new(FALSE);
		bulk->xcrushSend = xcrush_context_new(TRUE);

		bulk->CompressionLevel = context->settings->CompressionLevel;

		bulk->TotalCompressedBytes = 0;
		bulk->TotalUncompressedBytes = 0;
	}

	return bulk;
}

void bulk_free(rdpBulk* bulk)
{
	if (!bulk)
		return;

	mppc_context_free(bulk->mppcSend);
	mppc_context_free(bulk->mppcRecv);

	ncrush_context_free(bulk->ncrushRecv);
	ncrush_context_free(bulk->ncrushSend);

	xcrush_context_free(bulk->xcrushRecv);
	xcrush_context_free(bulk->xcrushSend);

	free(bulk);
}
