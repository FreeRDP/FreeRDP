/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Client Interface
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CLIENT_X11_CLIENT_H
#define FREERDP_CLIENT_X11_CLIENT_H

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>
#include <winpr/collections.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/client.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/region.h>
#include <freerdp/cache/cache.h>
#include <freerdp/channels/channels.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * Client Interface
	 */

	FREERDP_API int RdpClientEntry(RDP_CLIENT_ENTRY_POINTS* pEntryPoints);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_X11_CLIENT_H */
