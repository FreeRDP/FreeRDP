/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Time Zone Redirection
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __TIMEZONE_H
#define __TIMEZONE_H

#include "rdp.h"

#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>
#include <freerdp/locale/timezone.h>

void rdp_read_system_time(STREAM* s, SYSTEM_TIME* system_time);
void rdp_write_system_time(STREAM* s, SYSTEM_TIME* system_time);
void rdp_get_client_time_zone(STREAM* s, rdpSettings* settings);
boolean rdp_read_client_time_zone(STREAM* s, rdpSettings* settings);
void rdp_write_client_time_zone(STREAM* s, rdpSettings* settings);

#endif /* __TIMEZONE_H */
