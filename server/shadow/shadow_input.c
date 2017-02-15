/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "shadow.h"

BOOL shadow_input_synchronize_event(rdpInput* input, UINT32 flags)
{
	rdpShadowClient* client = (rdpShadowClient*) input->context;
	rdpShadowSubsystem* subsystem = client->server->subsystem;

	if (!client->mayInteract)
		return TRUE;

	if (subsystem->SynchronizeEvent)
	{
		subsystem->SynchronizeEvent(subsystem, client, flags);
	}
	return TRUE;
}

BOOL shadow_input_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	rdpShadowClient* client = (rdpShadowClient*) input->context;
	rdpShadowSubsystem* subsystem = client->server->subsystem;

	if (!client->mayInteract)
		return TRUE;
	
	if (subsystem->KeyboardEvent)
	{
		subsystem->KeyboardEvent(subsystem, client, flags, code);
	}
	return TRUE;
}

BOOL shadow_input_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	rdpShadowClient* client = (rdpShadowClient*) input->context;
	rdpShadowSubsystem* subsystem = client->server->subsystem;

	if (!client->mayInteract)
		return TRUE;

	if (subsystem->UnicodeKeyboardEvent)
	{
		subsystem->UnicodeKeyboardEvent(subsystem, client, flags, code);
	}
	return TRUE;
}

BOOL shadow_input_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	rdpShadowClient* client = (rdpShadowClient*) input->context;
	rdpShadowSubsystem* subsystem = client->server->subsystem;

	if (client->server->shareSubRect)
	{
		x += client->server->subRect.left;
		y += client->server->subRect.top;
	}

	if (!(flags & PTR_FLAGS_WHEEL))
	{
		client->pointerX = x;
		client->pointerY = y;

		if ((client->pointerX == subsystem->pointerX) &&
				(client->pointerY == subsystem->pointerY))
		{
			flags &= ~PTR_FLAGS_MOVE;

			if (!(flags & (PTR_FLAGS_BUTTON1 | PTR_FLAGS_BUTTON2 | PTR_FLAGS_BUTTON3)))
				return TRUE;
		}
	}

	if (!client->mayInteract)
		return TRUE;

	if (subsystem->MouseEvent)
	{
		subsystem->MouseEvent(subsystem, client, flags, x, y);
	}
	return TRUE;
}

BOOL shadow_input_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	rdpShadowClient* client = (rdpShadowClient*) input->context;
	rdpShadowSubsystem* subsystem = client->server->subsystem;

	if (client->server->shareSubRect)
	{
		x += client->server->subRect.left;
		y += client->server->subRect.top;
	}

	client->pointerX = x;
	client->pointerY = y;

	if (!client->mayInteract)
		return TRUE;

	if (subsystem->ExtendedMouseEvent)
	{
		subsystem->ExtendedMouseEvent(subsystem, client, flags, x, y);
	}
	return TRUE;
}

void shadow_input_register_callbacks(rdpInput* input)
{
	input->SynchronizeEvent = shadow_input_synchronize_event;
	input->KeyboardEvent = shadow_input_keyboard_event;
	input->UnicodeKeyboardEvent = shadow_input_unicode_keyboard_event;
	input->MouseEvent = shadow_input_mouse_event;
	input->ExtendedMouseEvent = shadow_input_extended_mouse_event;
}
