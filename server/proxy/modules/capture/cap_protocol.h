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

#include <winpr/stream.h>
#include <freerdp/settings.h>

#include <freerdp/server/proxy/proxy_context.h>

/* protocol message sizes */
#define HEADER_SIZE 6
#define SESSION_INFO_PDU_BASE_SIZE 46
#define SESSION_END_PDU_BASE_SIZE 0
#define CAPTURED_FRAME_PDU_BASE_SIZE 0

/* protocol message types */
#define MESSAGE_TYPE_SESSION_INFO 1
#define MESSAGE_TYPE_CAPTURED_FRAME 2
#define MESSAGE_TYPE_SESSION_END 3

wStream* capture_plugin_packet_new(UINT32 payload_size, UINT16 type);
wStream* capture_plugin_create_session_info_packet(pClientContext* pc);
