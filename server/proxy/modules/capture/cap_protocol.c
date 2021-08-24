/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server Session Capture Module
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#include "cap_protocol.h"

wStream* capture_plugin_packet_new(UINT32 payload_size, UINT16 type)
{
	wStream* stream = Stream_New(NULL, HEADER_SIZE + payload_size);

	if (!stream)
		return NULL;

	Stream_Write_UINT32(stream, payload_size);
	Stream_Write_UINT16(stream, type);
	return stream;
}

wStream* capture_plugin_create_session_info_packet(pClientContext* pc)
{
	size_t username_length;
	wStream* s = NULL;
	rdpSettings* settings;

	if (!pc)
		return NULL;

	settings = pc->context.settings;

	if (!settings || !settings->Username)
		return NULL;

	username_length = strlen(settings->Username);
	if ((username_length == 0) || (username_length > UINT16_MAX))
		return NULL;

	s = capture_plugin_packet_new(SESSION_INFO_PDU_BASE_SIZE + username_length,
	                              MESSAGE_TYPE_SESSION_INFO);
	if (!s)
		return NULL;

	Stream_Write_UINT16(s, username_length);                         /* username length (2 bytes) */
	Stream_Write(s, settings->Username, username_length);            /* username */
	Stream_Write_UINT32(s, settings->DesktopWidth);                  /* desktop width (4 bytes) */
	Stream_Write_UINT32(s, settings->DesktopHeight);                 /* desktop height (4 bytes) */
	Stream_Write_UINT32(s, settings->ColorDepth);                    /* color depth (4 bytes) */
	Stream_Write(s, pc->pdata->session_id, PROXY_SESSION_ID_LENGTH); /* color depth (32 bytes) */
	return s;
}
