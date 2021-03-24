/**
 * WinPR: Windows Portable Runtime
 * APC implementation
 *
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

#ifndef WINPR_APC_H
#define WINPR_APC_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

#include <pthread.h>

typedef struct winpr_thread WINPR_THREAD;
typedef struct winpr_APC_item WINPR_APC_ITEM;
typedef struct winpr_poll_set WINPR_POLL_SET;

typedef void (*apc_treatment)(LPVOID arg);

typedef enum
{
	APC_TYPE_USER,
	APC_TYPE_TIMER,
	APC_TYPE_HANDLE_FREE
} ApcType;

struct winpr_APC_item
{
	ApcType type;
	int pollFd;
	DWORD pollMode;
	apc_treatment completion;
	LPVOID completionArgs;
	BOOL markedForFree;

	/* private fields used by the APC */
	BOOL alwaysSignaled;
	BOOL isSignaled;
	DWORD boundThread;
	BOOL linked;
	BOOL markedForRemove;
	WINPR_APC_ITEM *last, *next;
};

typedef enum
{
	APC_REMOVE_OK,
	APC_REMOVE_ERROR,
	APC_REMOVE_DELAY_FREE
} APC_REMOVE_RESULT;

typedef struct
{
	pthread_mutex_t mutex;
	DWORD length;
	WINPR_APC_ITEM *head, *tail;
	BOOL treatingCompletions;
} APC_QUEUE;

BOOL apc_init(APC_QUEUE* apc);
BOOL apc_uninit(APC_QUEUE* apc);
void apc_register(WINPR_THREAD* thread, WINPR_APC_ITEM* addItem);
APC_REMOVE_RESULT apc_remove(WINPR_APC_ITEM* item);
BOOL apc_collectFds(WINPR_THREAD* thread, WINPR_POLL_SET* set, BOOL* haveAutoSignaled);
int apc_executeCompletions(WINPR_THREAD* thread, WINPR_POLL_SET* set, size_t startIndex);
void apc_cleanupThread(WINPR_THREAD* thread);
#endif

#endif /* WINPR_APC_H */
