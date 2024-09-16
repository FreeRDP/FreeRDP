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

#include <winpr/assert.h>
#include <winpr/library.h>
#include <winpr/ncrypt.h>

#ifndef _WIN32

#include <winpr/print.h>
#include "../log.h"

#include "ncrypt.h"

#define TAG WINPR_TAG("ncrypt")

const static char NCRYPT_MAGIC[6] = { 'N', 'C', 'R', 'Y', 'P', 'T' };

SECURITY_STATUS checkNCryptHandle(NCRYPT_HANDLE handle, NCryptHandleType matchType)
{
	if (!handle)
	{
		WLog_VRB(TAG, "invalid handle '%p'", handle);
		return ERROR_INVALID_PARAMETER;
	}

	const NCryptBaseHandle* base = (NCryptBaseHandle*)handle;
	if (memcmp(base->magic, NCRYPT_MAGIC, ARRAYSIZE(NCRYPT_MAGIC)) != 0)
	{
		char magic1[ARRAYSIZE(NCRYPT_MAGIC) + 1] = { 0 };
		char magic2[ARRAYSIZE(NCRYPT_MAGIC) + 1] = { 0 };

		memcpy(magic1, base->magic, ARRAYSIZE(NCRYPT_MAGIC));
		memcpy(magic2, NCRYPT_MAGIC, ARRAYSIZE(NCRYPT_MAGIC));

		WLog_VRB(TAG, "handle '%p' invalid magic '%s' instead of '%s'", base, magic1, magic2);
		return ERROR_INVALID_PARAMETER;
	}

	switch (base->type)
	{
		case WINPR_NCRYPT_PROVIDER:
		case WINPR_NCRYPT_KEY:
			break;
		default:
			WLog_VRB(TAG, "handle '%p' invalid type %d", base, base->type);
			return ERROR_INVALID_PARAMETER;
	}

	if ((matchType != WINPR_NCRYPT_INVALID) && (base->type != matchType))
	{
		WLog_VRB(TAG, "handle '%p' invalid type %d, expected %d", base, base->type, matchType);
		return ERROR_INVALID_PARAMETER;
	}
	return ERROR_SUCCESS;
}

void* ncrypt_new_handle(NCryptHandleType kind, size_t len, NCryptGetPropertyFn getProp,
                        NCryptReleaseFn dtor)
{
	NCryptBaseHandle* ret = calloc(1, len);
	if (!ret)
		return NULL;

	memcpy(ret->magic, NCRYPT_MAGIC, sizeof(ret->magic));
	ret->type = kind;
	ret->getPropertyFn = getProp;
	ret->releaseFn = dtor;
	return ret;
}

SECURITY_STATUS winpr_NCryptDefault_dtor(NCRYPT_HANDLE handle)
{
	NCryptBaseHandle* h = (NCryptBaseHandle*)handle;
	if (h)
	{
		memset(h->magic, 0, sizeof(h->magic));
		h->type = WINPR_NCRYPT_INVALID;
		h->releaseFn = NULL;
		free(h);
	}
	return ERROR_SUCCESS;
}

SECURITY_STATUS NCryptEnumStorageProviders(DWORD* wProviderCount,
                                           NCryptProviderName** ppProviderList, DWORD dwFlags)
{
	NCryptProviderName* ret = NULL;
	size_t stringAllocSize = 0;
#ifdef WITH_PKCS11
	LPWSTR strPtr = NULL;
	static const WCHAR emptyComment[] = { 0 };
	size_t copyAmount = 0;
#endif

	*wProviderCount = 0;
	*ppProviderList = NULL;

#ifdef WITH_PKCS11
	*wProviderCount += 1;
	stringAllocSize += (_wcslen(MS_SCARD_PROV) + 1) * 2;
	stringAllocSize += sizeof(emptyComment);
#endif

	if (!*wProviderCount)
		return ERROR_SUCCESS;

	ret = malloc(*wProviderCount * sizeof(NCryptProviderName) + stringAllocSize);
	if (!ret)
		return NTE_NO_MEMORY;

#ifdef WITH_PKCS11
	strPtr = (LPWSTR)(ret + *wProviderCount);

	ret->pszName = strPtr;
	copyAmount = (_wcslen(MS_SCARD_PROV) + 1) * 2;
	memcpy(strPtr, MS_SCARD_PROV, copyAmount);
	strPtr += copyAmount / 2;

	ret->pszComment = strPtr;
	copyAmount = sizeof(emptyComment);
	memcpy(strPtr, emptyComment, copyAmount);

	*ppProviderList = ret;
#endif

	return ERROR_SUCCESS;
}

SECURITY_STATUS NCryptOpenStorageProvider(NCRYPT_PROV_HANDLE* phProvider, LPCWSTR pszProviderName,
                                          DWORD dwFlags)
{
	return winpr_NCryptOpenStorageProviderEx(phProvider, pszProviderName, dwFlags, NULL);
}

