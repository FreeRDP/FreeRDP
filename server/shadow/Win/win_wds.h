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

#ifndef FREERDP_SERVER_SHADOW_WIN_WDS_H
#define FREERDP_SERVER_SHADOW_WIN_WDS_H

#define WITH_WDS_API 1

#ifndef CINTERFACE
#define CINTERFACE
#endif

#include <rdpencomapi.h>

#ifndef DISPID_RDPAPI_EVENT_ON_BOUNDING_RECT_CHANGED
#define DISPID_RDPAPI_EVENT_ON_BOUNDING_RECT_CHANGED 340
#endif

#include "win_shadow.h"

#ifdef __cplusplus
extern "C"
{
#endif

	int win_shadow_wds_init(winShadowSubsystem* subsystem);
	int win_shadow_wds_uninit(winShadowSubsystem* subsystem);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_WIN_WDS_H */
