/**
 * WinPR: Windows Portable Runtime
 * Memory Allocation
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <winpr/heap.h>

/* Memory Allocation: http://msdn.microsoft.com/en-us/library/hk1k7x6x.aspx */
/* Memory Management Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa366781/ */

#ifndef _WIN32

HANDLE GetProcessHeap(void)
{
	return NULL;
}

LPVOID HeapAlloc(HANDLE hHeap, DWORD dwFlags, SIZE_T dwBytes)
{
	LPVOID lpMem = NULL;

	if (dwFlags & HEAP_ZERO_MEMORY)
		lpMem = calloc(1, dwBytes);
	else
		lpMem = malloc(dwBytes);

	return lpMem;
}

LPVOID HeapReAlloc(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem, SIZE_T dwBytes)
{
	LPVOID lpNewMem;

	lpNewMem = realloc(lpMem, dwBytes);

	return lpNewMem;
}

BOOL HeapFree(HANDLE hHeap, DWORD dwFlags, LPVOID lpMem)
{
	free(lpMem);
	return TRUE;
}

#endif
