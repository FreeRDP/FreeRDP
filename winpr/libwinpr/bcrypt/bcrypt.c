/**
 * WinPR: Windows Portable Runtime
 * Cryptography API: Next Generation
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

#include <winpr/config.h>
#include <winpr/wlog.h>

#ifndef _WIN32
#include <winpr/bcrypt.h>

/**
 * Cryptography API: Next Generation:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa376210/
 */

NTSTATUS BCryptOpenAlgorithmProvider(WINPR_ATTR_UNUSED BCRYPT_ALG_HANDLE* phAlgorithm,
                                     WINPR_ATTR_UNUSED LPCWSTR pszAlgId,
                                     WINPR_ATTR_UNUSED LPCWSTR pszImplementation,
                                     WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptCloseAlgorithmProvider(WINPR_ATTR_UNUSED BCRYPT_ALG_HANDLE hAlgorithm,
                                      WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptGetProperty(WINPR_ATTR_UNUSED BCRYPT_HANDLE hObject,
                           WINPR_ATTR_UNUSED LPCWSTR pszProperty, WINPR_ATTR_UNUSED PUCHAR pbOutput,
                           WINPR_ATTR_UNUSED ULONG cbOutput, WINPR_ATTR_UNUSED ULONG* pcbResult,
                           WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptCreateHash(WINPR_ATTR_UNUSED BCRYPT_ALG_HANDLE hAlgorithm,
                          WINPR_ATTR_UNUSED BCRYPT_HASH_HANDLE* phHash,
                          WINPR_ATTR_UNUSED PUCHAR pbHashObject,
                          WINPR_ATTR_UNUSED ULONG cbHashObject, WINPR_ATTR_UNUSED PUCHAR pbSecret,
                          WINPR_ATTR_UNUSED ULONG cbSecret, WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptDestroyHash(WINPR_ATTR_UNUSED BCRYPT_HASH_HANDLE hHash)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptHashData(WINPR_ATTR_UNUSED BCRYPT_HASH_HANDLE hHash,
                        WINPR_ATTR_UNUSED PUCHAR pbInput, WINPR_ATTR_UNUSED ULONG cbInput,
                        WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptFinishHash(WINPR_ATTR_UNUSED BCRYPT_HASH_HANDLE hHash,
                          WINPR_ATTR_UNUSED PUCHAR pbOutput, WINPR_ATTR_UNUSED ULONG cbOutput,
                          WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptGenRandom(WINPR_ATTR_UNUSED BCRYPT_ALG_HANDLE hAlgorithm,
                         WINPR_ATTR_UNUSED PUCHAR pbBuffer, WINPR_ATTR_UNUSED ULONG cbBuffer,
                         WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptGenerateSymmetricKey(WINPR_ATTR_UNUSED BCRYPT_ALG_HANDLE hAlgorithm,
                                    WINPR_ATTR_UNUSED BCRYPT_KEY_HANDLE* phKey,
                                    WINPR_ATTR_UNUSED PUCHAR pbKeyObject,
                                    WINPR_ATTR_UNUSED ULONG cbKeyObject,
                                    WINPR_ATTR_UNUSED PUCHAR pbSecret,
                                    WINPR_ATTR_UNUSED ULONG cbSecret,
                                    WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptGenerateKeyPair(WINPR_ATTR_UNUSED BCRYPT_ALG_HANDLE hAlgorithm,
                               WINPR_ATTR_UNUSED BCRYPT_KEY_HANDLE* phKey,
                               WINPR_ATTR_UNUSED ULONG dwLength, WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptImportKey(WINPR_ATTR_UNUSED BCRYPT_ALG_HANDLE hAlgorithm,
                         WINPR_ATTR_UNUSED BCRYPT_KEY_HANDLE hImportKey,
                         WINPR_ATTR_UNUSED LPCWSTR pszBlobType,
                         WINPR_ATTR_UNUSED BCRYPT_KEY_HANDLE* phKey,
                         WINPR_ATTR_UNUSED PUCHAR pbKeyObject, WINPR_ATTR_UNUSED ULONG cbKeyObject,
                         WINPR_ATTR_UNUSED PUCHAR pbInput, WINPR_ATTR_UNUSED ULONG cbInput,
                         WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptDestroyKey(WINPR_ATTR_UNUSED BCRYPT_KEY_HANDLE hKey)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptEncrypt(WINPR_ATTR_UNUSED BCRYPT_KEY_HANDLE hKey, WINPR_ATTR_UNUSED PUCHAR pbInput,
                       WINPR_ATTR_UNUSED ULONG cbInput, WINPR_ATTR_UNUSED VOID* pPaddingInfo,
                       WINPR_ATTR_UNUSED PUCHAR pbIV, WINPR_ATTR_UNUSED ULONG cbIV,
                       WINPR_ATTR_UNUSED PUCHAR pbOutput, WINPR_ATTR_UNUSED ULONG cbOutput,
                       WINPR_ATTR_UNUSED ULONG* pcbResult, WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

NTSTATUS BCryptDecrypt(WINPR_ATTR_UNUSED BCRYPT_KEY_HANDLE hKey, WINPR_ATTR_UNUSED PUCHAR pbInput,
                       WINPR_ATTR_UNUSED ULONG cbInput, WINPR_ATTR_UNUSED VOID* pPaddingInfo,
                       WINPR_ATTR_UNUSED PUCHAR pbIV, WINPR_ATTR_UNUSED ULONG cbIV,
                       WINPR_ATTR_UNUSED PUCHAR pbOutput, WINPR_ATTR_UNUSED ULONG cbOutput,
                       WINPR_ATTR_UNUSED ULONG* pcbResult, WINPR_ATTR_UNUSED ULONG dwFlags)
{
	WLog_ERR("TODO", "TODO: implement");
	return 0;
}

#endif /* _WIN32 */
