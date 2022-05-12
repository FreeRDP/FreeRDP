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

#ifndef FREERDP_CODEC_NCRUSH_H
#define FREERDP_CODEC_NCRUSH_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include "mppc.h"

#include <winpr/bitstream.h>

typedef struct s_NCRUSH_CONTEXT NCRUSH_CONTEXT;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_LOCAL int ncrush_compress(NCRUSH_CONTEXT* ncrush, const BYTE* pSrcData, UINT32 SrcSize,
	                                  BYTE* pDstBuffer, const BYTE** ppDstData, UINT32* pDstSize,
	                                  UINT32* pFlags);
	FREERDP_LOCAL int ncrush_decompress(NCRUSH_CONTEXT* ncrush, const BYTE* pSrcData,
	                                    UINT32 SrcSize, const BYTE** ppDstData, UINT32* pDstSize,
	                                    UINT32 flags);

	FREERDP_LOCAL void ncrush_context_reset(NCRUSH_CONTEXT* ncrush, BOOL flush);

	FREERDP_LOCAL NCRUSH_CONTEXT* ncrush_context_new(BOOL Compressor);
	FREERDP_LOCAL void ncrush_context_free(NCRUSH_CONTEXT* ncrush);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CODEC_NCRUSH_H */
