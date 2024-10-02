/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Primitives copy
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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

#ifndef FREERDP_LIB_PRIM_COPY_H
#define FREERDP_LIB_PRIM_COPY_H

#include <winpr/wtypes.h>
#include <freerdp/config.h>
#include <freerdp/primitives.h>

pstatus_t generic_image_copy_no_overlap_convert(
    BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
    UINT32 nWidth, UINT32 nHeight, const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
    UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* WINPR_RESTRICT palette,
    SSIZE_T srcVMultiplier, SSIZE_T srcVOffset, SSIZE_T dstVMultiplier, SSIZE_T dstVOffset);

pstatus_t generic_image_copy_no_overlap_memcpy(
    BYTE* WINPR_RESTRICT pDstData, DWORD DstFormat, UINT32 nDstStep, UINT32 nXDst, UINT32 nYDst,
    UINT32 nWidth, UINT32 nHeight, const BYTE* WINPR_RESTRICT pSrcData, DWORD SrcFormat,
    UINT32 nSrcStep, UINT32 nXSrc, UINT32 nYSrc, const gdiPalette* WINPR_RESTRICT palette,
    SSIZE_T srcVMultiplier, SSIZE_T srcVOffset, SSIZE_T dstVMultiplier, SSIZE_T dstVOffset,
    UINT32 flags);

void primitives_init_copy_sse41(primitives_t* prims);

#if defined(WITH_AVX2)
void primitives_init_copy_avx2(primitives_t* prims);
#endif

#endif
