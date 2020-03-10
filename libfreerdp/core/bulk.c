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

#define TAG "com.freerdp.core"

//#define WITH_BULK_DEBUG		1
struct rdp_bulk
{
	rdpContext* context;
	UINT32 CompressionLevel;
	UINT32 CompressionMaxSize;
	MPPC_CONTEXT* mppcSend;
	MPPC_CONTEXT* mppcRecv;
	NCRUSH_CONTEXT* ncrushRecv;
	NCRUSH_CONTEXT* ncrushSend;
	XCRUSH_CONTEXT* xcrushRecv;
	XCRUSH_CONTEXT* xcrushSend;
	BYTE OutputBuffer[65536];
};

#if WITH_BULK_DEBUG
static INLINE const char* bulk_get_compression_flags_string(UINT32 flags)
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
#endif

static UINT32 bulk_compression_level(rdpBulk* bulk)
{
	rdpSettings* settings = bulk->context->settings;
	bulk->CompressionLevel = (settings->CompressionLevel >= PACKET_COMPR_TYPE_RDP61)
	                             ? PACKET_COMPR_TYPE_RDP61
	                             : settings->CompressionLevel;
	return bulk->CompressionLevel;
}

UINT32 bulk_compression_max_size(rdpBulk* bulk)
{
	bulk_compression_level(bulk);
	bulk->CompressionMaxSize = (bulk->CompressionLevel < PACKET_COMPR_TYPE_64K) ? 8192 : 65536;
	return bulk->CompressionMaxSize;
}

#if WITH_BULK_DEBUG
static INLINE int bulk_compress_validate(rdpBulk* bulk, BYTE* pSrcData, UINT32 SrcSize,
                                         BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
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
		WLog_DBG(TAG, "compression/decompression failure");
		return status;
	}

	if (_DstSize != SrcSize)
	{
		WLog_DBG(TAG,
		         "compression/decompression size mismatch: Actual: %" PRIu32 ", Expected: %" PRIu32
		         "",
		         _DstSize, SrcSize);
		return -1;
	}

	if (memcmp(_pDstData, pSrcData, SrcSize) != 0)
	{
		WLog_DBG(TAG, "compression/decompression input/output mismatch! flags: 0x%08" PRIX32 "",
		         _Flags);
#if 1
		WLog_DBG(TAG, "Actual:");
		winpr_HexDump(TAG, WLOG_DEBUG, _pDstData, SrcSize);
		WLog_DBG(TAG, "Expected:");
		winpr_HexDump(TAG, WLOG_DEBUG, pSrcData, SrcSize);
#endif
		return -1;
	}

	return status;
}
#endif

