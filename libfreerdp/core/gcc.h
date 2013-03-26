/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * T.124 Generic Conference Control (GCC)
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

#ifndef FREERDP_CORE_GCC_H
#define FREERDP_CORE_GCC_H

#include "mcs.h"

#include <freerdp/crypto/per.h>

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>

#include <winpr/stream.h>

BOOL gcc_read_conference_create_request(wStream* s, rdpSettings* settings);
void gcc_write_conference_create_request(wStream* s, wStream* user_data);
BOOL gcc_read_conference_create_response(wStream* s, rdpSettings* settings);
void gcc_write_conference_create_response(wStream* s, wStream* user_data);
BOOL gcc_read_client_data_blocks(wStream* s, rdpSettings *settings, int length);
void gcc_write_client_data_blocks(wStream* s, rdpSettings *settings);
BOOL gcc_read_server_data_blocks(wStream* s, rdpSettings *settings, int length);
void gcc_write_server_data_blocks(wStream* s, rdpSettings *settings);
BOOL gcc_read_user_data_header(wStream* s, UINT16* type, UINT16* length);
void gcc_write_user_data_header(wStream* s, UINT16 type, UINT16 length);
BOOL gcc_read_client_core_data(wStream* s, rdpSettings *settings, UINT16 blockLength);
void gcc_write_client_core_data(wStream* s, rdpSettings *settings);
BOOL gcc_read_server_core_data(wStream* s, rdpSettings *settings);
void gcc_write_server_core_data(wStream* s, rdpSettings *settings);
BOOL gcc_read_client_security_data(wStream* s, rdpSettings *settings, UINT16 blockLength);
void gcc_write_client_security_data(wStream* s, rdpSettings *settings);
BOOL gcc_read_server_security_data(wStream* s, rdpSettings *settings);
void gcc_write_server_security_data(wStream* s, rdpSettings *settings);
BOOL gcc_read_client_network_data(wStream* s, rdpSettings *settings, UINT16 blockLength);
void gcc_write_client_network_data(wStream* s, rdpSettings *settings);
BOOL gcc_read_server_network_data(wStream* s, rdpSettings *settings);
void gcc_write_server_network_data(wStream* s, rdpSettings *settings);
BOOL gcc_read_client_cluster_data(wStream* s, rdpSettings *settings, UINT16 blockLength);
void gcc_write_client_cluster_data(wStream* s, rdpSettings *settings);
BOOL gcc_read_client_monitor_data(wStream* s, rdpSettings *settings, UINT16 blockLength);
void gcc_write_client_monitor_data(wStream* s, rdpSettings *settings);

#endif /* FREERDP_CORE_GCC_H */
