/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Remote Applications Integrated Locally (RAIL)
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/constants.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/rail.h>

#include "rail_core.h"
#include "rail_channel_orders.h"

/*
// Initialization stage in UI for RAIL:
// 1) create a sequence of rail_notify_client_sysparam_update()
//    calls with all current client system parameters
//
// 2) if Language Bar capability enabled - call updating Language Bar PDU
//
// 3) prepare and call rail_client_execute(exe_or_file, working_dir, args)
//
*/

/*
Flow of init stage over channel;

   Client notify UI about session start
   and go to RAIL_ESTABLISHING state.

   Client wait for Server Handshake PDU
   	   	   	   	   	   	   	   	   	   	   	   	   Server send Handshake request
   Client check Handshake response.
   If NOT OK - exit with specified reason
   Client send Handshake response
   	   	   	   	   	   	   	   	   	   	   	   	   Server send Server System
   	   	   	   	   	   	   	   	   	   	   	   	   Parameters Update (in parallel)
   Client send Client Information
   Client send Client System Parameters Update
   Client send Client Execute
   Server send Server Execute Result
   Client check Server Execute Result. If NOT OK - exit with specified reason

   Client notify UI about success session establishing and go to
   RAIL_ESTABLISHED state.
*/

void init_vchannel_event(RAIL_VCHANNEL_EVENT* event, uint32 event_id)
{
	memset(event, 0, sizeof(RAIL_VCHANNEL_EVENT));
	event->event_id = event_id;
}

void init_rail_string(RAIL_STRING * rail_string, const char * string)
{
	rail_string->buffer = (uint8*)string;
	rail_string->length = strlen(string) + 1;
}

void rail_string2unicode_string(RAIL_SESSION* session, RAIL_STRING* string, UNICODE_STRING* unicode_string)
{
	size_t   result_length = 0;
	char*    result_buffer = NULL;

	unicode_string->string = NULL;
	unicode_string->length = 0;

	if (string->length == 0) return;

	result_buffer = freerdp_uniconv_out(session->uniconv, (char*) string->buffer, &result_length);

	unicode_string->string = (uint8*) result_buffer;
	unicode_string->length = (uint16) result_length;
}

void rail_unicode_string2string(RAIL_SESSION* session, UNICODE_STRING* unicode_string, RAIL_STRING* string)
{
	char* result_buffer = NULL;

	string->buffer = NULL;
	string->length = 0;

	if (unicode_string->length == 0)
		return;

	result_buffer = freerdp_uniconv_in(session->uniconv, unicode_string->string, unicode_string->length);

	string->buffer = (uint8*)result_buffer;
	string->length = strlen(result_buffer) + 1;
}

RAIL_SESSION* rail_core_session_new(RAIL_VCHANNEL_DATA_SENDER* data_sender, RAIL_VCHANNEL_EVENT_SENDER* event_sender)
{
	RAIL_SESSION* self;

	self = (RAIL_SESSION*) xzalloc(sizeof(RAIL_SESSION));

	if (self != NULL)
	{
		self->data_sender = data_sender;
		self->event_sender = event_sender;
		self->uniconv = freerdp_uniconv_new();
	}

	return self;
}

void rail_core_session_free(RAIL_SESSION* rail_session)
{
	if (rail_session != NULL)
	{
		freerdp_uniconv_free(rail_session->uniconv);
		xfree(rail_session);
	}
}

void rail_core_on_channel_connected(RAIL_SESSION* session)
{
	DEBUG_RAIL("RAIL channel connected.");
}

void rail_core_on_channel_terminated(RAIL_SESSION* session)
{
	DEBUG_RAIL("RAIL channel terminated.");
}

void rail_core_handle_server_handshake(RAIL_SESSION* session, uint32 build_number)
{
	uint32 client_build_number = 0x00001db0;
	RAIL_VCHANNEL_EVENT event = {0};

	DEBUG_RAIL("rail_core_handle_server_handshake: session=0x%p buildNumber=0x%X.", session, build_number);

	// Step 1. Send Handshake PDU (2.2.2.2.1)
	// Fixed: MS-RDPERP 1.3.2.1 is not correct!
	rail_vchannel_send_handshake_order(session, client_build_number);

	// Step 2. Send Client Information PDU (2.2.2.2.1)
	rail_vchannel_send_client_information_order(session, RAIL_CLIENTSTATUS_ALLOWLOCALMOVESIZE);

	// Step 3. Notify UI about session establishing and about requirements to
	//         start UI initialization stage.
	init_vchannel_event(&event, RAIL_VCHANNEL_EVENT_SESSION_ESTABLISHED);
	session->event_sender->send_rail_vchannel_event(session->event_sender->event_sender_object, &event);

	// Step 4. Send Client Execute
	// FIXME:
	// According to "3.1.1.1 Server State Machine" Client Execute
	// will be processed after Destop Sync processed.
	// So maybe send after receive Destop Sync sequence?
	rail_core_send_client_execute(session, False, "||firefox", "", "");
}

