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

#include <winpr/assert.h>
#include <freerdp/config.h>

#include <freerdp/log.h>
#include "shadow.h"

#define TAG SERVER_TAG("shadow.input")

static BOOL shadow_input_synchronize_event(rdpInput* input, UINT32 flags)
{
	WINPR_ASSERT(input);
	rdpShadowClient* client = (rdpShadowClient*)input->context;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);
	rdpShadowSubsystem* subsystem = client->server->subsystem;
	WINPR_ASSERT(subsystem);

	WLog_DBG(TAG, "[%s] flags=0x%04" PRIx16, client->mayInteract ? "use" : "discard", flags);
	if (!client->mayInteract)
		return TRUE;

	return IFCALLRESULT(TRUE, subsystem->SynchronizeEvent, subsystem, client, flags);
}

static BOOL shadow_input_keyboard_event(rdpInput* input, UINT16 flags, UINT8 code)
{
	WINPR_ASSERT(input);
	rdpShadowClient* client = (rdpShadowClient*)input->context;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);
	rdpShadowSubsystem* subsystem = client->server->subsystem;
	WINPR_ASSERT(subsystem);

	WLog_DBG(TAG, "[%s] flags=0x%04" PRIx16, client->mayInteract ? "use" : "discard", flags);
	if (!client->mayInteract)
		return TRUE;

	return IFCALLRESULT(TRUE, subsystem->KeyboardEvent, subsystem, client, flags, code);
}

static BOOL shadow_input_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	WINPR_ASSERT(input);
	rdpShadowClient* client = (rdpShadowClient*)input->context;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);
	rdpShadowSubsystem* subsystem = client->server->subsystem;
	WINPR_ASSERT(subsystem);

	WLog_DBG(TAG, "[%s] flags=0x%04" PRIx16, client->mayInteract ? "use" : "discard", flags);
	if (!client->mayInteract)
		return TRUE;

	return IFCALLRESULT(TRUE, subsystem->UnicodeKeyboardEvent, subsystem, client, flags, code);
}

static BOOL shadow_input_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	WINPR_ASSERT(input);
	rdpShadowClient* client = (rdpShadowClient*)input->context;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);
	rdpShadowSubsystem* subsystem = client->server->subsystem;
	WINPR_ASSERT(subsystem);

	if (client->server->shareSubRect)
	{
		x += client->server->subRect.left;
		y += client->server->subRect.top;
	}

	if ((flags & (PTR_FLAGS_WHEEL | PTR_FLAGS_HWHEEL | PTR_FLAGS_WHEEL_NEGATIVE)) == 0)
	{
		client->pointerX = x;
		client->pointerY = y;

		if ((client->pointerX == subsystem->pointerX) && (client->pointerY == subsystem->pointerY))
		{
			flags &= ~PTR_FLAGS_MOVE;

			if (!(flags & (PTR_FLAGS_BUTTON1 | PTR_FLAGS_BUTTON2 | PTR_FLAGS_BUTTON3)))
				return TRUE;
		}
	}

	WLog_DBG(TAG, "[%s] flags=0x%04" PRIx16 ", x=%" PRIu16 ", y=%" PRIu16,
	         client->mayInteract ? "use" : "discard", flags, x, y);
	if (!client->mayInteract)
		return TRUE;

	return IFCALLRESULT(TRUE, subsystem->MouseEvent, subsystem, client, flags, x, y);
}

static BOOL shadow_input_rel_mouse_event(rdpInput* input, UINT16 flags, INT16 xDelta, INT16 yDelta)
{
	WINPR_ASSERT(input);

	rdpShadowClient* client = (rdpShadowClient*)input->context;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);

	rdpShadowSubsystem* subsystem = client->server->subsystem;
	WINPR_ASSERT(subsystem);

	WLog_DBG(TAG, "[%s] flags=0x%04" PRIx16 ", x=%" PRId16 ", y=%" PRId16,
	         client->mayInteract ? "use" : "discard", flags, xDelta, yDelta);
	const uint16_t mask = PTR_FLAGS_MOVE | PTR_FLAGS_DOWN | PTR_FLAGS_BUTTON1 | PTR_FLAGS_BUTTON2 |
	                      PTR_FLAGS_BUTTON3 | PTR_XFLAGS_BUTTON1 | PTR_XFLAGS_BUTTON2;
	if ((flags & mask) != 0)
	{
		WLog_WARN(TAG, "Unknown flags 0x%04x", WINPR_CXX_COMPAT_CAST(unsigned, flags & ~mask));
	}
	if (!client->mayInteract)
		return TRUE;

	return IFCALLRESULT(TRUE, subsystem->RelMouseEvent, subsystem, client, flags, xDelta, yDelta);
}

static BOOL shadow_input_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	WINPR_ASSERT(input);
	rdpShadowClient* client = (rdpShadowClient*)input->context;
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);
	rdpShadowSubsystem* subsystem = client->server->subsystem;
	WINPR_ASSERT(subsystem);

	if (client->server->shareSubRect)
	{
		x += client->server->subRect.left;
		y += client->server->subRect.top;
	}

	client->pointerX = x;
	client->pointerY = y;

	WLog_DBG(TAG, "[%s] flags=0x%04" PRIx16 ", x=%" PRIu16 ", y=%" PRIu16,
	         client->mayInteract ? "use" : "discard", flags, x, y);
	if (!client->mayInteract)
		return TRUE;

	return IFCALLRESULT(TRUE, subsystem->ExtendedMouseEvent, subsystem, client, flags, x, y);
}

void shadow_input_register_callbacks(rdpInput* input)
{
	WINPR_ASSERT(input);
	input->SynchronizeEvent = shadow_input_synchronize_event;
	input->KeyboardEvent = shadow_input_keyboard_event;
	input->UnicodeKeyboardEvent = shadow_input_unicode_keyboard_event;
	input->MouseEvent = shadow_input_mouse_event;
	input->ExtendedMouseEvent = shadow_input_extended_mouse_event;
	input->RelMouseEvent = shadow_input_rel_mouse_event;
}
