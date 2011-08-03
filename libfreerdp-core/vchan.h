/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Virtual Channels
 *
 * Copyright 2011 Vic Lee
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

#ifndef __VCHAN_H
#define __VCHAN_H

struct rdp_vchan
{
	freerdp* instance;
};
typedef struct rdp_vchan rdpVchan;

int vchan_send(rdpVchan* vchan, uint16 channel_id, uint8* data, int size);
void vchan_process(rdpVchan* vchan, STREAM* s, uint16 channel_id);

rdpVchan* vchan_new(freerdp* instance);
void vchan_free(rdpVchan* vchan);

#endif /* __VCHAN_H */
