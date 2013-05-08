/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Applications Integrated Locally (RAIL) Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>

#include <winpr/crt.h>

#include <freerdp/types.h>
#include <freerdp/utils/rail.h>

#include <freerdp/rail.h>

void rail_unicode_string_alloc(RAIL_UNICODE_STRING* unicode_string, UINT16 cbString)
{
	unicode_string->length = cbString;
	unicode_string->string = malloc(cbString);
	memset(unicode_string->string, 0, cbString);
}

void rail_unicode_string_free(RAIL_UNICODE_STRING* unicode_string)
{
	unicode_string->length = 0;

	if (unicode_string->string != NULL)
		free(unicode_string->string);
}

BOOL rail_read_unicode_string(wStream* s, RAIL_UNICODE_STRING* unicode_string)
{
	if (Stream_GetRemainingLength(s) < 2)
		return FALSE;

	Stream_Read_UINT16(s, unicode_string->length); /* cbString (2 bytes) */

	if (Stream_GetRemainingLength(s) < unicode_string->length)
		return FALSE;

	if (unicode_string->string == NULL)
		unicode_string->string = (BYTE*) malloc(unicode_string->length);
	else
		unicode_string->string = (BYTE*) realloc(unicode_string->string, unicode_string->length);

	Stream_Read(s, unicode_string->string, unicode_string->length);

	return TRUE;
}

void rail_write_unicode_string(wStream* s, RAIL_UNICODE_STRING* unicode_string)
{
	Stream_EnsureRemainingCapacity(s, 2 + unicode_string->length);
	Stream_Write_UINT16(s, unicode_string->length); /* cbString (2 bytes) */
	Stream_Write(s, unicode_string->string, unicode_string->length); /* string */
}

void rail_write_unicode_string_value(wStream* s, RAIL_UNICODE_STRING* unicode_string)
{
	if (unicode_string->length > 0)
	{
		Stream_EnsureRemainingCapacity(s, unicode_string->length);
		Stream_Write(s, unicode_string->string, unicode_string->length); /* string */
	}
}

void rail_read_rectangle_16(wStream* s, RECTANGLE_16* rectangle_16)
{
	Stream_Read_UINT16(s, rectangle_16->left); /* left (2 bytes) */
	Stream_Read_UINT16(s, rectangle_16->top); /* top (2 bytes) */
	Stream_Read_UINT16(s, rectangle_16->right); /* right (2 bytes) */
	Stream_Read_UINT16(s, rectangle_16->bottom); /* bottom (2 bytes) */
}

void rail_write_rectangle_16(wStream* s, RECTANGLE_16* rectangle_16)
{
	Stream_Write_UINT16(s, rectangle_16->left); /* left (2 bytes) */
	Stream_Write_UINT16(s, rectangle_16->top); /* top (2 bytes) */
	Stream_Write_UINT16(s, rectangle_16->right); /* right (2 bytes) */
	Stream_Write_UINT16(s, rectangle_16->bottom); /* bottom (2 bytes) */
}