void rail_core_handle_exec_result(RAIL_SESSION* session, uint16 flags, uint16 exec_result, uint32 raw_result, UNICODE_STRING* exe_or_file)
{
	RAIL_VCHANNEL_EVENT event = {0};
	RAIL_STRING exe_or_file_;

	DEBUG_RAIL("rail_core_handle_exec_result: session=0x%p flags=0x%X "
		"exec_result=0x%X raw_result=0x%X exe_or_file=(length=%d dump>)",
		session, flags, exec_result, raw_result, exe_or_file->length);

#ifdef WITH_DEBUG_RAIL
	freerdp_hexdump(exe_or_file->string, exe_or_file->length);
#endif

	rail_unicode_string2string(session, exe_or_file, &exe_or_file_);

	init_vchannel_event(&event, RAIL_VCHANNEL_EVENT_EXEC_RESULT_RETURNED);
	event.param.exec_result_info.flags = flags;
	event.param.exec_result_info.exec_result = exec_result;
	event.param.exec_result_info.raw_result = raw_result;
	event.param.exec_result_info.exe_or_file = exe_or_file_.buffer;

	session->event_sender->send_rail_vchannel_event(session->event_sender->event_sender_object, &event);
}

void rail_core_handle_server_sysparam(RAIL_SESSION* session, RAIL_SERVER_SYSPARAM* sysparam)
{
	RAIL_VCHANNEL_EVENT event = {0};

	DEBUG_RAIL("rail_core_handle_server_sysparam: session=0x%p "
		"type=0x%X scr_enabled=%d scr_lock_enabled=%d",
		session, sysparam->type, sysparam->value.screen_saver_enabled,
		sysparam->value.screen_saver_lock_enabled);

	init_vchannel_event(&event, RAIL_VCHANNEL_EVENT_SERVER_SYSPARAM_RECEIVED);
	event.param.server_param_info.param_type = sysparam->type;
	event.param.server_param_info.screen_saver_enabled =
		((sysparam->value.screen_saver_enabled != 0) ? True: False);

	event.param.server_param_info.screen_saver_lock_enabled =
		((sysparam->value.screen_saver_lock_enabled != 0) ? True: False);

	session->event_sender->send_rail_vchannel_event(session->event_sender->event_sender_object, &event);
}

void rail_core_handle_server_movesize(RAIL_SESSION* session, uint32 window_id,
		uint16 move_size_started, uint16 move_size_type, uint16 pos_x, uint16 pos_y)
{
	RAIL_VCHANNEL_EVENT event = {0};

	DEBUG_RAIL("rail_core_handle_server_movesize: session=0x%p "
		"window_id=0x%X started=%d move_size_type=%d pos_x=%d pos_y=%d",
		session, window_id, move_size_started, move_size_type, pos_x, pos_y);

	init_vchannel_event(&event, ((move_size_started != 0) ?
		RAIL_VCHANNEL_EVENT_MOVESIZE_STARTED:
		RAIL_VCHANNEL_EVENT_MOVESIZE_FINISHED));

	event.param.movesize_info.window_id = window_id;
	event.param.movesize_info.move_size_type = move_size_type;
	event.param.movesize_info.pos_x = pos_x;
	event.param.movesize_info.pos_y = pos_y;

	session->event_sender->send_rail_vchannel_event(session->event_sender->event_sender_object, &event);
}

void rail_core_handle_server_minmax_info(RAIL_SESSION* session,
	uint32 window_id, uint16 max_width, uint16 max_height, uint16 max_pos_x, uint16 max_pos_y,
	uint16 min_track_width, uint16 min_track_height, uint16 max_track_width, uint16 max_track_height)
{
	RAIL_VCHANNEL_EVENT event = {0};

	DEBUG_RAIL("rail_core_handle_server_minmax_info: session=0x%p "
		"window_id=0x%X max_width=%d max_height=%d max_pos_x=%d max_pos_y=%d "
		"min_track_width=%d min_track_height=%d max_track_width=%d max_track_height=%d",
		session, window_id, max_width, max_height, max_pos_x, max_pos_y,
		min_track_width, min_track_height, max_track_width, max_track_height);

	init_vchannel_event(&event, RAIL_VCHANNEL_EVENT_MINMAX_INFO_UPDATED);

	event.param.minmax_info.window_id = window_id;

	event.param.minmax_info.max_width = max_width;
	event.param.minmax_info.max_height = max_height;
	event.param.minmax_info.max_pos_x = max_pos_x;
	event.param.minmax_info.max_pos_y = max_pos_y;
	event.param.minmax_info.min_track_width = min_track_width;
	event.param.minmax_info.min_track_height = min_track_height;
	event.param.minmax_info.max_track_width = max_track_width;
	event.param.minmax_info.max_track_height = max_track_height;

	session->event_sender->send_rail_vchannel_event(session->event_sender->event_sender_object, &event);
}

