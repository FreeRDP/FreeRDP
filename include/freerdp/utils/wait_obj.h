/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Virtual Channel Manager
 *
 * Copyright 2009-2011 Jay Sorg
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

#ifndef __WAIT_OBJ_UTILS
#define __WAIT_OBJ_UTILS

#include <freerdp/api.h>

FREERDP_API struct wait_obj* wait_obj_new(void);
FREERDP_API struct wait_obj* wait_obj_new_with_fd(void* fd);
FREERDP_API void wait_obj_free(struct wait_obj* obj);
FREERDP_API int wait_obj_is_set(struct wait_obj* obj);
FREERDP_API void wait_obj_set(struct wait_obj* obj);
FREERDP_API void wait_obj_clear(struct wait_obj* obj);
FREERDP_API int wait_obj_select(struct wait_obj** listobj, int numobj, int timeout);
FREERDP_API void wait_obj_get_fds(struct wait_obj* obj, void** fds, int* count);

#endif
