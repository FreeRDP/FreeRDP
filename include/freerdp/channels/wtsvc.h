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

typedef struct WTSVirtualChannelManager WTSVirtualChannelManager;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WTSVirtualChannelManager functions are FreeRDP extensions to the API.
 */
FREERDP_API WTSVirtualChannelManager* WTSCreateVirtualChannelManager(freerdp_peer* client);
FREERDP_API void WTSDestroyVirtualChannelManager(WTSVirtualChannelManager* vcm);

FREERDP_API void WTSVirtualChannelManagerGetFileDescriptor(WTSVirtualChannelManager* vcm, void** fds, int* fds_count);
FREERDP_API BOOL WTSVirtualChannelManagerCheckFileDescriptor(WTSVirtualChannelManager* vcm);
FREERDP_API HANDLE WTSVirtualChannelManagerGetEventHandle(WTSVirtualChannelManager* vcm);

FREERDP_API BOOL WTSVirtualChannelManagerIsChannelJoined(WTSVirtualChannelManager* vcm, const char* name);

FREERDP_API HANDLE WTSVirtualChannelManagerOpenEx(WTSVirtualChannelManager* vcm, LPSTR pVirtualName, DWORD flags);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_WTSVC_H */
