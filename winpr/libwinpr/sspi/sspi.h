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

/* Macro converting a ANSII character to a little endian WCHAR */
#if defined(__BIG_ENDIAN__)
#define W(c) (((WCHAR)(char)c) << 8)
#else
#define W(c) (((WCHAR)(char)c))
#endif

#define SCHANNEL_CB_MAX_TOKEN 0x00006000

typedef struct
{
	DWORD flags;
	ULONG fCredentialUse;
	SEC_GET_KEY_FN pGetKeyFn;
	void* pvGetKeyArgument;
	SEC_WINNT_AUTH_IDENTITY identity;
	SEC_WINPR_NTLM_SETTINGS_V2* ntlmSettingsV2;
} SSPI_CREDENTIALS;

void sspi_CredentialsFree(SSPI_CREDENTIALS* credentials);

WINPR_ATTR_MALLOC(sspi_CredentialsFree, 1)
SSPI_CREDENTIALS* sspi_CredentialsNew(void);

PSecBuffer sspi_FindSecBuffer(PSecBufferDesc pMessage, ULONG BufferType);

void sspi_SecureHandleFree(SecHandle* handle);

WINPR_ATTR_MALLOC(sspi_SecureHandleFree, 1)
SecHandle* sspi_SecureHandleAlloc(void);

void sspi_SecureHandleInvalidate(SecHandle* handle);

WINPR_ATTR_NODISCARD
void* sspi_SecureHandleGetLowerPointer(SecHandle* handle);

void sspi_SecureHandleSetLowerPointer(SecHandle* handle, void* pointer);

WINPR_ATTR_NODISCARD
void* sspi_SecureHandleGetUpperPointer(SecHandle* handle);

void sspi_SecureHandleSetUpperPointer(SecHandle* handle, void* pointer);

/* Package identity of a SecHandle (1-based; 0 == unset). The order MUST match
 * SecPkgInfo{A,W}_LIST and SecurityFunctionTable{A,W}_NAME_LIST in sspi_winpr.c;
 * that is asserted at compile time there. */
typedef enum WINPR_C23_ENUM_TYPE(uint32_t)
{
	SSPI_PACKAGE_NONE = 0,
	SSPI_PACKAGE_NTLM = 1,
	SSPI_PACKAGE_KERBEROS = 2,
	SSPI_PACKAGE_NEGOTIATE = 3,
	SSPI_PACKAGE_CREDSSP = 4,
	SSPI_PACKAGE_SCHANNEL = 5,
	SSPI_PACKAGE_COUNT /* number of identifiers, SSPI_PACKAGE_NONE included; keep last */
} SSPI_PACKAGE_ID;

/* Typed access to the package identity. Shares the upper-pointer slot and its encoding
 * with sspi_SecureHandleGet/SetUpperPointer, but never forms a pointer from the
 * identifier, so no integer-to-pointer conversion appears at any call site. */
WINPR_ATTR_NODISCARD SSPI_PACKAGE_ID sspi_SecureHandleGetPackageId(SecHandle* handle);
void sspi_SecureHandleSetPackageId(SecHandle* handle, SSPI_PACKAGE_ID id);

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
	SetContextAttributesIndex = 28,
	SetCredentialsAttributesIndex = 29
};

WINPR_ATTR_NODISCARD
BOOL IsSecurityStatusError(SECURITY_STATUS status);

#include "sspi_gss.h"
#include "sspi_winpr.h"

#endif /* WINPR_SSPI_PRIVATE_H */
