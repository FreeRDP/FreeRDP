/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include <freerdp/types.h>
#include <freerdp/utils/memory.h>

#include <freerdp/utils/rail.h>
#include <freerdp/rail.h>

void rail_unicode_string_alloc(UNICODE_STRING* unicode_string, uint16 cbString)
{
	unicode_string->length = cbString;
	unicode_string->string = xzalloc(cbString);
}

void rail_unicode_string_free(UNICODE_STRING* unicode_string)
{
	unicode_string->length = 0;

	if (unicode_string->string != NULL)
		xfree(unicode_string->string);
}

void rail_read_unicode_string(STREAM* s, UNICODE_STRING* unicode_string)
{
	stream_read_uint16(s, unicode_string->length); /* cbString (2 bytes) */

	if (unicode_string->string == NULL)
		unicode_string->string = (uint8*) xmalloc(unicode_string->length);
	else
		unicode_string->string = (uint8*) xrealloc(unicode_string->string, unicode_string->length);

	stream_read(s, unicode_string->string, unicode_string->length);
}

void rail_write_unicode_string(STREAM* s, UNICODE_STRING* unicode_string)
{
	stream_check_size(s, 2 + unicode_string->length);
	stream_write_uint16(s, unicode_string->length); /* cbString (2 bytes) */
	stream_write(s, unicode_string->string, unicode_string->length); /* string */
}

void rail_write_unicode_string_value(STREAM* s, UNICODE_STRING* unicode_string)
{
	if (unicode_string->length > 0)
	{
		stream_check_size(s, unicode_string->length);
		stream_write(s, unicode_string->string, unicode_string->length); /* string */
	}
}

void rail_read_rectangle_16(STREAM* s, RECTANGLE_16* rectangle_16)
{
	stream_read_uint16(s, rectangle_16->left); /* left (2 bytes) */
	stream_read_uint16(s, rectangle_16->top); /* top (2 bytes) */
	stream_read_uint16(s, rectangle_16->right); /* right (2 bytes) */
	stream_read_uint16(s, rectangle_16->bottom); /* bottom (2 bytes) */
}

void rail_write_rectangle_16(STREAM* s, RECTANGLE_16* rectangle_16)
{
	stream_write_uint16(s, rectangle_16->left); /* left (2 bytes) */
	stream_write_uint16(s, rectangle_16->top); /* top (2 bytes) */
	stream_write_uint16(s, rectangle_16->right); /* right (2 bytes) */
	stream_write_uint16(s, rectangle_16->bottom); /* bottom (2 bytes) */
}

