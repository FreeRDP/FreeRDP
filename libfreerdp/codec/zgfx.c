/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * ZGFX (RDP8) Bulk Data Compression
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

#include <freerdp/codec/zgfx.h>

int zgfx_decompress(ZGFX_CONTEXT* zgfx, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32 flags)
{
	int status = 0;

	return status;
}

int zgfx_compress(ZGFX_CONTEXT* zgfx, BYTE* pSrcData, UINT32 SrcSize, BYTE** ppDstData, UINT32* pDstSize, UINT32* pFlags)
{
	int status = 0;

	return 1;
}

void zgfx_context_reset(ZGFX_CONTEXT* zgfx, BOOL flush)
{

}

ZGFX_CONTEXT* zgfx_context_new(BOOL Compressor)
{
	ZGFX_CONTEXT* zgfx;

	zgfx = (ZGFX_CONTEXT*) calloc(1, sizeof(ZGFX_CONTEXT));

	if (zgfx)
	{
		zgfx->Compressor = Compressor;

		zgfx_context_reset(zgfx, FALSE);
	}

	return zgfx;
}

void zgfx_context_free(ZGFX_CONTEXT* zgfx)
{
	if (zgfx)
	{
		free(zgfx);
	}
}

