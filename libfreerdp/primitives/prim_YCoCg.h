/* FreeRDP: A Remote Desktop Protocol Client
 * YCoCg<->RGB color conversion operations.
 * vi:ts=4 sw=4
 *
 * (c) Copyright 2014 Hewlett-Packard Development Company, L.P.
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

#ifdef __GNUC__
# pragma once
#endif

#ifndef __PRIM_YCOCG_H_INCLUDED__
#define __PRIM_YCOCG_H_INCLUDED__

pstatus_t general_YCoCgRToRGB_8u_AC4R(const BYTE *pSrc, INT32 srcStep, BYTE *pDst, INT32 dstStep, UINT32 width, UINT32 height, UINT8 shift, BOOL withAlpha, BOOL invert);

void primitives_init_YCoCg_opt(primitives_t* prims);

#endif /* !__PRIM_YCOCG_H_INCLUDED__ */
