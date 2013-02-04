/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Asynchronous Event Queue
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

#include "event.h"

#include <winpr/crt.h>
#include <winpr/collections.h>

/* Input */

static void event_SynchronizeEvent(rdpInput* input, UINT32 flags)
{
	MessageQueue_Post(input->queue, (void*) input,
			MakeMessageId(Input, SynchronizeEvent), (void*) (size_t) flags, NULL);
}

static void event_KeyboardEvent(rdpInput* input, UINT16 flags, UINT16 code)
{
	MessageQueue_Post(input->queue, (void*) input,
			MakeMessageId(Input, KeyboardEvent), (void*) (size_t) flags, (void*) (size_t) code);
}

static void event_UnicodeKeyboardEvent(rdpInput* input, UINT16 flags, UINT16 code)
{
	MessageQueue_Post(input->queue, (void*) input,
			MakeMessageId(Input, UnicodeKeyboardEvent), (void*) (size_t) flags, (void*) (size_t) code);
}

static void event_MouseEvent(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	UINT32 pos = (x << 16) | y;

	MessageQueue_Post(input->queue, (void*) input,
			MakeMessageId(Input, MouseEvent), (void*) (size_t) flags, (void*) (size_t) pos);
}

static void event_ExtendedMouseEvent(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	UINT32 pos = (x << 16) | y;

	MessageQueue_Post(input->queue, (void*) input,
			MakeMessageId(Input, ExtendedMouseEvent), (void*) (size_t) flags, (void*) (size_t) pos);
}

/* Event Queue */

int event_process_input_class(rdpEvent* event, wMessage* msg, int type)
{
	int status = 0;

	switch (type)
	{
		case Input_SynchronizeEvent:
			IFCALL(event->SynchronizeEvent, msg->context, (UINT32) (size_t) msg->wParam);
			break;

		case Input_KeyboardEvent:
			IFCALL(event->KeyboardEvent, msg->context, (UINT16) (size_t) msg->wParam, (UINT16) (size_t) msg->lParam);
			break;

		case Input_UnicodeKeyboardEvent:
			IFCALL(event->UnicodeKeyboardEvent, msg->context, (UINT16) (size_t) msg->wParam, (UINT16) (size_t) msg->lParam);
			break;

		case Input_MouseEvent:
			{
				UINT32 pos;
				UINT16 x, y;

				pos = (UINT32) (size_t) msg->lParam;
				x = ((pos & 0xFFFF0000) >> 16);
				y = (pos & 0x0000FFFF);

				IFCALL(event->MouseEvent, msg->context, (UINT16) (size_t) msg->wParam, x, y);
			}
			break;

		case Input_ExtendedMouseEvent:
			{
				UINT32 pos;
				UINT16 x, y;

				pos = (UINT32) (size_t) msg->lParam;
				x = ((pos & 0xFFFF0000) >> 16);
				y = (pos & 0x0000FFFF);

				IFCALL(event->ExtendedMouseEvent, msg->context, (UINT16) (size_t) msg->wParam, x, y);
			}
			break;

		default:
			status = -1;
			break;
	}

	return status;
}


int event_process_class(rdpEvent* event, wMessage* msg, int msgClass, int msgType)
{
	int status = 0;

	switch (msgClass)
	{
		case Input_Class:
			status = event_process_input_class(event, msg, msgType);
			break;

		default:
			status = -1;
			break;
	}

	if (status < 0)
		printf("Unknown event: class: %d type: %d\n", msgClass, msgType);

	return status;
}

int event_process_pending_input(rdpInput* input)
{
	int status;
	int msgClass;
	int msgType;
	wMessage message;
	wMessageQueue* queue;

	queue = input->queue;

	while (1)
	{
		status = MessageQueue_Peek(queue, &message, TRUE);

		if (!status)
			break;

		if (message.type == WMQ_QUIT)
			break;

		msgClass = GetMessageClass(message.type);
		msgType = GetMessageType(message.type);

		status = event_process_class(input->event, &message, msgClass, msgType);
	}

	return 0;
}

void event_register_input(rdpEvent* event, rdpInput* input)
{
	/* Input */

	event->SynchronizeEvent = input->SynchronizeEvent;
	event->KeyboardEvent = input->KeyboardEvent;
	event->UnicodeKeyboardEvent = input->UnicodeKeyboardEvent;
	event->MouseEvent = input->MouseEvent;
	event->ExtendedMouseEvent = input->ExtendedMouseEvent;

	input->SynchronizeEvent = event_SynchronizeEvent;
	input->KeyboardEvent = event_KeyboardEvent;
	input->UnicodeKeyboardEvent = event_UnicodeKeyboardEvent;
	input->MouseEvent = event_MouseEvent;
	input->ExtendedMouseEvent = event_ExtendedMouseEvent;
}

rdpEvent* event_new(rdpInput* input)
{
	rdpEvent* event;

	event = (rdpEvent*) malloc(sizeof(rdpEvent));

	if (event)
	{
		ZeroMemory(event, sizeof(rdpEvent));

		event->input = input;
		input->queue = MessageQueue_New();
		event_register_input(event, input);
	}

	return event;
}

void event_free(rdpEvent* event)
{
	if (event)
	{
		MessageQueue_Free(event->input->queue);
		free(event);
	}
}
