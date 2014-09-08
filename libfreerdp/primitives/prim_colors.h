/* FreeRDP: A Remote Desktop Protocol Client
 * Color conversion operations.
 * vi:ts=4 sw=4
 *
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.  Algorithms used by
 * this code may be covered by patents by HP, Microsoft, or other parties.
 *
 */

#ifdef __GNUC__
# pragma once
#endif

#ifndef __PRIM_COLORS_H_INCLUDED__
#define __PRIM_COLORS_H_INCLUDED__

pstatus_t general_yCbCrToRGB_16s8u_P3AC4R(const INT16* pSrc[3], int srcStep, BYTE* pDst, int dstStep, const prim_size_t* roi);
pstatus_t general_yCbCrToRGB_16s16s_P3P3(const INT16 *pSrc[3], INT32 srcStep, INT16 *pDst[3], INT32 dstStep, const prim_size_t *roi);
pstatus_t general_RGBToYCbCr_16s16s_P3P3(const INT16 *pSrc[3], INT32 srcStep, INT16 *pDst[3], INT32 dstStep, const prim_size_t *roi);
pstatus_t general_RGBToRGB_16s8u_P3AC4R(const INT16 *pSrc[3], int srcStep, BYTE *pDst, int dstStep, const prim_size_t *roi);

void primitives_init_colors_opt(primitives_t* prims);

#endif /* !__PRIM_COLORS_H_INCLUDED__ */

