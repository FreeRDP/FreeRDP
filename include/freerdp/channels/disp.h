/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPEDISP Virtual Channel Extension
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_DISP_H
#define FREERDP_CHANNEL_DISP_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#define DISPLAY_CONTROL_PDU_TYPE_CAPS 0x00000005
#define DISPLAY_CONTROL_PDU_TYPE_MONITOR_LAYOUT 0x00000002
#define DISPLAY_CONTROL_MONITOR_LAYOUT_SIZE 40

#define DISP_DVC_CHANNEL_NAME "Microsoft::Windows::RDS::DisplayControl"
#define ORIENTATION_LANDSCAPE 0
#define ORIENTATION_PORTRAIT 90
#define ORIENTATION_LANDSCAPE_FLIPPED 180
#define ORIENTATION_PORTRAIT_FLIPPED 270

#define DISPLAY_CONTROL_MONITOR_PRIMARY 0x00000001
#define DISPLAY_CONTROL_HEADER_LENGTH 0x00000008

#define DISPLAY_CONTROL_MIN_MONITOR_WIDTH 200
#define DISPLAY_CONTROL_MAX_MONITOR_WIDTH 8192

#define DISPLAY_CONTROL_MIN_MONITOR_HEIGHT 200
#define DISPLAY_CONTROL_MAX_MONITOR_HEIGHT 8192

#define DISPLAY_CONTROL_MIN_PHYSICAL_MONITOR_WIDTH 10
#define DISPLAY_CONTROL_MAX_PHYSICAL_MONITOR_WIDTH 10000

#define DISPLAY_CONTROL_MIN_PHYSICAL_MONITOR_HEIGHT 10
#define DISPLAY_CONTROL_MAX_PHYSICAL_MONITOR_HEIGHT 10000

struct _DISPLAY_CONTROL_HEADER
{
	UINT32 type;
	UINT32 length;
};
typedef struct _DISPLAY_CONTROL_HEADER DISPLAY_CONTROL_HEADER;

struct _DISPLAY_CONTROL_MONITOR_LAYOUT
{
	UINT32 Flags;
	INT32 Left;
	INT32 Top;
	UINT32 Width;
	UINT32 Height;
	UINT32 PhysicalWidth;
	UINT32 PhysicalHeight;
	UINT32 Orientation;
	UINT32 DesktopScaleFactor;
	UINT32 DeviceScaleFactor;
};
typedef struct _DISPLAY_CONTROL_MONITOR_LAYOUT DISPLAY_CONTROL_MONITOR_LAYOUT;

struct _DISPLAY_CONTROL_MONITOR_LAYOUT_PDU
{
	UINT32 MonitorLayoutSize;
	UINT32 NumMonitors;
	DISPLAY_CONTROL_MONITOR_LAYOUT* Monitors;
};

typedef struct _DISPLAY_CONTROL_MONITOR_LAYOUT_PDU DISPLAY_CONTROL_MONITOR_LAYOUT_PDU;

#endif /* FREERDP_CHANNEL_DISP_H */
