/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NTLM over HTTP
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_CORE_GATEWAY_NTLM_H
#define FREERDP_LIB_CORE_GATEWAY_NTLM_H

typedef struct rdp_ntlm rdpNtlm;

#include <winpr/wtypes.h>
#include <freerdp/api.h>
#include <winpr/sspi.h>

FREERDP_LOCAL rdpNtlm* ntlm_new(void);
FREERDP_LOCAL void ntlm_free(rdpNtlm* ntlm);

FREERDP_LOCAL BOOL ntlm_authenticate(rdpNtlm* ntlm, BOOL* pbContinueNeeded);

FREERDP_LOCAL BOOL ntlm_client_init(rdpNtlm* ntlm, BOOL confidentiality, LPCTSTR user,
                                    LPCTSTR domain, LPCTSTR password,
                                    SecPkgContext_Bindings* Bindings);

FREERDP_LOCAL BOOL ntlm_client_make_spn(rdpNtlm* ntlm, LPCTSTR ServiceClass, LPCTSTR hostname);

FREERDP_LOCAL SSIZE_T ntlm_client_query_auth_size(rdpNtlm* ntlm);
FREERDP_LOCAL SSIZE_T ntlm_client_get_context_max_size(rdpNtlm* ntlm);

FREERDP_LOCAL BOOL ntlm_client_encrypt(rdpNtlm* ntlm, ULONG fQOP, SecBufferDesc* Message,
                                       size_t sequence);

FREERDP_LOCAL BOOL ntlm_client_set_input_buffer(rdpNtlm* ntlm, BOOL copy, const void* data,
                                                size_t size);
FREERDP_LOCAL const SecBuffer* ntlm_client_get_output_buffer(rdpNtlm* ntlm);

#endif /* FREERDP_LIB_CORE_GATEWAY_NTLM_H */
