/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Common client monitor scaling
 *
 * Copyright 2026 FreeRDP contributors
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
#include <freerdp/client/monitor.h>

void freerdp_client_apply_monitor_scale(const rdpSettings* settings, rdpMonitor* monitor)
{
	WINPR_ASSERT(settings);
	WINPR_ASSERT(monitor);

	/* Clients may update NumMonitorIds for detected monitors without allocating the arrays. */
	if (!freerdp_settings_get_pointer(settings, FreeRDP_MonitorIds) ||
	    !freerdp_settings_get_pointer(settings, FreeRDP_MonitorScales))
		return;

	const UINT32 count = freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds);
	for (UINT32 i = 0; i < count; i++)
	{
		const UINT32* id = freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorIds, i);
		if (!id || (*id != monitor->orig_screen))
			continue;

		const rdpMonitorScale* scale =
		    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorScales, i);
		if (!scale || (scale->desktopScaleFactor == 0))
			return;
		monitor->attributes.desktopScaleFactor = scale->desktopScaleFactor;
		monitor->attributes.deviceScaleFactor = scale->deviceScaleFactor;
		return;
	}
}
