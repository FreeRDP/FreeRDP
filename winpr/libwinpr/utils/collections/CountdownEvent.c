/**
 * WinPR: Windows Portable Runtime
 * Countdown Event
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

#include <winpr/config.h>
#include <winpr/assert.h>

#include <winpr/crt.h>

#include <winpr/collections.h>

struct CountdownEvent
{
	size_t count;
	CRITICAL_SECTION lock;
	HANDLE event;
	size_t initialCount;
};

/**
 * C equivalent of the C# CountdownEvent Class
 * http://msdn.microsoft.com/en-us/library/dd235708/
 */

/**
 * Properties
 */

/**
 * Gets the number of remaining signals required to set the event.
 */

size_t CountdownEvent_CurrentCount(wCountdownEvent* countdown)
{
	WINPR_ASSERT(countdown);
	EnterCriticalSection(&countdown->lock);
	const size_t rc = countdown->count;
	LeaveCriticalSection(&countdown->lock);
	return rc;
}

/**
 * Gets the numbers of signals initially required to set the event.
 */

size_t CountdownEvent_InitialCount(wCountdownEvent* countdown)
{
	WINPR_ASSERT(countdown);
	EnterCriticalSection(&countdown->lock);
	const size_t rc = countdown->initialCount;
	LeaveCriticalSection(&countdown->lock);
	return rc;
}

/**
 * Determines whether the event is set.
 */

BOOL CountdownEvent_IsSet(wCountdownEvent* countdown)
{
	BOOL status = FALSE;

	WINPR_ASSERT(countdown);
	if (WaitForSingleObject(countdown->event, 0) == WAIT_OBJECT_0)
		status = TRUE;

	return status;
}

/**
 * Gets a WaitHandle that is used to wait for the event to be set.
 */

HANDLE CountdownEvent_WaitHandle(wCountdownEvent* countdown)
{
	WINPR_ASSERT(countdown);
	return countdown->event;
}

/**
 * Methods
 */

/**
 * Increments the CountdownEvent's current count by a specified value.
 */

void CountdownEvent_AddCount(wCountdownEvent* countdown, size_t signalCount)
{
	WINPR_ASSERT(countdown);
	EnterCriticalSection(&countdown->lock);

	const BOOL signalSet = countdown->count == 0;
	countdown->count += signalCount;

	if (signalSet)
		(void)ResetEvent(countdown->event);

	LeaveCriticalSection(&countdown->lock);
}

/**
 * Registers multiple signals with the CountdownEvent, decrementing the value of CurrentCount by the
 * specified amount.
 */

BOOL CountdownEvent_Signal(wCountdownEvent* countdown, size_t signalCount)
{
	BOOL status = FALSE;
	BOOL newStatus = FALSE;
	BOOL oldStatus = FALSE;

	WINPR_ASSERT(countdown);

	EnterCriticalSection(&countdown->lock);

	if (WaitForSingleObject(countdown->event, 0) == WAIT_OBJECT_0)
		oldStatus = TRUE;

	if (signalCount <= countdown->count)
		countdown->count -= signalCount;
	else
		countdown->count = 0;

	if (countdown->count == 0)
		newStatus = TRUE;

	if (newStatus && (!oldStatus))
	{
		(void)SetEvent(countdown->event);
		status = TRUE;
	}

	LeaveCriticalSection(&countdown->lock);

	return status;
}

/**
 * Resets the InitialCount property to a specified value.
 */

void CountdownEvent_Reset(wCountdownEvent* countdown, size_t count)
{
	WINPR_ASSERT(countdown);
	countdown->initialCount = count;
}

/**
 * Construction, Destruction
 */

wCountdownEvent* CountdownEvent_New(size_t initialCount)
{
	wCountdownEvent* countdown = (wCountdownEvent*)calloc(1, sizeof(wCountdownEvent));

	if (!countdown)
		return NULL;

	countdown->count = initialCount;
	countdown->initialCount = initialCount;

	if (!InitializeCriticalSectionAndSpinCount(&countdown->lock, 4000))
		goto fail;

	countdown->event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!countdown->event)
		goto fail;

	if (countdown->count == 0)
	{
		if (!SetEvent(countdown->event))
			goto fail;
	}

	return countdown;

fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	CountdownEvent_Free(countdown);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void CountdownEvent_Free(wCountdownEvent* countdown)
{
	if (!countdown)
		return;

	DeleteCriticalSection(&countdown->lock);
	(void)CloseHandle(countdown->event);

	free(countdown);
}