void* rail_clone_order(uint32 event_type, void* order)
{
	struct
	{
		uint32 type;
		uint32 size;
	} ordersize_table[] =
	{
		{RDP_EVENT_TYPE_RAIL_CHANNEL_GET_SYSPARAMS, sizeof(RAIL_SYSPARAM_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CHANNEL_EXEC_RESULTS, sizeof(RAIL_EXEC_RESULT_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_SYSPARAM, sizeof(RAIL_SYSPARAM_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_MINMAXINFO, sizeof(RAIL_MINMAXINFO_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CHANNEL_SERVER_LOCALMOVESIZE, sizeof(RAIL_LOCALMOVESIZE_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CHANNEL_APPID_RESP, sizeof(RAIL_GET_APPID_RESP_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CHANNEL_LANGBARINFO, sizeof(RAIL_LANGBAR_INFO_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CLIENT_SET_SYSPARAMS, sizeof(RAIL_SYSPARAM_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CLIENT_EXEC_REMOTE_APP, sizeof(RDP_PLUGIN_DATA)},
		{RDP_EVENT_TYPE_RAIL_CLIENT_ACTIVATE, sizeof(RAIL_ACTIVATE_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CLIENT_SYSMENU, sizeof(RAIL_SYSMENU_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CLIENT_SYSCOMMAND, sizeof(RAIL_SYSCOMMAND_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CLIENT_NOTIFY_EVENT, sizeof(RAIL_NOTIFY_EVENT_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CLIENT_WINDOW_MOVE, sizeof(RAIL_WINDOW_MOVE_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CLIENT_APPID_REQ, sizeof(RAIL_GET_APPID_REQ_ORDER)},
		{RDP_EVENT_TYPE_RAIL_CLIENT_LANGBARINFO, sizeof(RAIL_LANGBAR_INFO_ORDER)},
	};
	size_t i = 0;
	size_t order_size = 0;
	void*  new_order = NULL;

	for (i = 0; i < RAIL_ARRAY_SIZE(ordersize_table); i++)
	{
		if (event_type == ordersize_table[i].type)
		{
			order_size = ordersize_table[i].size;
			break;
		}
	}

	// Event type not found.
	if (order_size == 0) return NULL;

	new_order = xmalloc(order_size);
	memcpy(new_order, order, order_size);

	//printf("rail_clone_order: type=%d order=%p\n", event_type, new_order);

	// Create copy of variable data for some orders
	if ((event_type == RDP_EVENT_TYPE_RAIL_CHANNEL_GET_SYSPARAMS) ||
		(event_type == RDP_EVENT_TYPE_RAIL_CLIENT_SET_SYSPARAMS))
	{
		RAIL_SYSPARAM_ORDER* new_sysparam = (RAIL_SYSPARAM_ORDER*)new_order;
		RAIL_SYSPARAM_ORDER* old_sysparam = (RAIL_SYSPARAM_ORDER*)order;

		rail_unicode_string_alloc(&new_sysparam->highContrast.colorScheme,
			old_sysparam->highContrast.colorScheme.length);

		memcpy(new_sysparam->highContrast.colorScheme.string,
			old_sysparam->highContrast.colorScheme.string,
			old_sysparam->highContrast.colorScheme.length);
	}

	if (event_type == RDP_EVENT_TYPE_RAIL_CHANNEL_EXEC_RESULTS)
	{
		RAIL_EXEC_RESULT_ORDER* new_exec_result = (RAIL_EXEC_RESULT_ORDER*)new_order;
		RAIL_EXEC_RESULT_ORDER* old_exec_result = (RAIL_EXEC_RESULT_ORDER*)order;

		rail_unicode_string_alloc(&new_exec_result->exeOrFile,
				old_exec_result->exeOrFile.length);

		memcpy(new_exec_result->exeOrFile.string,
			old_exec_result->exeOrFile.string,
			old_exec_result->exeOrFile.length);
	}

	if (event_type == RDP_EVENT_TYPE_RAIL_CHANNEL_APPID_RESP)
	{
		RAIL_GET_APPID_RESP_ORDER* new_app_resp = (RAIL_GET_APPID_RESP_ORDER*)new_order;

		new_app_resp->applicationId.string = &new_app_resp->applicationIdBuffer[0];
	}

	return new_order;
}

void rail_free_cloned_order(uint32 event_type, void* order)
{
	//printf("rail_free_cloned_order: type=%d order=%p\n", event_type, order);
	if ((event_type == RDP_EVENT_TYPE_RAIL_CHANNEL_GET_SYSPARAMS) ||
		(event_type == RDP_EVENT_TYPE_RAIL_CLIENT_SET_SYSPARAMS))
	{
		RAIL_SYSPARAM_ORDER* sysparam = (RAIL_SYSPARAM_ORDER*)order;
		rail_unicode_string_free(&sysparam->highContrast.colorScheme);
	}

	if (event_type == RDP_EVENT_TYPE_RAIL_CHANNEL_EXEC_RESULTS)
	{
		RAIL_EXEC_RESULT_ORDER* exec_result = (RAIL_EXEC_RESULT_ORDER*)order;
		rail_unicode_string_free(&exec_result->exeOrFile);
	}
	xfree(order);
}
