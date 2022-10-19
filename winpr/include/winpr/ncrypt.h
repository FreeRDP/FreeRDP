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
#include <winpr/winpr.h>

#ifndef __SECSTATUS_DEFINED__
typedef LONG SECURITY_STATUS;
#define __SECSTATUS_DEFINED__
#endif

typedef ULONG_PTR NCRYPT_HANDLE;
typedef ULONG_PTR NCRYPT_PROV_HANDLE;
typedef ULONG_PTR NCRYPT_KEY_HANDLE;

#define MS_KEY_STORAGE_PROVIDER                   \
	(const WCHAR*)"M\x00i\x00"                    \
	              "c\x00r\x00o\x00s\x00o\x00"     \
	              "f\x00t\x00 "                   \
	              "\x00S\x00o\x00"                \
	              "f\x00t\x00w\x00"               \
	              "a\x00r\x00"                    \
	              "e\x00 \x00K\x00"               \
	              "e\x00y\x00 "                   \
	              "\x00S\x00t\x00o\x00r\x00"      \
	              "a\x00g\x00"                    \
	              "e\x00 "                        \
	              "\x00P\x00r\x00o\x00v\x00i\x00" \
	              "d\x00"                         \
	              "e\x00r\x00\x00"
#define MS_SMART_CARD_KEY_STORAGE_PROVIDER        \
	(const WCHAR*)"M\x00i\x00"                    \
	              "c\x00r\x00o\x00s\x00o\x00"     \
	              "f\x00t\x00 \x00S\x00m\x00"     \
	              "a\x00r\x00t\x00 "              \
	              "\x00"                          \
	              "C\x00"                         \
	              "a\x00r\x00"                    \
	              "d\x00 \x00K\x00"               \
	              "e\x00y\x00 "                   \
	              "\x00S\x00t\x00o\x00r\x00"      \
	              "a\x00g\x00"                    \
	              "e\x00 "                        \
	              "\x00P\x00r\x00o\x00v\x00i\x00" \
	              "d\x00"                         \
	              "e\x00r\x00\x00"

#define MS_SCARD_PROV_A "Microsoft Base Smart Card Crypto Provider"
#define MS_SCARD_PROV                                \
	(const WCHAR*)("M\x00i\x00"                      \
	               "c\x00r\x00o\x00s\x00o\x00"       \
	               "f\x00t\x00 \x00"                 \
	               "B\x00"                           \
	               "a\x00s\x00"                      \
	               "e\x00 "                          \
	               "\x00S\x00m\x00"                  \
	               "a\x00r\x00t\x00 \x00"            \
	               "C\x00"                           \
	               "a\x00r\x00"                      \
	               "d\x00 "                          \
	               "\x00"                            \
	               "C\x00r\x00y\x00p\x00t\x00o\x00 " \
	               "\x00P\x00r\x00o\x00v\x00i\x00"   \
	               "d\x00"                           \
	               "e\x00r\x00\x00")

#define MS_PLATFORM_KEY_STORAGE_PROVIDER            \
	(const WCHAR*)"M\x00i\x00"                      \
	              "c\x00r\x00o\x00s\x00o\x00"       \
	              "f\x00t\x00 "                     \
	              "\x00P\x00l\x00"                  \
	              "a\x00t\x00"                      \
	              "f\x00o\x00r\x00m\x00 "           \
	              "\x00"                            \
	              "C\x00r\x00y\x00p\x00t\x00o\x00 " \
	              "\x00P\x00r\x00o\x00v\x00i\x00"   \
	              "d\x00"                           \
	              "e\x00r\x00\x00"

#define NCRYPT_CERTIFICATE_PROPERTY \
	(const WCHAR*)"S\x00m\x00"      \
	              "a\x00r\x00t\x00" \
	              "C\x00"           \
	              "a\x00r\x00"      \
	              "d\x00K\x00"      \
	              "e\x00y\x00"      \
	              "C\x00"           \
	              "e\x00r\x00t"     \
	              "\x00i\x00"       \
	              "f\x00i\x00"      \
	              "c\x00"           \
	              "a\x00t\x00"      \
	              "e\x00\x00"
#define NCRYPT_NAME_PROPERTY (const WCHAR*)"N\x00a\x00m\x00e\x00\x00"
#define NCRYPT_UNIQUE_NAME_PROPERTY           \
	(const WCHAR*)"U\x00n\x00i\x00q\x00u\x00" \
	              "e\x00 \x00N\x00"           \
	              "a\x00m\x00"                \
	              "e\x00\x00"
#define NCRYPT_READER_PROPERTY      \
	(const WCHAR*)"S\x00m\x00"      \
	              "a\x00r\x00t\x00" \
	              "C\x00"           \
	              "a\x00r\x00"      \
	              "d\x00R\x00"      \
	              "e\x00"           \
	              "a\x00"           \
	              "d\x00"           \
	              "e\x00r\x00\x00"

/* winpr specific properties */
#define NCRYPT_WINPR_SLOTID (const WCHAR*)"S\x00l\x00o\x00t\x00\x00"

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

/** @brief a provider name descriptor */
typedef struct NCryptProviderName
{
	LPWSTR pszName;
	LPWSTR pszComment;
} NCryptProviderName;

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API SECURITY_STATUS NCryptEnumStorageProviders(DWORD* wProviderCount,
	                                                     NCryptProviderName** ppProviderList,
	                                                     DWORD dwFlags);

	WINPR_API SECURITY_STATUS NCryptOpenStorageProvider(NCRYPT_PROV_HANDLE* phProvider,
	                                                    LPCWSTR pszProviderName, DWORD dwFlags);

	WINPR_API SECURITY_STATUS NCryptEnumKeys(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszScope,
	                                         NCryptKeyName** ppKeyName, PVOID* ppEnumState,
	                                         DWORD dwFlags);

	WINPR_API SECURITY_STATUS NCryptOpenKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE* phKey,
	                                        LPCWSTR pszKeyName, DWORD dwLegacyKeySpec,
	                                        DWORD dwFlags);

	WINPR_API SECURITY_STATUS NCryptGetProperty(NCRYPT_HANDLE hObject, LPCWSTR pszProperty,
	                                            PBYTE pbOutput, DWORD cbOutput, DWORD* pcbResult,
	                                            DWORD dwFlags);

	WINPR_API SECURITY_STATUS NCryptFreeObject(NCRYPT_HANDLE hObject);
	WINPR_API SECURITY_STATUS NCryptFreeBuffer(PVOID pvInput);

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
	WINPR_API SECURITY_STATUS winpr_NCryptOpenStorageProviderEx(NCRYPT_PROV_HANDLE* phProvider,
	                                                            LPCWSTR pszProviderName,
	                                                            DWORD dwFlags, LPCSTR* modulePaths);

	/**
	 * Gives a string representation of a SECURITY_STATUS
	 *
	 * @param status [in] SECURITY_STATUS that we want as string
	 * @return the string representation of status
	 */
	WINPR_API const char* winpr_NCryptSecurityStatusError(SECURITY_STATUS status);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_INCLUDE_WINPR_NCRYPT_H_ */
