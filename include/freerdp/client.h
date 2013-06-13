/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Client Interface
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

#ifndef FREERDP_CLIENT_H
#define FREERDP_CLIENT_H

typedef struct rdp_client rdpClient;

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FREERDP_WINDOW_STATE_NORMAL		0
#define FREERDP_WINDOW_STATE_MINIMIZED		1
#define FREERDP_WINDOW_STATE_MAXIMIZED		2
#define FREERDP_WINDOW_STATE_FULLSCREEN		3
#define FREERDP_WINDOW_STATE_ACTIVE		4

typedef void (*pOnResizeWindow)(freerdp* instance, int width, int height);
typedef void (*pOnWindowStateChange)(freerdp* instance, int state);
typedef void (*pOnErrorInfo)(freerdp* instance, UINT32 code);
typedef void (*pOnParamChange)(freerdp* instance, int id);

struct rdp_client
{
	pOnResizeWindow OnResizeWindow;
	pOnWindowStateChange OnWindowStateChange;
	pOnErrorInfo OnErrorInfo;
	pOnParamChange OnParamChange;
};

/**
 * Generic Client Interface
 */

// Public API functions

FREERDP_API int freerdp_client_global_init();
FREERDP_API int freerdp_client_global_uninit();

FREERDP_API int freerdp_client_start(rdpContext* cfc);
FREERDP_API int freerdp_client_stop(rdpContext* cfc);

FREERDP_API freerdp* freerdp_client_get_instance(rdpContext* cfc);
FREERDP_API HANDLE freerdp_client_get_thread(rdpContext* cfc);
FREERDP_API rdpClient* freerdp_client_get_interface(rdpContext* cfc);
FREERDP_API double freerdp_client_get_scale(rdpContext* cfc);
FREERDP_API void freerdp_client_reset_scale(rdpContext* cfc);

FREERDP_API rdpContext* freerdp_client_new(int argc, char** argv);
FREERDP_API void freerdp_client_free(rdpContext* cfc);

FREERDP_API BOOL freerdp_client_get_param_bool(rdpContext* cfc, int id);
FREERDP_API int freerdp_client_set_param_bool(rdpContext* cfc, int id, BOOL param);

FREERDP_API UINT32 freerdp_client_get_param_uint32(rdpContext* cfc, int id);
FREERDP_API int freerdp_client_set_param_uint32(rdpContext* cfc, int id, UINT32 param);

FREERDP_API UINT64 freerdp_client_get_param_uint64(rdpContext* cfc, int id);
FREERDP_API int freerdp_client_set_param_uint64(rdpContext* cfc, int id, UINT64 param);

FREERDP_API char* freerdp_client_get_param_string(rdpContext* cfc, int id);
FREERDP_API int freerdp_client_set_param_string(rdpContext* cfc, int id, char* param);

FREERDP_API void freerdp_client_mouse_event(rdpContext* cfc, DWORD flags, int x, int y);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CLIENT_H */
