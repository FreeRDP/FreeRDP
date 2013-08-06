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

#define RDPINPUT_HEADER_LENGTH				6

/* Protocol Version */

#define RDPINPUT_PROTOCOL_V10				0x00010000
#define RDPINPUT_PROTOCOL_V101				0x00010001

/* Client Ready Flags */

#define READY_FLAGS_SHOW_TOUCH_VISUALS			0x00000001
#define READY_FLAGS_DISABLE_TIMESTAMP_INJECTION		0x00000002

/* Input Event Ids */

#define EVENTID_SC_READY				0x0001
#define EVENTID_CS_READY				0x0002
#define EVENTID_TOUCH					0x0003
#define EVENTID_SUSPEND_TOUCH				0x0004
#define EVENTID_RESUME_TOUCH				0x0005
#define EVENTID_DISMISS_HOVERING_CONTACT		0x0006

#define RDPINPUT_CONTACT_STATE_INITIAL			0x0000
#define RDPINPUT_CONTACT_STATE_ENGAGED			0x0001
#define RDPINPUT_CONTACT_STATE_HOVERING			0x0002
#define RDPINPUT_CONTACT_STATE_OUT_OF_RANGE		0x0003

/**
 * Touch Contact State Transitions
 *
 * ENGAGED -> UPDATE | INRANGE | INCONTACT -> ENGAGED
 * ENGAGED -> UP | INRANGE -> HOVERING
 * ENGAGED -> UP -> OUT_OF_RANGE
 * ENGAGED -> UP | CANCELED -> OUT_OF_RANGE
 *
 * HOVERING -> UPDATE | INRANGE -> HOVERING
 * HOVERING -> DOWN | INRANGE | INCONTACT -> ENGAGED
 * HOVERING -> UPDATE -> OUT_OF_RANGE
 * HOVERING -> UPDATE | CANCELED -> OUT_OF_RANGE
 *
 * OUT_OF_RANGE -> DOWN | INRANGE | INCONTACT -> ENGAGED
 * OUT_OF_RANGE -> UPDATE | INRANGE -> HOVERING
 *
 * When a contact is in the "hovering" or "engaged" state, it is referred to as being "active".
 * "Hovering" contacts are in range of the digitizer, while "engaged" contacts are in range of
 * the digitizer and in contact with the digitizer surface. MS-RDPEI remotes only active contacts
 * and contacts that are transitioning to the "out of range" state; see section 2.2.3.3.1.1 for
 * an enumeration of valid state flags combinations.
 *
 * When transitioning from the "engaged" state to the "hovering" state, or from the "engaged"
 * state to the "out of range" state, the contact position cannot change; it is only allowed
 * to change after the transition has taken place.
 *
 */

struct _RDPINPUT_CONTACT_POINT
{
	int lastX;
	int lastY;
	BOOL dirty;
	BOOL active;
	UINT32 state;
	UINT32 flags;
	UINT32 contactId;
	int externalId;
	RDPINPUT_CONTACT_DATA data;
};
typedef struct _RDPINPUT_CONTACT_POINT RDPINPUT_CONTACT_POINT;

#ifdef WITH_DEBUG_DVC
#define DEBUG_DVC(fmt, ...) DEBUG_CLASS(DVC, fmt, ## __VA_ARGS__)
#else
#define DEBUG_DVC(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* FREERDP_CHANNEL_RDPEI_CLIENT_MAIN_H */

