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

#ifndef __RAIL_CHANNEL_ORDERS_H
#define	__RAIL_CHANNEL_ORDERS_H

#include "rail_core.h"

void rail_vchannel_send_handshake_order(RAIL_SESSION* session, uint32 build_number);
void rail_vchannel_send_client_information_order(RAIL_SESSION* session, uint32 flags);
void rail_vchannel_send_activate_order(RAIL_SESSION* session, uint32 window_id, uint8 enabled);
void rail_vchannel_send_exec_order(RAIL_SESSION* session, uint16 flags, RAIL_UNICODE_STRING* exe_or_file,
		RAIL_UNICODE_STRING* working_directory, RAIL_UNICODE_STRING* arguments);
void rail_vchannel_send_client_sysparam_update_order(RAIL_SESSION* session, RAIL_CLIENT_SYSPARAM* sysparam);
void rail_vchannel_send_syscommand_order(RAIL_SESSION* session, uint32 window_id, uint16 command);
void rail_vchannel_send_notify_event_order(RAIL_SESSION* session, uint32 window_id, uint32 notify_icon_id, uint32 message);
void rail_vchannel_send_client_windowmove_order(RAIL_SESSION* session, uint32 window_id, RAIL_RECT_16* new_position);
void rail_vchannel_send_client_system_menu_order(RAIL_SESSION* session, uint32 window_id, uint16 left, uint16 top);
void rail_vchannel_send_client_langbar_information_order(RAIL_SESSION* session, uint32 langbar_status);
void rail_vchannel_send_get_appid_req_order(RAIL_SESSION* session, uint32 window_id);
void rail_vchannel_process_received_vchannel_data(RAIL_SESSION* session, STREAM* s);

#endif /* __RAIL_CHANNEL_ORDERS_H */
