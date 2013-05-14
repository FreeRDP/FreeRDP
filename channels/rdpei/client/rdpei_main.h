/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Input Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_RDPEI_CLIENT_MAIN_H
#define FREERDP_CHANNEL_RDPEI_CLIENT_MAIN_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <freerdp/dvc.h>
#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/utils/debug.h>

#include <freerdp/client/rdpei.h>

#define RDPINPUT_HEADER_LENGTH			6

/* Protocol Version */

#define RDPINPUT_PROTOCOL_V1			0x00010000

/* Client Ready Flags */

#define READY_FLAGS_SHOW_TOUCH_VISUALS		0x00000001
#define READY_FLAGS_DISABLE_TIMESTAMP_INJECTION	0x00000002

/* Input Event Ids */

#define EVENTID_SC_READY			0x0001
#define EVENTID_CS_READY			0x0002
#define EVENTID_TOUCH				0x0003
#define EVENTID_SUSPEND_TOUCH			0x0004
#define EVENTID_RESUME_TOUCH			0x0005
#define EVENTID_DISMISS_HOVERING_CONTACT	0x0006

#define MAX_EXTERNAL_IDS	32

struct _RDPINPUT_CONTACT_POINT
{
	UINT32 flags;
	UINT32 contactId;
	int touchDownX;
	int touchDownY;
	int externalIdCount;
	int externalIds[MAX_EXTERNAL_IDS];
};
typedef struct _RDPINPUT_CONTACT_POINT RDPINPUT_CONTACT_POINT;

#ifdef WITH_DEBUG_DVC
#define DEBUG_DVC(fmt, ...) DEBUG_CLASS(DVC, fmt, ## __VA_ARGS__)
#else
#define DEBUG_DVC(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* FREERDP_CHANNEL_RDPEI_CLIENT_MAIN_H */

