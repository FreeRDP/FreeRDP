/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/sysinfo.h>

#include <freerdp/log.h>
#include <freerdp/codec/color.h>
#include <freerdp/codec/region.h>
#include <freerdp/server/server-common.h>

#include "sample_shadow.h"

#define TAG SERVER_TAG("shadow.sample")

static BOOL sample_shadow_input_synchronize_event(WINPR_ATTR_UNUSED rdpShadowSubsystem* subsystem,
                                                  WINPR_ATTR_UNUSED rdpShadowClient* client,
                                                  WINPR_ATTR_UNUSED UINT32 flags)
{
	WLog_WARN(TAG, "TODO: Implement!");
	return TRUE;
}

static BOOL sample_shadow_input_keyboard_event(WINPR_ATTR_UNUSED rdpShadowSubsystem* subsystem,
                                               WINPR_ATTR_UNUSED rdpShadowClient* client,
                                               WINPR_ATTR_UNUSED UINT16 flags,
                                               WINPR_ATTR_UNUSED UINT8 code)
{
	WLog_WARN(TAG, "TODO: Implement!");
	return TRUE;
}

static BOOL sample_shadow_input_unicode_keyboard_event(
    WINPR_ATTR_UNUSED rdpShadowSubsystem* subsystem, WINPR_ATTR_UNUSED rdpShadowClient* client,
    WINPR_ATTR_UNUSED UINT16 flags, WINPR_ATTR_UNUSED UINT16 code)
{
	WLog_WARN(TAG, "TODO: Implement!");
	return TRUE;
}

static BOOL sample_shadow_input_mouse_event(WINPR_ATTR_UNUSED rdpShadowSubsystem* subsystem,
                                            WINPR_ATTR_UNUSED rdpShadowClient* client,
                                            WINPR_ATTR_UNUSED UINT16 flags,
                                            WINPR_ATTR_UNUSED UINT16 x, WINPR_ATTR_UNUSED UINT16 y)
{
	WLog_WARN(TAG, "TODO: Implement!");
	return TRUE;
}

static BOOL sample_shadow_input_extended_mouse_event(
    WINPR_ATTR_UNUSED rdpShadowSubsystem* subsystem, WINPR_ATTR_UNUSED rdpShadowClient* client,
    WINPR_ATTR_UNUSED UINT16 flags, WINPR_ATTR_UNUSED UINT16 x, WINPR_ATTR_UNUSED UINT16 y)
{
	WLog_WARN(TAG, "TODO: Implement!");
	return TRUE;
}

static UINT32 sample_shadow_enum_monitors(WINPR_ATTR_UNUSED MONITOR_DEF* monitors,
                                          WINPR_ATTR_UNUSED UINT32 maxMonitors)
{
	WLog_WARN(TAG, "TODO: Implement!");
	return 0;
}

static int sample_shadow_subsystem_init(rdpShadowSubsystem* arg)
{
	sampleShadowSubsystem* subsystem = (sampleShadowSubsystem*)arg;
	WINPR_ASSERT(subsystem);

	subsystem->base.numMonitors = sample_shadow_enum_monitors(subsystem->base.monitors, 16);

	WLog_WARN(TAG, "TODO: Implement!");

	MONITOR_DEF* virtualScreen = &(subsystem->base.virtualScreen);
	virtualScreen->left = 0;
	virtualScreen->top = 0;
	virtualScreen->right = 0;
	virtualScreen->bottom = 0;
	virtualScreen->flags = 1;
	return 1;
}

static int sample_shadow_subsystem_uninit(rdpShadowSubsystem* arg)
{
	sampleShadowSubsystem* subsystem = (sampleShadowSubsystem*)arg;

	if (!subsystem)
		return -1;

	WLog_WARN(TAG, "TODO: Implement!");
	return 1;
}

static int sample_shadow_subsystem_start(rdpShadowSubsystem* arg)
{
	sampleShadowSubsystem* subsystem = (sampleShadowSubsystem*)arg;

	if (!subsystem)
		return -1;

	WLog_WARN(TAG, "TODO: Implement!");

	return 1;
}

static int sample_shadow_subsystem_stop(rdpShadowSubsystem* arg)
{
	sampleShadowSubsystem* subsystem = (sampleShadowSubsystem*)arg;

	if (!subsystem)
		return -1;

	WLog_WARN(TAG, "TODO: Implement!");

	return 1;
}

static void sample_shadow_subsystem_free(rdpShadowSubsystem* arg)
{
	sampleShadowSubsystem* subsystem = (sampleShadowSubsystem*)arg;

	if (!subsystem)
		return;

	sample_shadow_subsystem_uninit(arg);
	free(subsystem);
}

static rdpShadowSubsystem* sample_shadow_subsystem_new(void)
{
	sampleShadowSubsystem* subsystem =
	    (sampleShadowSubsystem*)calloc(1, sizeof(sampleShadowSubsystem));

	if (!subsystem)
		return NULL;

	subsystem->base.SynchronizeEvent = sample_shadow_input_synchronize_event;
	subsystem->base.KeyboardEvent = sample_shadow_input_keyboard_event;
	subsystem->base.UnicodeKeyboardEvent = sample_shadow_input_unicode_keyboard_event;
	subsystem->base.MouseEvent = sample_shadow_input_mouse_event;
	subsystem->base.ExtendedMouseEvent = sample_shadow_input_extended_mouse_event;
	return &subsystem->base;
}

const char* ShadowSubsystemName(void)
{
	return "Sample";
}

int ShadowSubsystemEntry(RDP_SHADOW_ENTRY_POINTS* pEntryPoints)
{
	pEntryPoints->New = sample_shadow_subsystem_new;
	pEntryPoints->Free = sample_shadow_subsystem_free;
	pEntryPoints->Init = sample_shadow_subsystem_init;
	pEntryPoints->Uninit = sample_shadow_subsystem_uninit;
	pEntryPoints->Start = sample_shadow_subsystem_start;
	pEntryPoints->Stop = sample_shadow_subsystem_stop;
	pEntryPoints->EnumMonitors = sample_shadow_enum_monitors;
	return 1;
}
