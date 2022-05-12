/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Audio Input Redirection Virtual Channel
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#ifndef FREERDP_CHANNEL_AINPUT_H
#define FREERDP_CHANNEL_AINPUT_H

#include <freerdp/api.h>
#include <freerdp/dvc.h>
#include <freerdp/types.h>

#define AINPUT_CHANNEL_NAME "ainput"
#define AINPUT_DVC_CHANNEL_NAME "FreeRDP::Advanced::Input"

typedef enum
{
	MSG_AINPUT_VERSION = 0x01,
	MSG_AINPUT_MOUSE = 0x02
} eAInputMsgType;

typedef enum
{
	AINPUT_FLAGS_WHEEL = 0x0001,
	AINPUT_FLAGS_MOVE = 0x0004,
	AINPUT_FLAGS_DOWN = 0x0008,

	AINPUT_FLAGS_REL = 0x0010,
	AINPUT_FLAGS_HAVE_REL = 0x0020,

	/* Pointer Flags */
	AINPUT_FLAGS_BUTTON1 = 0x1000, /* left */
	AINPUT_FLAGS_BUTTON2 = 0x2000, /* right */
	AINPUT_FLAGS_BUTTON3 = 0x4000, /* middle */

	/* Extended Pointer Flags */
	AINPUT_XFLAGS_BUTTON1 = 0x0100,
	AINPUT_XFLAGS_BUTTON2 = 0x0200
} AInputEventFlags;

typedef struct ainput_client_context AInputClientContext;

#define AINPUT_VERSION_MAJOR 1
#define AINPUT_VERSION_MINOR 0

#endif /* FREERDP_CHANNEL_AINPUT_H */
