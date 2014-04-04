/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smart Card Structure Packing
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "smartcard_pack.h"

UINT32 smartcard_unpack_establish_context_call(SMARTCARD_DEVICE* smartcard, wStream* s, EstablishContext_Call* call)
{
	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "EstablishContext_Call is too short: Actual: %d, Expected: %d\n",
				(int) Stream_GetRemainingLength(s), 4);
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(s, call->dwScope); /* dwScope (4 bytes) */

	return SCARD_S_SUCCESS;
}

UINT32 smartcard_unpack_list_readers_call(SMARTCARD_DEVICE* smartcard, wStream* s, ListReaders_Call* call)
{
	if (Stream_GetRemainingLength(s) < 16)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return SCARD_F_INTERNAL_ERROR;
	}

	Stream_Read_UINT32(s, call->cBytes); /* cBytes (4 bytes) */

	if (Stream_GetRemainingLength(s) < call->cBytes)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call is too short: Actual: %d, Expected: %d",
				(int) Stream_GetRemainingLength(s), call->cBytes);
		return SCARD_F_INTERNAL_ERROR;
	}

	if (call->cBytes)
	{
		call->mszGroups = malloc(call->cBytes);
		Stream_Read(s, call->mszGroups, call->cBytes); /* mszGroups */
	}
	else
	{
		call->mszGroups = NULL;
		Stream_Seek(s, 4); /* mszGroups */
	}

	Stream_Read_UINT32(s, call->fmszReadersIsNULL); /* fmszReadersIsNULL (4 bytes) */
	Stream_Read_UINT32(s, call->cchReaders); /* cchReaders (4 bytes) */

	if (Stream_GetRemainingLength(s) < 4)
	{
		WLog_Print(smartcard->log, WLOG_WARN, "ListReaders_Call is too short: %d",
				(int) Stream_GetRemainingLength(s));
		return SCARD_F_INTERNAL_ERROR;
	}

	return SCARD_S_SUCCESS;
}
