/* FreeRDP: A Remote Desktop Protocol Client
 * Add operations.
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

/* ----------------------------------------------------------------------------
 * 16-bit signed add with saturation (under and over).
 */
static pstatus_t general_add_16s(const INT16* pSrc1, const INT16* pSrc2, INT16* pDst, UINT32 len)
{
	while (len--)
	{
		INT32 k = (INT32)(*pSrc1++) + (INT32)(*pSrc2++);

		if (k > 32767)
			*pDst++ = ((INT16)32767);
		else if (k < -32768)
			*pDst++ = ((INT16)-32768);
		else
			*pDst++ = (INT16)k;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_add(primitives_t* prims)
{
	prims->add_16s = general_add_16s;
}
