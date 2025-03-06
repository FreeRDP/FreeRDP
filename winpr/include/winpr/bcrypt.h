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

#ifndef WINPR_BCRYPT_H
#define WINPR_BCRYPT_H

#ifdef _WIN32
#include <bcrypt.h>
#else

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

typedef PVOID BCRYPT_HANDLE;
typedef PVOID BCRYPT_ALG_HANDLE;
typedef PVOID BCRYPT_KEY_HANDLE;
typedef PVOID BCRYPT_HASH_HANDLE;
typedef PVOID BCRYPT_SECRET_HANDLE;

static const WCHAR BCRYPT_RSA_ALGORITHM[] = { 'R', 'S', 'A', '\0' };
static const WCHAR BCRYPT_RSA_SIGN_ALGORITHM[] = { 'R', 'S', 'A', '_', 'S', 'I', 'G', 'N', '\0' };
static const WCHAR BCRYPT_DH_ALGORITHM[] = { 'D', 'H', '\0' };
static const WCHAR BCRYPT_DSA_ALGORITHM[] = { 'D', 'S', 'A', '\0' };
static const WCHAR BCRYPT_RC2_ALGORITHM[] = { 'R', 'C', '2', '\0' };
static const WCHAR BCRYPT_RC4_ALGORITHM[] = { 'R', 'C', '4', '\0' };
static const WCHAR BCRYPT_AES_ALGORITHM[] = { 'A', 'E', 'S', '\0' };
static const WCHAR BCRYPT_DES_ALGORITHM[] = { 'D', 'E', 'S', '\0' };
static const WCHAR BCRYPT_DESX_ALGORITHM[] = { 'D', 'E', 'S', 'X', '\0' };
static const WCHAR BCRYPT_3DES_ALGORITHM[] = { '3', 'D', 'E', 'S', '\0' };
static const WCHAR BCRYPT_3DES_112_ALGORITHM[] = { '3', 'D', 'E', 'S', '_', '1', '1', '2', '\0' };
static const WCHAR BCRYPT_MD2_ALGORITHM[] = { 'M', 'D', '2', '\0' };
static const WCHAR BCRYPT_MD4_ALGORITHM[] = { 'M', 'D', '4', '\0' };
static const WCHAR BCRYPT_MD5_ALGORITHM[] = { 'M', 'D', '5', '\0' };
static const WCHAR BCRYPT_SHA1_ALGORITHM[] = { 'S', 'H', 'A', '1', '\0' };
static const WCHAR BCRYPT_SHA256_ALGORITHM[] = { 'S', 'H', 'A', '2', '5', '6', '\0' };
static const WCHAR BCRYPT_SHA384_ALGORITHM[] = { 'S', 'H', 'A', '3', '8', '4', '\0' };
static const WCHAR BCRYPT_SHA512_ALGORITHM[] = { 'S', 'H', 'A', '5', '1', '2', '\0' };
static const WCHAR BCRYPT_AES_GMAC_ALGORITHM[] = { 'A', 'E', 'S', '-', 'G', 'M', 'A', 'C', '\0' };
static const WCHAR BCRYPT_AES_CMAC_ALGORITHM[] = { 'A', 'E', 'S', '-', 'C', 'M', 'A', 'C', '\0' };
static const WCHAR BCRYPT_ECDSA_P256_ALGORITHM[] = { 'E', 'C', 'D', 'S', 'A', '_',
	                                                 'P', '2', '5', '6', '\0' };
static const WCHAR BCRYPT_ECDSA_P384_ALGORITHM[] = { 'E', 'C', 'D', 'S', 'A', '_',
	                                                 'P', '3', '8', '4', '\0' };
static const WCHAR BCRYPT_ECDSA_P521_ALGORITHM[] = { 'E', 'C', 'D', 'S', 'A', '_',
	                                                 'P', '5', '2', '1', '\0' };
static const WCHAR BCRYPT_ECDH_P256_ALGORITHM[] = { 'E', 'C', 'D', 'S', 'A', '_',
	                                                'P', '2', '5', '6', '\0' };
static const WCHAR BCRYPT_ECDH_P384_ALGORITHM[] = { 'E', 'C', 'D', 'S', 'A', '_',
	                                                'P', '3', '8', '4', '\0' };
static const WCHAR BCRYPT_ECDH_P521_ALGORITHM[] = { 'E', 'C', 'D', 'S', 'A', '_',
	                                                'P', '5', '2', '1', '\0' };
static const WCHAR BCRYPT_RNG_ALGORITHM[] = { 'R', 'N', 'G', '\0' };
static const WCHAR BCRYPT_RNG_FIPS186_DSA_ALGORITHM[] = { 'F', 'I', 'P', 'S', '1', '8', '6',
	                                                      'D', 'S', 'A', 'R', 'N', 'G', '\0' };
static const WCHAR BCRYPT_RNG_DUAL_EC_ALGORITHM[] = { 'D', 'U', 'A', 'L', 'E',
	                                                  'C', 'R', 'N', 'G', '\0' };

