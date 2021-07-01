/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server
 *
 * Copyright 2021 Alexandru Bagu <alexandru.bagu@gmail.com>
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

#ifndef FREERDP_SERVER_PROXY_DYNAMIC_PASSTHROUGH_H
#define FREERDP_SERVER_PROXY_DYNAMIC_PASSTHROUGH_H

#include "pf_context.h"

#define DYNAMIC_PASSTHROUGH_TEMP_TEST "PNPDR,URBDRC,RDCamera_Device_Enumerator,FileRedirectorChannel"

BOOL pf_init_dynamic_passthrough(proxyData* pdata, const char* channelname,
                                 DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL* channel);
void pf_free_dynamic_passthrough(proxyData* pdata, const char* channelname,
                                 DYNAMIC_PASSTHROUGH_DVCMAN_CHANNEL* channel);
void pf_server_clear_dynamic_passthrough(proxyData* pdata);

#endif /*FREERDP_SERVER_PROXY_DYNAMIC_PASSTHROUGH_H*/
