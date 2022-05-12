/* FreeRDP: A Remote Desktop Protocol Client
 * Routines to set a chunk of memory to a constant.
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

#include <freerdp/config.h>

#include <string.h>

#include <freerdp/types.h>
#include <freerdp/primitives.h>

#include "prim_internal.h"

/* ========================================================================= */
static pstatus_t general_set_8u(BYTE val, BYTE* pDst, UINT32 len)
{
	memset((void*)pDst, (int)val, (size_t)len);
	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static pstatus_t general_zero(void* pDst, size_t len)
{
	memset(pDst, 0, len);
	return PRIMITIVES_SUCCESS;
}

/* ========================================================================= */
static pstatus_t general_set_32s(INT32 val, INT32* pDst, UINT32 len)
{
	INT32* dptr = (INT32*)pDst;
	size_t span, remaining;
	primitives_t* prims;

	if (len < 256)
	{
		while (len--)
			*dptr++ = val;

		return PRIMITIVES_SUCCESS;
	}

	/* else quadratic growth memcpy algorithm */
	span = 1;
	*dptr = val;
	remaining = len - 1;
	prims = primitives_get();

	while (remaining)
	{
		size_t thiswidth = span;

		if (thiswidth > remaining)
			thiswidth = remaining;

		prims->copy_8u((BYTE*)dptr, (BYTE*)(dptr + span), thiswidth << 2);
		remaining -= thiswidth;
		span <<= 1;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
static pstatus_t general_set_32u(UINT32 val, UINT32* pDst, UINT32 len)
{
	UINT32* dptr = (UINT32*)pDst;
	size_t span, remaining;
	primitives_t* prims;

	if (len < 256)
	{
		while (len--)
			*dptr++ = val;

		return PRIMITIVES_SUCCESS;
	}

	/* else quadratic growth memcpy algorithm */
	span = 1;
	*dptr = val;
	remaining = len - 1;
	prims = primitives_get();

	while (remaining)
	{
		size_t thiswidth = span;

		if (thiswidth > remaining)
			thiswidth = remaining;

		prims->copy_8u((BYTE*)dptr, (BYTE*)(dptr + span), thiswidth << 2);
		remaining -= thiswidth;
		span <<= 1;
	}

	return PRIMITIVES_SUCCESS;
}

/* ------------------------------------------------------------------------- */
void primitives_init_set(primitives_t* prims)
{
	/* Start with the default. */
	prims->set_8u = general_set_8u;
	prims->set_32s = general_set_32s;
	prims->set_32u = general_set_32u;
	prims->zero = general_zero;
}
