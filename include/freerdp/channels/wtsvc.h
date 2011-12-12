/**
 * FreeRDP: A Remote Desktop Protocol client.
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

#ifndef __FREERDP_WTSVC_H
#define __FREERDP_WTSVC_H

#include <freerdp/types.h>
#include <freerdp/peer.h>

typedef struct WTSVirtualChannelManager WTSVirtualChannelManager;

#define WTS_CHANNEL_OPTION_DYNAMIC 0x00000001

typedef enum _WTS_VIRTUAL_CLASS
{
	WTSVirtualClientData,
	WTSVirtualFileHandle 
} WTS_VIRTUAL_CLASS;

/**
 * WTSVirtualChannelManager functions are FreeRDP extensions to the API.
 */
FREERDP_API WTSVirtualChannelManager* WTSCreateVirtualChannelManager(freerdp_peer* client);
FREERDP_API void WTSDestroyVirtualChannelManager(WTSVirtualChannelManager* vcm);
FREERDP_API void WTSVirtualChannelManagerGetFileDescriptor(WTSVirtualChannelManager* vcm,
	void** fds, int* fds_count);
FREERDP_API boolean WTSVirtualChannelManagerCheckFileDescriptor(WTSVirtualChannelManager* vcm);

/**
 * Opens a static or dynamic virtual channel and return the handle. If the
 * operation fails, a NULL handle is returned.
 * 
 * The original MS API has 'DWORD SessionId' as the first argument, while we
 * use our WTSVirtualChannelManager object instead.
 *
 * This functions should be called only from the main thread.
 */
FREERDP_API void* WTSVirtualChannelOpenEx(
	/* __in */ WTSVirtualChannelManager* vcm,
	/* __in */ const char* pVirtualName,
	/* __in */ uint32 flags);

/**
 * Returns information about a specified virtual channel.
 *
 * Servers use this function to gain access to a virtual channel file handle
 * that can be used for asynchronous I/O.
 */
FREERDP_API boolean WTSVirtualChannelQuery(
	/* __in */  void* hChannelHandle,
	/* __in */  WTS_VIRTUAL_CLASS WtsVirtualClass,
	/* __out */ void** ppBuffer,
	/* __out */ uint32* pBytesReturned);

/**
 * Frees memory allocated by WTSVirtualChannelQuery
 */
FREERDP_API void WTSFreeMemory(
	/* __in */ void* pMemory);

/**
 * Reads data from the server end of a virtual channel.
 *
 * FreeRDP behavior:
 *
 * This function will always return a complete channel data packet, i.e. chunks
 * are already assembled. If BufferSize argument is smaller than the packet
 * size, it will set the desired size in pBytesRead and return false. The
 * caller should allocate a large enough buffer and call this function again.
 * Returning false with pBytesRead set to zero indicates an error has occurred.
 * If no pending packet to be read, it will set pBytesRead to zero and return
 * true.
 *
 * TimeOut is not supported, and this function will always return immediately.
 * The caller should use the file handle returned by WTSVirtualChannelQuery to
 * determine whether a packet has arrived.
 */
FREERDP_API boolean WTSVirtualChannelRead(
	/* __in */  void* hChannelHandle,
	/* __in */  uint32 TimeOut,
	/* __out */ uint8* Buffer,
	/* __in */  uint32 BufferSize,
	/* __out */ uint32* pBytesRead);

/**
 * Writes data to the server end of a virtual channel.
 */
FREERDP_API boolean WTSVirtualChannelWrite(
	/* __in */  void* hChannelHandle,
	/* __in */  uint8* Buffer,
	/* __in */  uint32 Length,
	/* __out */ uint32* pBytesWritten);

/**
 * Closes an open virtual channel handle.
 */
FREERDP_API boolean WTSVirtualChannelClose(
	/* __in */ void* hChannelHandle);

#endif /* __FREERDP_WTSVC_H */
