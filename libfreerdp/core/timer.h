/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Timer implementation
 *
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#pragma once

#include <freerdp/api.h>
#include <freerdp/types.h>

typedef struct freerdp_timer_s FreeRDPTimer;

FREERDP_LOCAL void freerdp_timer_free(FreeRDPTimer* timer);

WINPR_ATTR_MALLOC(freerdp_timer_free, 1)
FREERDP_LOCAL FreeRDPTimer* freerdp_timer_new(rdpRdp* rdp);

FREERDP_LOCAL bool freerdp_timer_poll(FreeRDPTimer* timer);
FREERDP_LOCAL HANDLE freerdp_timer_get_event(FreeRDPTimer* timer);