static const WCHAR BCRYPT_ECDSA_ALGORITHM[] = { 'E', 'C', 'D', 'S', 'A', '\0' };
static const WCHAR BCRYPT_ECDH_ALGORITHM[] = { 'E', 'C', 'D', 'H', '\0' };
static const WCHAR BCRYPT_XTS_AES_ALGORITHM[] = { 'X', 'T', 'S', '-', 'A', 'E', 'S', '\0' };

static const WCHAR MS_PRIMITIVE_PROVIDER[] = { 'M', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't', ' ',
	                                           'P', 'r', 'i', 'm', 'i', 't', 'i', 'v', 'e', ' ',
	                                           'P', 'r', 'o', 'v', 'i', 'd', 'e', 'r', '\0' };
static const WCHAR MS_PLATFORM_CRYPTO_PROVIDER[] = { 'M', 'i', 'c', 'r', 'o', 's', 'o', 'f', 't',
	                                                 ' ', 'P', 'l', 'a', 't', 'f', 'o', 'r', 'm',
	                                                 ' ', 'C', 'r', 'y', 'p', 't', 'o', ' ', 'P',
	                                                 'r', 'o', 'v', 'i', 'd', 'e', 'r', '\0' };

#define BCRYPT_ALG_HANDLE_HMAC_FLAG 0x00000008
#define BCRYPT_PROV_DISPATCH 0x00000001

static const WCHAR BCRYPT_OBJECT_LENGTH[] = { 'O', 'b', 'j', 'e', 'c', 't', 'L',
	                                          'e', 'n', 'g', 't', 'h', '\0' };
static const WCHAR BCRYPT_ALGORITHM_NAME[] = { 'A', 'l', 'g', 'o', 'r', 'i', 't',
	                                           'h', 'm', 'N', 'a', 'm', 'e', '\0' };
static const WCHAR BCRYPT_PROVIDER_HANDLE[] = { 'P', 'r', 'o', 'v', 'i', 'd', 'e', 'r',
	                                            'H', 'a', 'n', 'd', 'l', 'e', '\0' };
static const WCHAR BCRYPT_CHAINING_MODE[] = { 'C', 'h', 'a', 'i', 'n', 'i', 'n',
	                                          'g', 'M', 'o', 'd', 'e', '\0' };
static const WCHAR BCRYPT_BLOCK_LENGTH[] = { 'B', 'l', 'o', 'c', 'k', 'L',
	                                         'e', 'n', 'g', 't', 'h', '\0' };
static const WCHAR BCRYPT_KEY_LENGTH[] = { 'K', 'e', 'y', 'L', 'e', 'n', 'g', 't', 'h', '\0' };
static const WCHAR BCRYPT_KEY_OBJECT_LENGTH[] = { 'K', 'e', 'y', 'O', 'b', 'j', 'e', 'c',
	                                              't', 'L', 'e', 'n', 'g', 't', 'h', '\0' };
static const WCHAR BCRYPT_KEY_STRENGTH[] = { 'K', 'e', 'y', 'S', 't', 'r',
	                                         'e', 'n', 'g', 't', 'h', '\0' };
static const WCHAR BCRYPT_KEY_LENGTHS[] = {
	'K', 'e', 'y', 'L', 'e', 'n', 'g', 't', 'h', 's', '\0'
};
static const WCHAR BCRYPT_BLOCK_SIZE_LIST[] = { 'B', 'l', 'o', 'c', 'k', 'S', 'i',
	                                            'z', 'e', 'L', 'i', 's', 't', '\0' };
static const WCHAR BCRYPT_EFFECTIVE_KEY_LENGTH[] = { 'E', 'f', 'f', 'e', 'c', 't', 'i',
	                                                 'v', 'e', 'K', 'e', 'y', 'L', 'e',
	                                                 'n', 'g', 't', 'h', '\0' };
static const WCHAR BCRYPT_HASH_LENGTH[] = { 'H', 'a', 's', 'h', 'D', 'i', 'g', 'e', 's',
	                                        't', 'L', 'e', 'n', 'g', 't', 'h', '\0' };
static const WCHAR BCRYPT_HASH_OID_LIST[] = { 'H', 'a', 's', 'h', 'O', 'I',
	                                          'D', 'L', 'i', 's', 't', '\0' };
static const WCHAR BCRYPT_PADDING_SCHEMES[] = { 'P', 'a', 'd', 'd', 'i', 'n', 'g', 'S',
	                                            'c', 'h', 'e', 'm', 'e', 's', '\0' };
static const WCHAR BCRYPT_SIGNATURE_LENGTH[] = { 'S', 'i', 'g', 'n', 'a', 't', 'u', 'r',
	                                             'e', 'L', 'e', 'n', 'g', 't', 'h', '\0' };
static const WCHAR BCRYPT_HASH_BLOCK_LENGTH[] = { 'H', 'a', 's', 'h', 'B', 'l', 'o', 'c',
	                                              'k', 'L', 'e', 'n', 'g', 't', 'h', '\0' };
