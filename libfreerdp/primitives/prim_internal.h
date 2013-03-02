/* prim_internal.h
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

#ifndef __PRIM_INTERNAL_H_INCLUDED__
#define __PRIM_INTERNAL_H_INCLUDED__

#ifndef CMAKE_BUILD_TYPE
#define CMAKE_BUILD_TYPE Release
#endif

#include <freerdp/primitives.h>


/* Use lddqu for unaligned; load for 16-byte aligned. */
#define LOAD_SI128(_ptr_) \
	(((ULONG_PTR) (_ptr_) & 0x0f) \
		? _mm_lddqu_si128((__m128i *) (_ptr_)) \
		: _mm_load_si128((__m128i *) (_ptr_)))

/* Function prototypes for all the init/deinit routines. */
extern void primitives_init_copy(
	primitives_t *prims);
extern void primitives_deinit_copy(
	primitives_t *prims);

extern void primitives_init_set(
	primitives_t *prims);
extern void primitives_deinit_set(
	primitives_t *prims);

extern void primitives_init_add(
	primitives_t *prims);
extern void primitives_deinit_add(
	primitives_t *prims);

extern void primitives_init_andor(
	primitives_t *prims);
extern void primitives_deinit_andor(
	primitives_t *prims);

extern void primitives_init_shift(
	primitives_t *prims);
extern void primitives_deinit_shift(
	primitives_t *prims);

extern void primitives_init_sign(
	primitives_t *prims);
extern void primitives_deinit_sign(
	primitives_t *prims);

extern void primitives_init_alphaComp(
	primitives_t *prims);
extern void primitives_deinit_alphaComp(
	primitives_t *prims);

extern void primitives_init_colors(
	primitives_t *prims);
extern void primitives_deinit_colors(
	primitives_t *prims);

#endif /* !__PRIM_INTERNAL_H_INCLUDED__ */
