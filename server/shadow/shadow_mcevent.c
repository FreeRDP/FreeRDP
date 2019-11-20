/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2015 Jiang Zihao <zihao.jiang@yahoo.com>
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

#include <assert.h>
#include <freerdp/log.h>
#include "shadow.h"

#define TAG SERVER_TAG("shadow.mcevent")

struct rdp_shadow_multiclient_event
{
	HANDLE event;        /* Kickoff event */
	HANDLE barrierEvent; /* Represents that all clients have consumed event */
	HANDLE doneEvent;    /* Event handling finished. Server could continue */
	wArrayList* subscribers;
	CRITICAL_SECTION lock;
	int consuming;
	int waiting;

	/* For debug */
	int eventid;
};

struct rdp_shadow_multiclient_subscriber
{
	rdpShadowMultiClientEvent* ref;
	BOOL pleaseHandle; /* Indicate if server expects my handling in this turn */
};

rdpShadowMultiClientEvent* shadow_multiclient_new(void)
{
	rdpShadowMultiClientEvent* event =
	    (rdpShadowMultiClientEvent*)calloc(1, sizeof(rdpShadowMultiClientEvent));
	if (!event)
		goto out_error;

	event->event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!event->event)
		goto out_free;

	event->barrierEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!event->barrierEvent)
		goto out_free_event;

	event->doneEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (!event->doneEvent)
		goto out_free_barrierEvent;

	event->subscribers = ArrayList_New(TRUE);
	if (!event->subscribers)
		goto out_free_doneEvent;

	if (!InitializeCriticalSectionAndSpinCount(&(event->lock), 4000))
		goto out_free_subscribers;

	event->consuming = 0;
	event->waiting = 0;
	event->eventid = 0;
	SetEvent(event->doneEvent);
	return event;

out_free_subscribers:
	ArrayList_Free(event->subscribers);
out_free_doneEvent:
	CloseHandle(event->doneEvent);
out_free_barrierEvent:
	CloseHandle(event->barrierEvent);
out_free_event:
	CloseHandle(event->event);
out_free:
	free(event);
out_error:
	return (rdpShadowMultiClientEvent*)NULL;
}

void shadow_multiclient_free(rdpShadowMultiClientEvent* event)
{
	if (!event)
		return;

	DeleteCriticalSection(&(event->lock));

	ArrayList_Free(event->subscribers);
	CloseHandle(event->doneEvent);
	CloseHandle(event->barrierEvent);
	CloseHandle(event->event);
	free(event);

	return;
}

static void _Publish(rdpShadowMultiClientEvent* event)
{
	wArrayList* subscribers;
	struct rdp_shadow_multiclient_subscriber* subscriber = NULL;
	int i;

	subscribers = event->subscribers;

	assert(event->consuming == 0);

	/* Count subscribing clients */
	ArrayList_Lock(subscribers);
	for (i = 0; i < ArrayList_Count(subscribers); i++)
	{
		subscriber = (struct rdp_shadow_multiclient_subscriber*)ArrayList_GetItem(subscribers, i);
		/* Set flag to subscriber: I acknowledge and please handle */
		subscriber->pleaseHandle = TRUE;
		event->consuming++;
	}
	ArrayList_Unlock(subscribers);

	if (event->consuming > 0)
	{
		event->eventid = (event->eventid & 0xff) + 1;
		WLog_VRB(TAG, "Server published event %d. %d clients.\n", event->eventid, event->consuming);
		ResetEvent(event->doneEvent);
		SetEvent(event->event);
	}

	return;
}

static void _WaitForSubscribers(rdpShadowMultiClientEvent* event)
{
	if (event->consuming > 0)
	{
		/* Wait for clients done */
		WLog_VRB(TAG, "Server wait event %d. %d clients.\n", event->eventid, event->consuming);
		LeaveCriticalSection(&(event->lock));
		WaitForSingleObject(event->doneEvent, INFINITE);
		EnterCriticalSection(&(event->lock));
		WLog_VRB(TAG, "Server quit event %d. %d clients.\n", event->eventid, event->consuming);
	}

	/* Last subscriber should have already reset the event */
	assert(WaitForSingleObject(event->event, 0) != WAIT_OBJECT_0);

	return;
}

void shadow_multiclient_publish(rdpShadowMultiClientEvent* event)
{
	if (!event)
		return;

	EnterCriticalSection(&(event->lock));
	_Publish(event);
	LeaveCriticalSection(&(event->lock));

	return;
}
void shadow_multiclient_wait(rdpShadowMultiClientEvent* event)
{
	if (!event)
		return;

	EnterCriticalSection(&(event->lock));
	_WaitForSubscribers(event);
	LeaveCriticalSection(&(event->lock));

	return;
}
void shadow_multiclient_publish_and_wait(rdpShadowMultiClientEvent* event)
{
	if (!event)
		return;

	EnterCriticalSection(&(event->lock));
	_Publish(event);
	_WaitForSubscribers(event);
	LeaveCriticalSection(&(event->lock));

	return;
}

