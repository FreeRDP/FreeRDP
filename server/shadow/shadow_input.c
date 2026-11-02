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

/* A key in rdpShadowClient::keysDown: the scan code with its extended flags */
#define KEY_INDEX_EXTENDED 0x100
#define KEY_INDEX_EXTENDED1 0x200
#define KEY_INDEX_COUNT 0x400

static size_t shadow_input_key_index(UINT16 flags, UINT8 code)
{
	size_t index = code;
	if (flags & KBD_FLAGS_EXTENDED)
		index |= KEY_INDEX_EXTENDED;
	if (flags & KBD_FLAGS_EXTENDED1)
		index |= KEY_INDEX_EXTENDED1;
	return index;
}

static BOOL shadow_input_key_is_down(const rdpShadowClient* client, size_t index)
{
	WINPR_ASSERT(client);
	WINPR_ASSERT(index < KEY_INDEX_COUNT);
	return (client->keysDown[index / 8] & (1u << (index % 8))) != 0;
}

/* Marks the key as pressed or released by this client and returns whether any client sharing
 * the desktop still holds it.
 *
 * rdpShadowServer::keysHeld counts the clients holding each key, so that this needs no walk over
 * server->clients: a capture thread may hold that list's lock while it waits for this client to
 * take a frame, and this runs on the client thread. Guarded by server->lock, which nothing else
 * takes. */
static BOOL shadow_input_key_update(rdpShadowClient* client, size_t index, BOOL down)
{
	WINPR_ASSERT(client);
	WINPR_ASSERT(index < KEY_INDEX_COUNT);
	rdpShadowServer* server = client->server;
	WINPR_ASSERT(server);

	const BYTE bit = (BYTE)(1u << (index % 8));

	EnterCriticalSection(&server->lock);
	if (down && !shadow_input_key_is_down(client, index))
	{
		client->keysDown[index / 8] |= bit;
		server->keysHeld[index]++;
	}
	else if (!down && shadow_input_key_is_down(client, index))
	{
		client->keysDown[index / 8] &= (BYTE)~bit;
		WINPR_ASSERT(server->keysHeld[index] > 0);
		server->keysHeld[index]--;
	}
	const BOOL held = server->keysHeld[index] > 0;
	LeaveCriticalSection(&server->lock);
	return held;
}

void shadow_input_release_keys(rdpShadowClient* client)
{
	WINPR_ASSERT(client);
	WINPR_ASSERT(client->server);
	rdpShadowSubsystem* subsystem = client->server->subsystem;
	WINPR_ASSERT(subsystem);

	for (size_t index = 0; index < KEY_INDEX_COUNT; index++)
	{
		if (!shadow_input_key_is_down(client, index))
			continue;
		/* another client sharing the desktop still holds it: it stays down for that one */
		if (shadow_input_key_update(client, index, FALSE))
			continue;

		UINT16 flags = KBD_FLAGS_RELEASE;
		if (index & KEY_INDEX_EXTENDED)
			flags |= KBD_FLAGS_EXTENDED;
		if (index & KEY_INDEX_EXTENDED1)
			flags |= KBD_FLAGS_EXTENDED1;
		const UINT8 code = (UINT8)(index & 0xFF);

		WLog_DBG(TAG, "releasing key 0x%02" PRIx8 " flags=0x%04" PRIx16 " the client left down",
		         code, flags);
		if (!IFCALLRESULT(TRUE, subsystem->KeyboardEvent, subsystem, client, flags, code))
			WLog_WARN(TAG, "releasing key 0x%02" PRIx8 " flags=0x%04" PRIx16 " failed", code,
			          flags);
	}
}

WINPR_ATTR_NODISCARD
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

	/* the client starts its keyboard state over (it sends this when it gets the focus back):
	 * what it held before is up now */
	shadow_input_release_keys(client);
	return IFCALLRESULT(TRUE, subsystem->SynchronizeEvent, subsystem, client, flags);
}

WINPR_ATTR_NODISCARD
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

	const BOOL rc = IFCALLRESULT(TRUE, subsystem->KeyboardEvent, subsystem, client, flags, code);
	if (rc)
		(void)shadow_input_key_update(client, shadow_input_key_index(flags, code),
		                              (flags & KBD_FLAGS_RELEASE) == 0);
	return rc;
}

WINPR_ATTR_NODISCARD
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

WINPR_ATTR_NODISCARD
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

WINPR_ATTR_NODISCARD
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

WINPR_ATTR_NODISCARD
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
