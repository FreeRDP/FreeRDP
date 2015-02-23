/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Android Event System
 *
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Thincast Technologies GmbH, Author: Martin Fleisz
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>

#include <sys/select.h>
#include <sys/types.h>

#include <freerdp/freerdp.h>
#include <freerdp/log.h>

#define TAG CLIENT_TAG("android")

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "android_freerdp.h"
#include "android_cliprdr.h"

int android_is_event_set(ANDROID_EVENT_QUEUE * queue)
{
	fd_set rfds;
	int num_set;
	struct timeval time;

	FD_ZERO(&rfds);
	FD_SET(queue->pipe_fd[0], &rfds);
	memset(&time, 0, sizeof(time));
	num_set = select(queue->pipe_fd[0] + 1, &rfds, 0, 0, &time);

	return (num_set == 1);
}

void android_set_event(ANDROID_EVENT_QUEUE * queue)
{
	int length;

	length = write(queue->pipe_fd[1], "sig", 4);

	if (length != 4)
		WLog_ERR(TAG, "android_set_event: error");
}


void android_clear_event(ANDROID_EVENT_QUEUE * queue)
{
	int length;

	while (android_is_event_set(queue))
	{
		length = read(queue->pipe_fd[0], &length, 4);

		if (length != 4)
			WLog_ERR(TAG, "android_clear_event: error");
	}
}

void android_push_event(freerdp * inst, ANDROID_EVENT* event)
{

	androidContext* aCtx = (androidContext*)inst->context;
	if (aCtx->event_queue->count >= aCtx->event_queue->size)
	{
		aCtx->event_queue->size = aCtx->event_queue->size * 2;
		aCtx->event_queue->events = realloc((void*) aCtx->event_queue->events,
				sizeof(ANDROID_EVENT*) * aCtx->event_queue->size);
	}

	aCtx->event_queue->events[(aCtx->event_queue->count)++] = event;

	android_set_event(aCtx->event_queue);
}

ANDROID_EVENT* android_peek_event(ANDROID_EVENT_QUEUE * queue)
{
	ANDROID_EVENT* event;

	if (queue->count < 1)
		return NULL;

	event = queue->events[0];

	return event;
}

ANDROID_EVENT* android_pop_event(ANDROID_EVENT_QUEUE * queue)
{
	int i;
	ANDROID_EVENT* event;

	if (queue->count < 1)
		return NULL;

	event = queue->events[0];
	(queue->count)--;

	for (i = 0; i < queue->count; i++)
	{
		queue->events[i] = queue->events[i + 1];
	}

	return event;
}

int android_process_event(ANDROID_EVENT_QUEUE* queue, freerdp* inst)
{
	ANDROID_EVENT* event;
	rdpContext* context = inst->context;
	androidContext* afc  = (androidContext*) context;

	while (android_peek_event(queue))
	{
		event = android_pop_event(queue);

		if (event->type == EVENT_TYPE_KEY)
		{
			ANDROID_EVENT_KEY* key_event = (ANDROID_EVENT_KEY*) event;
			inst->input->KeyboardEvent(inst->input, key_event->flags, key_event->scancode);
			android_event_key_free(key_event);
		}
		else if (event->type == EVENT_TYPE_KEY_UNICODE)
		{
			ANDROID_EVENT_KEY* key_event = (ANDROID_EVENT_KEY*) event;
			inst->input->UnicodeKeyboardEvent(inst->input, key_event->flags, key_event->scancode);
			android_event_key_free(key_event);
		}
		else if (event->type == EVENT_TYPE_CURSOR)
		{
			ANDROID_EVENT_CURSOR* cursor_event = (ANDROID_EVENT_CURSOR*) event;
			inst->input->MouseEvent(inst->input, cursor_event->flags, cursor_event->x, cursor_event->y);
			android_event_cursor_free(cursor_event);
		}
		else if (event->type == EVENT_TYPE_CLIPBOARD)
		{
			BYTE* data;
			UINT32 size;
			UINT32 formatId;
			ANDROID_EVENT_CLIPBOARD* clipboard_event = (ANDROID_EVENT_CLIPBOARD*) event;

			formatId = ClipboardRegisterFormat(afc->clipboard, "UTF8_STRING");

			size = clipboard_event->data_length;

			if (size)
			{
				data = (BYTE*) malloc(size);

				if (!data)
					return -1;

				CopyMemory(data, clipboard_event->data, size);

				ClipboardSetData(afc->clipboard, formatId, (void*) data, size);
			}
			else
			{
				ClipboardEmpty(afc->clipboard);
			}

			android_cliprdr_send_client_format_list(afc->cliprdr);

			android_event_clipboard_free(clipboard_event);
		}
		else if (event->type == EVENT_TYPE_DISCONNECT)
		{
			android_event_disconnect_free(event);
			return 1;
		}
	}

	return 0;
}

BOOL android_get_fds(freerdp * inst, void ** read_fds,
		int * read_count, void ** write_fds, int * write_count)
{
	androidContext* aCtx = (androidContext*)inst->context;
	if (aCtx->event_queue->pipe_fd[0] == -1)
		return TRUE;

	read_fds[*read_count] = (void *)(long) aCtx->event_queue->pipe_fd[0];

	(*read_count)++;
	return TRUE;
}

