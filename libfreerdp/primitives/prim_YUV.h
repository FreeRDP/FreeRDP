/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_PRIMITIVES_YUV_H
#define FREERDP_PRIMITIVES_YUV_H

pstatus_t general_yCbCrToRGB_16s8u_P3AC4R(const INT16* pSrc[3], int srcStep, BYTE* pDst, int dstStep, const prim_size_t* roi);

void primitives_init_YUV(primitives_t* prims);
void primitives_init_YUV_opt(primitives_t* prims);
void primitives_deinit_YUV(primitives_t* prims);

#endif /* FREERDP_PRIMITIVES_YUV_H */
