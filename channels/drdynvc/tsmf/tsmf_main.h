/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Video Redirection Virtual Channel
 *
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

#ifndef __TSMF_MAIN_H
#define __TSMF_MAIN_H

void tsmf_playback_ack(IWTSVirtualChannelCallback* pChannelCallback,
	uint32 message_id, uint64 duration, uint32 data_size);
boolean tsmf_push_event(IWTSVirtualChannelCallback* pChannelCallback,
	RDP_EVENT* event);

#endif

