/**
 * WinPR: Windows Portable Runtime
 * NCrypt library
 *
 * Copyright 2021 David Fort <contact@hardening-consulting.com>
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

#ifndef WINPR_LIBWINPR_NCRYPT_NCRYPT_H_
#define WINPR_LIBWINPR_NCRYPT_NCRYPT_H_

#include <winpr/config.h>

#include <winpr/bcrypt.h>
#include <winpr/crypto.h>
#include <winpr/ncrypt.h>
#include <winpr/error.h>
#include <winpr/string.h>

/** @brief type of ncrypt object */
typedef enum
{
	WINPR_NCRYPT_INVALID,
	WINPR_NCRYPT_PROVIDER,
	WINPR_NCRYPT_KEY
} NCryptHandleType;

/** @brief dtor function for ncrypt object */
typedef SECURITY_STATUS (*NCryptReleaseFn)(NCRYPT_HANDLE handle);

/** @brief an enum for the kind of property to retrieve */
typedef enum
{
	NCRYPT_PROPERTY_CERTIFICATE,
	NCRYPT_PROPERTY_READER,
	NCRYPT_PROPERTY_SLOTID,
	NCRYPT_PROPERTY_NAME,
	NCRYPT_PROPERTY_UNKNOWN
} NCryptKeyGetPropertyEnum;

typedef SECURITY_STATUS (*NCryptGetPropertyFn)(NCRYPT_HANDLE hObject,
                                               NCryptKeyGetPropertyEnum property, PBYTE pbOutput,
                                               DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags);

/** @brief common ncrypt handle items */
typedef struct
{
	char magic[6];
	NCryptHandleType type;
	NCryptGetPropertyFn getPropertyFn;
	NCryptReleaseFn releaseFn;
} NCryptBaseHandle;

typedef SECURITY_STATUS (*NCryptEnumKeysFn)(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszScope,
                                            NCryptKeyName** ppKeyName, PVOID* ppEnumState,
                                            DWORD dwFlags);
typedef SECURITY_STATUS (*NCryptOpenKeyFn)(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE* phKey,
                                           LPCWSTR pszKeyName, DWORD dwLegacyKeySpec,
                                           DWORD dwFlags);

/** @brief common ncrypt provider items */
typedef struct
{
	NCryptBaseHandle baseHandle;

	NCryptEnumKeysFn enumKeysFn;
	NCryptOpenKeyFn openKeyFn;
} NCryptBaseProvider;

SECURITY_STATUS checkNCryptHandle(NCRYPT_HANDLE handle, NCryptHandleType matchType);
void* ncrypt_new_handle(NCryptHandleType kind, size_t len, NCryptGetPropertyFn getProp,
                        NCryptReleaseFn dtor);
SECURITY_STATUS winpr_NCryptDefault_dtor(NCRYPT_HANDLE handle);

#if defined(WITH_PKCS11)
SECURITY_STATUS NCryptOpenP11StorageProviderEx(NCRYPT_PROV_HANDLE* phProvider,
                                               LPCWSTR pszProviderName, DWORD dwFlags,
                                               LPCSTR* modulePaths);
#endif

#endif /* WINPR_LIBWINPR_NCRYPT_NCRYPT_H_ */
