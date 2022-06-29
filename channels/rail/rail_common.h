/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RAIL Virtual Channel Plugin
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2011 Roman Barabanov <romanbarabanov@gmail.com>
 * Copyright 2011 Vic Lee
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_CHANNEL_RAIL_COMMON_H
#define FREERDP_CHANNEL_RAIL_COMMON_H

#include <freerdp/rail.h>

#define RAIL_PDU_HEADER_LENGTH 4

/* Fixed length of PDUs, excluding variable lengths */
#define RAIL_HANDSHAKE_ORDER_LENGTH 4             /* fixed */
#define RAIL_HANDSHAKE_EX_ORDER_LENGTH 8          /* fixed */
#define RAIL_CLIENT_STATUS_ORDER_LENGTH 4         /* fixed */
#define RAIL_EXEC_ORDER_LENGTH 8                  /* variable */
#define RAIL_EXEC_RESULT_ORDER_LENGTH 12          /* variable */
#define RAIL_SYSPARAM_ORDER_LENGTH 4              /* variable */
#define RAIL_MINMAXINFO_ORDER_LENGTH 20           /* fixed */
#define RAIL_LOCALMOVESIZE_ORDER_LENGTH 12        /* fixed */
#define RAIL_ACTIVATE_ORDER_LENGTH 5              /* fixed */
#define RAIL_SYSMENU_ORDER_LENGTH 8               /* fixed */
#define RAIL_SYSCOMMAND_ORDER_LENGTH 6            /* fixed */
#define RAIL_NOTIFY_EVENT_ORDER_LENGTH 12         /* fixed */
#define RAIL_WINDOW_MOVE_ORDER_LENGTH 12          /* fixed */
#define RAIL_SNAP_ARRANGE_ORDER_LENGTH 12         /* fixed */
#define RAIL_GET_APPID_REQ_ORDER_LENGTH 4         /* fixed */
#define RAIL_LANGBAR_INFO_ORDER_LENGTH 4          /* fixed */
#define RAIL_LANGUAGEIME_INFO_ORDER_LENGTH 42     /* fixed */
#define RAIL_COMPARTMENT_INFO_ORDER_LENGTH 16     /* fixed */
#define RAIL_CLOAK_ORDER_LENGTH 5                 /* fixed */
#define RAIL_TASKBAR_INFO_ORDER_LENGTH 12         /* fixed */
#define RAIL_Z_ORDER_SYNC_ORDER_LENGTH 4          /* fixed */
#define RAIL_POWER_DISPLAY_REQUEST_ORDER_LENGTH 4 /* fixed */
#define RAIL_GET_APPID_RESP_ORDER_LENGTH 524      /* fixed */
#define RAIL_GET_APPID_RESP_EX_ORDER_LENGTH 1048  /* fixed */

UINT rail_read_handshake_order(wStream* s, RAIL_HANDSHAKE_ORDER* handshake);
void rail_write_handshake_order(wStream* s, const RAIL_HANDSHAKE_ORDER* handshake);
UINT rail_read_handshake_ex_order(wStream* s, RAIL_HANDSHAKE_EX_ORDER* handshakeEx);
void rail_write_handshake_ex_order(wStream* s, const RAIL_HANDSHAKE_EX_ORDER* handshakeEx);

wStream* rail_pdu_init(size_t length);
UINT rail_read_pdu_header(wStream* s, UINT16* orderType, UINT16* orderLength);
void rail_write_pdu_header(wStream* s, UINT16 orderType, UINT16 orderLength);

UINT rail_write_unicode_string(wStream* s, const RAIL_UNICODE_STRING* unicode_string);
UINT rail_write_unicode_string_value(wStream* s, const RAIL_UNICODE_STRING* unicode_string);

UINT rail_read_sysparam_order(wStream* s, RAIL_SYSPARAM_ORDER* sysparam, BOOL extendedSpiSupported);
UINT rail_write_sysparam_order(wStream* s, const RAIL_SYSPARAM_ORDER* sysparam,
                               BOOL extendedSpiSupported);
BOOL rail_is_extended_spi_supported(UINT32 channelsFlags);
const char* rail_get_order_type_string(UINT16 orderType);
const char* rail_get_order_type_string_full(UINT16 orderType, char* buffer, size_t length);

#endif /* FREERDP_CHANNEL_RAIL_COMMON_H */
