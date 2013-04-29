/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP X11 Server Interface
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

#ifndef XFREERDP_SERVER_INTERFACE_H
#define XFREERDP_SERVER_INTERFACE_H

#include <winpr/crt.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

typedef struct xf_info xfInfo;
typedef struct xf_server xfServer;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Server Interface
 */

FREERDP_API int freerdp_server_global_init();
FREERDP_API int freerdp_server_global_uninit();

FREERDP_API int freerdp_server_start(xfServer* server);
FREERDP_API int freerdp_server_stop(xfServer* server);

FREERDP_API HANDLE freerdp_server_get_thread(xfServer* server);

FREERDP_API xfServer* freerdp_server_new(int argc, char** argv);
FREERDP_API void freerdp_server_free(xfServer* server);

#ifdef __cplusplus
}
#endif

#endif /* XFREERDP_SERVER_INTERFACE_H */
