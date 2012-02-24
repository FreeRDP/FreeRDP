/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Security Support Provider Interface (SSPI)
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

#ifndef FREERDP_AUTH_SSPI_PRIVATE_H
#define FREERDP_AUTH_SSPI_PRIVATE_H

#include <freerdp/types.h>
#include <freerdp/auth/sspi.h>

struct _CREDENTIALS
{
	SEC_AUTH_IDENTITY identity;
};
typedef struct _CREDENTIALS CREDENTIALS;

CREDENTIALS* sspi_CredentialsNew();
void sspi_CredentialsFree(CREDENTIALS* credentials);

SEC_HANDLE* sspi_SecureHandleAlloc();
void sspi_SecureHandleInit(SEC_HANDLE* handle);
void sspi_SecureHandleInvalidate(SEC_HANDLE* handle);
void* sspi_SecureHandleGetLowerPointer(SEC_HANDLE* handle);
void sspi_SecureHandleSetLowerPointer(SEC_HANDLE* handle, void* pointer);
void* sspi_SecureHandleGetUpperPointer(SEC_HANDLE* handle);
void sspi_SecureHandleSetUpperPointer(SEC_HANDLE* handle, void* pointer);
void sspi_SecureHandleFree(SEC_HANDLE* handle);

#endif /* FREERDP_AUTH_SSPI_PRIVATE_H */
