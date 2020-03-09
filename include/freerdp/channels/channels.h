/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Virtual Channel Manager
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

#ifndef FREERDP_CHANNELS_H
#define FREERDP_CHANNELS_H

#include <winpr/crt.h>
#include <winpr/wtsapi.h>

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API int freerdp_channels_client_load(rdpChannels* channels, rdpSettings* settings,
	                                             PVIRTUALCHANNELENTRY entry, void* data);
	FREERDP_API int freerdp_channels_client_load_ex(rdpChannels* channels, rdpSettings* settings,
	                                                PVIRTUALCHANNELENTRYEX entryEx, void* data);
	FREERDP_API int freerdp_channels_load_plugin(rdpChannels* channels, rdpSettings* settings,
	                                             const char* name, void* data);
	FREERDP_API BOOL freerdp_channels_get_fds(rdpChannels* channels, freerdp* instance,
	                                          void** read_fds, int* read_count, void** write_fds,
	                                          int* write_count);
	FREERDP_API BOOL freerdp_channels_check_fds(rdpChannels* channels, freerdp* instance);

	FREERDP_API void* freerdp_channels_get_static_channel_interface(rdpChannels* channels,
	                                                                const char* name);

	FREERDP_API HANDLE freerdp_channels_get_event_handle(freerdp* instance);
	FREERDP_API int freerdp_channels_process_pending_messages(freerdp* instance);

	FREERDP_API BOOL freerdp_channels_data(freerdp* instance, UINT16 channelId, const BYTE* data,
	                                       size_t dataSize, UINT32 flags, size_t totalSize);

	FREERDP_API UINT16 freerdp_channels_get_id_by_name(freerdp* instance, const char* channel_name);
	FREERDP_API const char* freerdp_channels_get_name_by_id(freerdp* instance, UINT16 channelId);

	FREERDP_API PWtsApiFunctionTable FreeRDP_InitWtsApi(void);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNELS_H */
