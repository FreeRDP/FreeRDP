/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPEDISP Virtual Channel Extension
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_DISP_SERVER_MAIN_H
#define FREERDP_CHANNEL_DISP_SERVER_MAIN_H

#include <freerdp/server/disp.h>

struct _disp_server_private
{
	BOOL isReady;
	wStream* input_stream;
	HANDLE channelEvent;
	HANDLE thread;
	HANDLE stopEvent;
	DWORD SessionId;

	void* disp_channel;
};

#endif /* FREERDP_CHANNEL_DISP_SERVER_MAIN_H */
