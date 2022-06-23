/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Channels
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNELS_CLIENT_H
#define FREERDP_CHANNELS_CLIENT_H

#include <freerdp/api.h>
#include <freerdp/addin.h>
#include <freerdp/channels/channels.h>

typedef struct
{
	IWTSVirtualChannelCallback iface;
	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
} GENERIC_CHANNEL_CALLBACK;

typedef struct
{
	IWTSListenerCallback iface;
	IWTSPlugin* plugin;
	IWTSVirtualChannelManager* channel_mgr;
	IWTSVirtualChannel* channel;
	GENERIC_CHANNEL_CALLBACK* channel_callback;
} GENERIC_LISTENER_CALLBACK;

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API void* freerdp_channels_client_find_static_entry(const char* name,
	                                                            const char* identifier);
	FREERDP_API PVIRTUALCHANNELENTRY freerdp_channels_load_static_addin_entry(LPCSTR pszName,
	                                                                          LPCSTR pszSubsystem,
	                                                                          LPCSTR pszType,
	                                                                          DWORD dwFlags);

	FREERDP_API FREERDP_ADDIN** freerdp_channels_list_addins(LPCSTR lpName, LPCSTR lpSubsystem,
	                                                         LPCSTR lpType, DWORD dwFlags);
	FREERDP_API void freerdp_channels_addin_list_free(FREERDP_ADDIN** ppAddins);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNELS_CLIENT_H */
