/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#include <winpr/stream.h>

#include <freerdp/utils/debug.h>

struct cliprdr_plugin
{
	rdpSvcPlugin plugin;
	BOOL received_caps;
	BOOL use_long_format_names;
	BOOL stream_fileclip_enabled;
	BOOL fileclip_no_file_paths;
	BOOL can_lock_clipdata;
	CLIPRDR_FORMAT_NAME* format_names;
	int num_format_names;
};
typedef struct cliprdr_plugin cliprdrPlugin;

wStream* cliprdr_packet_new(UINT16 msgType, UINT16 msgFlags, UINT32 dataLen);
void cliprdr_packet_send(cliprdrPlugin* cliprdr, wStream* data_out);

#ifdef WITH_DEBUG_CLIPRDR
#define DEBUG_CLIPRDR(fmt, ...) DEBUG_CLASS(CLIPRDR, fmt, ## __VA_ARGS__)
#else
#define DEBUG_CLIPRDR(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* __CLIPRDR_MAIN_H */
