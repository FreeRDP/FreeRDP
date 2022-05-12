/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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
#ifndef _WIN32

#include "apc.h"
#include "thread.h"
#include "../log.h"
#include "../synch/pollset.h"
#include <winpr/assert.h>

#define TAG WINPR_TAG("apc")

BOOL apc_init(APC_QUEUE* apc)
{
	pthread_mutexattr_t attr;
	BOOL ret = FALSE;

	WINPR_ASSERT(apc);

	pthread_mutexattr_init(&attr);
	if (pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE) != 0)
	{
		WLog_ERR(TAG, "failed to initialize mutex attributes to recursive");
		return FALSE;
	}

	memset(apc, 0, sizeof(*apc));

	if (pthread_mutex_init(&apc->mutex, &attr) != 0)
	{
		WLog_ERR(TAG, "failed to initialize main thread APC mutex");
		goto out;
	}

	ret = TRUE;
out:
	pthread_mutexattr_destroy(&attr);
	return ret;
}

BOOL apc_uninit(APC_QUEUE* apc)
{
	WINPR_ASSERT(apc);
	return pthread_mutex_destroy(&apc->mutex) == 0;
}

void apc_register(WINPR_THREAD* thread, WINPR_APC_ITEM* addItem)
{
	WINPR_APC_ITEM** nextp;
	APC_QUEUE* apc;

	WINPR_ASSERT(thread);
	WINPR_ASSERT(addItem);

	apc = &thread->apc;
	WINPR_ASSERT(apc);

	pthread_mutex_lock(&apc->mutex);
	if (apc->tail)
	{
		nextp = &apc->tail->next;
		addItem->last = apc->tail;
	}
	else
	{
		nextp = &apc->head;
	}

	*nextp = addItem;
	apc->tail = addItem;
	apc->length++;

	addItem->markedForRemove = FALSE;
	addItem->boundThread = GetCurrentThreadId();
	addItem->linked = TRUE;
	pthread_mutex_unlock(&apc->mutex);
}

static INLINE void apc_item_remove(APC_QUEUE* apc, WINPR_APC_ITEM* item)
{
	WINPR_ASSERT(apc);
	WINPR_ASSERT(item);

	if (!item->last)
		apc->head = item->next;
	else
		item->last->next = item->next;

	if (!item->next)
		apc->tail = item->last;
	else
		item->next->last = item->last;

	apc->length--;
}

APC_REMOVE_RESULT apc_remove(WINPR_APC_ITEM* item)
{
	WINPR_THREAD* thread = winpr_GetCurrentThread();
	APC_QUEUE* apc;
	APC_REMOVE_RESULT ret = APC_REMOVE_OK;

	WINPR_ASSERT(item);

	if (!item->linked)
		return APC_REMOVE_OK;

	if (item->boundThread != GetCurrentThreadId())
	{
		WLog_ERR(TAG, "removing an APC entry should be done in the creating thread");
		return APC_REMOVE_ERROR;
	}

	if (!thread)
	{
		WLog_ERR(TAG, "unable to retrieve current thread");
		return APC_REMOVE_ERROR;
	}

	apc = &thread->apc;
	WINPR_ASSERT(apc);

	pthread_mutex_lock(&apc->mutex);
	if (apc->treatingCompletions)
	{
		item->markedForRemove = TRUE;
		ret = APC_REMOVE_DELAY_FREE;
		goto out;
	}

	apc_item_remove(apc, item);

out:
	pthread_mutex_unlock(&apc->mutex);
	item->boundThread = 0xFFFFFFFF;
	item->linked = FALSE;
	return ret;
}

BOOL apc_collectFds(WINPR_THREAD* thread, WINPR_POLL_SET* set, BOOL* haveAutoSignaled)
{
	WINPR_APC_ITEM* item;
	BOOL ret = FALSE;
	APC_QUEUE* apc;

	WINPR_ASSERT(thread);

	apc = &thread->apc;
	WINPR_ASSERT(apc);

	*haveAutoSignaled = FALSE;
	pthread_mutex_lock(&apc->mutex);
	item = apc->head;
	for (; item; item = item->next)
	{
		if (item->alwaysSignaled)
		{
			WINPR_ASSERT(haveAutoSignaled);
			*haveAutoSignaled = TRUE;
		}
		else if (!pollset_add(set, item->pollFd, item->pollMode))
			goto out;
	}

	ret = TRUE;
out:
	pthread_mutex_unlock(&apc->mutex);
	return ret;
}

int apc_executeCompletions(WINPR_THREAD* thread, WINPR_POLL_SET* set, size_t idx)
{
	APC_QUEUE* apc;
	WINPR_APC_ITEM *item, *nextItem;
	int ret = 0;

	WINPR_ASSERT(thread);

	apc = &thread->apc;
	WINPR_ASSERT(apc);

	pthread_mutex_lock(&apc->mutex);
	apc->treatingCompletions = TRUE;

	/* first pass to compute signaled items */
	for (item = apc->head; item; item = item->next)
	{
		item->isSignaled = item->alwaysSignaled || pollset_isSignaled(set, idx);
		if (!item->alwaysSignaled)
			idx++;
	}

	/* second pass: run completions */
	for (item = apc->head; item; item = nextItem)
	{
		if (item->isSignaled)
		{
			if (item->completion && !item->markedForRemove)
				item->completion(item->completionArgs);
			ret++;
		}

		nextItem = item->next;
	}

	/* third pass: to do final cleanup */
	for (item = apc->head; item; item = nextItem)
	{
		nextItem = item->next;

		if (item->markedForRemove)
		{
			apc_item_remove(apc, item);
			if (item->markedForFree)
				free(item);
		}
	}

	apc->treatingCompletions = FALSE;
	pthread_mutex_unlock(&apc->mutex);

	return ret;
}

void apc_cleanupThread(WINPR_THREAD* thread)
{
	WINPR_APC_ITEM* item;
	WINPR_APC_ITEM* nextItem;
	APC_QUEUE* apc;

	WINPR_ASSERT(thread);

	apc = &thread->apc;
	WINPR_ASSERT(apc);

	pthread_mutex_lock(&apc->mutex);
	item = apc->head;
	for (; item; item = nextItem)
	{
		nextItem = item->next;

		if (item->type == APC_TYPE_HANDLE_FREE)
			item->completion(item->completionArgs);

		item->last = item->next = NULL;
		item->linked = FALSE;
		if (item->markedForFree)
			free(item);
	}

	apc->head = apc->tail = NULL;
	pthread_mutex_unlock(&apc->mutex);
}

#endif
