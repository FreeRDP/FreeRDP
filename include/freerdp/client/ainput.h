/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Display Update Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_CHANNEL_AINPUT_CLIENT_AINPUT_H
#define FREERDP_CHANNEL_AINPUT_CLIENT_AINPUT_H

#include <winpr/assert.h>
#include <freerdp/channels/disp.h>

typedef enum
{
	AINPUT_FLAGS_WHEEL = 0x0001,
	AINPUT_FLAGS_HWHEEL = 0x0002,
	AINPUT_FLAGS_MOVE = 0x0004,
	AINPUT_FLAGS_DOWN = 0x0008,

	AINPUT_FLAGS_REL = 0x0010,

	/* Pointer Flags */
	AINPUT_FLAGS_BUTTON1 = 0x1000, /* left */
	AINPUT_FLAGS_BUTTON2 = 0x2000, /* right */
	AINPUT_FLAGS_BUTTON3 = 0x4000, /* middle */

	/* Extended Pointer Flags */
	AINPUT_XFLAGS_BUTTON1 = 0x0100,
	AINPUT_XFLAGS_BUTTON2 = 0x0200
} AInputEventFlags;

typedef struct ainput_client_context AInputClientContext;

typedef UINT (*pcAInputSendInputEvent)(AInputClientContext* context, UINT64 flags, INT32 x,
                                       INT32 y);

struct ainput_client_context
{
	void* handle;
	void* custom;

	pcAInputSendInputEvent AInputSendInputEvent;
};

#endif /* FREERDP_CHANNEL_AINPUT_CLIENT_AINPUT_H */
