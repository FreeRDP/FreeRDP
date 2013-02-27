/* primitives.c
 * This code queries processor features and calls the init/deinit routines.
 * vi:ts=4 sw=4
 *
 * Copyright 2011 Martin Fleisz <mfleisz@thinstuff.com>
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

#include <freerdp/primitives.h>

#include "prim_internal.h"

/* Singleton pointer used throughout the program when requested. */
static primitives_t* pPrimitives = NULL;

/* ------------------------------------------------------------------------- */
void primitives_init(void)
{
	if (pPrimitives == NULL)
	{
		pPrimitives = calloc(1, sizeof(primitives_t));

		if (pPrimitives == NULL)
			return;
	}

	/* Now call each section's initialization routine. */
	primitives_init_add(pPrimitives);
	primitives_init_andor(pPrimitives);
	primitives_init_alphaComp(pPrimitives);
	primitives_init_copy(pPrimitives);
	primitives_init_set(pPrimitives);
	primitives_init_shift(pPrimitives);
	primitives_init_sign(pPrimitives);
	primitives_init_colors(pPrimitives);
}

/* ------------------------------------------------------------------------- */
primitives_t* primitives_get(void)
{
	if (pPrimitives == NULL)
		primitives_init();

	return pPrimitives;
}

/* ------------------------------------------------------------------------- */
void primitives_deinit(void)
{
	if (pPrimitives == NULL)
		return;

	/* Call each section's de-initialization routine. */
	primitives_deinit_add(pPrimitives);
	primitives_deinit_andor(pPrimitives);
	primitives_deinit_alphaComp(pPrimitives);
	primitives_deinit_copy(pPrimitives);
	primitives_deinit_set(pPrimitives);
	primitives_deinit_shift(pPrimitives);
	primitives_deinit_sign(pPrimitives);
	primitives_deinit_colors(pPrimitives);

	free((void*) pPrimitives);
	pPrimitives = NULL;
}
