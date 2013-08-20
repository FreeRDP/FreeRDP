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
//#include <winpr/wtsapi.h>

typedef enum _WTS_VIRTUAL_CLASS
{
	WTSVirtualClientData,
	WTSVirtualFileHandle,
	WTSVirtualEventHandle, /* Extended */
	WTSVirtualChannelReady /* Extended */
} WTS_VIRTUAL_CLASS;

#define WTS_CHANNEL_OPTION_DYNAMIC			0x00000001
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_LOW		0x00000000
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_MED		0x00000002
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_HIGH		0x00000004
#define WTS_CHANNEL_OPTION_DYNAMIC_PRI_REAL		0x00000006
#define WTS_CHANNEL_OPTION_DYNAMIC_NO_COMPRESS		0x00000008

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

/**
 * Opens a static or dynamic virtual channel and return the handle. If the
 * operation fails, a NULL handle is returned.
 * 
 * The original MS API has 'DWORD SessionId' as the first argument, while we
 * use our WTSVirtualChannelManager object instead.
 *
 * Static virtual channels must be opened from the main thread. Dynamic virtual channels
 * can be opened from any thread.
 */

// WINPR_API HANDLE WTSVirtualChannelOpenEx(DWORD SessionId, LPSTR pVirtualName, DWORD flags);

WINPR_API HANDLE WTSVirtualChannelManagerOpenEx(WTSVirtualChannelManager* vcm, LPSTR pVirtualName, DWORD flags);

/**
 * Returns information about a specified virtual channel.
 *
 * Servers use this function to gain access to a virtual channel file handle
 * that can be used for asynchronous I/O.
 */

WINPR_API BOOL WTSVirtualChannelQuery(HANDLE hChannelHandle, WTS_VIRTUAL_CLASS WtsVirtualClass, PVOID* ppBuffer, DWORD* pBytesReturned);

/**
 * Frees memory allocated by WTSVirtualChannelQuery
 */

WINPR_API VOID WTSFreeMemory(PVOID pMemory);

/**
 * Reads data from the server end of a virtual channel.
 *
 * FreeRDP behavior:
 *
 * This function will always return a complete channel data packet, i.e. chunks
 * are already assembled. If BufferSize argument is smaller than the packet
 * size, it will set the desired size in pBytesRead and return FALSE. The
 * caller should allocate a large enough buffer and call this function again.
 * Returning FALSE with pBytesRead set to zero indicates an error has occurred.
 * If no pending packet to be read, it will set pBytesRead to zero and return
 * TRUE.
 *
 * TimeOut is not supported, and this function will always return immediately.
 * The caller should use the file handle returned by WTSVirtualChannelQuery to
 * determine whether a packet has arrived.
 */

WINPR_API BOOL WTSVirtualChannelRead(HANDLE hChannelHandle, ULONG TimeOut, PCHAR Buffer, ULONG BufferSize, PULONG pBytesRead);

/**
 * Writes data to the server end of a virtual channel.
 */

WINPR_API BOOL WTSVirtualChannelWrite(HANDLE hChannelHandle, PCHAR Buffer, ULONG Length, PULONG pBytesWritten);

/**
 * Closes an open virtual channel handle.
 */

WINPR_API BOOL WTSVirtualChannelClose(HANDLE hChannelHandle);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_WTSVC_H */
