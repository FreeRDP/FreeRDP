/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Transport Packets (TPKTs)
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

#ifndef __TPKT_H
#define __TPKT_H

#include "tpdu.h"
#include "transport.h"

#include <freerdp/utils/stream.h>

#define TPKT_HEADER_LENGTH	4

boolean tpkt_verify_header(STREAM* s);
uint16 tpkt_read_header(STREAM* s);
void tpkt_write_header(STREAM* s, uint16 length);

#endif /* __TPKT_H */
