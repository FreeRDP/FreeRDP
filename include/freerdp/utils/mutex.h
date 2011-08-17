/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Mutex Utils
 *
 * Copyright 2011 Vic Lee
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

#ifndef __MUTEX_UTILS_H
#define __MUTEX_UTILS_H

#include <freerdp/api.h>

typedef void* freerdp_mutex;

FREERDP_API freerdp_mutex freerdp_mutex_new(void);
FREERDP_API void freerdp_mutex_free(freerdp_mutex mutex);
FREERDP_API void freerdp_mutex_lock(freerdp_mutex mutex);
FREERDP_API void freerdp_mutex_unlock(freerdp_mutex mutex);

#endif /* __MUTEX_UTILS_H */
