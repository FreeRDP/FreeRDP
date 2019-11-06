/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Display update notifications
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#ifndef FREERDP_DISPLAY_H
#define FREERDP_DISPLAY_H

#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API BOOL freerdp_display_send_monitor_layout(rdpContext* context, UINT32 monitorCount,
	                                                     const MONITOR_DEF* monitorDefArray);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_DISPLAY_UPDATE_H */
