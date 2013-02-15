/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * DirectFB Event Handling
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

#ifndef __DF_EVENT_H
#define __DF_EVENT_H

#include "dfreerdp.h"

void df_keyboard_init(void);
BOOL df_event_process(freerdp* instance, DFBEvent* event);

#endif /* __DF_EVENT_H */
