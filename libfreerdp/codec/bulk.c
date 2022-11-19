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

#include <winpr/assert.h>

#include <freerdp/config.h>

#include "bulk.h"
#include "../codec/mppc.h"
#include "../codec/ncrush.h"
#include "../codec/xcrush.h"

#include <freerdp/log.h>
#define TAG FREERDP_TAG("core")

//#define WITH_BULK_DEBUG 1

struct rdp_bulk
{
	ALIGN64 rdpContext* context;
	ALIGN64 UINT32 CompressionLevel;
	ALIGN64 UINT32 CompressionMaxSize;
	ALIGN64 MPPC_CONTEXT* mppcSend;
	ALIGN64 MPPC_CONTEXT* mppcRecv;
	ALIGN64 NCRUSH_CONTEXT* ncrushRecv;
	ALIGN64 NCRUSH_CONTEXT* ncrushSend;
	ALIGN64 XCRUSH_CONTEXT* xcrushRecv;
	ALIGN64 XCRUSH_CONTEXT* xcrushSend;
	ALIGN64 BYTE OutputBuffer[65536];
};

#if defined(WITH_BULK_DEBUG)
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
	rdpSettings* settings;
	WINPR_ASSERT(bulk);
	WINPR_ASSERT(bulk->context);
	settings = bulk->context->settings;
	WINPR_ASSERT(settings);
	bulk->CompressionLevel = (settings->CompressionLevel >= PACKET_COMPR_TYPE_RDP61)
	                             ? PACKET_COMPR_TYPE_RDP61
	                             : settings->CompressionLevel;
	return bulk->CompressionLevel;
}

UINT32 bulk_compression_max_size(rdpBulk* bulk)
{
	WINPR_ASSERT(bulk);
	bulk_compression_level(bulk);
	bulk->CompressionMaxSize = (bulk->CompressionLevel < PACKET_COMPR_TYPE_64K) ? 8192 : 65536;
	return bulk->CompressionMaxSize;
}

#if defined(WITH_BULK_DEBUG)
static INLINE int bulk_compress_validate(rdpBulk* bulk, const BYTE* pSrcData, UINT32 SrcSize,
                                         const BYTE* pDstData, UINT32 DstSize, UINT32 Flags)
{
	int status;
	const BYTE* v_pSrcData = NULL;
	const BYTE* v_pDstData = NULL;
	UINT32 v_SrcSize = 0;
	UINT32 v_DstSize = 0;
	UINT32 v_Flags = 0;

	WINPR_ASSERT(bulk);
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(pDstData);

	v_pSrcData = pDstData;
	v_SrcSize = DstSize;
	v_Flags = Flags | bulk->CompressionLevel;
	status = bulk_decompress(bulk, v_pSrcData, v_SrcSize, &v_pDstData, &v_DstSize, v_Flags);

	if (status < 0)
	{
		WLog_DBG(TAG, "compression/decompression failure");
		return status;
	}

	if (v_DstSize != SrcSize)
	{
		WLog_DBG(TAG,
		         "compression/decompression size mismatch: Actual: %" PRIu32 ", Expected: %" PRIu32
		         "",
		         v_DstSize, SrcSize);
		return -1;
	}

	if (memcmp(v_pDstData, pSrcData, SrcSize) != 0)
	{
		WLog_DBG(TAG, "compression/decompression input/output mismatch! flags: 0x%08" PRIX32 "",
		         v_Flags);
#if 1
		WLog_DBG(TAG, "Actual:");
		winpr_HexDump(TAG, WLOG_DEBUG, v_pDstData, SrcSize);
		WLog_DBG(TAG, "Expected:");
		winpr_HexDump(TAG, WLOG_DEBUG, pSrcData, SrcSize);
#endif
		return -1;
	}

	return status;
}
#endif

