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

#ifndef FREERDP_SERVER_SHADOW_SUBSYSTEM_H
#define FREERDP_SERVER_SHADOW_SUBSYSTEM_H

#include <freerdp/server/shadow.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

#ifdef __cplusplus
extern "C"
{
#endif

	rdpShadowSubsystem* shadow_subsystem_new(void);
	void shadow_subsystem_free(rdpShadowSubsystem* subsystem);

	int shadow_subsystem_init(rdpShadowSubsystem* subsystem, rdpShadowServer* server);
	void shadow_subsystem_uninit(rdpShadowSubsystem* subsystem);

	int shadow_subsystem_start(rdpShadowSubsystem* subsystem);
	int shadow_subsystem_stop(rdpShadowSubsystem* subsystem);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SERVER_SHADOW_SUBSYSTEM_H */
