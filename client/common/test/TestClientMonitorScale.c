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

#include <freerdp/client.h>
#include <freerdp/client/monitor.h>
#include <winpr/crt.h>

#define CHECK(condition)                                                          \
	do                                                                            \
	{                                                                             \
		if (!(condition))                                                         \
		{                                                                         \
			(void)fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #condition); \
			goto fail;                                                            \
		}                                                                         \
	} while (0)

static BOOL test_parse(void)
{
	BOOL rc = FALSE;
	rdpSettings* settings = freerdp_settings_new(0);
	static const char* invalid[] = { nullptr,
		                             "",
		                             ",",
		                             "1",
		                             "1:175",
		                             "1:175:",
		                             ":175:100",
		                             "1::100",
		                             "1:175:100:",
		                             "1:175:100,",
		                             ",1:175:100",
		                             "1:175:100,,2:100:100",
		                             "-1:175:100",
		                             "+1:175:100",
		                             " 1:175:100",
		                             "1:175:100 ",
		                             "1: 175:100",
		                             "1:175.0:100",
		                             "1:99:100",
		                             "1:501:100",
		                             "1:175:175",
		                             "1:175:0",
		                             "4294967296:175:100",
		                             "1:4294967296:100",
		                             "1:175:4294967296",
		                             "18446744073709551616:175:100",
		                             "1:175:100,1:200:140",
		                             "1:175:100,01:200:140",
		                             "1=175/100",
		                             "1:175:100,2=100/100",
		                             "1:175:100,2:100:invalid" };
	CHECK(settings);
	for (size_t i = 0; i < ARRAYSIZE(invalid); i++)
	{
		CHECK(!freerdp_client_parse_monitor_scales(settings, invalid[i]));
		CHECK(freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorScales) == 0);
		CHECK(!freerdp_settings_get_pointer(settings, FreeRDP_MonitorScales));
	}
	CHECK(freerdp_client_parse_monitor_scales(settings, "0:100:100,3:175:140,4294967295:500:180"));
	CHECK(freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorScales) == 3);
	CHECK(freerdp_settings_get_bool(settings, FreeRDP_HasMonitorAttributes));
	CHECK(freerdp_client_validate_monitor_scales(settings, nullptr, 0));
	const rdpMonitorScale* scale =
	    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorScales, 2);
	CHECK(scale && scale->id == UINT32_MAX && scale->desktopScaleFactor == 500 &&
	      scale->deviceScaleFactor == 180);
	CHECK(!freerdp_client_parse_monitor_scales(settings, "2:200:100"));
	CHECK(freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorScales) == 3);
	CHECK(freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorScales, 2) == scale);
	rc = TRUE;
fail:
	freerdp_settings_free(settings);
	return rc;
}

static BOOL test_apply(void)
{
	BOOL rc = FALSE;
	rdpSettings* settings = freerdp_settings_new(0);
	const UINT32 ids[] = { 3, 0, 2 };
	const UINT32 missing[] = { 0, 2 };
	rdpMonitor monitors[] = {
		{ .x = 0,
		  .y = 0,
		  .is_primary = TRUE,
		  .orig_screen = 3,
		  .attributes = { 600, 340, 90, 200, 180 } },
		{ .x = -1920, .y = 0, .orig_screen = 0, .attributes = { 530, 300, 0, 125, 100 } },
		{ .x = 3840, .y = 0, .orig_screen = 2, .attributes = { 400, 240, 0, 150, 140 } }
	};
	CHECK(settings);
	CHECK(freerdp_client_parse_monitor_scales(settings, "0:100:100,3:175:140"));
	CHECK(freerdp_client_validate_monitor_scales(settings, ids, ARRAYSIZE(ids)));
	CHECK(!freerdp_client_validate_monitor_scales(settings, missing, ARRAYSIZE(missing)));
	CHECK(!freerdp_client_validate_monitor_scales(settings, ids, 0));
	// Native IDs, not array indices or primary-monitor ordering, select the overrides.
	for (size_t i = 0; i < ARRAYSIZE(monitors); i++)
		freerdp_client_apply_monitor_scale(settings, &monitors[i]);
	CHECK(monitors[0].attributes.desktopScaleFactor == 175 &&
	      monitors[0].attributes.deviceScaleFactor == 140);
	CHECK(monitors[0].attributes.physicalWidth == 600 && monitors[0].attributes.orientation == 90);
	CHECK(monitors[1].attributes.desktopScaleFactor == 100 && monitors[1].x == -1920);
	CHECK(monitors[2].attributes.desktopScaleFactor == 150 &&
	      monitors[2].attributes.deviceScaleFactor == 140);
	CHECK(freerdp_settings_set_monitor_def_array_sorted(settings, monitors, ARRAYSIZE(monitors)));
	for (size_t i = 0; i < ARRAYSIZE(monitors); i++)
	{
		rdpMonitor* monitor =
		    freerdp_settings_get_pointer_array_writable(settings, FreeRDP_MonitorDefArray, i);
		CHECK(monitor);
		// Simulate redetection for a later Display Control layout.
		monitor->attributes.desktopScaleFactor = 200;
		freerdp_client_apply_monitor_scale(settings, monitor);
		CHECK(monitor->attributes.desktopScaleFactor == ((monitor->orig_screen == 0)   ? 100
		                                                 : (monitor->orig_screen == 3) ? 175
		                                                                               : 200));
	}
	CHECK(freerdp_settings_set_uint64(settings, FreeRDP_MonitorOverrideFlags,
	                                  FREERDP_MONITOR_OVERRIDE_ORIENTATION));
	CHECK(freerdp_client_validate_monitor_scales(settings, ids, ARRAYSIZE(ids)));
	CHECK(freerdp_settings_set_uint64(settings, FreeRDP_MonitorOverrideFlags,
	                                  FREERDP_MONITOR_OVERRIDE_DEVICE_SCALE));
	CHECK(!freerdp_client_validate_monitor_scales(settings, nullptr, 0));
	rc = TRUE;
fail:
	freerdp_settings_free(settings);
	return rc;
}

