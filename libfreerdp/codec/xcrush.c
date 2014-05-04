/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * XCrush (RDP6.1) Bulk Data Compression
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

#include <freerdp/codec/xcrush.h>

int xcrush_decompress(XCRUSH_CONTEXT* xcrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags)
{
	return 1;
}

int xcrush_compress(XCRUSH_CONTEXT* xcrush, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
{
	return 1;
}

void xcrush_context_reset(XCRUSH_CONTEXT* xcrush)
{

}

XCRUSH_CONTEXT* xcrush_context_new(BOOL Compressor)
{
	XCRUSH_CONTEXT* xcrush;

	xcrush = (XCRUSH_CONTEXT*) calloc(1, sizeof(XCRUSH_CONTEXT));

	if (xcrush)
	{
		xcrush->Compressor = Compressor;

		xcrush_context_reset(xcrush);
	}

	return xcrush;
}

void xcrush_context_free(XCRUSH_CONTEXT* xcrush)
{
	if (xcrush)
	{
		free(xcrush);
	}
}