int bulk_decompress(rdpBulk* bulk, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData,
                    UINT32* pDstSize, UINT32 flags)
{
	UINT32 type;
	int status = -1;
	rdpMetrics* metrics;
	UINT32 CompressedBytes;
	UINT32 UncompressedBytes;
	double CompressionRatio;
	metrics = bulk->context->metrics;
	bulk_compression_max_size(bulk);
	type = flags & BULK_COMPRESSION_TYPE_MASK;

	if (flags & BULK_COMPRESSION_FLAGS_MASK)
	{
		switch (type)
		{
			case PACKET_COMPR_TYPE_8K:
				mppc_set_compression_level(bulk->mppcRecv, 0);
				status =
				    mppc_decompress(bulk->mppcRecv, pSrcData, SrcSize, ppDstData, pDstSize, flags);
				break;

			case PACKET_COMPR_TYPE_64K:
				mppc_set_compression_level(bulk->mppcRecv, 1);
				status =
				    mppc_decompress(bulk->mppcRecv, pSrcData, SrcSize, ppDstData, pDstSize, flags);
				break;

			case PACKET_COMPR_TYPE_RDP6:
				status = ncrush_decompress(bulk->ncrushRecv, pSrcData, SrcSize, ppDstData, pDstSize,
				                           flags);
				break;

			case PACKET_COMPR_TYPE_RDP61:
				status = xcrush_decompress(bulk->xcrushRecv, pSrcData, SrcSize, ppDstData, pDstSize,
				                           flags);
				break;

			case PACKET_COMPR_TYPE_RDP8:
				WLog_ERR(TAG, "Unsupported bulk compression type %08" PRIx32,
				         bulk->CompressionLevel);
				status = -1;
				break;
			default:
				WLog_ERR(TAG, "Unknown bulk compression type %08" PRIx32, bulk->CompressionLevel);
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
		CompressionRatio = metrics_write_bytes(metrics, UncompressedBytes, CompressedBytes);
#ifdef WITH_BULK_DEBUG
		{
			WLog_DBG(TAG,
			         "Decompress Type: %" PRIu32 " Flags: %s (0x%08" PRIX32
			         ") Compression Ratio: %f (%" PRIu32 " / %" PRIu32 "), Total: %f (%" PRIu64
			         " / %" PRIu64 ")",
			         type, bulk_get_compression_flags_string(flags), flags, CompressionRatio,
			         CompressedBytes, UncompressedBytes, metrics->TotalCompressionRatio,
			         metrics->TotalCompressedBytes, metrics->TotalUncompressedBytes);
		}
#else
		WINPR_UNUSED(CompressionRatio);
#endif
	}
	else
	{
		WLog_ERR(TAG, "Decompression failure!");
	}

	return status;
}

int bulk_compress(rdpBulk* bulk, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize,
                  UINT32* pFlags)
{
	int status = -1;
	rdpMetrics* metrics;
	UINT32 CompressedBytes;
	UINT32 UncompressedBytes;
	double CompressionRatio;
	metrics = bulk->context->metrics;

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

	switch (bulk->CompressionLevel)
	{
		case PACKET_COMPR_TYPE_8K:
		case PACKET_COMPR_TYPE_64K:
			mppc_set_compression_level(bulk->mppcSend, bulk->CompressionLevel);
			status = mppc_compress(bulk->mppcSend, pSrcData, SrcSize, ppDstData, pDstSize, pFlags);
			break;
		case PACKET_COMPR_TYPE_RDP6:
			status =
			    ncrush_compress(bulk->ncrushSend, pSrcData, SrcSize, ppDstData, pDstSize, pFlags);
			break;
		case PACKET_COMPR_TYPE_RDP61:
			status =
			    xcrush_compress(bulk->xcrushSend, pSrcData, SrcSize, ppDstData, pDstSize, pFlags);
			break;
		case PACKET_COMPR_TYPE_RDP8:
			WLog_ERR(TAG, "Unsupported bulk compression type %08" PRIx32, bulk->CompressionLevel);
			status = -1;
			break;
		default:
			WLog_ERR(TAG, "Unknown bulk compression type %08" PRIx32, bulk->CompressionLevel);
			status = -1;
			break;
	}

	if (status >= 0)
	{
		CompressedBytes = *pDstSize;
		UncompressedBytes = SrcSize;
		CompressionRatio = metrics_write_bytes(metrics, UncompressedBytes, CompressedBytes);
#ifdef WITH_BULK_DEBUG
		{
			WLog_DBG(TAG,
			         "Compress Type: %" PRIu32 " Flags: %s (0x%08" PRIX32
			         ") Compression Ratio: %f (%" PRIu32 " / %" PRIu32 "), Total: %f (%" PRIu64
			         " / %" PRIu64 ")",
			         bulk->CompressionLevel, bulk_get_compression_flags_string(*pFlags), *pFlags,
			         CompressionRatio, CompressedBytes, UncompressedBytes,
			         metrics->TotalCompressionRatio, metrics->TotalCompressedBytes,
			         metrics->TotalUncompressedBytes);
		}
#else
		WINPR_UNUSED(CompressionRatio);
#endif
	}

#if WITH_BULK_DEBUG

	if (bulk_compress_validate(bulk, pSrcData, SrcSize, ppDstData, pDstSize, pFlags) < 0)
		status = -1;

#endif
	return status;
}

void bulk_reset(rdpBulk* bulk)
{
	mppc_context_reset(bulk->mppcSend, FALSE);
	mppc_context_reset(bulk->mppcRecv, FALSE);
	ncrush_context_reset(bulk->ncrushRecv, FALSE);
	ncrush_context_reset(bulk->ncrushSend, FALSE);
	xcrush_context_reset(bulk->xcrushRecv, FALSE);
	xcrush_context_reset(bulk->xcrushSend, FALSE);
}

rdpBulk* bulk_new(rdpContext* context)
{
	rdpBulk* bulk;
	bulk = (rdpBulk*)calloc(1, sizeof(rdpBulk));

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
