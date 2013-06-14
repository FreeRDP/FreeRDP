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

#ifndef __MF_INTERFACE_H
#define __MF_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mfreerdp.h"
#include <freerdp/api.h>

// Callback type codes. Move elsewhere?
#define CALLBACK_TYPE_PARAM_CHANGE		0x01
#define CALLBACK_TYPE_CONNECTED			0x02
#define CALLBACK_TYPE_DISCONNECTED		0x03

// Public API functions

FREERDP_API int freerdp_client_global_init();
FREERDP_API int freerdp_client_global_uninit();

FREERDP_API int freerdp_client_start(void* cfi); // mfInfo
FREERDP_API int freerdp_client_stop(void* cfi); // mfInfo

FREERDP_API void* freerdp_client_get_thread(void* cfi); // HANDLE, mfInfo
FREERDP_API void* freerdp_client_get_instance(void* cfi); // freerdp, mfInfo
FREERDP_API void* freerdp_client_get_interface(void* cfi); // rdpClient, mfInfo

FREERDP_API int freerdp_client_focus_in(void* cfi); // mfInfo
FREERDP_API int freerdp_client_focus_out(void* cfi); // mfInfo

FREERDP_API int freerdp_client_set_window_size(void* cfi, int width, int height);

FREERDP_API cfInfo* freerdp_client_new(int argc, char** argv); // cfInfo*
FREERDP_API void freerdp_client_free(cfInfo* cfi); // mfInfo*

FREERDP_API int freerdp_client_set_client_callback_function(void* cfi, void* callbackFunc);

FREERDP_API void* freerdp_client_get_settings(void* cfi); // rdpSettings*, mfInfo*

FREERDP_API int freerdp_client_load_settings_from_rdp_file(void* cfi, char* filename); // mfInfo*
FREERDP_API int freerdp_client_save_settings_to_rdp_file(void* cfi, char* filename); // mfInfo*

FREERDP_API BOOL freerdp_client_get_param_bool(void* cfi, int id);
FREERDP_API int freerdp_client_set_param_bool(void* cfi, int id, BOOL param);

FREERDP_API UINT32 freerdp_client_get_param_uint32(void* cfi, int id);
FREERDP_API int freerdp_client_set_param_uint32(void* cfi, int id, UINT32 param);

FREERDP_API UINT64 freerdp_client_get_param_uint64(void* cfi, int id);
FREERDP_API int freerdp_client_set_param_uint64(void* cfi, int id, UINT64 param);

FREERDP_API char* freerdp_client_get_param_string(void* cfi, int id);
FREERDP_API int freerdp_client_set_param_string(void* cfi, int id, char* param);

FREERDP_API void freerdp_client_mouse_event(void* cfi, DWORD flags, int x, int y);

#ifdef __cplusplus
}
#endif

#endif