void rail_core_handle_server_langbar_info(RAIL_SESSION* session, uint32 langbar_status)
{
	RAIL_VCHANNEL_EVENT event = {0};

	DEBUG_RAIL("rail_core_handle_server_langbar_info: session=0x%p "
		"langbar_status=0x%X", session, langbar_status);

	init_vchannel_event(&event, RAIL_VCHANNEL_EVENT_LANGBAR_STATUS_UPDATED);

	event.param.langbar_info.status = langbar_status;

	session->event_sender->send_rail_vchannel_event(session->event_sender->event_sender_object, &event);
}

void rail_core_handle_server_get_app_resp(RAIL_SESSION* session, uint32 window_id, UNICODE_STRING * app_id)
{
	RAIL_VCHANNEL_EVENT event = { 0 };
	RAIL_STRING app_id_;

	DEBUG_RAIL("rail_core_handle_server_get_app_resp: session=0x%p "
		"window_id=0x%X app_id=(length=%d dump>)", session, window_id, app_id->length);

#ifdef WITH_DEBUG_RAIL
	freerdp_hexdump(app_id->string, app_id->length);
#endif

	rail_unicode_string2string(session, app_id, &app_id_);

	init_vchannel_event(&event, RAIL_VCHANNEL_EVENT_LANGBAR_STATUS_UPDATED);

	event.param.app_response_info.window_id= window_id;
	event.param.app_response_info.application_id = app_id_.buffer;

	session->event_sender->send_rail_vchannel_event(session->event_sender->event_sender_object, &event);
}

void rail_core_send_client_execute(RAIL_SESSION* session,
	boolean exec_or_file_is_file_path, const char* rail_exe_or_file,
	const char* rail_working_directory, const char* rail_arguments)
{
	RAIL_STRING exe_or_file_;
	RAIL_STRING working_directory_;
	RAIL_STRING arguments_;
	UNICODE_STRING exe_or_file;
	UNICODE_STRING working_directory;
	UNICODE_STRING arguments;
	uint16 flags;

	DEBUG_RAIL("RAIL_ORDER_EXEC");

	init_rail_string(&exe_or_file_, rail_exe_or_file);
	init_rail_string(&working_directory_, rail_working_directory);
	init_rail_string(&arguments_, rail_arguments);

	rail_string2unicode_string(session, &exe_or_file_, &exe_or_file);
	rail_string2unicode_string(session, &working_directory_, &working_directory);
	rail_string2unicode_string(session, &arguments_, &arguments);

	flags = (RAIL_EXEC_FLAG_EXPAND_WORKINGDIRECTORY | RAIL_EXEC_FLAG_EXPAND_ARGUMENTS);

	if (exec_or_file_is_file_path)
	{
		flags |= (RAIL_EXEC_FLAG_TRANSLATE_FILES | RAIL_EXEC_FLAG_FILE);
	}

	rail_vchannel_send_exec_order(session, flags, &exe_or_file,
		&working_directory,	&arguments);

	rail_unicode_string_free(&exe_or_file);
	rail_unicode_string_free(&working_directory);
	rail_unicode_string_free(&arguments);
}

uint8 boolean2uint8(boolean value)
{
	return ((value == True) ? 1 : 0);
}

uint8 copy_rail_rect_16(RECTANGLE_16* src, RECTANGLE_16* dst)
{
	memcpy(dst, src, sizeof(RECTANGLE_16));
	return 0;
}

