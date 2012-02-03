/**
 * FreeRDP: A Remote Desktop Protocol Client
 * X11 Server Event Handling
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <freerdp/utils/memory.h>

#include "xf_event.h"

int xf_is_event_set(xfEventQueue* event_queue)
{
	fd_set rfds;
	int num_set;
	struct timeval time;

	FD_ZERO(&rfds);
	FD_SET(event_queue->pipe_fd[0], &rfds);
	memset(&time, 0, sizeof(time));
	num_set = select(event_queue->pipe_fd[0] + 1, &rfds, 0, 0, &time);

	return (num_set == 1);
}

void xf_signal_event(xfEventQueue* event_queue)
{
	int length;

	length = write(event_queue->pipe_fd[1], "sig", 4);

	if (length != 4)
		printf("xf_signal_event: error\n");
}

void xf_set_event(xfEventQueue* event_queue)
{
	int length;

	length = write(event_queue->pipe_fd[1], "sig", 4);

	if (length != 4)
		printf("xf_set_event: error\n");
}

void xf_clear_event(xfEventQueue* event_queue)
{
	int length;

	while (xf_is_event_set(event_queue))
	{
		length = read(event_queue->pipe_fd[0], &length, 4);

		if (length != 4)
			printf("xf_clear_event: error\n");
	}
}

void xf_event_push(xfEventQueue* event_queue, xfEvent* event)
{
	pthread_mutex_lock(&(event_queue->mutex));

	if (event_queue->count >= event_queue->size)
	{
		event_queue->size *= 2;
		event_queue->events = (xfEvent**) xrealloc((void*) event_queue->events, sizeof(xfEvent*) * event_queue->size);
	}

	event_queue->events[(event_queue->count)++] = event;

	xf_set_event(event_queue);

	pthread_mutex_unlock(&(event_queue->mutex));
}

xfEvent* xf_event_peek(xfEventQueue* event_queue)
{
	xfEvent* event;

	pthread_mutex_lock(&(event_queue->mutex));

	if (event_queue->count < 1)
		event = NULL;
	else
		event = event_queue->events[0];

	pthread_mutex_unlock(&(event_queue->mutex));

	return event;
}

xfEvent* xf_event_pop(xfEventQueue* event_queue)
{
	xfEvent* event;

	pthread_mutex_lock(&(event_queue->mutex));

	if (event_queue->count < 1)
		return NULL;

	event = event_queue->events[0];
	(event_queue->count)--;

	memmove(&event_queue->events[0], &event_queue->events[1], event_queue->count * sizeof(void*));

	pthread_mutex_unlock(&(event_queue->mutex));

	return event;
}

xfEventRegion* xf_event_region_new(int x, int y, int width, int height)
{
	xfEventRegion* event_region = xnew(xfEventRegion);

	if (event_region != NULL)
	{
		event_region->x = x;
		event_region->y = y;
		event_region->width = width;
		event_region->height = height;
	}

	return event_region;
}

void xf_event_region_free(xfEventRegion* event_region)
{
	xfree(event_region);
}

xfEvent* xf_event_new(int type)
{
	xfEvent* event = xnew(xfEvent);
	event->type = type;
	return event;
}

void xf_event_free(xfEvent* event)
{
	xfree(event);
}

xfEventQueue* xf_event_queue_new()
{
	xfEventQueue* event_queue = xnew(xfEventQueue);

	if (event_queue != NULL)
	{
		event_queue->pipe_fd[0] = -1;
		event_queue->pipe_fd[1] = -1;

		event_queue->size = 16;
		event_queue->count = 0;
		event_queue->events = (xfEvent**) xzalloc(sizeof(xfEvent*) * event_queue->size);

		if (pipe(event_queue->pipe_fd) < 0)
			printf("xf_event_queue_new: pipe failed\n");

		pthread_mutex_init(&(event_queue->mutex), NULL);
	}

	return event_queue;
}

void xf_event_queue_free(xfEventQueue* event_queue)
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
