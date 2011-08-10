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

#ifndef __RAIL_CORE_H
#define	__RAIL_CORE_H

#include <freerdp/types.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/unicode.h>
#include <freerdp/rail.h>

#include <assert.h>
#include <freerdp/utils/debug.h>

#define WITH_DEBUG_RAIL	1

#ifdef WITH_DEBUG_RAIL
#define DEBUG_RAIL(fmt, ...) DEBUG_CLASS(RAIL, fmt, ## __VA_ARGS__)
#else
#define DEBUG_RAIL(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#define RAIL_ARRAY_SIZE(array)	(sizeof(array) / sizeof(array[0]))

typedef struct _RAIL_STRING
{
	uint16  length;
	uint8	*buffer;
} RAIL_STRING;

typedef struct _RAIL_HIGHCONTRAST
{
	uint32 flags;
	UNICODE_STRING color_scheme;

} RAIL_HIGHCONTRAST;

typedef struct _RAIL_CLIENT_SYSPARAM
{
	uint32 type;

	union
	{
		uint8 full_window_drag_enabled;
		uint8 menu_access_key_always_underlined;
		uint8 keyboard_for_user_prefered;
		uint8 left_right_mouse_buttons_swapped;
		RECTANGLE_16 work_area;
		RECTANGLE_16 display_resolution;
		RECTANGLE_16 taskbar_size;
		RAIL_HIGHCONTRAST high_contrast_system_info;
	} value;
} RAIL_CLIENT_SYSPARAM;

struct _RAIL_SERVER_SYSPARAM
{
	uint32 type;

	union
	{
		uint8 screen_saver_enabled;
		uint8 screen_saver_lock_enabled;
	} value;
};
typedef struct _RAIL_SERVER_SYSPARAM RAIL_SERVER_SYSPARAM;

struct _RAIL_VCHANNEL_DATA_SENDER
{
	void* data_sender_object;
	void  (*send_rail_vchannel_data)(void* sender_object, void* data, size_t length);
};
typedef struct _RAIL_VCHANNEL_DATA_SENDER RAIL_VCHANNEL_DATA_SENDER;

struct _RAIL_VCHANNEL_EVENT_SENDER
{
	void * event_sender_object;
	void (*send_rail_vchannel_event)(void* ui_event_sender_object, RAIL_VCHANNEL_EVENT* event);
};
typedef struct _RAIL_VCHANNEL_EVENT_SENDER RAIL_VCHANNEL_EVENT_SENDER;

struct _RAIL_SESSION
{
	UNICONV* uniconv;
	RAIL_VCHANNEL_DATA_SENDER* data_sender;
	RAIL_VCHANNEL_EVENT_SENDER* event_sender;
};
typedef struct _RAIL_SESSION RAIL_SESSION;

RAIL_SESSION* rail_core_session_new(RAIL_VCHANNEL_DATA_SENDER *data_sender, RAIL_VCHANNEL_EVENT_SENDER *event_sender);
void rail_core_session_free(RAIL_SESSION * rail_session);

// RAIL Core Handlers for events from channel plugin interface

void rail_core_on_channel_connected(RAIL_SESSION* rail_session);
void rail_core_on_channel_terminated(RAIL_SESSION* rail_session);
void rail_core_on_channel_data_received(RAIL_SESSION* rail_session, void* data, size_t length);

// RAIL Core Handlers for events from UI

void rail_core_handle_ui_event(RAIL_SESSION* session, RAIL_UI_EVENT* event);

// RAIL Core Handlers for events from channel orders reader
void rail_core_handle_server_handshake(RAIL_SESSION* session, uint32 build_number);

void rail_core_handle_exec_result(RAIL_SESSION* session, uint16 flags,
		uint16 exec_result, uint32 raw_result, UNICODE_STRING* exe_or_file);

void rail_core_handle_server_sysparam(RAIL_SESSION* session, RAIL_SERVER_SYSPARAM* sysparam);

void rail_core_handle_server_movesize(RAIL_SESSION* session, uint32 window_id,
		uint16 move_size_started, uint16 move_size_type, uint16 pos_x, uint16 pos_y);

void rail_core_handle_server_minmax_info(RAIL_SESSION* session, uint32 window_id,
	uint16 max_width, uint16 max_height, uint16 max_pos_x, uint16 max_pos_y,
	uint16 min_track_width, uint16 min_track_height, uint16 max_track_width, uint16 max_track_height);

void rail_core_handle_server_langbar_info(RAIL_SESSION* session, uint32 langbar_status);
void rail_core_handle_server_get_app_resp(RAIL_SESSION* session, uint32 window_id, UNICODE_STRING* app_id);

void rail_core_send_client_execute(RAIL_SESSION* session,
	boolean exec_or_file_is_file_path, const char* rail_exe_or_file,
	const char* rail_working_directory, const char* rail_arguments);

void rail_core_handle_ui_update_client_sysparam(RAIL_SESSION* session, RAIL_UI_EVENT* event);

#endif /* __RAIL_CORE_H */
