/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Android Display Update Virtual Channel
 *
 * Copyright 2026 Ibrahim Sevinc <ibrahim.sevinc.mail@gmail.com>
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

#include <freerdp/config.h>

#include <winpr/assert.h>
#include <winpr/wlog.h>

#include <freerdp/channels/disp.h>
#include <freerdp/client/disp.h>

#include "android_disp.h"

#define TAG CLIENT_TAG("android.disp")

BOOL android_disp_init(androidContext* afc, DispClientContext* disp)
{
	WINPR_ASSERT(afc);
	WINPR_ASSERT(disp);

	afc->disp = disp;
	disp->custom = afc;

	WLog_DBG(TAG, "disp channel connected");
	return TRUE;
}

BOOL android_disp_uninit(androidContext* afc, DispClientContext* disp)
{
	WINPR_ASSERT(afc);
	WINPR_UNUSED(disp);

	afc->disp = NULL;
	WLog_DBG(TAG, "disp channel disconnected");
	return TRUE;
}

BOOL android_disp_send_monitor_layout(androidContext* afc, UINT32 width, UINT32 height)
{
	WINPR_ASSERT(afc);

	DispClientContext* disp = afc->disp;
	if (!disp || !disp->SendMonitorLayout)
	{
		WLog_WARN(TAG, "disp channel not available");
		return FALSE;
	}

	rdpSettings* settings = afc->common.context.settings;

	DISPLAY_CONTROL_MONITOR_LAYOUT layout = WINPR_C_ARRAY_INIT;
	layout.Flags = DISPLAY_CONTROL_MONITOR_PRIMARY;
	layout.Top = layout.Left = 0;
	layout.Width = width;
	layout.Height = height;
	layout.PhysicalWidth = 0;
	layout.PhysicalHeight = 0;
	layout.Orientation = freerdp_settings_get_uint16(settings, FreeRDP_DesktopOrientation);
	layout.DesktopScaleFactor = freerdp_settings_get_uint32(settings, FreeRDP_DesktopScaleFactor);
	layout.DeviceScaleFactor = freerdp_settings_get_uint32(settings, FreeRDP_DeviceScaleFactor);

	UINT rc = disp->SendMonitorLayout(disp, 1, &layout);
	if (rc != CHANNEL_RC_OK)
	{
		WLog_ERR(TAG, "SendMonitorLayout failed: %" PRIu32, rc);
		return FALSE;
	}

	WLog_DBG(TAG, "SendMonitorLayout: %" PRIu32 "x%" PRIu32, width, height);
	return TRUE;
}