SECURITY_STATUS winpr_NCryptOpenStorageProviderEx(NCRYPT_PROV_HANDLE* phProvider,
                                                  LPCWSTR pszProviderName, DWORD dwFlags,
                                                  LPCSTR* modulePaths)
{
#if defined(WITH_PKCS11)
	if (pszProviderName && ((_wcscmp(pszProviderName, MS_SMART_CARD_KEY_STORAGE_PROVIDER) == 0) ||
	                        (_wcscmp(pszProviderName, MS_SCARD_PROV) == 0)))
		return NCryptOpenP11StorageProviderEx(phProvider, pszProviderName, dwFlags, modulePaths);

	char buffer[128] = { 0 };
	(void)ConvertWCharToUtf8(pszProviderName, buffer, sizeof(buffer));
	WLog_WARN(TAG, "provider '%s' not supported", buffer);
	return ERROR_NOT_SUPPORTED;
#else
	WLog_WARN(TAG, "rebuild with -DWITH_PKCS11=ON to enable smartcard logon support");
	return ERROR_NOT_SUPPORTED;
#endif
}

SECURITY_STATUS NCryptEnumKeys(NCRYPT_PROV_HANDLE hProvider, LPCWSTR pszScope,
                               NCryptKeyName** ppKeyName, PVOID* ppEnumState, DWORD dwFlags)
{
	SECURITY_STATUS ret = 0;
	NCryptBaseProvider* provider = (NCryptBaseProvider*)hProvider;

	ret = checkNCryptHandle((NCRYPT_HANDLE)hProvider, WINPR_NCRYPT_PROVIDER);
	if (ret != ERROR_SUCCESS)
		return ret;

	return provider->enumKeysFn(hProvider, pszScope, ppKeyName, ppEnumState, dwFlags);
}

SECURITY_STATUS NCryptOpenKey(NCRYPT_PROV_HANDLE hProvider, NCRYPT_KEY_HANDLE* phKey,
                              LPCWSTR pszKeyName, DWORD dwLegacyKeySpec, DWORD dwFlags)
{
	SECURITY_STATUS ret = 0;
	NCryptBaseProvider* provider = (NCryptBaseProvider*)hProvider;

	ret = checkNCryptHandle((NCRYPT_HANDLE)hProvider, WINPR_NCRYPT_PROVIDER);
	if (ret != ERROR_SUCCESS)
		return ret;
	if (!phKey || !pszKeyName)
		return ERROR_INVALID_PARAMETER;

	return provider->openKeyFn(hProvider, phKey, pszKeyName, dwLegacyKeySpec, dwFlags);
}

static NCryptKeyGetPropertyEnum propertyStringToEnum(LPCWSTR pszProperty)
{
	if (_wcscmp(pszProperty, NCRYPT_CERTIFICATE_PROPERTY) == 0)
	{
		return NCRYPT_PROPERTY_CERTIFICATE;
	}
	else if (_wcscmp(pszProperty, NCRYPT_READER_PROPERTY) == 0)
	{
		return NCRYPT_PROPERTY_READER;
	}
	else if (_wcscmp(pszProperty, NCRYPT_WINPR_SLOTID) == 0)
	{
		return NCRYPT_PROPERTY_SLOTID;
	}
	else if (_wcscmp(pszProperty, NCRYPT_NAME_PROPERTY) == 0)
	{
		return NCRYPT_PROPERTY_NAME;
	}

	return NCRYPT_PROPERTY_UNKNOWN;
}

SECURITY_STATUS NCryptGetProperty(NCRYPT_HANDLE hObject, LPCWSTR pszProperty, PBYTE pbOutput,
                                  DWORD cbOutput, DWORD* pcbResult, DWORD dwFlags)
{
	NCryptKeyGetPropertyEnum property = NCRYPT_PROPERTY_UNKNOWN;
	NCryptBaseHandle* base = NULL;

	if (!hObject)
		return ERROR_INVALID_PARAMETER;

	base = (NCryptBaseHandle*)hObject;
	if (memcmp(base->magic, NCRYPT_MAGIC, 6) != 0)
		return ERROR_INVALID_HANDLE;

	property = propertyStringToEnum(pszProperty);
	if (property == NCRYPT_PROPERTY_UNKNOWN)
		return ERROR_NOT_SUPPORTED;

	return base->getPropertyFn(hObject, property, pbOutput, cbOutput, pcbResult, dwFlags);
}

SECURITY_STATUS NCryptFreeObject(NCRYPT_HANDLE hObject)
{
	NCryptBaseHandle* base = NULL;
	SECURITY_STATUS ret = checkNCryptHandle(hObject, WINPR_NCRYPT_INVALID);
	if (ret != ERROR_SUCCESS)
		return ret;

	base = (NCryptBaseHandle*)hObject;
	if (base->releaseFn)
		ret = base->releaseFn(hObject);

	return ret;
}

