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

#include <freerdp/config.h>

#include <freerdp/dvc.h>
#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/channels/log.h>

#include <freerdp/channels/rdpei.h>
#include <freerdp/client/rdpei.h>

#define TAG CHANNELS_TAG("rdpei.client")

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

typedef struct
{
	BOOL dirty;
	BOOL active;
	UINT32 contactId;
	INT32 externalId;
	RDPINPUT_CONTACT_DATA data;
} RDPINPUT_CONTACT_POINT;

typedef struct
{
	BOOL dirty;
	BOOL active;
	INT32 externalId;
	RDPINPUT_PEN_CONTACT data;
} RDPINPUT_PEN_CONTACT_POINT;

#ifdef WITH_DEBUG_DVC
#define DEBUG_DVC(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_DVC(...) \
	do                 \
	{                  \
	} while (0)
#endif

#endif /* FREERDP_CHANNEL_RDPEI_CLIENT_MAIN_H */
