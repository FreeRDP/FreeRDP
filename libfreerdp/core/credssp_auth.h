/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 *
 * Copyright 2022 Isaac Klein <fifthdegree@protonmail.com>
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

#ifndef FREERDP_LIB_CORE_CREDSSP_AUTH_H
#define FREERDP_LIB_CORE_CREDSSP_AUTH_H

#define CREDSSP_AUTH_PKG_SPNEGO "Negotiate"
#define CREDSSP_AUTH_PKG_NTLM "NTLM"
#define CREDSSP_AUTH_PKG_KERBEROS "Kerberos"
#define CREDSSP_AUTH_PKG_SCHANNEL "Schannel"

typedef struct rdp_credssp_auth rdpCredsspAuth;

#include <freerdp/freerdp.h>
#include <winpr/tchar.h>
#include <winpr/sspi.h>

FREERDP_LOCAL rdpCredsspAuth* credssp_auth_new(const rdpContext* context);
FREERDP_LOCAL BOOL credssp_auth_init(rdpCredsspAuth* auth, TCHAR* pkg_name,
                                     SecPkgContext_Bindings* bindings);
FREERDP_LOCAL BOOL credssp_auth_setup_client(rdpCredsspAuth* auth, const char* target_service,
                                             const char* target_hostname,
                                             const SEC_WINNT_AUTH_IDENTITY* identity,
                                             const char* pkinit);
FREERDP_LOCAL BOOL credssp_auth_setup_server(rdpCredsspAuth* auth);
FREERDP_LOCAL void credssp_auth_set_flags(rdpCredsspAuth* auth, ULONG flags);
FREERDP_LOCAL int credssp_auth_authenticate(rdpCredsspAuth* auth);
FREERDP_LOCAL BOOL credssp_auth_encrypt(rdpCredsspAuth* auth, const SecBuffer* plaintext,
                                        SecBuffer* ciphertext, size_t* signature_length,
                                        ULONG sequence);
FREERDP_LOCAL BOOL credssp_auth_decrypt(rdpCredsspAuth* auth, const SecBuffer* ciphertext,
                                        SecBuffer* plaintext, ULONG sequence);
FREERDP_LOCAL BOOL credssp_auth_impersonate(rdpCredsspAuth* auth);
FREERDP_LOCAL BOOL credssp_auth_revert_to_self(rdpCredsspAuth* auth);
FREERDP_LOCAL BOOL credssp_auth_set_spn(rdpCredsspAuth* auth, const char* service,
                                        const char* hostname);
FREERDP_LOCAL void credssp_auth_take_input_buffer(rdpCredsspAuth* auth, SecBuffer* buffer);
FREERDP_LOCAL const SecBuffer* credssp_auth_get_output_buffer(rdpCredsspAuth* auth);
FREERDP_LOCAL BOOL credssp_auth_have_output_token(rdpCredsspAuth* auth);
FREERDP_LOCAL BOOL credssp_auth_is_complete(rdpCredsspAuth* auth);
FREERDP_LOCAL const char* credssp_auth_pkg_name(rdpCredsspAuth* auth);
FREERDP_LOCAL size_t credssp_auth_trailer_size(rdpCredsspAuth* auth);
FREERDP_LOCAL void credssp_auth_free(rdpCredsspAuth* auth);

#endif /* FREERDP_LIB_CORE_CREDSSP_AUTH_H */
