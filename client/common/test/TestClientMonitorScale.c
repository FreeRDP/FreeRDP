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
#include <freerdp/client/cmdline.h>
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

static rdpSettings* parse(const char* monitors, int* status)
{
	char option[256] = { 0 };
	char* argv[] = { "client", "/v:localhost", option };
	rdpSettings* settings = freerdp_settings_new(0);
	if (!settings)
		return nullptr;
	(void)_snprintf(option, sizeof(option), "/monitors:%s", monitors);
	*status = freerdp_client_settings_parse_command_line(settings, ARRAYSIZE(argv), argv, FALSE);
	return settings;
}

static BOOL check_entry(const rdpSettings* settings, size_t index, UINT32 id, UINT32 desktop,
                        UINT32 device)
{
	const UINT32* pid = freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorIds, index);
	const rdpMonitorScale* scale =
	    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorScales, index);
	if (pid && scale && (*pid == id) && (scale->desktopScaleFactor == desktop) &&
	    (scale->deviceScaleFactor == device))
		return TRUE;
	(void)fprintf(stderr,
	              "entry %" PRIuz " is %" PRIu32 ":%" PRIu32 ":%" PRIu32 ", expected %" PRIu32
	              ":%" PRIu32 ":%" PRIu32 "\n",
	              index, pid ? *pid : UINT32_MAX, scale ? scale->desktopScaleFactor : UINT32_MAX,
	              scale ? scale->deviceScaleFactor : UINT32_MAX, id, desktop, device);
	return FALSE;
}

static BOOL check_scale(const rdpSettings* settings, size_t index, UINT32 desktop, UINT32 device)
{
	const rdpMonitorScale* scale =
	    freerdp_settings_get_pointer_array(settings, FreeRDP_MonitorScales, index);
	if (scale && (scale->desktopScaleFactor == desktop) && (scale->deviceScaleFactor == device))
		return TRUE;
	(void)fprintf(stderr, "scale %" PRIuz " does not match %" PRIu32 ":%" PRIu32 "\n", index,
	              desktop, device);
	return FALSE;
}

static BOOL test_parse(void)
{
	static const char* invalid[] = {
		"1:",     "1::100",    "1:175:",  "1:175:100:", "1:175:100:5",  "1:99",
		"1:501",  "1:175:175", "1:175:0", "1:-175",     "1:+175",       "1: 175",
		"1:175 ", "1:175.0",   "1:175x",  "1:175:100x", "1:4294967396", "1:175:4294967396"
	};
	BOOL rc = FALSE;
	int status = 0;
	rdpSettings* settings = parse("3:175:140,0,4:500:180,2:100:100,5:125", &status);
	CHECK(settings && (status == 0));
	CHECK(freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds) == 5);
	CHECK(check_entry(settings, 0, 3, 175, 140));
	CHECK(check_entry(settings, 1, 0, 0, 0));
	CHECK(check_entry(settings, 2, 4, 500, 180));
	CHECK(check_entry(settings, 3, 2, 100, 100));
	// The device scale defaults to 100 when only the desktop scale is given.
	CHECK(check_entry(settings, 4, 5, 125, 100));
	// The scales are only sent with the monitor attributes.
	CHECK(freerdp_settings_get_bool(settings, FreeRDP_HasMonitorAttributes));
	freerdp_settings_free(settings);

	// Plain monitor lists keep their previous meaning.
	settings = parse("3,2", &status);
	CHECK(settings && (status == 0));
	CHECK(check_entry(settings, 0, 3, 0, 0));
	CHECK(check_entry(settings, 1, 2, 0, 0));
	CHECK(!freerdp_settings_get_bool(settings, FreeRDP_HasMonitorAttributes));
	freerdp_settings_free(settings);
	settings = nullptr;

	for (size_t i = 0; i < ARRAYSIZE(invalid); i++)
	{
		settings = parse(invalid[i], &status);
		CHECK(settings);
		if (status >= 0)
		{
			(void)fprintf(stderr, "/monitors:%s was accepted\n", invalid[i]);
			goto fail;
		}
		freerdp_settings_free(settings);
		settings = nullptr;
	}
	rc = TRUE;
fail:
	freerdp_settings_free(settings);
	return rc;
}

