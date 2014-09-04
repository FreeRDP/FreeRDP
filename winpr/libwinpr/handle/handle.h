/**
 * WinPR: Windows Portable Runtime
 * Handle Management
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

#ifndef WINPR_HANDLE_PRIVATE_H
#define WINPR_HANDLE_PRIVATE_H

#include <winpr/handle.h>
#include <winpr/file.h>

#define HANDLE_TYPE_NONE			0
#define HANDLE_TYPE_PROCESS			1
#define HANDLE_TYPE_THREAD			2
#define HANDLE_TYPE_EVENT			3
#define HANDLE_TYPE_MUTEX			4
#define HANDLE_TYPE_SEMAPHORE			5
#define HANDLE_TYPE_TIMER			6
#define HANDLE_TYPE_NAMED_PIPE			7
#define HANDLE_TYPE_ANONYMOUS_PIPE		8
#define HANDLE_TYPE_ACCESS_TOKEN		9
#define HANDLE_TYPE_FILE			10
#define HANDLE_TYPE_TIMER_QUEUE			11
#define HANDLE_TYPE_TIMER_QUEUE_TIMER		12
#define HANDLE_TYPE_COMM			13

#define WINPR_HANDLE_DEF() \
	ULONG Type

struct winpr_handle
{
	WINPR_HANDLE_DEF();
};
typedef struct winpr_handle WINPR_HANDLE;

#define WINPR_HANDLE_SET_TYPE(_handle, _type) \
	_handle->Type = _type

static INLINE BOOL winpr_Handle_GetInfo(HANDLE handle, ULONG* pType, PVOID* pObject)
{
	WINPR_HANDLE* wHandle;

	if (handle == NULL || handle == INVALID_HANDLE_VALUE)
		return FALSE;

	wHandle = (WINPR_HANDLE*) handle;

	*pType = wHandle->Type;
	*pObject = handle;

	return TRUE;
}

typedef BOOL (*pcIsHandled)(HANDLE handle);
typedef BOOL (*pcCloseHandle)(HANDLE handle);

typedef struct _HANDLE_CLOSE_CB
{
	pcIsHandled IsHandled;
	pcCloseHandle CloseHandle;
} HANDLE_CLOSE_CB;

BOOL RegisterHandleCloseCb(HANDLE_CLOSE_CB *pHandleClose);

#endif /* WINPR_HANDLE_PRIVATE_H */
