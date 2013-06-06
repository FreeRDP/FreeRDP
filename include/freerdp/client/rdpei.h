/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Dynamic Virtual Channel Extension
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

#ifndef FREERDP_CHANNEL_CLIENT_RDPEI_H
#define FREERDP_CHANNEL_CLIENT_RDPEI_H

#define CONTACT_DATA_CONTACTRECT_PRESENT	0x0001
#define CONTACT_DATA_ORIENTATION_PRESENT	0x0002
#define CONTACT_DATA_PRESSURE_PRESENT		0x0004

#define CONTACT_FLAG_DOWN			0x0001
#define CONTACT_FLAG_UPDATE			0x0002
#define CONTACT_FLAG_UP				0x0004
#define CONTACT_FLAG_INRANGE			0x0008
#define CONTACT_FLAG_INCONTACT			0x0010
#define CONTACT_FLAG_CANCELED			0x0020

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

struct _RDPINPUT_TOUCH_FRAME
{
	UINT32 contactCount;
	UINT64 frameOffset;
	RDPINPUT_CONTACT_DATA* contacts;
};
typedef struct _RDPINPUT_TOUCH_FRAME RDPINPUT_TOUCH_FRAME;

/**
 * Client Interface
 */

#define RDPEI_DVC_CHANNEL_NAME	"Microsoft::Windows::RDS::Input"

typedef struct _rdpei_client_context RdpeiClientContext;

typedef int (*pcRdpeiGetVersion)(RdpeiClientContext* context);
typedef int (*pcRdpeiAddContact)(RdpeiClientContext* context, RDPINPUT_CONTACT_DATA* contact);

typedef int (*pcRdpeiTouchBegin)(RdpeiClientContext* context, int externalId, int x, int y);
typedef int (*pcRdpeiTouchUpdate)(RdpeiClientContext* context, int externalId, int x, int y);
typedef int (*pcRdpeiTouchEnd)(RdpeiClientContext* context, int externalId, int x, int y);

struct _rdpei_client_context
{
	void* handle;
	void* custom;

	pcRdpeiGetVersion GetVersion;

	pcRdpeiAddContact AddContact;

	pcRdpeiTouchBegin TouchBegin;
	pcRdpeiTouchUpdate TouchUpdate;
	pcRdpeiTouchEnd TouchEnd;
};

#endif /* FREERDP_CHANNEL_CLIENT_RDPEI_H */
