/*
   FreeRDP: A Remote Desktop Protocol client.
   Remote Applications Integrated Locally (RAIL)

   Copyright 2009 Marc-Andre Moreau <marcandre.moreau@gmail.com>
   Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/constants.h>
#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/svc_plugin.h>
#include <freerdp/rail.h>

#include "rail_core.h"
#include "rail_main.h"

/*
 * RAIL_PDU_HEADER
 * begin
 *   orderType   = 2 bytes
 *   orderLength = 2 bytes
   end
 */

static size_t RAIL_PDU_HEADER_SIZE = 4;

void stream_init_by_allocated_data(STREAM* s, void* data, size_t size)
{
	s->data = data;
	s->size = size;
	s->p = s->data;
}

void* rail_alloc_order_data(size_t length)
{
	uint8 * order_start = xmalloc(length + RAIL_PDU_HEADER_SIZE);
	return (order_start + RAIL_PDU_HEADER_SIZE);
}

void write_rail_unicode_string_content(STREAM* s, RAIL_UNICODE_STRING* string)
{
	if (string->length > 0)
		stream_write(s, string->buffer, string->length);
}

void write_rail_unicode_string(STREAM* s, RAIL_UNICODE_STRING* string)
{
	stream_write_uint16(s, string->length);

	if (string->length > 0)
		stream_write(s, string->buffer, string->length);
}

void write_rail_rect_16(STREAM* s, RAIL_RECT_16* rect)
{
	stream_write_uint16(s, rect->left); /*Left*/
	stream_write_uint16(s, rect->top); /*Top*/
	stream_write_uint16(s, rect->right); /*Right*/
	stream_write_uint16(s, rect->bottom); /*Bottom*/
}

void read_rail_unicode_string(STREAM* s, RAIL_UNICODE_STRING * string)
{
	stream_read_uint16(s, string->length);

	string->buffer = NULL;
	if (string->length > 0)
	{
		string->buffer = xmalloc(string->length);
		stream_read(s, string->buffer, string->length);
	}
}

void free_rail_unicode_string(RAIL_UNICODE_STRING * string)
{
	if (string->buffer != NULL)
	{
		xfree(string->buffer);
		string->buffer = NULL;
		string->length = 0;
	}
}

// Used by 'rail_vchannel_send_' routines for sending constructed RAIL PDU to
// the 'rail' channel
static void rail_vchannel_send_order_data(RAIL_SESSION* session, uint16 order_type, void* allocated_order_data, uint16 data_length)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;
	uint8* header_start = ((uint8*)allocated_order_data - RAIL_PDU_HEADER_SIZE);


	data_length += RAIL_PDU_HEADER_SIZE;

	stream_init_by_allocated_data(s, header_start, RAIL_PDU_HEADER_SIZE);

	stream_write_uint16(s, order_type);
	stream_write_uint16(s, data_length);

	session->data_sender->send_rail_vchannel_data(
			session->data_sender->data_sender_object,
			header_start, data_length);

	// In there we free memory which we allocated in rail_alloc_order_data(..)
	xfree(header_start);
}

/*
 * The Handshake PDU is exchanged between the server and the client to
 * establish that both endpoints are ready to begin RAIL mode.
 * The server sends the Handshake PDU and the client responds
 * with the Handshake PDU.
 */
void rail_vchannel_send_handshake_order(RAIL_SESSION * session, uint32 build_number)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;
	uint16 data_length = 4;
	void*  data = rail_alloc_order_data(data_length);

	stream_init_by_allocated_data(s, data, data_length);

	stream_write_uint32(s, build_number);

	rail_vchannel_send_order_data(session, RDP_RAIL_ORDER_HANDSHAKE, data, data_length);
}

/*
 * The Client Activate PDU is sent from client to server
 * when a local RAIL window on the client is activated or deactivated.
 */
void rail_vchannel_send_activate_order(RAIL_SESSION* session, uint32 window_id, uint8 enabled)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;
	uint16 data_length = 5;
	void*  data = rail_alloc_order_data(data_length);

	stream_init_by_allocated_data(s, data, data_length);

	stream_write_uint32(s, window_id);
	stream_write_uint8(s, enabled);

	rail_vchannel_send_order_data(session, RDP_RAIL_ORDER_ACTIVATE, data, data_length);
}