int bulk_decompress(rdpBulk* bulk, const BYTE* pSrcData, UINT32 SrcSize, const BYTE** ppDstData,
                    UINT32* pDstSize, UINT32 flags)
{
	UINT32 type;
	int status = -1;
	rdpMetrics* metrics;
	UINT32 CompressedBytes;
	UINT32 UncompressedBytes;
	double CompressionRatio;

	WINPR_ASSERT(bulk);
	WINPR_ASSERT(bulk->context);
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(ppDstData);
	WINPR_ASSERT(pDstSize);

	metrics = bulk->context->metrics;
	WINPR_ASSERT(metrics);

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

int bulk_compress(rdpBulk* bulk, const BYTE* pSrcData, UINT32 SrcSize, const BYTE** ppDstData,
                  UINT32* pDstSize, UINT32* pFlags)
{
	int status = -1;
	rdpMetrics* metrics;
	UINT32 CompressedBytes;
	UINT32 UncompressedBytes;
	double CompressionRatio;

	WINPR_ASSERT(bulk);
	WINPR_ASSERT(bulk->context);
	WINPR_ASSERT(pSrcData);
	WINPR_ASSERT(ppDstData);
	WINPR_ASSERT(pDstSize);

	metrics = bulk->context->metrics;
	WINPR_ASSERT(metrics);

	if ((SrcSize <= 50) || (SrcSize >= 16384))
	{
		*ppDstData = pSrcData;
		*pDstSize = SrcSize;
		return 0;
	}

	*pDstSize = sizeof(bulk->OutputBuffer);
	bulk_compression_level(bulk);
	bulk_compression_max_size(bulk);

	switch (bulk->CompressionLevel)
	{
		case PACKET_COMPR_TYPE_8K:
		case PACKET_COMPR_TYPE_64K:
			mppc_set_compression_level(bulk->mppcSend, bulk->CompressionLevel);
			status = mppc_compress(bulk->mppcSend, pSrcData, SrcSize, bulk->OutputBuffer, ppDstData,
			                       pDstSize, pFlags);
			break;
		case PACKET_COMPR_TYPE_RDP6:
			status = ncrush_compress(bulk->ncrushSend, pSrcData, SrcSize, bulk->OutputBuffer,
			                         ppDstData, pDstSize, pFlags);
			break;
		case PACKET_COMPR_TYPE_RDP61:
			status = xcrush_compress(bulk->xcrushSend, pSrcData, SrcSize, bulk->OutputBuffer,
			                         ppDstData, pDstSize, pFlags);
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

#if defined(WITH_BULK_DEBUG)

	if (bulk_compress_validate(bulk, pSrcData, SrcSize, *ppDstData, *pDstSize, *pFlags) < 0)
		status = -1;

#endif
	return status;
}

void bulk_reset(rdpBulk* bulk)
{
	WINPR_ASSERT(bulk);

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
	WINPR_ASSERT(context);

	bulk = (rdpBulk*)calloc(1, sizeof(rdpBulk));

	if (!bulk)
		goto fail;

	bulk->context = context;
	bulk->mppcSend = mppc_context_new(1, TRUE);
	if (!bulk->mppcSend)
		goto fail;
	bulk->mppcRecv = mppc_context_new(1, FALSE);
	if (!bulk->mppcRecv)
		goto fail;
	bulk->ncrushRecv = ncrush_context_new(FALSE);
	if (!bulk->ncrushRecv)
		goto fail;
	bulk->ncrushSend = ncrush_context_new(TRUE);
	if (!bulk->ncrushSend)
		goto fail;
	bulk->xcrushRecv = xcrush_context_new(FALSE);
	if (!bulk->xcrushRecv)
		goto fail;
	bulk->xcrushSend = xcrush_context_new(TRUE);
	if (!bulk->xcrushSend)
		goto fail;
	bulk->CompressionLevel = context->settings->CompressionLevel;

	return bulk;
fail:
	bulk_free(bulk);
	return NULL;
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
