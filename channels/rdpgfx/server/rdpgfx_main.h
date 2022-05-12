/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Graphics Pipeline Extension
 *
 * Copyright 2016 Jiang Zihao <zihao.jiang@yahoo.com>
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

#ifndef FREERDP_CHANNEL_RDPGFX_SERVER_MAIN_H
#define FREERDP_CHANNEL_RDPGFX_SERVER_MAIN_H

#include <freerdp/server/rdpgfx.h>
#include <freerdp/codec/zgfx.h>

struct s_rdpgfx_server_private
{
	ZGFX_CONTEXT* zgfx;
	BOOL ownThread;
	HANDLE thread;
	HANDLE stopEvent;
	HANDLE channelEvent;
	void* rdpgfx_channel;
	DWORD SessionId;
	wStream* input_stream;
	BOOL isOpened;
	BOOL isReady;
};

#endif /* FREERDP_CHANNEL_RDPGFX_SERVER_MAIN_H */
