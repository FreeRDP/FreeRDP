/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Semaphore Utils
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef __SEMAPHORE_UTILS_H
#define __SEMAPHORE_UTILS_H

#include <freerdp/api.h>

typedef void* freerdp_sem;

FREERDP_API freerdp_sem freerdp_sem_new(int iv);
FREERDP_API void freerdp_sem_free(freerdp_sem sem);
FREERDP_API void freerdp_sem_signal(freerdp_sem sem);
FREERDP_API void freerdp_sem_wait(freerdp_sem sem);

#endif /* __SEMAPHORE_UTILS_H */
