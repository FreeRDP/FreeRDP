/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * OS X Server Event Handling
 *
 * Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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

#include <freerdp/config.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "mf_event.h"

#include <freerdp/log.h>
#define TAG SERVER_TAG("mac")

int mf_is_event_set(mfEventQueue* event_queue)
{
	fd_set rfds;
	int num_set;
	struct timeval time = { 0 };

	FD_ZERO(&rfds);
	FD_SET(event_queue->pipe_fd[0], &rfds);
	num_set = select(event_queue->pipe_fd[0] + 1, &rfds, 0, 0, &time);

	return (num_set == 1);
}

void mf_signal_event(mfEventQueue* event_queue)
{
	int length;

	length = write(event_queue->pipe_fd[1], "sig", 4);

	if (length != 4)
		WLog_ERR(TAG, "mf_signal_event: error");
}

void mf_set_event(mfEventQueue* event_queue)
{
	int length;

	length = write(event_queue->pipe_fd[1], "sig", 4);

	if (length != 4)
		WLog_ERR(TAG, "mf_set_event: error");
}

void mf_clear_events(mfEventQueue* event_queue)
{
	int length;

	while (mf_is_event_set(event_queue))
	{
		length = read(event_queue->pipe_fd[0], &length, 4);

		if (length != 4)
			WLog_ERR(TAG, "mf_clear_event: error");
	}
}

void mf_clear_event(mfEventQueue* event_queue)
{
	int length;

	length = read(event_queue->pipe_fd[0], &length, 4);

	if (length != 4)
		WLog_ERR(TAG, "mf_clear_event: error");
}

void mf_event_push(mfEventQueue* event_queue, mfEvent* event)
{
	pthread_mutex_lock(&(event_queue->mutex));

	if (event_queue->count >= event_queue->size)
	{
		event_queue->size *= 2;
		event_queue->events =
		    (mfEvent**)realloc((void*)event_queue->events, sizeof(mfEvent*) * event_queue->size);
	}

	event_queue->events[(event_queue->count)++] = event;

	pthread_mutex_unlock(&(event_queue->mutex));

	mf_set_event(event_queue);
}

mfEvent* mf_event_peek(mfEventQueue* event_queue)
{
	mfEvent* event;

	pthread_mutex_lock(&(event_queue->mutex));

	if (event_queue->count < 1)
		event = NULL;
	else
		event = event_queue->events[0];

	pthread_mutex_unlock(&(event_queue->mutex));

	return event;
}

mfEvent* mf_event_pop(mfEventQueue* event_queue)
{
	mfEvent* event;

	pthread_mutex_lock(&(event_queue->mutex));

	if (event_queue->count < 1)
		return NULL;

	/* remove event signal */
	mf_clear_event(event_queue);

	event = event_queue->events[0];
	(event_queue->count)--;

	memmove(&event_queue->events[0], &event_queue->events[1], event_queue->count * sizeof(void*));

	pthread_mutex_unlock(&(event_queue->mutex));

	return event;
}

mfEventRegion* mf_event_region_new(int x, int y, int width, int height)
{
	mfEventRegion* event_region = malloc(sizeof(mfEventRegion));

	if (event_region != NULL)
	{
		event_region->x = x;
		event_region->y = y;
		event_region->width = width;
		event_region->height = height;
	}

	return event_region;
}

void mf_event_region_free(mfEventRegion* event_region)
{
	free(event_region);
}

mfEvent* mf_event_new(int type)
{
	mfEvent* event = malloc(sizeof(mfEvent));
	if (!event)
		return NULL;
	event->type = type;
	return event;
}

void mf_event_free(mfEvent* event)
{
	free(event);
}

mfEventQueue* mf_event_queue_new()
{
	mfEventQueue* event_queue = malloc(sizeof(mfEventQueue));

	if (event_queue != NULL)
	{
		event_queue->pipe_fd[0] = -1;
		event_queue->pipe_fd[1] = -1;

		event_queue->size = 16;
		event_queue->count = 0;
		event_queue->events = (mfEvent**)malloc(sizeof(mfEvent*) * event_queue->size);

		if (pipe(event_queue->pipe_fd) < 0)
		{
			free(event_queue);
			return NULL;
		}

		pthread_mutex_init(&(event_queue->mutex), NULL);
	}

	return event_queue;
}

void mf_event_queue_free(mfEventQueue* event_queue)
{
	if (event_queue->pipe_fd[0] != -1)
	{
		close(event_queue->pipe_fd[0]);
		event_queue->pipe_fd[0] = -1;
	}

	if (event_queue->pipe_fd[1] != -1)
	{
		close(event_queue->pipe_fd[1]);
		event_queue->pipe_fd[1] = -1;
	}

	pthread_mutex_destroy(&(event_queue->mutex));
}
