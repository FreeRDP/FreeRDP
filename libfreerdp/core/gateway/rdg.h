/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Desktop Gateway (RDG)
 *
 * Copyright 2015 Denis Vincent <dvincent@devolutions.net>
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

#ifndef FREERDP_LIB_CORE_GATEWAY_RDG_H
#define FREERDP_LIB_CORE_GATEWAY_RDG_H

#include <freerdp/api.h>

#include <freerdp/types.h>

typedef struct rdp_rdg rdpRdg;

#include "../transport.h"

FREERDP_LOCAL rdpRdg* rdg_new(rdpTransport* transport);
FREERDP_LOCAL void rdg_free(rdpRdg* rdg);

FREERDP_LOCAL BOOL rdg_connect(rdpRdg* rdg, int timeout, BOOL* rpcFallback);
FREERDP_LOCAL DWORD rdg_get_event_handles(rdpRdg* rdg, HANDLE* events,
        DWORD count);

FREERDP_LOCAL BIO* rdg_get_bio(rdpRdg* rdg);

#endif /* FREERDP_LIB_CORE_GATEWAY_RDG_H */
