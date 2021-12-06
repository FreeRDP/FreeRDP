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

#ifndef WINPR_INCLUDE_WINPR_NCRYPT_H_
#define WINPR_INCLUDE_WINPR_NCRYPT_H_

#ifdef _WIN32
#include <wincrypt.h>
#include <ncrypt.h>
#else

#include <winpr/wtypes.h>

#ifndef __SECSTATUS_DEFINED__
typedef LONG SECURITY_STATUS;
#define __SECSTATUS_DEFINED__
#endif

typedef ULONG_PTR NCRYPT_HANDLE;
typedef ULONG_PTR NCRYPT_PROV_HANDLE;
typedef ULONG_PTR NCRYPT_KEY_HANDLE;

#define MS_KEY_STORAGE_PROVIDER                                                       \
	(const WCHAR*)"M\x00i\x00c\x00r\x00o\x00s\x00o\x00f\x00t\x00 "                    \
	              "\x00S\x00o\x00f\x00t\x00w\x00a\x00r\x00e\x00 \x00K\x00e\x00y\x00 " \
	              "\x00S\x00t\x00o\x00r\x00a\x00g\x00e\x00 "                          \
	              "\x00P\x00r\x00o\x00v\x00i\x00d\x00e\x00r\x00\x00"
#define MS_SMART_CARD_KEY_STORAGE_PROVIDER                                                       \
	(const WCHAR*)"M\x00i\x00c\x00r\x00o\x00s\x00o\x00f\x00t\x00 \x00S\x00m\x00a\x00r\x00t\x00 " \
	              "\x00C\x00a\x00r\x00d\x00 \x00K\x00e\x00y\x00 "                                \
	              "\x00S\x00t\x00o\x00r\x00a\x00g\x00e\x00 "                                     \
	              "\x00P\x00r\x00o\x00v\x00i\x00d\x00e\x00r\x00"
#define MS_SCARD_PROV                                                                       \
	(const WCHAR*)"M\x00i\x00c\x00r\x00o\x00s\x00o\x00f\x00t\x00 \x00B\x00a\x00s\x00e\x00 " \
	              "\x00S\x00m\x00a\x00r\x00t\x00 \x00C\x00a\x00r\x00d\x00 "                 \
	              "\x00C\x00r\x00y\x00p\x00t\x00o\x00 "                                     \
	              "\x00P\x00r\x00o\x00v\x00i\x00d\x00e\x00r\x00\x00"
#define MS_PLATFORM_KEY_STORAGE_PROVIDER                           \
	(const WCHAR*)"M\x00i\x00c\x00r\x00o\x00s\x00o\x00f\x00t\x00 " \
	              "\x00P\x00l\x00a\x00t\x00f\x00o\x00r\x00m\x00 "  \
	              "\x00C\x00r\x00y\x00p\x00t\x00o\x00 "            \
	              "\x00P\x00r\x00o\x00v\x00i\x00d\x00e\x00r\x00\x00"

#define NCRYPT_CERTIFICATE_PROPERTY                                                              \
	(const WCHAR*)"S\x00m\x00a\x00r\x00t\x00C\x00a\x00r\x00d\x00K\x00e\x00y\x00C\x00e\x00r\x00t" \
	              "\x00i\x00f\x00i\x00c\x00a\x00t\x00e\x00\x00"

#define NCRYPT_MACHINE_KEY_FLAG 0x20
#define NCRYPT_SILENT_FLAG 0x40

/** @brief a key name descriptor */
typedef struct NCryptKeyName
{
	LPWSTR pszName;
	LPWSTR pszAlgid;
	DWORD dwLegacyKeySpec;
	DWORD dwFlags;
} NCryptKeyName;

#ifdef __cplusplus
extern "C"
{
#endif

	SECURITY_STATUS NCryptOpenStorageProvider(NCRYPT_PROV_HANDLE* phProvider,
	                                          LPCWSTR pszProviderName, DWORD dwFlags);

	SECURITY_STATUS NCryptEnumKeys(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszScope,
	                               NCryptKeyName** ppKeyName, PVOID* ppEnumState, DWORD dwFlags);

	SECURITY_STATUS NCryptOpenKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE* phKey,
	                              LPCWSTR pszKeyName, DWORD dwLegacyKeySpec, DWORD dwFlags);

	SECURITY_STATUS NCryptGetProperty(NCRYPT_HANDLE hObject, LPCWSTR pszProperty, PBYTE pbOutput,
	                                  DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags);

	SECURITY_STATUS NCryptFreeObject(NCRYPT_HANDLE hObject);
	SECURITY_STATUS NCryptFreeBuffer(PVOID pvInput);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * custom NCryptOpenStorageProvider that allows to provide a list of modules to load
	 *
	 * @param phProvider [out] resulting provider handle
	 * @param dwFlags [in] the flags to use
	 * @param modulePaths [in] an array of library path to try to load ended with a NULL string
	 * @return ERROR_SUCCESS or an NTE error code something failed
	 */
	SECURITY_STATUS winpr_NCryptOpenStorageProviderEx(NCRYPT_PROV_HANDLE* phProvider,
	                                                  LPCWSTR pszProviderName, DWORD dwFlags,
	                                                  LPCSTR* modulePaths);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_INCLUDE_WINPR_NCRYPT_H_ */
