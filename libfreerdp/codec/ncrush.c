/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NCrush (RDP6) Bulk Data Compression
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

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/bitstream.h>

#include <freerdp/codec/ncrush.h>

int ncrush_compress(NCRUSH_CONTEXT* ncrush, BYTE* pSrcData, UINT32 SrcSize, BYTE* pDstData, UINT32* pDstSize, UINT32* pFlags)
{
	return 1;
}

int ncrush_decompress(NCRUSH_CONTEXT* ncrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags)
{
	return 1;
}

NCRUSH_CONTEXT* ncrush_context_new(BOOL Compressor)
{
	NCRUSH_CONTEXT* ncrush;

	ncrush = (NCRUSH_CONTEXT*) calloc(1, sizeof(NCRUSH_CONTEXT));

	if (ncrush)
	{
		ncrush->Compressor = Compressor;

		ncrush->bs = BitStream_New();

		ncrush->HistoryBufferSize = 65536;
		ZeroMemory(&(ncrush->HistoryBuffer), sizeof(ncrush->HistoryBuffer));

		ncrush->HistoryOffset = 0;
		ncrush->HistoryPtr = &(ncrush->HistoryBuffer[ncrush->HistoryOffset]);
	}

	return ncrush;
}

void ncrush_context_free(NCRUSH_CONTEXT* ncrush)
{
	if (ncrush)
	{
		BitStream_Free(ncrush->bs);

		free(ncrush);
	}
}
