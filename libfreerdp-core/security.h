/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Security
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

#ifndef __SECURITY_H
#define __SECURITY_H

#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>

void security_read_license_packet(STREAM* s, uint16 sec_flags);
void security_read_redirection_packet(STREAM* s, uint16 sec_flags);

#endif /* __SECURITY_H */