static BOOL test_apply(void)
{
	BOOL rc = FALSE;
	int status = 0;
	rdpMonitor monitors[] = {
		{ .x = 0,
		  .y = 0,
		  .is_primary = TRUE,
		  .orig_screen = 3,
		  .attributes = { 600, 340, 90, 200, 180 } },
		{ .x = -1920, .y = 0, .orig_screen = 0, .attributes = { 530, 300, 0, 125, 100 } },
		{ .x = 3840, .y = 0, .orig_screen = 2, .attributes = { 400, 240, 0, 150, 140 } },
		{ .x = 5760, .y = 0, .orig_screen = 7, .attributes = { 400, 240, 0, 150, 140 } }
	};
	rdpSettings* settings = parse("0:100,3:175:140,2", &status);
	CHECK(settings && (status == 0));
	// Native IDs, not array indices or primary-monitor ordering, select the overrides.
	for (size_t i = 0; i < ARRAYSIZE(monitors); i++)
		freerdp_client_apply_monitor_scale(settings, &monitors[i]);
	CHECK(monitors[0].attributes.desktopScaleFactor == 175 &&
	      monitors[0].attributes.deviceScaleFactor == 140);
	CHECK(monitors[0].attributes.physicalWidth == 600 && monitors[0].attributes.orientation == 90);
	CHECK(monitors[1].attributes.desktopScaleFactor == 100 &&
	      monitors[1].attributes.deviceScaleFactor == 100 && monitors[1].x == -1920);
	// A monitor without a scale, or not in the list, keeps the detected values.
	CHECK(monitors[2].attributes.desktopScaleFactor == 150 &&
	      monitors[2].attributes.deviceScaleFactor == 140);
	CHECK(monitors[3].attributes.desktopScaleFactor == 150);

	// Redetection for a later Display Control layout keeps the override.
	monitors[0].attributes.desktopScaleFactor = 200;
	freerdp_client_apply_monitor_scale(settings, &monitors[0]);
	CHECK(monitors[0].attributes.desktopScaleFactor == 175);

	// Clients may report detected monitors through the count only; nothing is applied then.
	CHECK(freerdp_settings_set_pointer_len(settings, FreeRDP_MonitorIds, nullptr, 0));
	CHECK(freerdp_settings_set_uint32(settings, FreeRDP_NumMonitorIds, 3));
	monitors[0].attributes.desktopScaleFactor = 200;
	freerdp_client_apply_monitor_scale(settings, &monitors[0]);
	CHECK(monitors[0].attributes.desktopScaleFactor == 200);
	rc = TRUE;
fail:
	freerdp_settings_free(settings);
	return rc;
}

static BOOL test_settings(void)
{
	BOOL rc = FALSE;
	int status = 0;
	size_t length = 0;
	char* json = nullptr;
	rdpSettings* copy = nullptr;
	rdpSettings* restored = nullptr;
	rdpSettings* settings = freerdp_settings_new(0);
	CHECK(settings);
	// Empty settings must also survive JSON round trips.
	json = freerdp_settings_serialize(settings, FALSE, &length);
	CHECK(json);
	restored = freerdp_settings_deserialize(json, length);
	CHECK(restored);
	CHECK(!freerdp_settings_get_pointer(restored, FreeRDP_MonitorScales));
	freerdp_settings_free(restored);
	restored = nullptr;
	freerdp_settings_free(settings);
	free(json);
	json = nullptr;

	settings = parse("0:175:140,3", &status);
	CHECK(settings && (status == 0));
	copy = freerdp_settings_clone(settings);
	CHECK(copy);
	CHECK(freerdp_settings_get_pointer(copy, FreeRDP_MonitorScales) !=
	      freerdp_settings_get_pointer(settings, FreeRDP_MonitorScales));
	CHECK(check_entry(copy, 0, 0, 175, 140));
	CHECK(check_entry(copy, 1, 3, 0, 0));

	json = freerdp_settings_serialize(copy, FALSE, &length);
	CHECK(json);
	restored = freerdp_settings_deserialize(json, length);
	CHECK(restored);
	CHECK(freerdp_settings_get_uint32(restored, FreeRDP_NumMonitorIds) == 2);
	CHECK(check_scale(restored, 0, 175, 140));
	CHECK(check_scale(restored, 1, 0, 0));

	const rdpMonitorScale replacement = { 200, 180 };
	CHECK(freerdp_settings_set_pointer_array(restored, FreeRDP_MonitorScales, 1, &replacement));
	CHECK(check_scale(restored, 1, 200, 180));
	CHECK(check_entry(copy, 1, 3, 0, 0));
	CHECK(!freerdp_settings_set_pointer_array(restored, FreeRDP_MonitorScales, 2, &replacement));

	// The scales always have one entry per monitor ID.
	const rdpMonitorScale scales[3] = { 0 };
	CHECK(!freerdp_settings_set_pointer_len(restored, FreeRDP_MonitorScales, scales, 3));
	CHECK(freerdp_settings_set_pointer_len(restored, FreeRDP_MonitorScales, scales, 2));
	CHECK(check_scale(restored, 1, 0, 0));

	// A new monitor ID list starts without scales.
	const UINT32 ids[] = { 5, 6, 7 };
	CHECK(freerdp_settings_set_pointer_len(copy, FreeRDP_MonitorIds, ids, ARRAYSIZE(ids)));
	CHECK(check_entry(copy, 0, 5, 0, 0));
	CHECK(check_entry(copy, 2, 7, 0, 0));
	CHECK(freerdp_settings_set_pointer_len(copy, FreeRDP_MonitorIds, nullptr, 0));
	CHECK(!freerdp_settings_get_pointer(copy, FreeRDP_MonitorScales));
	rc = TRUE;
fail:
	free(json);
	freerdp_settings_free(restored);
	freerdp_settings_free(copy);
	freerdp_settings_free(settings);
	return rc;
}

int TestClientMonitorScale(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	return (test_parse() && test_apply() && test_settings()) ? 0 : -1;
}