static BOOL test_settings(void)
{
	BOOL rc = FALSE;
	size_t length = 0;
	char* json = nullptr;
	rdpSettings* settings = freerdp_settings_new(0);
	rdpSettings* copy = nullptr;
	rdpSettings* restored = nullptr;
	rdpMonitor monitor = { .orig_screen = 0 };
	CHECK(settings);
	// Empty settings must also survive JSON round trips.
	json = freerdp_settings_serialize(settings, FALSE, &length);
	CHECK(json);
	restored = freerdp_settings_deserialize(json, length);
	CHECK(restored);
	CHECK(freerdp_settings_get_uint32(restored, FreeRDP_NumMonitorScales) == 0);
	freerdp_settings_free(restored);
	restored = nullptr;
	free(json);
	json = nullptr;
	CHECK(freerdp_client_parse_monitor_scales(settings, "0:175:140,3:100:100"));
	copy = freerdp_settings_clone(settings);
	CHECK(copy);
	CHECK(freerdp_settings_get_pointer(copy, FreeRDP_MonitorScales) !=
	      freerdp_settings_get_pointer(settings, FreeRDP_MonitorScales));
	freerdp_settings_free(settings);
	settings = nullptr;
	freerdp_client_apply_monitor_scale(copy, &monitor);
	CHECK(monitor.attributes.desktopScaleFactor == 175 &&
	      monitor.attributes.deviceScaleFactor == 140);
	json = freerdp_settings_serialize(copy, FALSE, &length);
	CHECK(json);
	restored = freerdp_settings_deserialize(json, length);
	CHECK(restored);
	CHECK(freerdp_settings_get_uint32(restored, FreeRDP_NumMonitorScales) == 2);
	CHECK(freerdp_client_validate_monitor_scales(restored, nullptr, 0));
	monitor.attributes.desktopScaleFactor = 100;
	freerdp_client_apply_monitor_scale(restored, &monitor);
	CHECK(monitor.attributes.desktopScaleFactor == 175 &&
	      monitor.attributes.deviceScaleFactor == 140);
	const rdpMonitorScale replacement = { 0, 200, 180 };
	CHECK(freerdp_settings_set_pointer_array(restored, FreeRDP_MonitorScales, 0, &replacement));
	freerdp_client_apply_monitor_scale(restored, &monitor);
	CHECK(monitor.attributes.desktopScaleFactor == 200 &&
	      monitor.attributes.deviceScaleFactor == 180);
	freerdp_client_apply_monitor_scale(copy, &monitor);
	CHECK(monitor.attributes.desktopScaleFactor == 175);
	CHECK(freerdp_settings_set_pointer_len(restored, FreeRDP_MonitorScales, nullptr, 0));
	CHECK(freerdp_settings_get_uint32(restored, FreeRDP_NumMonitorScales) == 0);
	CHECK(!freerdp_settings_get_pointer(restored, FreeRDP_MonitorScales));
	rc = TRUE;
fail:
	free(json);
	freerdp_settings_free(restored);
	freerdp_settings_free(copy);
	freerdp_settings_free(settings);
	return rc;
}

static BOOL test_command_line(void)
{
	static char* cases[][5] = {
		{ "client", "/v:localhost", "/monitor-scale:0:175:100,3:100:100", nullptr, nullptr },
		{ "client", "/v:localhost", "/scale:140", "/monitor-scale:0:175:100", nullptr },
		{ "client", "/v:localhost", "/monitor-scale:0:175:100", "/scale-desktop:175", nullptr },
		{ "client", "/v:localhost", "/scale-device:140", "/monitor-scale:0:175:100", nullptr },
		{ "client", "/v:localhost", "/monitor-scale:0:175:100", "/monitor-scale:3:100:100",
		  nullptr },
		{ "client", "/v:localhost", "/monitor-scale:0:175:100,0:100:100", nullptr, nullptr },
		{ "client", "/v:localhost", "/monitor-scale:0:175:100,3:99:100", nullptr, nullptr }
	};
	for (size_t i = 0; i < ARRAYSIZE(cases); i++)
	{
		rdpSettings* settings = freerdp_settings_new(0);
		if (!settings)
			return FALSE;
		const int argc = cases[i][3] ? 4 : 3;
		const int result =
		    freerdp_client_settings_parse_command_line(settings, argc, cases[i], FALSE);
		const bool valid = (i == 0) ? (result == 0 && freerdp_settings_get_uint32(
		                                                  settings, FreeRDP_NumMonitorScales) == 2)
		                            : (result < 0);
		freerdp_settings_free(settings);
		if (!valid)
		{
			(void)fprintf(stderr, "monitor-scale command line case %zu: unexpected status %d\n", i,
			              result);
			return FALSE;
		}
	}
	return TRUE;
}

int TestClientMonitorScale(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	return (test_parse() && test_apply() && test_settings() && test_command_line()) ? 0 : -1;
}
