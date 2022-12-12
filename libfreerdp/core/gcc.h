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

#ifndef FREERDP_LIB_CORE_GCC_H
#define FREERDP_LIB_CORE_GCC_H

#include "mcs.h"

#include <freerdp/crypto/per.h>

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/api.h>

#include <winpr/stream.h>

FREERDP_LOCAL BOOL gcc_read_conference_create_request(wStream* s, rdpMcs* mcs);
FREERDP_LOCAL BOOL gcc_write_conference_create_request(wStream* s, wStream* userData);
FREERDP_LOCAL BOOL gcc_read_conference_create_response(wStream* s, rdpMcs* mcs);
FREERDP_LOCAL BOOL gcc_write_conference_create_response(wStream* s, wStream* userData);
FREERDP_LOCAL BOOL gcc_write_client_data_blocks(wStream* s, const rdpMcs* mcs);
FREERDP_LOCAL BOOL gcc_write_server_data_blocks(wStream* s, rdpMcs* mcs);

#endif /* FREERDP_LIB_CORE_GCC_H */