/*
 * Indicates a Client Execute PDU from client to server to request that a
 * remote application launch on the server.
 * */
void rail_vchannel_send_exec_order(RAIL_SESSION* session, uint16 flags,
		RAIL_UNICODE_STRING* exe_or_file, RAIL_UNICODE_STRING* working_directory, RAIL_UNICODE_STRING* arguments)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;

	uint16 exe_or_file_length        = exe_or_file->length;
	uint16 working_directory_length  = working_directory->length;
	uint16 arguments_length          = arguments->length;

	size_t data_length =
		2 +                         /*Flags (2 bytes)*/
		2 +                         /*ExeOrFileLength (2 bytes)*/
		2 +                         /*WorkingDirLength (2 bytes)*/
		2 +                         /*ArgumentsLen (2 bytes)*/
		exe_or_file_length +        /*ExeOrFile (variable)*/
		working_directory_length +  /*WorkingDir (variable)*/
		arguments_length;           /*Arguments (variable):*/

	void*  data = rail_alloc_order_data(data_length);

	stream_init_by_allocated_data(s, data, data_length);

	stream_write_uint16(s, flags);
	stream_write_uint16(s, exe_or_file_length);
	stream_write_uint16(s, working_directory_length);
	stream_write_uint16(s, arguments_length);

	write_rail_unicode_string_content(s, exe_or_file);
	write_rail_unicode_string_content(s, working_directory);
	write_rail_unicode_string_content(s, arguments);

	rail_vchannel_send_order_data(session, RDP_RAIL_ORDER_EXEC, data, data_length);
}

size_t get_sysparam_size_in_rdp_stream(RAIL_CLIENT_SYSPARAM * sysparam)
{
	switch (sysparam->type)
	{
	case SPI_SETDRAGFULLWINDOWS: {return 1;}
	case SPI_SETKEYBOARDCUES:    {return 1;}
	case SPI_SETKEYBOARDPREF:    {return 1;}
	case SPI_SETMOUSEBUTTONSWAP: {return 1;}
	case SPI_SETWORKAREA:        {return 8;}
	case RAIL_SPI_DISPLAYCHANGE: {return 8;}
	case RAIL_SPI_TASKBARPOS:    {return 8;}
	case SPI_SETHIGHCONTRAST:
		{
			return (4 + /*Flags (4 bytes)*/
					4 + /*ColorSchemeLength (4 bytes)*/
					2 + /*UNICODE_STRING.cbString (2 bytes)*/
					sysparam->value.high_contrast_system_info.color_scheme.length);
		}
	};

	assert(!"Unknown sysparam type");
	return 0;
}

/*
 * Indicates a Client System Parameters Update PDU from client to server to
 * synchronize system parameters on the server with those on the client.
 */
void rail_vchannel_send_client_sysparam_update_order(RAIL_SESSION* session, RAIL_CLIENT_SYSPARAM* sysparam)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;
	size_t data_length = 4; /*SystemParam (4 bytes)*/
	void*  data = 0;

	data_length += get_sysparam_size_in_rdp_stream(sysparam);

	data = rail_alloc_order_data(data_length);
	stream_init_by_allocated_data(s, data, data_length);

	stream_write_uint32(s, sysparam->type);

	switch (sysparam->type)
	{
		case SPI_SETDRAGFULLWINDOWS:
			stream_write_uint8(s, sysparam->value.full_window_drag_enabled);
			break;

		case SPI_SETKEYBOARDCUES:
			stream_write_uint8(s, sysparam->value.menu_access_key_always_underlined);
			break;

		case SPI_SETKEYBOARDPREF:
			stream_write_uint8(s, sysparam->value.keyboard_for_user_prefered);
			break;

		case SPI_SETMOUSEBUTTONSWAP:
			stream_write_uint8(s, sysparam->value.left_right_mouse_buttons_swapped);
			break;

		case SPI_SETWORKAREA:
			write_rail_rect_16(s, &sysparam->value.work_area);
			break;

		case RAIL_SPI_DISPLAYCHANGE:
			write_rail_rect_16(s, &sysparam->value.display_resolution);
			break;

		case RAIL_SPI_TASKBARPOS:
			write_rail_rect_16(s, &sysparam->value.taskbar_size);
			break;

		case SPI_SETHIGHCONTRAST:
			{
				uint32 color_scheme_length = 2 +
						sysparam->value.high_contrast_system_info.color_scheme.length;

				stream_write_uint32(s, sysparam->value.high_contrast_system_info.flags);
				stream_write_uint32(s, color_scheme_length);
				write_rail_unicode_string(s, &sysparam->value.high_contrast_system_info.color_scheme);
				break;
			}

		default:
			assert(!"Unknown sysparam type");
			break;
	}

	rail_vchannel_send_order_data(session, RDP_RAIL_ORDER_SYSPARAM, data, data_length);
}

