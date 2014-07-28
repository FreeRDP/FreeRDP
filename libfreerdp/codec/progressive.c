/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Progressive Codec Bitmap Compression
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

#include <freerdp/codec/color.h>
#include <freerdp/codec/progressive.h>

int progressive_decompress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize,
		BYTE** ppDstData, DWORD DstFormat, int nDstStep, int nXDst, int nYDst, int nWidth, int nHeight)
{
	return 1;
}

int progressive_compress(PROGRESSIVE_CONTEXT* progressive, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize)
{
	return 1;
}

void progressive_context_reset(PROGRESSIVE_CONTEXT* progressive)
{

}

PROGRESSIVE_CONTEXT* progressive_context_new(BOOL Compressor)
{
	PROGRESSIVE_CONTEXT* progressive;

	progressive = (PROGRESSIVE_CONTEXT*) calloc(1, sizeof(PROGRESSIVE_CONTEXT));

	if (progressive)
	{
		progressive->Compressor = Compressor;
	}

	return progressive;
}

void progressive_context_free(PROGRESSIVE_CONTEXT* progressive)
{
	if (!progressive)
		return;

	free(progressive);
}

