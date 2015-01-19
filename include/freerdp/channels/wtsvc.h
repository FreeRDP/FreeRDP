/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Virtual Channel Interface
 *
 * Copyright 2011-2012 Vic Lee
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

/**
 * The server-side virtual channel API follows the Microsoft Remote Desktop
 * Services API functions WTSVirtualChannel* defined in:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa383464.aspx
 *
 * Difference between the MS API are documented in this header. All functions
 * are implemented in and integrated with libfreerdp-channels.
 *
 * Unlike MS API, all functions except WTSVirtualChannelOpenEx in this
 * implementation are thread-safe.
 */

#ifndef FREERDP_WTSVC_H
#define FREERDP_WTSVC_H

#include <freerdp/types.h>
#include <freerdp/peer.h>

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#include <winpr/wtsapi.h>

#define WTSVirtualEventHandle	3 /* Extended */
#define WTSVirtualChannelReady	4 /* Extended */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WTSVirtualChannelManager functions are FreeRDP extensions to the API.
 */

FREERDP_API void WTSVirtualChannelManagerGetFileDescriptor(HANDLE hServer, void** fds, int* fds_count);
FREERDP_API BOOL WTSVirtualChannelManagerCheckFileDescriptor(HANDLE hServer);
FREERDP_API HANDLE WTSVirtualChannelManagerGetEventHandle(HANDLE hServer);
FREERDP_API BOOL WTSVirtualChannelManagerIsChannelJoined(HANDLE hServer, const char* name);

/**
 * Extended FreeRDP WTS functions for channel handling
 */
FREERDP_API UINT16 WTSChannelGetId(freerdp_peer *client, const char *channel_name);
FREERDP_API BOOL WTSIsChannelJoinedByName(freerdp_peer *client, const char *channel_name);
FREERDP_API BOOL WTSIsChannelJoinedById(freerdp_peer *client, const UINT16 channel_id);
FREERDP_API BOOL WTSChannelSetHandleByName(freerdp_peer *client, const char *channel_name, void *handle);
FREERDP_API BOOL WTSChannelSetHandleById(freerdp_peer *client, const UINT16 channel_id, void *handle);
FREERDP_API void *WTSChannelGetHandleByName(freerdp_peer *client, const char *channel_name);
FREERDP_API void *WTSChannelGetHandleById(freerdp_peer *client, const UINT16 channel_id);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_WTSVC_H */