/*
 * Indicates a Client System Command PDU from client to server when a local
 * RAIL window on the client receives a command to perform an action on the
 * window, such as minimize or maximize.
 */
void rail_vchannel_send_syscommand_order(RAIL_SESSION* session, uint32 window_id, uint16 command)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;
	uint16 data_length = 4 + 2;
	void*  data = rail_alloc_order_data(data_length);

	stream_init_by_allocated_data(s, data, data_length);

	stream_write_uint32(s, window_id);
	stream_write_uint16(s, command);

	rail_vchannel_send_order_data(session, RDP_RAIL_ORDER_SYSCOMMAND, data, data_length);
}

/*
 * The Client Notify Event PDU packet is sent from a client to a server when
 * a local RAIL Notification Icon on the client receives a keyboard or mouse
 * message from the user. This notification is forwarded to the server via
 * the Notify Event PDU.
 * */
void rail_vchannel_send_notify_event_order(RAIL_SESSION * session, uint32 window_id, uint32 notify_icon_id, uint32 message)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;
	uint16 data_length = 4 * 3;
	void*  data = rail_alloc_order_data(data_length);

	stream_init_by_allocated_data(s, data, data_length);

	stream_write_uint32(s, window_id);
	stream_write_uint32(s, notify_icon_id);
	stream_write_uint32(s, message);

	rail_vchannel_send_order_data(session, RDP_RAIL_ORDER_NOTIFY_EVENT, data, data_length);
}

/*
 * The Client Window Move PDU packet is sent from the client to the server
 * when a local window is ending a move or resize. The client communicates the
 * locally moved or resized window's position to the server by using this packet.
 * The server uses this information to reposition its window.*/
void rail_vchannel_send_client_windowmove_order(RAIL_SESSION* session, uint32 window_id, RAIL_RECT_16* new_position)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;
	uint16 data_length = 4 + 2 * 4;
	void*  data = rail_alloc_order_data(data_length);

	stream_init_by_allocated_data(s, data, data_length);

	stream_write_uint32(s, window_id);
	stream_write_uint16(s, new_position->left);
	stream_write_uint16(s, new_position->top);
	stream_write_uint16(s, new_position->right);
	stream_write_uint16(s, new_position->bottom);

	rail_vchannel_send_order_data(session, RDP_RAIL_ORDER_WINDOWMOVE, data, data_length);
}

/*
 * The Client Information PDU is sent from client to server and contains
 * information about RAIL client state and features supported by the client.
 * */
void rail_vchannel_send_client_information_order(RAIL_SESSION* session, uint32 flags)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;
	uint16 data_length = 4;
	void*  data = rail_alloc_order_data(data_length);

	stream_init_by_allocated_data(s, data, data_length);

	stream_write_uint32(s, flags);

	rail_vchannel_send_order_data(session, RDP_RAIL_ORDER_CLIENTSTATUS, data, data_length);
}

/*
 * The Client System Menu PDU packet is sent from the client to the server
 * when a local RAIL window on the client receives a command to display its
 * System menu. This command is forwarded to the server via
 * the System menu PDU.
 */
void rail_vchannel_send_client_system_menu_order(RAIL_SESSION* session, uint32 window_id, uint16 left, uint16 top)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;
	uint16 data_length = 4 + 2 * 2;
	void*  data = rail_alloc_order_data(data_length);

	stream_init_by_allocated_data(s, data, data_length);

	stream_write_uint32(s, window_id);
	stream_write_uint16(s, left);
	stream_write_uint16(s, top);

	rail_vchannel_send_order_data(session, RDP_RAIL_ORDER_SYSMENU, data,
			data_length);
}