void* rail_clone_order(UINT32 event_type, void* order)
{
	struct
	{
		UINT32 type;
		UINT32 size;
	} ordersize_table[] =
	{
		{RailChannel_GetSystemParam, sizeof(RAIL_SYSPARAM_ORDER)},
		{RailChannel_ServerExecuteResult, sizeof(RAIL_EXEC_RESULT_ORDER)},
		{RailChannel_ServerSystemParam, sizeof(RAIL_SYSPARAM_ORDER)},
		{RailChannel_ServerMinMaxInfo, sizeof(RAIL_MINMAXINFO_ORDER)},
		{RailChannel_ServerLocalMoveSize, sizeof(RAIL_LOCALMOVESIZE_ORDER)},
		{RailChannel_ServerGetAppIdResponse, sizeof(RAIL_GET_APPID_RESP_ORDER)},
		{RailChannel_ServerLanguageBarInfo, sizeof(RAIL_LANGBAR_INFO_ORDER)},
		{RailChannel_ClientSystemParam, sizeof(RAIL_SYSPARAM_ORDER)},
		{RailChannel_ClientExecute, sizeof(RDP_PLUGIN_DATA)},
		{RailChannel_ClientActivate, sizeof(RAIL_ACTIVATE_ORDER)},
		{RailChannel_ClientSystemMenu, sizeof(RAIL_SYSMENU_ORDER)},
		{RailChannel_ClientSystemCommand, sizeof(RAIL_SYSCOMMAND_ORDER)},
		{RailChannel_ClientNotifyEvent, sizeof(RAIL_NOTIFY_EVENT_ORDER)},
		{RailChannel_ClientWindowMove, sizeof(RAIL_WINDOW_MOVE_ORDER)},
		{RailChannel_ClientGetAppIdRequest, sizeof(RAIL_GET_APPID_REQ_ORDER)},
		{RailChannel_ClientLanguageBarInfo, sizeof(RAIL_LANGBAR_INFO_ORDER)},
	};
	size_t i = 0;
	size_t order_size = 0;
	void*  new_order = NULL;

	for (i = 0; i < ARRAYSIZE(ordersize_table); i++)
	{
		if (event_type == ordersize_table[i].type)
		{
			order_size = ordersize_table[i].size;
			break;
		}
	}

	// Event type not found.
	if (order_size == 0)
		return NULL;

	new_order = malloc(order_size);
	CopyMemory(new_order, order, order_size);

	//fprintf(stderr, "rail_clone_order: type=%d order=%p\n", event_type, new_order);

	// Create copy of variable data for some orders
	if ((event_type == RailChannel_GetSystemParam) ||
		(event_type == RailChannel_ClientSystemParam))
	{
		RAIL_SYSPARAM_ORDER* new_sysparam = (RAIL_SYSPARAM_ORDER*) new_order;
		RAIL_SYSPARAM_ORDER* old_sysparam = (RAIL_SYSPARAM_ORDER*) order;

		rail_unicode_string_alloc(&new_sysparam->highContrast.colorScheme,
			old_sysparam->highContrast.colorScheme.length);

		CopyMemory(new_sysparam->highContrast.colorScheme.string,
			old_sysparam->highContrast.colorScheme.string,
			old_sysparam->highContrast.colorScheme.length);
	}

	if (event_type == RailChannel_ServerExecuteResult)
	{
		RAIL_EXEC_RESULT_ORDER* new_exec_result = (RAIL_EXEC_RESULT_ORDER*) new_order;
		RAIL_EXEC_RESULT_ORDER* old_exec_result = (RAIL_EXEC_RESULT_ORDER*) order;

		rail_unicode_string_alloc(&new_exec_result->exeOrFile,
				old_exec_result->exeOrFile.length);

		CopyMemory(new_exec_result->exeOrFile.string,
			old_exec_result->exeOrFile.string,
			old_exec_result->exeOrFile.length);
	}

	if (event_type == RailChannel_ServerGetAppIdResponse)
	{
		RAIL_GET_APPID_RESP_ORDER* new_app_resp = (RAIL_GET_APPID_RESP_ORDER*)new_order;

		new_app_resp->applicationId.string = &new_app_resp->applicationIdBuffer[0];
	}

	return new_order;
}

void rail_free_cloned_order(UINT32 event_type, void* order)
{
	//fprintf(stderr, "rail_free_cloned_order: type=%d order=%p\n", event_type, order);
	if ((event_type == RailChannel_GetSystemParam) ||
		(event_type == RailChannel_ClientSystemParam))
	{
		RAIL_SYSPARAM_ORDER* sysparam = (RAIL_SYSPARAM_ORDER*) order;
		rail_unicode_string_free(&sysparam->highContrast.colorScheme);
	}

	if (event_type == RailChannel_ServerExecuteResult)
	{
		RAIL_EXEC_RESULT_ORDER* exec_result = (RAIL_EXEC_RESULT_ORDER*) order;
		rail_unicode_string_free(&exec_result->exeOrFile);
	}

	free(order);
}
