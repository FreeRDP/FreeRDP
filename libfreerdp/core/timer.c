/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Timer implementation
 *
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#include <winpr/thread.h>
#include <winpr/collections.h>

#include <freerdp/timer.h>
#include <freerdp/log.h>
#include "rdp.h"
#include "utils.h"
#include "timer.h"

#if !defined(EMSCRIPTEN)
#define FREERDP_TIMER_SUPPORTED
#else
#define TAG FREERDP_TAG("timer")
#endif

typedef ALIGN64 struct
{
	FreeRDP_TimerID id;
	uint64_t intervallNS;
	uint64_t nextRunTimeNS;
	FreeRDP_TimerCallback cb;
	void* userdata;
	rdpContext* context;
	bool mainloop;
} timer_entry_t;

struct ALIGN64 freerdp_timer_s
{
	rdpRdp* rdp;
	wArrayList* entries;
	HANDLE thread;
	HANDLE event;
	HANDLE mainevent;
	size_t maxIdx;
	bool running;
};

FreeRDP_TimerID freerdp_timer_add(rdpContext* context, uint64_t intervalNS,
                                  FreeRDP_TimerCallback callback, void* userdata, bool mainloop)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->rdp);

#if !defined(FREERDP_TIMER_SUPPORTED)
	WINPR_UNUSED(context);
	WINPR_UNUSED(intervalNS);
	WINPR_UNUSED(callback);
	WINPR_UNUSED(userdata);
	WINPR_UNUSED(mainloop);
	WLog_WARN(TAG, "Platform does not support freerdp_timer_* API");
	return 0;
#else
	FreeRDPTimer* timer = context->rdp->timer;
	WINPR_ASSERT(timer);

	if ((intervalNS == 0) || !callback)
		return false;

	const uint64_t cur = winpr_GetTickCount64NS();
	const timer_entry_t entry = { .id = ++timer->maxIdx,
		                          .intervallNS = intervalNS,
		                          .nextRunTimeNS = cur + intervalNS,
		                          .cb = callback,
		                          .userdata = userdata,
		                          .context = context,
		                          .mainloop = mainloop };

	if (!ArrayList_Append(timer->entries, &entry))
		return 0;
	(void)SetEvent(timer->event);
	return entry.id;
#endif
}

static BOOL foreach_entry(void* data, WINPR_ATTR_UNUSED size_t index, va_list ap)
{
	timer_entry_t* entry = data;
	WINPR_ASSERT(entry);

	FreeRDP_TimerID id = va_arg(ap, FreeRDP_TimerID);

	if (entry->id == id)
	{
		/* Mark the timer to be disabled.
		 * It will be removed on next rescheduling event
		 */
		entry->intervallNS = 0;
		return FALSE;
	}
	return TRUE;
}

bool freerdp_timer_remove(rdpContext* context, FreeRDP_TimerID id)
{
	WINPR_ASSERT(context);
	WINPR_ASSERT(context->rdp);

	FreeRDPTimer* timer = context->rdp->timer;
	WINPR_ASSERT(timer);

	return !ArrayList_ForEach(timer->entries, foreach_entry, id);
}

static BOOL runTimerEvent(timer_entry_t* entry, uint64_t* now)
{
	WINPR_ASSERT(entry);

	entry->intervallNS =
	    entry->cb(entry->context, entry->userdata, entry->id, *now, entry->intervallNS);
	*now = winpr_GetTickCount64NS();
	entry->nextRunTimeNS = *now + entry->intervallNS;
	return TRUE;
}

static BOOL runExpiredTimer(void* data, WINPR_ATTR_UNUSED size_t index,
                            WINPR_ATTR_UNUSED va_list ap)
{
	timer_entry_t* entry = data;
	WINPR_ASSERT(entry);
	WINPR_ASSERT(entry->cb);

	/* Skip all timers that have been deactivated. */
	if (entry->intervallNS == 0)
		return TRUE;

	uint64_t* now = va_arg(ap, uint64_t*);
	WINPR_ASSERT(now);

	bool* mainloop = va_arg(ap, bool*);
	WINPR_ASSERT(mainloop);

	if (entry->nextRunTimeNS > *now)
		return TRUE;

	if (entry->mainloop)
		*mainloop = true;
	else
		runTimerEvent(entry, now);

	return TRUE;
}

#if defined(FREERDP_TIMER_SUPPORTED)
static uint64_t expire_and_reschedule(FreeRDPTimer* timer)
{
	WINPR_ASSERT(timer);

	bool mainloop = false;
	uint64_t next = UINT64_MAX;
	uint64_t now = winpr_GetTickCount64NS();

	ArrayList_Lock(timer->entries);
	ArrayList_ForEach(timer->entries, runExpiredTimer, &now, &mainloop);
	if (mainloop)
		(void)SetEvent(timer->mainevent);

	size_t pos = 0;
	while (pos < ArrayList_Count(timer->entries))
	{
		timer_entry_t* entry = ArrayList_GetItem(timer->entries, pos);
		WINPR_ASSERT(entry);
		if (entry->intervallNS == 0)
		{
			ArrayList_RemoveAt(timer->entries, pos);
			continue;
		}
		if (next > entry->nextRunTimeNS)
			next = entry->nextRunTimeNS;
		pos++;
	}
	ArrayList_Unlock(timer->entries);

	return next;
}