static BOOL _Consume(struct rdp_shadow_multiclient_subscriber* subscriber, BOOL wait)
{
	rdpShadowMultiClientEvent* event = subscriber->ref;
	BOOL ret = FALSE;

	if (WaitForSingleObject(event->event, 0) == WAIT_OBJECT_0 && subscriber->pleaseHandle)
	{
		/* Consume my share. Server is waiting for us */
		event->consuming--;
		ret = TRUE;
	}

	assert(event->consuming >= 0);

	if (event->consuming == 0)
	{
		/* Last client reset event before notify clients to continue */
		ResetEvent(event->event);

		if (event->waiting > 0)
		{
			/* Notify other clients to continue */
			SetEvent(event->barrierEvent);
		}
		else
		{
			/* Only one client. Notify server directly */
			SetEvent(event->doneEvent);
		}
	}
	else /* (event->consuming > 0) */
	{
		if (wait)
		{
			/*
			 * This client need to wait. That means the client will
			 * continue waiting for other clients to finish.
			 * The last client should reset barrierEvent.
			 */
			event->waiting++;
			LeaveCriticalSection(&(event->lock));
			WaitForSingleObject(event->barrierEvent, INFINITE);
			EnterCriticalSection(&(event->lock));
			event->waiting--;
			if (event->waiting == 0)
			{
				/*
				 * This is last client waiting for barrierEvent.
				 * We can now discard barrierEvent and notify
				 * server to continue.
				 */
				ResetEvent(event->barrierEvent);
				SetEvent(event->doneEvent);
			}
		}
	}

	return ret;
}

void* shadow_multiclient_get_subscriber(rdpShadowMultiClientEvent* event)
{
	struct rdp_shadow_multiclient_subscriber* subscriber;

	if (!event)
		return NULL;

	EnterCriticalSection(&(event->lock));

	subscriber = (struct rdp_shadow_multiclient_subscriber*)calloc(
	    1, sizeof(struct rdp_shadow_multiclient_subscriber));
	if (!subscriber)
		goto out_error;

	subscriber->ref = event;
	subscriber->pleaseHandle = FALSE;

	if (ArrayList_Add(event->subscribers, subscriber) < 0)
		goto out_free;

	WLog_VRB(TAG, "Get subscriber %p. Wait event %d. %d clients.\n", (void*)subscriber,
	         event->eventid, event->consuming);
	(void)_Consume(subscriber, TRUE);
	WLog_VRB(TAG, "Get subscriber %p. Quit event %d. %d clients.\n", (void*)subscriber,
	         event->eventid, event->consuming);

	LeaveCriticalSection(&(event->lock));

	return subscriber;

out_free:
	free(subscriber);
out_error:
	LeaveCriticalSection(&(event->lock));
	return NULL;
}

/*
 * Consume my share and release my register
 * If we have update event and pleaseHandle flag
 * We need to consume. Anyway we need to clear
 * pleaseHandle flag
 */
void shadow_multiclient_release_subscriber(void* subscriber)
{
	struct rdp_shadow_multiclient_subscriber* s;
	rdpShadowMultiClientEvent* event;

	if (!subscriber)
		return;

	s = (struct rdp_shadow_multiclient_subscriber*)subscriber;
	event = s->ref;

	EnterCriticalSection(&(event->lock));

	WLog_VRB(TAG, "Release Subscriber %p. Drop event %d. %d clients.\n", subscriber, event->eventid,
	         event->consuming);
	(void)_Consume(s, FALSE);
	WLog_VRB(TAG, "Release Subscriber %p. Quit event %d. %d clients.\n", subscriber, event->eventid,
	         event->consuming);

	ArrayList_Remove(event->subscribers, subscriber);

	LeaveCriticalSection(&(event->lock));

	free(subscriber);

	return;
}

BOOL shadow_multiclient_consume(void* subscriber)
{
	struct rdp_shadow_multiclient_subscriber* s;
	rdpShadowMultiClientEvent* event;
	BOOL ret = FALSE;

	if (!subscriber)
		return ret;

	s = (struct rdp_shadow_multiclient_subscriber*)subscriber;
	event = s->ref;

	EnterCriticalSection(&(event->lock));

	WLog_VRB(TAG, "Subscriber %p wait event %d. %d clients.\n", subscriber, event->eventid,
	         event->consuming);
	ret = _Consume(s, TRUE);
	WLog_VRB(TAG, "Subscriber %p quit event %d. %d clients.\n", subscriber, event->eventid,
	         event->consuming);

	LeaveCriticalSection(&(event->lock));

	return ret;
}

HANDLE shadow_multiclient_getevent(void* subscriber)
{
	if (!subscriber)
		return (HANDLE)NULL;

	return ((struct rdp_shadow_multiclient_subscriber*)subscriber)->ref->event;
}
