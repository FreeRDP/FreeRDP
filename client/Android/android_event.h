/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Android Event System
 *
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef FREERDP_ANDROID_EVENT_H
#define FREERDP_ANDROID_EVENT_H
#include <freerdp/freerdp.h>

#define EVENT_TYPE_KEY			1
#define EVENT_TYPE_CURSOR		2
#define EVENT_TYPE_DISCONNECT		3
#define EVENT_TYPE_KEY_UNICODE		4
#define EVENT_TYPE_CLIPBOARD	5

struct _ANDROID_EVENT
{
	int type;
};
typedef struct _ANDROID_EVENT ANDROID_EVENT;

struct _ANDROID_EVENT_KEY
{
	int type;
	int flags;
	UINT16 scancode;
};
typedef struct _ANDROID_EVENT_KEY ANDROID_EVENT_KEY;

struct _ANDROID_EVENT_CURSOR
{
	int type;
	UINT16 flags;
	UINT16 x;
	UINT16 y;
};
typedef struct _ANDROID_EVENT_CURSOR ANDROID_EVENT_CURSOR;

struct _ANDROID_EVENT_CLIPBOARD
{
	int type;
	void* data;
	int data_length;
};
typedef struct _ANDROID_EVENT_CLIPBOARD ANDROID_EVENT_CLIPBOARD;

struct _ANDROID_EVENT_QUEUE
{
	int size;
	int count;
	HANDLE isSet;
	ANDROID_EVENT **events;
};
typedef struct _ANDROID_EVENT_QUEUE ANDROID_EVENT_QUEUE;

BOOL android_push_event(freerdp * inst, ANDROID_EVENT* event);

HANDLE android_get_handle(freerdp* inst);
BOOL android_check_handle(freerdp * inst);

ANDROID_EVENT_KEY* android_event_key_new(int flags, UINT16 scancode);
ANDROID_EVENT_KEY* android_event_unicodekey_new(UINT16 key);
ANDROID_EVENT_CURSOR* android_event_cursor_new(UINT16 flags, UINT16 x, UINT16 y);
ANDROID_EVENT* android_event_disconnect_new(void);
ANDROID_EVENT_CLIPBOARD* android_event_clipboard_new(void* data, int data_length);

void android_event_free(ANDROID_EVENT* event);

BOOL android_event_queue_init(freerdp * inst);
void android_event_queue_uninit(freerdp * inst);

#endif /* FREERDP_ANDROID_EVENT_H */
