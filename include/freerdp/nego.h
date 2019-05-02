/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Protocol Security Negotiation
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2019 Idan Freiberg <speidy@gmail.com>
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

#ifndef FREERDP_NEGO_H
#define FREERDP_NEGO_H

typedef struct rdp_nego rdpNego;

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API const char* freerdp_nego_get_routing_token(rdpContext* context, DWORD* length);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_NEGO_H */
