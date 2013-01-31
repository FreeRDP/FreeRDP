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

#ifndef FREERDP_CORE_EVENT_PRIVATE_H
#define FREERDP_CORE_EVENT_PRIVATE_H

#include "message.h"

#include <freerdp/freerdp.h>

/* Input */

#define Input_Class					1

#define Input_SynchronizeEvent				1
#define Input_KeyboardEvent				2
#define Input_UnicodeKeyboardEvent			3
#define Input_MouseEvent				4
#define Input_ExtendedMouseEvent			5

/* Event Queue */

struct rdp_event
{
	/* Input */

	pSynchronizeEvent SynchronizeEvent;
	pKeyboardEvent KeyboardEvent;
	pUnicodeKeyboardEvent UnicodeKeyboardEvent;
	pMouseEvent MouseEvent;
	pExtendedMouseEvent ExtendedMouseEvent;

	/* Internal */

	rdpInput* input;
};

int event_process_pending_input(rdpInput* input);

rdpEvent* event_new(rdpInput* input);
void event_free(rdpEvent* event);

#endif /* FREERDP_CORE_EVENT_PRIVATE_H */