/*
 * The Language Bar Information PDU is used to set the language bar status.
 * It is sent from a client to a server or a server to a client, but only when
 * both support the Language Bar docking capability
 * (TS_RAIL_LEVEL_DOCKED_LANGBAR_SUPPORTED).
 * This PDU contains information about the language bar status.
 * */
void rail_vchannel_send_client_langbar_information_order(RAIL_SESSION* session, uint32 langbar_status)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;
	uint16 data_length = 4;
	void*  data = rail_alloc_order_data(data_length);

	stream_init_by_allocated_data(s, data, data_length);

	stream_write_uint32(s, langbar_status);

	rail_vchannel_send_order_data(session, RDP_RAIL_ORDER_LANGBARINFO, data, data_length);
}

/*
 * The Client Get Application ID PDU is sent from a client to a server.
 * This PDU requests information from the server about the Application ID
 * that the window SHOULD <15> have on the client.
 * */
void rail_vchannel_send_get_appid_req_order(RAIL_SESSION* session, uint32 window_id)
{
	STREAM st_stream = {0};
	STREAM* s = &st_stream;
	uint16 data_length = 4;
	void*  data = rail_alloc_order_data(data_length);

	stream_init_by_allocated_data(s, data, data_length);

	stream_write_uint32(s, window_id);

	rail_vchannel_send_order_data(session, RDP_RAIL_ORDER_GET_APPID_REQ, data, data_length);
}

/*
 * Look at rail_vchannel_send_handshake_order(...)
 */
void rail_vchannel_process_handshake_order(RAIL_SESSION* session, STREAM* s)
{
	uint32 build_number = 0;

	stream_read_uint32(s, build_number);
	rail_core_handle_server_handshake(session, build_number);
}

/*
 * The Server Execute Result PDU is sent from server to client in response to
 * a Client Execute PDU request, and contains the result of the server's
 * attempt to launch the requested executable.
 */
void rail_vchannel_process_exec_result_order(RAIL_SESSION* session, STREAM* s)
{
	uint16 flags = 0;
	uint16 exec_result = 0;
	uint32 raw_result = 0;
	RAIL_UNICODE_STRING exe_or_file = {0};

	stream_read_uint16(s, flags); /*Flags (2 bytes)*/
	stream_read_uint16(s, exec_result); /*ExecResult (2 bytes)*/
	stream_read_uint32(s, raw_result); /*RawResult (4 bytes)*/
	stream_seek(s, 2);  /*Padding (2 bytes)*/
	read_rail_unicode_string(s, &exe_or_file); /*ExeOrFileLength with ExeOrFile (variable)*/

	rail_core_handle_exec_result(session, flags, exec_result, raw_result, &exe_or_file);
	free_rail_unicode_string(&exe_or_file);
}

/*
 * The Server System Parameters Update PDU is sent from the server to client to
 * synchronize system parameters on the client with those on the server.
 */
void rail_vchannel_process_server_sysparam_update_order(RAIL_SESSION* session, STREAM* s)
{
	RAIL_SERVER_SYSPARAM sysparam = {0};

	stream_read_uint32(s, sysparam.type);

	switch (sysparam.type)
	{
	case SPI_SETSCREENSAVEACTIVE:
		stream_read_uint8(s, sysparam.value.screen_saver_enabled);
		break;

	case SPI_SETSCREENSAVESECURE:
		stream_read_uint8(s, sysparam.value.screen_saver_lock_enabled);
		break;

	default:
		assert(!"Undocumented RAIL server sysparam type");
		break;
	};

	rail_core_handle_server_sysparam(session, &sysparam);
}

/*
 * The Server Move/Size Start PDU packet is sent by the server when a window on
 * the server is beginning a move or resize.
 * The client uses this information to initiate a local move or resize of the
 * corresponding local window.
 *
 * The Server Move/Size End PDU is sent by the server when a window on the
 * server is completing a move or resize.
 * The client uses this information to end a local move/resize of the
 * corresponding local window.
 *
 */
void rail_vchannel_process_server_movesize_order(RAIL_SESSION* session, STREAM* s)
{
	uint32 window_id = 0;
	uint16 move_size_started = 0;
	uint16 move_size_type = 0;
	uint16 pos_x = 0;
	uint16 pos_y = 0;

	stream_read_uint32(s, window_id);
	stream_read_uint16(s, move_size_started);
	stream_read_uint16(s, move_size_type);
	stream_read_uint16(s, pos_x);
	stream_read_uint16(s, pos_y);

	rail_core_handle_server_movesize(session, window_id, move_size_started, move_size_type, pos_x, pos_y);
}

