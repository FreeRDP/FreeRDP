/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Level Authentication (NLA)
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
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

#ifndef FREERDP_LIB_CORE_AAD_H
#define FREERDP_LIB_CORE_AAD_H

typedef struct rdp_aad rdpAad;

typedef enum
{
	AAD_STATE_INITIAL,
	AAD_STATE_AUTH,
	AAD_STATE_FINAL
} AAD_STATE;

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

FREERDP_LOCAL BOOL aad_is_supported(void);

FREERDP_LOCAL int aad_client_begin(rdpAad* aad);
FREERDP_LOCAL int aad_recv(rdpAad* aad, wStream* s);

FREERDP_LOCAL AAD_STATE aad_get_state(rdpAad* aad);

FREERDP_LOCAL rdpAad* aad_new(rdpContext* context, rdpTransport* transport);
FREERDP_LOCAL void aad_free(rdpAad* aad);

#endif /* FREERDP_LIB_CORE_AAD_H */