void rail_core_handle_ui_update_client_sysparam(RAIL_SESSION* session, RAIL_UI_EVENT* event)
{
	RAIL_CLIENT_SYSPARAM sys_param;

	memset(&sys_param, 0, sizeof(sys_param));

	sys_param.type = event->param.sysparam_info.param;

	sys_param.value.full_window_drag_enabled =
		boolean2uint8(event->param.sysparam_info.value.full_window_drag_enabled);

	sys_param.value.menu_access_key_always_underlined =
		boolean2uint8(event->param.sysparam_info.value.menu_access_key_always_underlined);

	sys_param.value.keyboard_for_user_prefered =
		boolean2uint8(event->param.sysparam_info.value.keyboard_for_user_prefered);

	sys_param.value.left_right_mouse_buttons_swapped =
		boolean2uint8(event->param.sysparam_info.value.left_right_mouse_buttons_swapped);

	copy_rail_rect_16(&event->param.sysparam_info.value.work_area,
		&sys_param.value.work_area);

	copy_rail_rect_16(&event->param.sysparam_info.value.display_resolution,
		&sys_param.value.display_resolution);

	copy_rail_rect_16(&event->param.sysparam_info.value.taskbar_size,
		&sys_param.value.taskbar_size);

	sys_param.value.high_contrast_system_info.flags =
		event->param.sysparam_info.value.high_contrast_system_info.flags;


	if (sys_param.type == SPI_SET_HIGH_CONTRAST)
	{
		RAIL_STRING color_scheme;

		init_rail_string(&color_scheme,
			event->param.sysparam_info.value.high_contrast_system_info.color_scheme);

		rail_string2unicode_string(session, &color_scheme, &sys_param.value.high_contrast_system_info.color_scheme);
	}

	rail_vchannel_send_client_sysparam_update_order(session, &sys_param);
	rail_unicode_string_free(&sys_param.value.high_contrast_system_info.color_scheme);
}

static void rail_core_handle_ui_execute_remote_app(RAIL_SESSION* session, RAIL_UI_EVENT* event)
{
	rail_core_send_client_execute(session,
		event->param.execute_info.exec_or_file_is_file_path,
		event->param.execute_info.exe_or_file,
		event->param.execute_info.working_directory,
		event->param.execute_info.arguments);
}

static void rail_core_handle_ui_activate(RAIL_SESSION* session, RAIL_UI_EVENT* event)
{
	rail_vchannel_send_activate_order(
		session, event->param.activate_info.window_id,
		((event->param.activate_info.enabled == True) ? 1 : 0));
}

static void rail_core_handle_ui_sys_command(RAIL_SESSION* session, RAIL_UI_EVENT* event)
{
	rail_vchannel_send_syscommand_order(session,
		event->param.syscommand_info.window_id,
		event->param.syscommand_info.syscommand);
}

static void rail_core_handle_ui_notify(RAIL_SESSION* session, RAIL_UI_EVENT* event)
{
	rail_vchannel_send_notify_event_order(session,
		event->param.notify_info.window_id,
		event->param.notify_info.notify_icon_id,
		event->param.notify_info.message);
}

static void rail_core_handle_ui_window_move(RAIL_SESSION* session, RAIL_UI_EVENT* event)
{
	rail_vchannel_send_client_windowmove_order(session,
		event->param.window_move_info.window_id,
		&event->param.window_move_info.new_position);
}

static void rail_core_handle_ui_system_menu(RAIL_SESSION* session, RAIL_UI_EVENT* event)
{
	rail_vchannel_send_client_system_menu_order(session,
		event->param.system_menu_info.window_id,
		event->param.system_menu_info.left,
		event->param.system_menu_info.top);
}

static void rail_core_handle_ui_get_app_id(RAIL_SESSION* session, RAIL_UI_EVENT* event)
{
	rail_vchannel_send_get_appid_req_order(session, event->param.get_app_id_info.window_id);
}

void rail_core_handle_ui_event(RAIL_SESSION* session, RAIL_UI_EVENT* event)
{

	struct
	{
		uint32 event_id;
		void (*event_handler)(RAIL_SESSION* session, RAIL_UI_EVENT* event);
	} handlers_table[] =
	{
			{RAIL_UI_EVENT_UPDATE_CLIENT_SYSPARAM, rail_core_handle_ui_update_client_sysparam},
			{RAIL_UI_EVENT_EXECUTE_REMOTE_APP, rail_core_handle_ui_execute_remote_app},
			{RAIL_UI_EVENT_ACTIVATE, rail_core_handle_ui_activate},
			{RAIL_UI_EVENT_SYS_COMMAND, rail_core_handle_ui_sys_command},
			{RAIL_UI_EVENT_NOTIFY, rail_core_handle_ui_notify},
			{RAIL_UI_EVENT_WINDOW_MOVE, rail_core_handle_ui_window_move},
			{RAIL_UI_EVENT_SYSTEM_MENU, rail_core_handle_ui_system_menu},
			{RAIL_UI_EVENT_LANGBAR_INFO, rail_core_handle_ui_system_menu},
			{RAIL_UI_EVENT_GET_APP_ID, rail_core_handle_ui_get_app_id}
	};

	int i = 0;

	for (i = 0; i < RAIL_ARRAY_SIZE(handlers_table); i++)
	{
		if ((event->event_id == handlers_table[i].event_id) &&
			(handlers_table[i].event_handler != NULL))
		{
			(handlers_table[i].event_handler)(session, event);
		}
	}
}

