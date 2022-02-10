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
	RDPINPUT_PROTOCOL_V101 = 0x00010001,
	RDPINPUT_PROTOCOL_V200 = 0x00020000,
	RDPINPUT_PROTOCOL_V300 = 0x00030000
};

/* Server feature flags */
#define SC_READY_MULTIPEN_INJECTION_SUPPORTED 0x0001

/* Client Ready Flags */
#define CS_READY_FLAGS_SHOW_TOUCH_VISUALS 0x00000001
#define CS_READY_FLAGS_DISABLE_TIMESTAMP_INJECTION 0x00000002
#define CS_READY_FLAGS_ENABLE_MULTIPEN_INJECTION 0x00000004

/* 2.2.3.3.1.1 RDPINPUT_TOUCH_CONTACT */
#define CONTACT_DATA_CONTACTRECT_PRESENT 0x0001
#define CONTACT_DATA_ORIENTATION_PRESENT 0x0002
#define CONTACT_DATA_PRESSURE_PRESENT 0x0004

typedef enum
{
	RDPINPUT_PEN_CONTACT_PENFLAGS_PRESENT = 0x0001,
	RDPINPUT_PEN_CONTACT_PRESSURE_PRESENT = 0x0002,
	RDPINPUT_PEN_CONTACT_ROTATION_PRESENT = 0x0004,
	RDPINPUT_PEN_CONTACT_TILTX_PRESENT = 0x0008,
	RDPINPUT_PEN_CONTACT_TILTY_PRESENT = 0x0010
} RDPINPUT_PEN_FIELDS_PRESENT;

/*
 * Valid combinations of RDPINPUT_CONTACT_FLAGS:
 *
 * See [MS-RDPEI] 2.2.3.3.1.1 RDPINPUT_TOUCH_CONTACT and 3.1.1.1 Touch Contact State Transitions
 *
 * UP
 * UP | CANCELED
 * UPDATE
 * UPDATE | CANCELED
 * DOWN | INRANGE | INCONTACT
 * UPDATE | INRANGE | INCONTACT
 * UP | INRANGE
 * UPDATE | INRANGE
 */
typedef enum
{
	RDPINPUT_CONTACT_FLAG_DOWN = 0x0001,
	RDPINPUT_CONTACT_FLAG_UPDATE = 0x0002,
	RDPINPUT_CONTACT_FLAG_UP = 0x0004,
	RDPINPUT_CONTACT_FLAG_INRANGE = 0x0008,
	RDPINPUT_CONTACT_FLAG_INCONTACT = 0x0010,
	RDPINPUT_CONTACT_FLAG_CANCELED = 0x0020
} RDPINPUT_CONTACT_FLAGS;

typedef enum
{
	RDPINPUT_PEN_FLAG_BARREL_PRESSED = 0x0001,
	RDPINPUT_PEN_FLAG_ERASER_PRESSED = 0x0002,
	RDPINPUT_PEN_FLAG_INVERTED = 0x0004
} RDPINPUT_PEN_FLAGS;

/** @brief a contact point */
typedef struct
{
	UINT32 contactId;
	UINT16 fieldsPresent; /* Mask of CONTACT_DATA_*_PRESENT values */
	INT32 x;
	INT32 y;
	UINT32 contactFlags;     /* See RDPINPUT_CONTACT_FLAG*  */
	INT16 contactRectLeft;   /* Present if CONTACT_DATA_CONTACTRECT_PRESENT */
	INT16 contactRectTop;    /* Present if CONTACT_DATA_CONTACTRECT_PRESENT */
	INT16 contactRectRight;  /* Present if CONTACT_DATA_CONTACTRECT_PRESENT */
	INT16 contactRectBottom; /* Present if CONTACT_DATA_CONTACTRECT_PRESENT */
	UINT32 orientation; /* Present if CONTACT_DATA_ORIENTATION_PRESENT, values in degree, [0-359] */
	UINT32 pressure;    /* Present if CONTACT_DATA_PRESSURE_PRESENT, normalized value [0-1024] */
} RDPINPUT_CONTACT_DATA;

/** @brief a frame containing contact points */
typedef struct
{
	UINT16 contactCount;
	UINT64 frameOffset;
	RDPINPUT_CONTACT_DATA* contacts;
} RDPINPUT_TOUCH_FRAME;

/** @brief a touch event with some frames*/
typedef struct
{
	UINT32 encodeTime;
	UINT16 frameCount;
	RDPINPUT_TOUCH_FRAME* frames;
} RDPINPUT_TOUCH_EVENT;

typedef struct
{
	UINT8 deviceId;
	UINT16 fieldsPresent; /* Mask of RDPINPUT_PEN_FIELDS_PRESENT values */
	INT32 x;
	INT32 y;
	UINT32 contactFlags; /* See RDPINPUT_CONTACT_FLAG*  */
	UINT32 penFlags;     /* Present if RDPINPUT_PEN_CONTACT_PENFLAGS_PRESENT, values see
	                        RDPINPUT_PEN_FLAGS */
	UINT16 rotation;     /* Present if RDPINPUT_PEN_CONTACT_ROTATION_PRESENT, In degree, [0-359] */
	UINT32
	pressure;    /* Present if RDPINPUT_PEN_CONTACT_PRESSURE_PRESENT, normalized value [0-1024] */
	INT16 tiltX; /* Present if PEN_CONTACT_TILTX_PRESENT, range [-90, 90] */
	INT16 tiltY; /* Present if PEN_CONTACT_TILTY_PRESENT, range [-90, 90] */
} RDPINPUT_PEN_CONTACT;

typedef struct
{
	UINT16 contactCount;
	UINT64 frameOffset;
	RDPINPUT_PEN_CONTACT* contacts;
} RDPINPUT_PEN_FRAME;

/** @brief a touch event with some frames*/
typedef struct
{
	UINT32 encodeTime;
	UINT16 frameCount;
	RDPINPUT_PEN_FRAME* frames;
} RDPINPUT_PEN_EVENT;

#endif /* FREERDP_CHANNEL_RDPEI_H */
