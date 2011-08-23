/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Server Redirection
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

#ifndef __REDIRECTION_H
#define __REDIRECTION_H

#include "rdp.h"

#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>

boolean rdp_recv_redirection_packet(rdpRdp* rdp, STREAM* s);
boolean rdp_recv_enhanced_security_redirection_packet(rdpRdp* rdp, STREAM* s);

#endif /* __REDIRECTION_H */
