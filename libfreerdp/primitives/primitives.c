/* primitives.c
 * This code queries processor features and calls the init/deinit routines.
 * vi:ts=4 sw=4
 *
 * Copyright 2011 Martin Fleisz <martin.fleisz@thincast.com>
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License. You may obtain
 * a copy of the License at http://www.apache.org/licenses/LICENSE-2.0.
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdlib.h>

#include <winpr/synch.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

/* Singleton pointer used throughout the program when requested. */
static primitives_t pPrimitives = { 0 };
static primitives_t pPrimitivesGeneric = { 0 };
static INIT_ONCE generic_primitives_InitOnce = INIT_ONCE_STATIC_INIT;
static INIT_ONCE primitives_InitOnce = INIT_ONCE_STATIC_INIT;


/* ------------------------------------------------------------------------- */
static BOOL CALLBACK primitives_init_generic(PINIT_ONCE once, PVOID param, PVOID* context)
{
	primitives_init_add(&pPrimitivesGeneric);
	primitives_init_andor(&pPrimitivesGeneric);
	primitives_init_alphaComp(&pPrimitivesGeneric);
	primitives_init_copy(&pPrimitivesGeneric);
	primitives_init_set(&pPrimitivesGeneric);
	primitives_init_shift(&pPrimitivesGeneric);
	primitives_init_sign(&pPrimitivesGeneric);
	primitives_init_colors(&pPrimitivesGeneric);
	primitives_init_YCoCg(&pPrimitivesGeneric);
	primitives_init_YUV(&pPrimitivesGeneric);
	return TRUE;
}

#if defined(HAVE_OPTIMIZED_PRIMITIVES)
static BOOL CALLBACK primitives_init(PINIT_ONCE once, PVOID param, PVOID* context)
{
	/* Now call each section's initialization routine. */
	primitives_init_add_opt(&pPrimitives);
	primitives_init_andor_opt(&pPrimitives);
	primitives_init_alphaComp_opt(&pPrimitives);
	primitives_init_copy_opt(&pPrimitives);
	primitives_init_set_opt(&pPrimitives);
	primitives_init_shift_opt(&pPrimitives);
	primitives_init_sign_opt(&pPrimitives);
	primitives_init_colors_opt(&pPrimitives);
	primitives_init_YCoCg_opt(&pPrimitives);
	primitives_init_YUV_opt(&pPrimitives);
	return TRUE;
}
#endif

/* ------------------------------------------------------------------------- */
primitives_t* primitives_get(void)
{
	InitOnceExecuteOnce(&generic_primitives_InitOnce, primitives_init_generic, NULL, NULL);
#if defined(HAVE_OPTIMIZED_PRIMITIVES)
	InitOnceExecuteOnce(&primitives_InitOnce, primitives_init, NULL, NULL);
#endif
	return &pPrimitives;
}

primitives_t* primitives_get_generic(void)
{
	InitOnceExecuteOnce(&generic_primitives_InitOnce, primitives_init_generic, NULL, NULL);
	return &pPrimitivesGeneric;
}

