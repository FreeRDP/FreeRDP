/* FreeRDP: A Remote Desktop Protocol Client
 * Routines to set a chunk of memory to a constant.
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

#ifndef __PRIM_SET_H_INCLUDED__
#define __PRIM_SET_H_INCLUDED__

pstatus_t general_set_8u(BYTE val, BYTE *pDst, INT32 len);
pstatus_t general_zero(void *pDst, size_t len);
pstatus_t general_set_32s(INT32 val, INT32 *pDst, INT32 len);
pstatus_t general_set_32u(UINT32 val, UINT32 *pDst, INT32 len);


void primitives_init_set_opt(primitives_t *prims);

#endif /* !__PRIM_SET_H_INCLUDED__ */

