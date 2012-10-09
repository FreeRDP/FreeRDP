/**
 * FreeRDP: A Remote Desktop Protocol client.
 * File System Virtual Channel
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2010-2011 Vic Lee
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

#ifndef __RDPDR_CAPABILITIES_H
#define __RDPDR_CAPABILITIES_H

#include "rdpdr_main.h"

void rdpdr_process_capability_request(rdpdrPlugin* rdpdr, STREAM* data_in);
void rdpdr_send_capability_response(rdpdrPlugin* rdpdr);

#endif /* __RDPDR_CAPABILITIES_H */
