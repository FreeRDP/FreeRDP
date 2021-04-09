/**
 * WinPR: Windows Portable Runtime
 * Asynchronous I/O Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2021 David Fort <contact@hardening-consulting.com>
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

#ifndef WINPR_IO_PRIVATE_H
#define WINPR_IO_PRIVATE_H

#ifndef _WIN32

#include <winpr/io.h>
#include <winpr/collections.h>

#include "../handle/handle.h"
#include "../thread/apc.h"

typedef struct _DEVICE_OBJECT_EX DEVICE_OBJECT_EX;

struct _DEVICE_OBJECT_EX
{
	char* DeviceName;
	char* DeviceFileName;
};

typedef void (*pendingOperationFreeFn)(void* ptr);
typedef struct
{
	DWORD threadId;
	WINPR_APC_ITEM apc;
	wQueue* pendingOperations;
	pendingOperationFreeFn freeOpFn;
} PerThreadOverlap;

PerThreadOverlap* ensureThreadOverlap(DWORD threadId, wHashTable* overlaps, BOOL* created);

struct winpr_overlap_impl
{
	wHashTable* readOverlaps;
	wHashTable* writeOverlaps;
};
typedef struct winpr_overlap_impl WINPR_OVERLAP_IMPL;

BOOL winpr_overlap_init(WINPR_OVERLAP_IMPL* over);
void winpr_overlap_uninit(WINPR_OVERLAP_IMPL* over);

#endif

#endif /* WINPR_IO_PRIVATE_H */