static const WCHAR BCRYPT_AUTH_TAG_LENGTH[] = { 'A', 'u', 't', 'h', 'T', 'a', 'g',
	                                            'L', 'e', 'n', 'g', 't', 'h', '\0' };
static const WCHAR BCRYPT_PRIMITIVE_TYPE[] = { 'P', 'r', 'i', 'm', 'i', 't', 'i',
	                                           'v', 'e', 'T', 'y', 'p', 'e', '\0' };
static const WCHAR BCRYPT_IS_KEYED_HASH[] = { 'I', 's', 'K', 'e', 'y', 'e',
	                                          'd', 'H', 'a', 's', 'h', '\0' };
static const WCHAR BCRYPT_KEY_DATA_BLOB[] = { 'K', 'e', 'y', 'D', 'a', 't',
	                                          'a', 'B', 'l', 'o', 'b', '\0' };

#define BCRYPT_BLOCK_PADDING 0x00000001

#define BCRYPT_KEY_DATA_BLOB_MAGIC 0x4d42444b
#define BCRYPT_KEY_DATA_BLOB_VERSION1 0x1

typedef struct
{
	ULONG dwMagic;
	ULONG dwVersion;
	ULONG cbKeyData;
} BCRYPT_KEY_DATA_BLOB_HEADER, *PBCRYPT_KEY_DATA_BLOB_HEADER;

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API NTSTATUS BCryptOpenAlgorithmProvider(BCRYPT_ALG_HANDLE* phAlgorithm, LPCWSTR pszAlgId,
	                                               LPCWSTR pszImplementation, ULONG dwFlags);

	WINPR_API NTSTATUS BCryptCloseAlgorithmProvider(BCRYPT_ALG_HANDLE hAlgorithm, ULONG dwFlags);

	WINPR_API NTSTATUS BCryptGetProperty(BCRYPT_HANDLE hObject, LPCWSTR pszProperty,
	                                     PUCHAR pbOutput, ULONG cbOutput, ULONG* pcbResult,
	                                     ULONG dwFlags);

	WINPR_API NTSTATUS BCryptCreateHash(BCRYPT_ALG_HANDLE hAlgorithm, BCRYPT_HASH_HANDLE* phHash,
	                                    PUCHAR pbHashObject, ULONG cbHashObject, PUCHAR pbSecret,
	                                    ULONG cbSecret, ULONG dwFlags);

	WINPR_API NTSTATUS BCryptDestroyHash(BCRYPT_HASH_HANDLE hHash);

	WINPR_API NTSTATUS BCryptHashData(BCRYPT_HASH_HANDLE hHash, PUCHAR pbInput, ULONG cbInput,
	                                  ULONG dwFlags);

	WINPR_API NTSTATUS BCryptFinishHash(BCRYPT_HASH_HANDLE hHash, PUCHAR pbOutput, ULONG cbOutput,
	                                    ULONG dwFlags);

	WINPR_API NTSTATUS BCryptGenRandom(BCRYPT_ALG_HANDLE hAlgorithm, PUCHAR pbBuffer,
	                                   ULONG cbBuffer, ULONG dwFlags);

	WINPR_API NTSTATUS BCryptGenerateSymmetricKey(BCRYPT_ALG_HANDLE hAlgorithm,
	                                              BCRYPT_KEY_HANDLE* phKey, PUCHAR pbKeyObject,
	                                              ULONG cbKeyObject, PUCHAR pbSecret,
	                                              ULONG cbSecret, ULONG dwFlags);

	WINPR_API NTSTATUS BCryptGenerateKeyPair(BCRYPT_ALG_HANDLE hAlgorithm, BCRYPT_KEY_HANDLE* phKey,
	                                         ULONG dwLength, ULONG dwFlags);

	WINPR_API NTSTATUS BCryptImportKey(BCRYPT_ALG_HANDLE hAlgorithm, BCRYPT_KEY_HANDLE hImportKey,
	                                   LPCWSTR pszBlobType, BCRYPT_KEY_HANDLE* phKey,
	                                   PUCHAR pbKeyObject, ULONG cbKeyObject, PUCHAR pbInput,
	                                   ULONG cbInput, ULONG dwFlags);

	WINPR_API NTSTATUS BCryptDestroyKey(BCRYPT_KEY_HANDLE hKey);

	WINPR_API NTSTATUS BCryptEncrypt(BCRYPT_KEY_HANDLE hKey, PUCHAR pbInput, ULONG cbInput,
	                                 VOID* pPaddingInfo, PUCHAR pbIV, ULONG cbIV, PUCHAR pbOutput,
	                                 ULONG cbOutput, ULONG* pcbResult, ULONG dwFlags);

	WINPR_API NTSTATUS BCryptDecrypt(BCRYPT_KEY_HANDLE hKey, PUCHAR pbInput, ULONG cbInput,
	                                 VOID* pPaddingInfo, PUCHAR pbIV, ULONG cbIV, PUCHAR pbOutput,
	                                 ULONG cbOutput, ULONG* pcbResult, ULONG dwFlags);

#ifdef __cplusplus
}
#endif

#endif /* _WIN32 */
#endif /* WINPR_BCRYPT_H */
