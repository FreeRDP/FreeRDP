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

#define HANDLE_TYPE_NONE			0
#define HANDLE_TYPE_THREAD			1
#define HANDLE_TYPE_EVENT			2
#define HANDLE_TYPE_MUTEX			3
#define HANDLE_TYPE_SEMAPHORE			4
#define HANDLE_TYPE_TIMER			5
#define HANDLE_TYPE_NAMED_PIPE			6
#define HANDLE_TYPE_ANONYMOUS_PIPE		7

#define WINPR_HANDLE_DEF() \
	ULONG Type

struct winpr_handle
{
	WINPR_HANDLE_DEF();
};
typedef struct winpr_handle WINPR_HANDLE;

#define WINPR_HANDLE_SET_TYPE(_handle, _type) \
	_handle->Type = _type

static inline BOOL winpr_Handle_GetInfo(HANDLE handle, ULONG* pType, PVOID* pObject)
{
	WINPR_HANDLE* wHandle;

	if (handle == NULL)
		return FALSE;

	wHandle = (WINPR_HANDLE*) handle;

	*pType = wHandle->Type;
	*pObject = handle;

	return TRUE;
}

#endif /* WINPR_HANDLE_PRIVATE_H */
