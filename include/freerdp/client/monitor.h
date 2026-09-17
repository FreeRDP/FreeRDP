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

#ifndef FREERDP_CLIENT_MONITOR_H
#define FREERDP_CLIENT_MONITOR_H

#include <freerdp/api.h>
#include <freerdp/settings.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** Parse id:desktop:device[,id:desktop:device...] into settings.
	 * IDs are native client monitor IDs (including zero), not positions in MonitorDefArray.
	 * Desktop scale must be 100..500; device scale must be 100, 140 or 180.
	 * Duplicate IDs and repeated configuration are rejected. Invalid input leaves settings
	 * unchanged.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_client_parse_monitor_scales(rdpSettings* settings, const char* value);

	/** Validate scale overrides and reject conflicting explicit global scale options.
	 * Pass the available native monitor IDs to also validate references. With ids == nullptr,
	 * only the configuration is checked; monitor discovery may not have run yet.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API BOOL freerdp_client_validate_monitor_scales(const rdpSettings* settings,
	                                                        const UINT32* ids, size_t count);

	/** Apply an override by monitor.orig_screen, preserving all other attributes and unlisted
	 * monitors. Clients should call this after detecting attributes, for initial and updated
	 * layouts.
	 */
	FREERDP_API void freerdp_client_apply_monitor_scale(const rdpSettings* settings,
	                                                    rdpMonitor* monitor);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_MONITOR_H */
