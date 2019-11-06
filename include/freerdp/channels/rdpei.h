/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Extended Input channel common definitions
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2014 Thincast Technologies Gmbh.
 * Copyright 2014 David FORT <contact@hardening-consulting.com>
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

#ifndef FREERDP_CHANNEL_RDPEI_H
#define FREERDP_CHANNEL_RDPEI_H

#include <winpr/wtypes.h>

#define RDPINPUT_HEADER_LENGTH 6

#define RDPEI_DVC_CHANNEL_NAME "Microsoft::Windows::RDS::Input"

/** @brief protocol version */
enum
{
	RDPINPUT_PROTOCOL_V10 = 0x00010000,
	RDPINPUT_PROTOCOL_V101 = 0x00010001
};

/* Client Ready Flags */
#define READY_FLAGS_SHOW_TOUCH_VISUALS 0x00000001
#define READY_FLAGS_DISABLE_TIMESTAMP_INJECTION 0x00000002

#define CONTACT_DATA_CONTACTRECT_PRESENT 0x0001
#define CONTACT_DATA_ORIENTATION_PRESENT 0x0002
#define CONTACT_DATA_PRESSURE_PRESENT 0x0004

#define CONTACT_FLAG_DOWN 0x0001
#define CONTACT_FLAG_UPDATE 0x0002
#define CONTACT_FLAG_UP 0x0004
#define CONTACT_FLAG_INRANGE 0x0008
#define CONTACT_FLAG_INCONTACT 0x0010
#define CONTACT_FLAG_CANCELED 0x0020

/** @brief a contact point */
struct _RDPINPUT_CONTACT_DATA
{
	UINT32 contactId;
	UINT32 fieldsPresent;
	INT32 x;
	INT32 y;
	UINT32 contactFlags;
	INT32 contactRectLeft;
	INT32 contactRectTop;
	INT32 contactRectRight;
	INT32 contactRectBottom;
	UINT32 orientation;
	UINT32 pressure;
};
typedef struct _RDPINPUT_CONTACT_DATA RDPINPUT_CONTACT_DATA;

/** @brief a frame containing contact points */
struct _RDPINPUT_TOUCH_FRAME
{
	UINT32 contactCount;
	UINT64 frameOffset;
	RDPINPUT_CONTACT_DATA* contacts;
};
typedef struct _RDPINPUT_TOUCH_FRAME RDPINPUT_TOUCH_FRAME;

/** @brief a touch event with some frames*/
struct _RDPINPUT_TOUCH_EVENT
{
	UINT32 encodeTime;
	UINT16 frameCount;
	RDPINPUT_TOUCH_FRAME* frames;
};
typedef struct _RDPINPUT_TOUCH_EVENT RDPINPUT_TOUCH_EVENT;

#endif /* FREERDP_CHANNEL_RDPEI_H */