BOOL android_check_fds(freerdp * inst)
{
	androidContext* aCtx = (androidContext*)inst->context;

	if (aCtx->event_queue->pipe_fd[0] == -1)
		return TRUE;

	if (android_is_event_set(aCtx->event_queue))
	{
		android_clear_event(aCtx->event_queue);
		if(android_process_event(aCtx->event_queue, inst) != 0)
			return FALSE;
	}

	return TRUE;
}

ANDROID_EVENT_KEY* android_event_key_new(int flags, UINT16 scancode)
{
	ANDROID_EVENT_KEY* event;

	event = (ANDROID_EVENT_KEY*) malloc(sizeof(ANDROID_EVENT_KEY));
	memset(event, 0, sizeof(ANDROID_EVENT_KEY));

	event->type = EVENT_TYPE_KEY;
	event->flags = flags;
	event->scancode = scancode;

	return event;
}

void android_event_key_free(ANDROID_EVENT_KEY* event)
{
	if (event != NULL)
		free(event);
}

ANDROID_EVENT_KEY* android_event_unicodekey_new(UINT16 key)
{
	ANDROID_EVENT_KEY* event;

	event = (ANDROID_EVENT_KEY*) malloc(sizeof(ANDROID_EVENT_KEY));
	memset(event, 0, sizeof(ANDROID_EVENT_KEY));

	event->type = EVENT_TYPE_KEY_UNICODE;
	event->scancode = key;

	return event;
}

void android_event_unicodekey_free(ANDROID_EVENT_KEY* event)
{
	if (event != NULL)
		free(event);
}

ANDROID_EVENT_CURSOR* android_event_cursor_new(UINT16 flags, UINT16 x, UINT16 y)
{
	ANDROID_EVENT_CURSOR* event;

	event = (ANDROID_EVENT_CURSOR*) malloc(sizeof(ANDROID_EVENT_CURSOR));
	memset(event, 0, sizeof(ANDROID_EVENT_CURSOR));

	event->type = EVENT_TYPE_CURSOR;
	event->x = x;
	event->y = y;
	event->flags = flags;

	return event;
}

void android_event_cursor_free(ANDROID_EVENT_CURSOR* event)
{
	if (event != NULL)
		free(event);
}

ANDROID_EVENT* android_event_disconnect_new()
{
	ANDROID_EVENT* event;

	event = (ANDROID_EVENT*) malloc(sizeof(ANDROID_EVENT));
	memset(event, 0, sizeof(ANDROID_EVENT));

	event->type = EVENT_TYPE_DISCONNECT;
	return event;
}

void android_event_disconnect_free(ANDROID_EVENT* event)
{
	if (event != NULL)
		free(event);
}

ANDROID_EVENT_CLIPBOARD* android_event_clipboard_new(void* data, int data_length)
{
	ANDROID_EVENT_CLIPBOARD* event;

	event = (ANDROID_EVENT_CLIPBOARD*) malloc(sizeof(ANDROID_EVENT_CLIPBOARD));
	memset(event, 0, sizeof(ANDROID_EVENT_CLIPBOARD));

	event->type = EVENT_TYPE_CLIPBOARD;
	if (data)
	{
		event->data = malloc(data_length);
		memcpy(event->data, data, data_length);
		event->data_length = data_length;
	}

	return event;
}

void android_event_clipboard_free(ANDROID_EVENT_CLIPBOARD* event)
{
	if (event != NULL)
	{
		if (event->data)
		{
			free(event->data);
		}
		free(event);
	}
}

void android_event_queue_init(freerdp * inst)
{
	androidContext* aCtx = (androidContext*)inst->context;

	aCtx->event_queue = (ANDROID_EVENT_QUEUE*) malloc(sizeof(ANDROID_EVENT_QUEUE));
	memset(aCtx->event_queue, 0, sizeof(ANDROID_EVENT_QUEUE));

	aCtx->event_queue->pipe_fd[0] = -1;
	aCtx->event_queue->pipe_fd[1] = -1;

	aCtx->event_queue->size = 16;
	aCtx->event_queue->count = 0;
	aCtx->event_queue->events = (ANDROID_EVENT**) malloc(sizeof(ANDROID_EVENT*) * aCtx->event_queue->size);

	if (pipe(aCtx->event_queue->pipe_fd) < 0)
		WLog_ERR(TAG, "android_pre_connect: pipe failed");
}

void android_event_queue_uninit(freerdp * inst)
{
	androidContext* aCtx = (androidContext*)inst->context;

	if (aCtx->event_queue->pipe_fd[0] != -1)
	{
		close(aCtx->event_queue->pipe_fd[0]);
		aCtx->event_queue->pipe_fd[0] = -1;
	}
	if (aCtx->event_queue->pipe_fd[1] != -1)
	{
		close(aCtx->event_queue->pipe_fd[1]);
		aCtx->event_queue->pipe_fd[1] = -1;
	}
}
