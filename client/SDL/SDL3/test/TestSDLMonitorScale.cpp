/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client monitor scale override tests
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

#include "../sdl_monitor_scale.hpp"

#include <array>
#include <exception>
#include <iostream>

static bool expect_invalid(const char* value)
{
	SdlMonitorScaleOverrides overrides;
	std::string error;
	return !sdl_parse_monitor_scale_overrides(value, overrides, error) && !error.empty();
}

int main()
try
{
	SdlMonitorScaleOverrides overrides;
	std::string error;
	if (!sdl_parse_monitor_scale_overrides("2:175:180", overrides, error) ||
	    overrides.size() != 1 || overrides.at(2).displayId != 2 ||
	    overrides.at(2).desktopScaleFactor != 175 || overrides.at(2).deviceScaleFactor != 180)
		return 1;

	if (!sdl_parse_monitor_scale_overrides("1:200:180,2:175:140,3:100:100", overrides, error))
	{
		std::cerr << error << '\n';
		return 1;
	}

	if (overrides.size() != 3 || overrides.at(1).desktopScaleFactor != 200 ||
	    overrides.at(1).deviceScaleFactor != 180 || overrides.at(2).desktopScaleFactor != 175 ||
	    overrides.at(2).deviceScaleFactor != 140 || overrides.at(3).desktopScaleFactor != 100 ||
	    overrides.at(3).deviceScaleFactor != 100)
		return 1;

	rdpMonitor monitor{};
	monitor.orig_screen = 2;
	monitor.attributes.desktopScaleFactor = 200;
	monitor.attributes.deviceScaleFactor = 100;
	if (!sdl_apply_monitor_scale_override(overrides, monitor) ||
	    monitor.attributes.desktopScaleFactor != 175 || monitor.attributes.deviceScaleFactor != 140)
		return 1;

	monitor.orig_screen = 99;
	monitor.attributes.desktopScaleFactor = 250;
	monitor.attributes.deviceScaleFactor = 180;
	if (sdl_apply_monitor_scale_override(overrides, monitor) ||
	    monitor.attributes.desktopScaleFactor != 250 || monitor.attributes.deviceScaleFactor != 180)
		return 1;

	const std::array<const char*, 2> desktopBoundaries = { "1:100:100", "1:500:180" };
	for (const auto value : desktopBoundaries)
	{
		SdlMonitorScaleOverrides parsed;
		if (!sdl_parse_monitor_scale_overrides(value, parsed, error))
			return 1;
	}

	const std::array<const char*, 25> invalid = {
		"",           "2",           "2:",        "2:175",      "2:175:",
		"2::180",     "foo:175:180", "2:foo:180", "2:175:foo",  "0:175:180",
		"2:99:100",   "2:501:180",   "2:175:175", "2:175:0",    "2:175:180,2:200:180",
		"2:175:180,", ",2:175:180",  ":175:180",  "2::175:180", "2:175:180:100",
		"2:175:180:", "2:175::180",  "2=175/180", "2=175:180",  "2:175/180",
	};
	for (const auto value : invalid)
	{
		if (!expect_invalid(value))
			return 1;
	}

	SdlMonitorScaleOverrides unchanged = overrides;
	if (sdl_parse_monitor_scale_overrides("2:175:175", unchanged, error) ||
	    unchanged.size() != overrides.size() || unchanged.at(1).desktopScaleFactor != 200)
		return 1;

	return 0;
}
catch (const std::exception& error)
{
	std::cerr << error.what() << '\n';
	return 1;
}
