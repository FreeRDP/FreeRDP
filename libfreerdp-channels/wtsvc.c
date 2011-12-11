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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <freerdp/freerdp.h>
#include <freerdp/utils/memory.h>
#include <freerdp/channels/wtsvc.h>

void* WTSVirtualChannelOpenEx(
	/* __in */ freerdp_peer* client,
	/* __in */ const char* pVirtualName,
	/* __in */ uint32 flags)
{
	return NULL;
}

boolean WTSVirtualChannelQuery(
	/* __in */  void* hChannelHandle,
	/* __in */  WTS_VIRTUAL_CLASS WtsVirtualClass,
	/* __out */ void** ppBuffer,
	/* __out */ uint32* pBytesReturned)
{
	return false;
}

void WTSFreeMemory(
	/* __in */ void* pMemory)
{
}

boolean WTSVirtualChannelRead(
	/* __in */  void* hChannelHandle,
	/* __in */  uint32 TimeOut,
	/* __out */ uint8* Buffer,
	/* __in */  uint32 BufferSize,
	/* __out */ uint32* pBytesRead)
{
	return false;
}

boolean WTSVirtualChannelWrite(
	/* __in */  void* hChannelHandle,
	/* __in */  uint8* Buffer,
	/* __in */  uint32 Length,
	/* __out */ uint32* pBytesWritten)
{
	return false;
}

boolean WTSVirtualChannelClose(
	/* __in */ void* hChannelHandle)
{
	return false;
}
