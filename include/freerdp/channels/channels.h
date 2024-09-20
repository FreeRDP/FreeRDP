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
#include <freerdp/settings.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** @since version 3.9.0 */
	typedef BOOL (*freerdp_channel_handle_fkt_t)(rdpContext* context, void* userdata);

	FREERDP_API int freerdp_channels_client_load(rdpChannels* channels, rdpSettings* settings,
	                                             PVIRTUALCHANNELENTRY entry, void* data);
	FREERDP_API int freerdp_channels_client_load_ex(rdpChannels* channels, rdpSettings* settings,
	                                                PVIRTUALCHANNELENTRYEX entryEx, void* data);
	FREERDP_API int freerdp_channels_load_plugin(rdpChannels* channels, rdpSettings* settings,
	                                             const char* name, void* data);
#if defined(WITH_FREERDP_DEPRECATED)
	FREERDP_API WINPR_DEPRECATED_VAR(
	    "Use freerdp_channels_get_event_handle",
	    BOOL freerdp_channels_get_fds(rdpChannels* channels, freerdp* instance, void** read_fds,
	                                  int* read_count, void** write_fds, int* write_count));
#endif
	FREERDP_API BOOL freerdp_channels_check_fds(rdpChannels* channels, freerdp* instance);

	FREERDP_API void* freerdp_channels_get_static_channel_interface(rdpChannels* channels,
	                                                                const char* name);

	/** @brief A channel may register an event handle and a callback function to be called by \b
	 * freerdp_check_event_handles
	 *
	 *  If a channel can not or does not want to use a thread to process data asynchronously it can
	 * register a \b non-blocking function that does this processing. Notification is done with the
	 * \b event-handle which will trigger the RDP loop. So this function is triggered until the \b
	 * event-handle is reset.
	 *  @note This function may be called even without the \b event-handle to be set, so it must be
	 * capable of handling these calls properly.
	 *
	 *  @param channels A pointer to the channels instance to register with. Must not be \b NULL
	 *  @param handle A \b event-handle to be used to notify the RDP main thread that the callback
	 * function should be called again. Must not be \b INVALID_HANDLE_PARAM
	 *  @param fkt The callback function responsible to handle the channel specifics. Must not be \b
	 * NULL
	 *  @param userdata A pointer to a channel specific context. Most likely the channel context.
	 * May be \b NULL if not required.
	 *
	 *  @return \b TRUE if successful, \b FALSE if any error occurs.
	 *  @since version 3.9.0 */
	FREERDP_API BOOL freerdp_client_channel_register(rdpChannels* channels, HANDLE handle,
	                                                 freerdp_channel_handle_fkt_t fkt,
	                                                 void* userdata);

	/** @brief Remove an existing registration for \b event-handle from the channels instance
	 *
	 *  @param channels A pointer to the channels instance to register with. Must not be \b NULL
	 *  @param handle A \b event-handle to be used to notify the RDP main thread that the callback
	 * function should be called again. Must not be \b INVALID_HANDLE_PARAM
	 *
	 *  @return \b TRUE if successful, \b FALSE if any error occurs.
	 *  @since version 3.9.0 */
	FREERDP_API BOOL freerdp_client_channel_unregister(rdpChannels* channels, HANDLE handle);

	FREERDP_API HANDLE freerdp_channels_get_event_handle(freerdp* instance);
	FREERDP_API int freerdp_channels_process_pending_messages(freerdp* instance);

	FREERDP_API BOOL freerdp_channels_data(freerdp* instance, UINT16 channelId, const BYTE* data,
	                                       size_t dataSize, UINT32 flags, size_t totalSize);

	FREERDP_API UINT16 freerdp_channels_get_id_by_name(freerdp* instance, const char* channel_name);
	FREERDP_API const char* freerdp_channels_get_name_by_id(freerdp* instance, UINT16 channelId);

	FREERDP_API const WtsApiFunctionTable* FreeRDP_InitWtsApi(void);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNELS_H */
