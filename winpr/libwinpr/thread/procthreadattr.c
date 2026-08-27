/**
 * WinPR: Windows Portable Runtime
 * Process Thread Attribute Lists
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#include <winpr/config.h>

#include <winpr/assert.h>
#include <winpr/error.h>
#include <winpr/thread.h>

#ifndef _WIN32

#include "thread.h"

#include "../handle/handle.h"
#include "../log.h"
#define TAG WINPR_TAG("thread")

BOOL InitializeProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
                                       DWORD dwAttributeCount, DWORD dwFlags, PSIZE_T lpSize)
{
	WINPR_UNUSED(dwFlags);

	if (!lpSize)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	const SIZE_T required = sizeof(struct WINPR_PROC_THREAD_ATTRIBUTE_LIST) +
	                        dwAttributeCount * sizeof(WINPR_PROC_THREAD_ATTRIBUTE_ENTRY);

	/* first call (lpAttributeList == NULL): report the required size and fail, matching real
	 * Windows' documented two-call sizing idiom */
	if (!lpAttributeList || (*lpSize < required))
	{
		*lpSize = required;
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}

	lpAttributeList->capacity = dwAttributeCount;
	lpAttributeList->count = 0;
	return TRUE;
}

BOOL UpdateProcThreadAttribute(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList,
                               WINPR_ATTR_UNUSED DWORD dwFlags, DWORD_PTR Attribute, PVOID lpValue,
                               SIZE_T cbSize, WINPR_ATTR_UNUSED PVOID lpPreviousValue,
                               WINPR_ATTR_UNUSED PSIZE_T lpReturnSize)
{
	if (!lpAttributeList || !lpValue)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	if (lpAttributeList->count >= lpAttributeList->capacity)
	{
		WLog_ERR(TAG, "attribute list is full (capacity=%" PRIu32 ")", lpAttributeList->capacity);
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}

	WINPR_PROC_THREAD_ATTRIBUTE_ENTRY* entry = &lpAttributeList->entries[lpAttributeList->count++];
	entry->Attribute = Attribute;
	entry->lpValue = lpValue;
	entry->cbSize = cbSize;

	if (Attribute == PROC_THREAD_ATTRIBUTE_HANDLE_LIST)
	{
		const HANDLE* handles = (const HANDLE*)lpValue;
		const size_t count = cbSize / sizeof(HANDLE);
		for (size_t i = 0; i < count; i++)
			winpr_Handle_AddRef(handles[i]);
	}

	return TRUE;
}

VOID DeleteProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList)
{
	if (!lpAttributeList)
		return;

	/* the lpAttributeList buffer itself is still the caller's own, allocated from the size
	 * InitializeProcThreadAttributeList() reported - matches real Windows, where
	 * DeleteProcThreadAttributeList() doesn't free that buffer either. Only release the
	 * references UpdateProcThreadAttribute() took on listed handles. */
	for (DWORD i = 0; i < lpAttributeList->count; i++)
	{
		const WINPR_PROC_THREAD_ATTRIBUTE_ENTRY* entry = &lpAttributeList->entries[i];
		if (entry->Attribute != PROC_THREAD_ATTRIBUTE_HANDLE_LIST)
			continue;

		const HANDLE* handles = (const HANDLE*)entry->lpValue;
		const size_t count = entry->cbSize / sizeof(HANDLE);
		for (size_t h = 0; h < count; h++)
			(void)winpr_Handle_Release(handles[h]);
	}
}

#endif