static DWORD WINAPI timer_thread(LPVOID arg)
{
	FreeRDPTimer* timer = arg;
	WINPR_ASSERT(timer);

	// TODO: Currently we only support ms granularity, look for ways to improve
	DWORD timeout = INFINITE;
	HANDLE handles[2] = { utils_get_abort_event(timer->rdp), timer->event };

	while (timer->running &&
	       (WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, timeout) != WAIT_OBJECT_0))
	{
		(void)ResetEvent(timer->event);
		const uint64_t next = expire_and_reschedule(timer);
		const uint64_t now = winpr_GetTickCount64NS();
		if (next == UINT64_MAX)
		{
			timeout = INFINITE;
			continue;
		}

		if (next <= now)
		{
			timeout = 0;
			continue;
		}
		const uint64_t diff = next - now;
		const uint64_t diffMS = diff / 1000000ull;
		timeout = INFINITE;
		if (diffMS < INFINITE)
			timeout = (uint32_t)diffMS;
	}
	return 0;
}
#endif

void freerdp_timer_free(FreeRDPTimer* timer)
{
	if (!timer)
		return;

	timer->running = false;
	if (timer->event)
		(void)SetEvent(timer->event);

	if (timer->thread)
	{
		(void)WaitForSingleObject(timer->thread, INFINITE);
		CloseHandle(timer->thread);
	}
	if (timer->mainevent)
		CloseHandle(timer->mainevent);
	if (timer->event)
		CloseHandle(timer->event);
	ArrayList_Free(timer->entries);
	free(timer);
}

static void* entry_new(const void* val)
{
	const timer_entry_t* entry = val;
	if (!entry)
		return NULL;

	timer_entry_t* copy = calloc(1, sizeof(timer_entry_t));
	if (!copy)
		return NULL;
	*copy = *entry;
	return copy;
}

FreeRDPTimer* freerdp_timer_new(rdpRdp* rdp)
{
	WINPR_ASSERT(rdp);
	FreeRDPTimer* timer = calloc(1, sizeof(FreeRDPTimer));
	if (!timer)
		return NULL;
	timer->rdp = rdp;

	timer->entries = ArrayList_New(TRUE);
	if (!timer->entries)
		goto fail;

	{
		wObject* obj = ArrayList_Object(timer->entries);
		WINPR_ASSERT(obj);
		obj->fnObjectNew = entry_new;
		obj->fnObjectFree = free;
	}

	timer->event = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!timer->event)
		goto fail;

	timer->mainevent = CreateEventA(NULL, TRUE, FALSE, NULL);
	if (!timer->mainevent)
		goto fail;

#if defined(FREERDP_TIMER_SUPPORTED)
	timer->running = true;
	timer->thread = CreateThread(NULL, 0, timer_thread, timer, 0, NULL);
	if (!timer->thread)
		goto fail;
#endif
	return timer;

fail:
	freerdp_timer_free(timer);
	return NULL;
}

static BOOL runExpiredTimerOnMainloop(void* data, WINPR_ATTR_UNUSED size_t index,
                                      WINPR_ATTR_UNUSED va_list ap)
{
	timer_entry_t* entry = data;
	WINPR_ASSERT(entry);
	WINPR_ASSERT(entry->cb);

	/* Skip events not on mainloop */
	if (!entry->mainloop)
		return TRUE;

	/* Skip all timers that have been deactivated. */
	if (entry->intervallNS == 0)
		return TRUE;

	uint64_t* now = va_arg(ap, uint64_t*);
	WINPR_ASSERT(now);

	if (entry->nextRunTimeNS > *now)
		return TRUE;

	runTimerEvent(entry, now);
	return TRUE;
}

bool freerdp_timer_poll(FreeRDPTimer* timer)
{
	WINPR_ASSERT(timer);

	if (WaitForSingleObject(timer->mainevent, 0) != WAIT_OBJECT_0)
		return true;

	ArrayList_Lock(timer->entries);
	(void)ResetEvent(timer->mainevent);
	uint64_t now = winpr_GetTickCount64NS();
	ArrayList_ForEach(timer->entries, runExpiredTimerOnMainloop, &now);
	(void)SetEvent(timer->event); // Trigger a wakeup of timer thread to reschedule
	ArrayList_Unlock(timer->entries);
	return true;
}

HANDLE freerdp_timer_get_event(FreeRDPTimer* timer)
{
	WINPR_ASSERT(timer);
	return timer->mainevent;
}
