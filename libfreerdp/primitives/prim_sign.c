/* FreeRDP: A Remote Desktop Protocol Client
 * Sign operations.
 * vi:ts=4 sw=4:
 *
 * (c) Copyright 2012 Hewlett-Packard Development Company, L.P.
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

#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

/* ----------------------------------------------------------------------------
 * Set pDst to the sign-value of the 16-bit values in pSrc (-1, 0, or 1).
 */
static pstatus_t general_sign_16s(const INT16* pSrc, INT16* pDst, UINT32 len)
{
	while (len--)
	{
		INT16 src = *pSrc++;
		*pDst++ = (src < 0) ? (-1) : ((src > 0) ? 1 : 0);
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_sign(primitives_t* prims)
{
	/* Start with the default. */
	prims->sign_16s = general_sign_16s;
}