/*
 * The Server Min Max Info PDU is sent from a server to a client when a window
 * move or resize on the server is being initiated.
 * This PDU contains information about the minimum and maximum extents to
 * which the window can be moved or sized.
 */
void rail_vchannel_process_server_minmax_info_order(RAIL_SESSION* session, STREAM* s)
{
	uint32 window_id = 0;
	uint16 max_width = 0;
	uint16 max_height = 0;
	uint16 max_pos_x = 0;
	uint16 max_pos_y = 0;
	uint16 min_track_width = 0;
	uint16 min_track_height = 0;
	uint16 max_track_width = 0;
	uint16 max_track_height = 0;

	stream_read_uint32(s, window_id);
	stream_read_uint16(s, max_width);
	stream_read_uint16(s, max_height);
	stream_read_uint16(s, max_pos_x);
	stream_read_uint16(s, max_pos_y);
	stream_read_uint16(s, min_track_width);
	stream_read_uint16(s, min_track_height);
	stream_read_uint16(s, max_track_width);
	stream_read_uint16(s, max_track_height);

	rail_core_handle_server_minmax_info(session, window_id, max_width,
	    max_height, max_pos_x, max_pos_y, min_track_width, min_track_height,
	    max_track_width, max_track_height);
}

/*
 *The Language Bar Information PDU is used to set the language bar status.
 */
void rail_vchannel_process_server_langbar_info_order(RAIL_SESSION* session, STREAM* s)
{
	uint32 langbar_status = 0;

	stream_read_uint32(s, langbar_status);

	rail_core_handle_server_langbar_info(session, langbar_status);
}

/*
 * The Server Get Application ID Response PDU is sent from a server to a client.
 * This PDU MAY be sent to the client as a response to a Client Get Application
 * ID PDU. This PDU specifies the Application ID that the specified window
 * SHOULD have on the client. The client MAY ignore this PDU.
 */
static void rail_vchannel_process_server_get_appid_resp_order(RAIL_SESSION* session, STREAM* s)
{
	uint32 window_id = 0;
	RAIL_UNICODE_STRING app_id = {0};

	app_id.length = 256;
	app_id.buffer = xmalloc(app_id.length);

	stream_read_uint32(s, window_id);
	stream_read(s, app_id.buffer, app_id.length);

	rail_core_handle_server_get_app_resp(session, window_id, &app_id);
	free_rail_unicode_string(&app_id);
}

void rail_vchannel_process_received_vchannel_data(RAIL_SESSION * session, STREAM* s)
{
	size_t length = 0;
	uint16 order_type = 0;
	uint16 order_length = 0;

	length = ((s->data + s->size) - s->p);

	stream_read_uint16(s, order_type);   /* orderType */
	stream_read_uint16(s, order_length); /* orderLength */

	DEBUG_RAIL("rail_on_channel_data_received: session=0x%p data_size=%d "
			    "orderType=0x%X orderLength=%d", session, (int) length, order_type, order_length);

	switch (order_type)
	{
		case RDP_RAIL_ORDER_HANDSHAKE:
			rail_vchannel_process_handshake_order(session, s);
			break;

		case RDP_RAIL_ORDER_EXEC_RESULT:
			rail_vchannel_process_exec_result_order(session, s);
			break;

		case RDP_RAIL_ORDER_SYSPARAM:
			rail_vchannel_process_server_sysparam_update_order(session, s);
			break;

		case RDP_RAIL_ORDER_LOCALMOVESIZE:
			rail_vchannel_process_server_movesize_order(session, s);
			break;

		case RDP_RAIL_ORDER_MINMAXINFO:
			rail_vchannel_process_server_minmax_info_order(session, s);
			break;

		case RDP_RAIL_ORDER_LANGBARINFO:
			rail_vchannel_process_server_langbar_info_order(session, s);
			break;

		case RDP_RAIL_ORDER_GET_APPID_RESP:
			rail_vchannel_process_server_get_appid_resp_order(session, s);
			break;

		default:
			DEBUG_RAIL("rail_on_channel_data_received: "
				"Undocumented RAIL server PDU: order_type=0x%X",order_type);
			break;
	}
}

