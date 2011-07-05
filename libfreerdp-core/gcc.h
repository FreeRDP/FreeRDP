/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#ifndef __GCC_H
#define __GCC_H

#include "per.h"

#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>

void
gcc_write_create_conference_request(STREAM* s, STREAM* user_data);

void
gcc_write_client_core_data(STREAM* s, rdpSettings *settings);
void
gcc_write_client_security_data(STREAM* s, rdpSettings *settings);
void
gcc_write_client_network_data(STREAM* s, rdpSettings *settings);
void
gcc_write_client_cluster_data(STREAM* s, rdpSettings *settings);
void
gcc_write_client_monitor_data(STREAM* s, rdpSettings *settings);

#endif /* __GCC_H */
