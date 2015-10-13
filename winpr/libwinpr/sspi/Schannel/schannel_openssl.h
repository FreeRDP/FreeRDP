/**
 * WinPR: Windows Portable Runtime
 * Schannel Security Package (OpenSSL)
 *
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

#ifndef WINPR_SSPI_SCHANNEL_OPENSSL_H
#define WINPR_SSPI_SCHANNEL_OPENSSL_H

#include <winpr/sspi.h>

#include "../sspi.h"

/* OpenSSL includes windows.h */
#include <winpr/windows.h>

typedef struct _SCHANNEL_OPENSSL SCHANNEL_OPENSSL;

int schannel_openssl_client_init(SCHANNEL_OPENSSL* context);
int schannel_openssl_server_init(SCHANNEL_OPENSSL* context);

SECURITY_STATUS schannel_openssl_client_process_tokens(SCHANNEL_OPENSSL* context, PSecBufferDesc pInput, PSecBufferDesc pOutput);
SECURITY_STATUS schannel_openssl_server_process_tokens(SCHANNEL_OPENSSL* context, PSecBufferDesc pInput, PSecBufferDesc pOutput);

SECURITY_STATUS schannel_openssl_encrypt_message(SCHANNEL_OPENSSL* context, PSecBufferDesc pMessage);
SECURITY_STATUS schannel_openssl_decrypt_message(SCHANNEL_OPENSSL* context, PSecBufferDesc pMessage);

SCHANNEL_OPENSSL* schannel_openssl_new(void);
void schannel_openssl_free(SCHANNEL_OPENSSL* context);

#endif /* WINPR_SSPI_SCHANNEL_OPENSSL_H */
