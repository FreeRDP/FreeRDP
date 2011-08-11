/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Events
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

#ifndef __EVENT_UTILS_H
#define __EVENT_UTILS_H

#include <freerdp/types.h>

FRDP_EVENT* freerdp_event_new(uint16 event_class, uint16 event_type,
	FRDP_EVENT_CALLBACK on_event_free_callback, void* user_data);
void freerdp_event_free(FRDP_EVENT* event);

#endif
