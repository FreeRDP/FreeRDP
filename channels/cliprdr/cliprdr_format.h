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

#ifndef __CLIPRDR_FORMAT_H
#define __CLIPRDR_FORMAT_H

void cliprdr_process_format_list_event(cliprdrPlugin* cliprdr, RDP_CB_FORMAT_LIST_EVENT* cb_event);
void cliprdr_process_format_list(cliprdrPlugin* cliprdr, STREAM* s, uint32 dataLen, uint16 msgFlags);
void cliprdr_process_format_list_response(cliprdrPlugin* cliprdr, STREAM* s, uint32 dataLen, uint16 msgFlags);

void cliprdr_process_format_data_request(cliprdrPlugin* cliprdr, STREAM* s, uint32 dataLen, uint16 msgFlags);
void cliprdr_process_format_data_response_event(cliprdrPlugin* cliprdr, RDP_CB_DATA_RESPONSE_EVENT* cb_event);

void cliprdr_process_format_data_request_event(cliprdrPlugin* cliprdr, RDP_CB_DATA_REQUEST_EVENT* cb_event);
void cliprdr_process_format_data_response(cliprdrPlugin* cliprdr, STREAM* s, uint32 dataLen, uint16 msgFlags);

#endif /* __CLIPRDR_FORMAT_H */
