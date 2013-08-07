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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <winpr/crt.h>

#include <winpr/collections.h>

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

DWORD CountdownEvent_CurrentCount(wCountdownEvent* countdown)
{
	return countdown->count;
}

/**
 * Gets the numbers of signals initially required to set the event.
 */

DWORD CountdownEvent_InitialCount(wCountdownEvent* countdown)
{
	return countdown->initialCount;
}

/**
 * Determines whether the event is set.
 */

BOOL CountdownEvent_IsSet(wCountdownEvent* countdown)
{
	BOOL status = FALSE;

	if (WaitForSingleObject(countdown->event, 0) == WAIT_OBJECT_0)
		status = TRUE;

	return status;
}

/**
 * Gets a WaitHandle that is used to wait for the event to be set.
 */

HANDLE CountdownEvent_WaitHandle(wCountdownEvent* countdown)
{
	return countdown->event;
}

/**
 * Methods
 */

/**
 * Increments the CountdownEvent's current count by a specified value.
 */

void CountdownEvent_AddCount(wCountdownEvent* countdown, DWORD signalCount)
{
	EnterCriticalSection(&countdown->lock);

	countdown->count += signalCount;

	if (countdown->count > 0)
		ResetEvent(countdown->event);

	LeaveCriticalSection(&countdown->lock);
}

/**
 * Registers multiple signals with the CountdownEvent, decrementing the value of CurrentCount by the specified amount.
 */

BOOL CountdownEvent_Signal(wCountdownEvent* countdown, DWORD signalCount)
{
	BOOL status;
	BOOL newStatus;
	BOOL oldStatus;

	status = newStatus = oldStatus = FALSE;

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
		SetEvent(countdown->event);
		status = TRUE;
	}

	LeaveCriticalSection(&countdown->lock);

	return status;
}

/**
 * Resets the InitialCount property to a specified value.
 */

void CountdownEvent_Reset(wCountdownEvent* countdown, DWORD count)
{
	countdown->initialCount = count;
}

/**
 * Construction, Destruction
 */

wCountdownEvent* CountdownEvent_New(DWORD initialCount)
{
	wCountdownEvent* countdown = NULL;

	countdown = (wCountdownEvent*) malloc(sizeof(wCountdownEvent));

	if (countdown)
	{
		countdown->count = initialCount;
		countdown->initialCount = initialCount;
		InitializeCriticalSectionAndSpinCount(&countdown->lock, 4000);
		countdown->event = CreateEvent(NULL, TRUE, FALSE, NULL);

		if (countdown->count == 0)
			SetEvent(countdown->event);
	}

	return countdown;
}

void CountdownEvent_Free(wCountdownEvent* countdown)
{
	DeleteCriticalSection(&countdown->lock);
	CloseHandle(countdown->event);

	free(countdown);
}
