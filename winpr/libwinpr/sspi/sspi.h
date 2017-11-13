/**
 * WinPR: Windows Portable Runtime
 * Security Support Provider Interface (SSPI)
 *
 * Copyright 2012-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_SSPI_PRIVATE_H
#define WINPR_SSPI_PRIVATE_H

#include <winpr/sspi.h>

#define SCHANNEL_CB_MAX_TOKEN	0x00006000

#define SSPI_CREDENTIALS_PASSWORD_HASH	0x00000001

#define SSPI_CREDENTIALS_HASH_LENGTH_FACTOR	64

struct _SSPI_CREDENTIALS
{
	DWORD flags;
	ULONG fCredentialUse;
	SEC_GET_KEY_FN pGetKeyFn;
	void* pvGetKeyArgument;
	SEC_WINNT_AUTH_IDENTITY identity;
};
typedef struct _SSPI_CREDENTIALS SSPI_CREDENTIALS;

SSPI_CREDENTIALS* sspi_CredentialsNew(void);
void sspi_CredentialsFree(SSPI_CREDENTIALS* credentials);

PSecBuffer sspi_FindSecBuffer(PSecBufferDesc pMessage, ULONG BufferType);

SecHandle* sspi_SecureHandleAlloc(void);
void sspi_SecureHandleInvalidate(SecHandle* handle);
void* sspi_SecureHandleGetLowerPointer(SecHandle* handle);
void sspi_SecureHandleSetLowerPointer(SecHandle* handle, void* pointer);
void* sspi_SecureHandleGetUpperPointer(SecHandle* handle);
void sspi_SecureHandleSetUpperPointer(SecHandle* handle, void* pointer);
void sspi_SecureHandleFree(SecHandle* handle);

enum SecurityFunctionTableIndex
{
    EnumerateSecurityPackagesIndex = 1,
    Reserved1Index = 2,
    QueryCredentialsAttributesIndex = 3,
    AcquireCredentialsHandleIndex = 4,
    FreeCredentialsHandleIndex = 5,
    Reserved2Index = 6,
    InitializeSecurityContextIndex = 7,
    AcceptSecurityContextIndex = 8,
    CompleteAuthTokenIndex = 9,
    DeleteSecurityContextIndex = 10,
    ApplyControlTokenIndex = 11,
    QueryContextAttributesIndex = 12,
    ImpersonateSecurityContextIndex = 13,
    RevertSecurityContextIndex = 14,
    MakeSignatureIndex = 15,
    VerifySignatureIndex = 16,
    FreeContextBufferIndex = 17,
    QuerySecurityPackageInfoIndex = 18,
    Reserved3Index = 19,
    Reserved4Index = 20,
    ExportSecurityContextIndex = 21,
    ImportSecurityContextIndex = 22,
    AddCredentialsIndex = 23,
    Reserved8Index = 24,
    QuerySecurityContextTokenIndex = 25,
    EncryptMessageIndex = 26,
    DecryptMessageIndex = 27,
    SetContextAttributesIndex = 28
};

BOOL IsSecurityStatusError(SECURITY_STATUS status);

#include "sspi_gss.h"
#include "sspi_winpr.h"

#endif /* WINPR_SSPI_PRIVATE_H */
