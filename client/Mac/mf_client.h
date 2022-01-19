/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Client
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CLIENT_MAC_CLIENT_H
#define FREERDP_CLIENT_MAC_CLIENT_H

#include <freerdp/client.h>

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API void mf_press_mouse_button(void* context, int button, int x, int y, BOOL down);
	FREERDP_API void mf_scale_mouse_event(void* context, UINT16 flags, UINT16 x, UINT16 y);
	FREERDP_API void mf_scale_mouse_event_ex(void* context, UINT16 flags, UINT16 x, UINT16 y);

	/**
	 * Client Interface
	 */

	FREERDP_API int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_MAC_CLIENT_H */
