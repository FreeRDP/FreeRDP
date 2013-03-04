/* FreeRDP: A Remote Desktop Protocol Client
 * Alpha blending routines.
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

#ifndef __PRIM_ALPHACOMP_H_INCLUDED__
#define __PRIM_ALPHACOMP_H_INCLUDED__

pstatus_t general_alphaComp_argb(const BYTE *pSrc1, INT32 src1Step, const BYTE *pSrc2, INT32 src2Step, BYTE *pDst, INT32 dstStep, INT32 width, INT32 height);

void primitives_init_alphaComp_opt(primitives_t* prims);

#endif /* !__PRIM_ALPHACOMP_H_INCLUDED__ */

