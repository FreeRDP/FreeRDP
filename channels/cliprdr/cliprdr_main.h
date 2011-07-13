/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Clipboard Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
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

#ifndef __CLIPRDR_MAIN_H
#define __CLIPRDR_MAIN_H

#include <freerdp/utils/stream.h>

typedef struct cliprdr_plugin cliprdrPlugin;
struct cliprdr_plugin
{
	rdpSvcPlugin plugin;
};

STREAM* cliprdr_packet_new(uint16 msgType, uint16 msgFlags, uint32 dataLen);
void cliprdr_packet_send(cliprdrPlugin* cliprdr, STREAM* data_out);

#endif /* __CLIPRDR_MAIN_H */
