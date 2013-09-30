/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Audio Virtual Channel
 *
 * Copyright 2012 Vic Lee
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_SERVER_RDPSND_MAIN_H
#define FREERDP_CHANNEL_SERVER_RDPSND_MAIN_H

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

#include <freerdp/codec/dsp.h>
#include <freerdp/channels/wtsvc.h>
#include <freerdp/server/rdpsnd.h>

struct _rdpsnd_server_private
{
	HANDLE Thread;
	HANDLE StopEvent;
	void* ChannelHandle;

	wStream* rdpsnd_pdu;
	BYTE* out_buffer;
	int out_buffer_size;
	int out_frames;
	int out_pending_frames;
	UINT32 src_bytes_per_sample;
	UINT32 src_bytes_per_frame;
	FREERDP_DSP_CONTEXT* dsp_context;
};

#endif /* FREERDP_CHANNEL_SERVER_RDPSND_MAIN_H */
