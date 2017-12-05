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

#ifndef FREERDP_CHANNEL_DISP_CLIENT_DISP_H
#define FREERDP_CHANNEL_DISP_CLIENT_DISP_H

#define ORIENTATION_LANDSCAPE				0
#define ORIENTATION_PORTRAIT				90
#define ORIENTATION_LANDSCAPE_FLIPPED			180
#define ORIENTATION_PORTRAIT_FLIPPED			270

#define DISPLAY_CONTROL_MONITOR_PRIMARY			0x00000001

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

/**
 * Client Interface
 */

#define DISP_DVC_CHANNEL_NAME	"Microsoft::Windows::RDS::DisplayControl"

typedef struct _disp_client_context DispClientContext;

typedef UINT (*pcDispCaps)(DispClientContext* context, UINT32 MaxNumMonitors, UINT32 MaxMonitorAreaFactorA,
							UINT32 MaxMonitorAreaFactorB);
typedef UINT (*pcDispSendMonitorLayout)(DispClientContext* context, UINT32 NumMonitors, DISPLAY_CONTROL_MONITOR_LAYOUT* Monitors);

struct _disp_client_context
{
	void* handle;
	void* custom;

	pcDispCaps DisplayControlCaps;
	pcDispSendMonitorLayout SendMonitorLayout;
};

#endif /* FREERDP_CHANNEL_DISP_CLIENT_DISP_H */

