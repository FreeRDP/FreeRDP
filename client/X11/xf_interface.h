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

#ifndef __XF_INTERFACE_H
#define __XF_INTERFACE_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

#include <freerdp/gdi/gdi.h>
#include <freerdp/gdi/dc.h>
#include <freerdp/gdi/region.h>
#include <freerdp/rail/rail.h>
#include <freerdp/cache/cache.h>
#include <freerdp/channels/channels.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/thread.h>

typedef struct xf_info xfInfo;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Client Interface
 */

#define cfInfo	xfInfo

FREERDP_API int freerdp_client_global_init();
FREERDP_API int freerdp_client_global_uninit();

FREERDP_API int freerdp_client_start(cfInfo* cfi);
FREERDP_API int freerdp_client_stop(cfInfo* cfi);

FREERDP_API freerdp* freerdp_client_get_instance(cfInfo* cfi);
FREERDP_API HANDLE freerdp_client_get_thread(cfInfo* cfi);
FREERDP_API rdpClient* freerdp_client_get_interface(cfInfo* cfi);
FREERDP_API double freerdp_client_get_scale(xfInfo* xfi);
FREERDP_API void freerdp_client_reset_scale(xfInfo* xfi);

FREERDP_API cfInfo* freerdp_client_new(int argc, char** argv);
FREERDP_API void freerdp_client_free(cfInfo* cfi);

#ifdef __cplusplus
}
#endif

#endif /* __XF_INTERFACE_H */