SECURITY_STATUS NCryptFreeBuffer(PVOID pvInput)
{
	if (!pvInput)
		return ERROR_INVALID_PARAMETER;

	free(pvInput);
	return ERROR_SUCCESS;
}

#else
SECURITY_STATUS winpr_NCryptOpenStorageProviderEx(NCRYPT_PROV_HANDLE* phProvider,
                                                  LPCWSTR pszProviderName, DWORD dwFlags,
                                                  LPCSTR* modulePaths)
{
	typedef SECURITY_STATUS (*NCryptOpenStorageProviderFn)(NCRYPT_PROV_HANDLE * phProvider,
	                                                       LPCWSTR pszProviderName, DWORD dwFlags);
	SECURITY_STATUS ret = NTE_PROV_DLL_NOT_FOUND;
	HANDLE lib = LoadLibraryA("ncrypt.dll");
	if (!lib)
		return NTE_PROV_DLL_NOT_FOUND;

	NCryptOpenStorageProviderFn ncryptOpenStorageProviderFn =
	    GetProcAddressAs(lib, "NCryptOpenStorageProvider", NCryptOpenStorageProviderFn);
	if (!ncryptOpenStorageProviderFn)
	{
		ret = NTE_PROV_DLL_NOT_FOUND;
		goto out_free_lib;
	}

	ret = ncryptOpenStorageProviderFn(phProvider, pszProviderName, dwFlags);

out_free_lib:
	FreeLibrary(lib);
	return ret;
}
#endif /* _WIN32 */

const char* winpr_NCryptSecurityStatusError(SECURITY_STATUS status)
{
#define NTE_CASE(S)            \
	case (SECURITY_STATUS)(S): \
		return #S

	switch (status)
	{
		NTE_CASE(ERROR_SUCCESS);
		NTE_CASE(ERROR_INVALID_PARAMETER);
		NTE_CASE(ERROR_INVALID_HANDLE);
		NTE_CASE(ERROR_NOT_SUPPORTED);

		NTE_CASE(NTE_BAD_UID);
		NTE_CASE(NTE_BAD_HASH);
		NTE_CASE(NTE_BAD_KEY);
		NTE_CASE(NTE_BAD_LEN);
		NTE_CASE(NTE_BAD_DATA);
		NTE_CASE(NTE_BAD_SIGNATURE);
		NTE_CASE(NTE_BAD_VER);
		NTE_CASE(NTE_BAD_ALGID);
		NTE_CASE(NTE_BAD_FLAGS);
		NTE_CASE(NTE_BAD_TYPE);
		NTE_CASE(NTE_BAD_KEY_STATE);
		NTE_CASE(NTE_BAD_HASH_STATE);
		NTE_CASE(NTE_NO_KEY);
		NTE_CASE(NTE_NO_MEMORY);
		NTE_CASE(NTE_EXISTS);
		NTE_CASE(NTE_PERM);
		NTE_CASE(NTE_NOT_FOUND);
		NTE_CASE(NTE_DOUBLE_ENCRYPT);
		NTE_CASE(NTE_BAD_PROVIDER);
		NTE_CASE(NTE_BAD_PROV_TYPE);
		NTE_CASE(NTE_BAD_PUBLIC_KEY);
		NTE_CASE(NTE_BAD_KEYSET);
		NTE_CASE(NTE_PROV_TYPE_NOT_DEF);
		NTE_CASE(NTE_PROV_TYPE_ENTRY_BAD);
		NTE_CASE(NTE_KEYSET_NOT_DEF);
		NTE_CASE(NTE_KEYSET_ENTRY_BAD);
		NTE_CASE(NTE_PROV_TYPE_NO_MATCH);
		NTE_CASE(NTE_SIGNATURE_FILE_BAD);
		NTE_CASE(NTE_PROVIDER_DLL_FAIL);
		NTE_CASE(NTE_PROV_DLL_NOT_FOUND);
		NTE_CASE(NTE_BAD_KEYSET_PARAM);
		NTE_CASE(NTE_FAIL);
		NTE_CASE(NTE_SYS_ERR);
		NTE_CASE(NTE_SILENT_CONTEXT);
		NTE_CASE(NTE_TOKEN_KEYSET_STORAGE_FULL);
		NTE_CASE(NTE_TEMPORARY_PROFILE);
		NTE_CASE(NTE_FIXEDPARAMETER);

		default:
			return "<unknown>";
	}

#undef NTE_CASE
}

const char* winpr_NCryptGetModulePath(NCRYPT_PROV_HANDLE phProvider)
{
#if defined(WITH_PKCS11)
	return NCryptGetModulePath(phProvider);
#else
	return NULL;
#endif
}
