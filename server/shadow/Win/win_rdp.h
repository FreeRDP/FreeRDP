/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_SERVER_SHADOW_WIN_RDP_H
#define FREERDP_SERVER_SHADOW_WIN_RDP_H

#include <freerdp/addin.h>
#include <freerdp/gdi/gdi.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/channels/channels.h>

typedef struct shw_context shwContext;

#include "win_shadow.h"

struct shw_context
{
	rdpClientContext common;

	HANDLE StopEvent;
	freerdp* instance;
	rdpSettings* settings;
	winShadowSubsystem* subsystem;
};

#ifdef __cplusplus
extern "C"
{
#endif

	int win_shadow_rdp_init(winShadowSubsystem* subsystem);
	int win_shadow_rdp_uninit(winShadowSubsystem* subsystem);

	int win_shadow_rdp_start(winShadowSubsystem* subsystem);
	int win_shadow_rdp_stop(winShadowSubsystem* subsystem);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_WIN_RDP_H */
