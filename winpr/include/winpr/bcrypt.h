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

#define BCRYPT_RSA_ALGORITHM   \
	(const WCHAR*)"R\x00S\x00" \
	              "A\x00\x00"
#define BCRYPT_RSA_SIGN_ALGORITHM \
	(const WCHAR*)"R\x00S\x00"    \
	              "A\x00_\x00S\x00I\x00G\x00N\x00\x00"
#define BCRYPT_DH_ALGORITHM (const WCHAR*)"D\x00H\x00\x00"
#define BCRYPT_DSA_ALGORITHM   \
	(const WCHAR*)"D\x00S\x00" \
	              "A\x00\x00"
#define BCRYPT_RC2_ALGORITHM \
	(const WCHAR*)"R\x00"    \
	              "C\x002\x00\x00"
#define BCRYPT_RC4_ALGORITHM \
	(const WCHAR*)"R\x00"    \
	              "C\x004\x00\x00"
#define BCRYPT_AES_ALGORITHM \
	(const WCHAR*)"A\x00"    \
	              "E\x00S\x00\x00"
#define BCRYPT_DES_ALGORITHM \
	(const WCHAR*)"D\x00"    \
	              "E\x00S\x00\x00"
#define BCRYPT_DESX_ALGORITHM \
	(const WCHAR*)"D\x00"     \
	              "E\x00S\x00X\x00\x00"
#define BCRYPT_3DES_ALGORITHM \
	(const WCHAR*)"3\x00"     \
	              "D\x00"     \
	              "E\x00S\x00\x00"
#define BCRYPT_3DES_112_ALGORITHM \
	(const WCHAR*)"3\x00"         \
	              "D\x00"         \
	              "E\x00S\x00_\x001\x001\x002\x00\x00"
#define BCRYPT_MD2_ALGORITHM \
	(const WCHAR*)"M\x00"    \
	              "D\x002\x00\x00"
#define BCRYPT_MD4_ALGORITHM \
	(const WCHAR*)"M\x00"    \
	              "D\x004\x00\x00"
#define BCRYPT_MD5_ALGORITHM \
	(const WCHAR*)"M\x00"    \
	              "D\x005\x00\x00"
#define BCRYPT_SHA1_ALGORITHM  \
	(const WCHAR*)"S\x00H\x00" \
	              "A\x001\x00\x00"
#define BCRYPT_SHA256_ALGORITHM \
	(const WCHAR*)"S\x00H\x00"  \
	              "A\x002\x005\x006\x00\x00"
#define BCRYPT_SHA384_ALGORITHM \
	(const WCHAR*)"S\x00H\x00"  \
	              "A\x003\x008\x004\x00\x00"
#define BCRYPT_SHA512_ALGORITHM \
	(const WCHAR*)"S\x00H\x00"  \
	              "A\x005\x001\x002\x00\x00"
#define BCRYPT_AES_GMAC_ALGORITHM             \
	(const WCHAR*)"A\x00"                     \
	              "E\x00S\x00-\x00G\x00M\x00" \
	              "A\x00"                     \
	              "C\x00\x00"
#define BCRYPT_ECDSA_ALGORITHM \
	(const WCHAR*)"E\x00"      \
	              "C\x00"      \
	              "D\x00S\x00" \
	              "A\x00\x00"
#define BCRYPT_ECDSA_P256_ALGORITHM \
	(const WCHAR*)"E\x00"           \
	              "C\x00"           \
	              "D\x00S\x00"      \
	              "A\x00_\x00P\x002\x005\x006\x00\x00"
#define BCRYPT_ECDSA_P384_ALGORITHM \
	(const WCHAR*)"E\x00"           \
	              "C\x00"           \
	              "D\x00S\x00"      \
	              "A\x00_\x00P\x003\x008\x004\x00\x00"
#define BCRYPT_ECDSA_P521_ALGORITHM \
	(const WCHAR*)"E\x00"           \
	              "C\x00"           \
	              "D\x00S\x00"      \
	              "A\x00_\x00P\x005\x002\x001\x00\x00"
#define BCRYPT_ECDH_P256_ALGORITHM \
	(const WCHAR*)"E\x00"          \
	              "C\x00"          \
	              "D\x00S\x00"     \
	              "A\x00_\x00P\x002\x005\x006\x00\x00"
#define BCRYPT_ECDH_P384_ALGORITHM \
	(const WCHAR*)"E\x00"          \
	              "C\x00"          \
	              "D\x00S\x00"     \
	              "A\x00_\x00P\x003\x008\x004\x00\x00"
#define BCRYPT_ECDH_P521_ALGORITHM \
	(const WCHAR*)"E\x00"          \
	              "C\x00"          \
	              "D\x00S\x00"     \
	              "A\x00_\x00P\x005\x002\x001\x00\x00"
#define BCRYPT_RNG_ALGORITHM (const WCHAR*)"R\x00N\x00G\x00\x00"
#define BCRYPT_RNG_FIPS186_DSA_ALGORITHM                \
	(const WCHAR*)"F\x00I\x00P\x00S\x001\x008\x006\x00" \
	              "D\x00S\x00"                          \
	              "A\x00R\x00N\x00G\x00\x00"
#define BCRYPT_RNG_DUAL_EC_ALGORITHM \
	(const WCHAR*)"D\x00U\x00"       \
	              "A\x00L\x00"       \
	              "E\x00R\x00N\x00G\x00\x00"

#define MS_PRIMITIVE_PROVIDER                                    \
	(const WCHAR*)"M\x00i\x00"                                   \
	              "c\x00r\x00o\x00s\x00o\x00"                    \
	              "f\x00t\x00 "                                  \
	              "\x00P\x00r\x00i\x00m\x00i\x00t\x00i\x00v\x00" \
	              "e\x00 "                                       \
	              "\x00P\x00r\x00o\x00v\x00i\x00"                \
	              "d\x00"                                        \
	              "e\x00r\x00\x00"

#define BCRYPT_ALG_HANDLE_HMAC_FLAG 0x00000008
#define BCRYPT_PROV_DISPATCH 0x00000001

#define BCRYPT_OBJECT_LENGTH        \
	(const WCHAR*)"O\x00"           \
	              "b\x00j\x00"      \
	              "e\x00"           \
	              "c\x00t\x00L\x00" \
	              "e\x00n\x00g\x00t\x00h\x00\x00"
#define BCRYPT_ALGORITHM_NAME                                          \
	(const WCHAR*)"A\x00l\x00g\x00o\x00r\x00i\x00t\x00h\x00m\x00N\x00" \
	              "a\x00m\x00"                                         \
	              "e\x00\x00"
#define BCRYPT_PROVIDER_HANDLE                \
	(const WCHAR*)"P\x00r\x00o\x00v\x00i\x00" \
	              "d\x00"                     \
	              "e\x00r\x00H\x00"           \
	              "a\x00n\x00"                \
	              "d\x00l\x00"                \
	              "e\x00\x00"
#define BCRYPT_CHAINING_MODE                                 \
	(const WCHAR*)"C\x00h\x00"                               \
	              "a\x00i\x00n\x00i\x00n\x00g\x00M\x00o\x00" \
	              "d\x00"                                    \
	              "e\x00\x00"
#define BCRYPT_BLOCK_LENGTH         \
	(const WCHAR*)"B\x00l\x00o\x00" \
	              "c\x00k\x00L\x00" \
	              "e\x00n\x00g\x00t\x00h\x00\x00"
#define BCRYPT_KEY_LENGTH           \
	(const WCHAR*)"K\x00"           \
	              "e\x00y\x00L\x00" \
	              "e\x00n\x00g\x00t\x00h\x00\x00"
#define BCRYPT_KEY_OBJECT_LENGTH              \
	(const WCHAR*)"K\x00"                     \
	              "e\x00y\x00O\x00"           \
	              "b\x00j\x00"                \
	              "e\x00"                     \
	              "c\x00t\x00L\x00"           \
	              "e\x00n\x00g\x00t\x00h\x00" \
	              "\x00"
#define BCRYPT_KEY_STRENGTH                   \
	(const WCHAR*)"K\x00"                     \
	              "e\x00y\x00S\x00t\x00r\x00" \
	              "e\x00n\x00g\x00t\x00h\x00\x00"
#define BCRYPT_KEY_LENGTHS          \
	(const WCHAR*)"K\x00"           \
	              "e\x00y\x00L\x00" \
	              "e\x00n\x00g\x00t\x00h\x00s\x00\x00"
#define BCRYPT_BLOCK_SIZE_LIST                \
	(const WCHAR*)"B\x00l\x00o\x00"           \
	              "c\x00k\x00S\x00i\x00z\x00" \
	              "e\x00L\x00i\x00s\x00t\x00\x00"
#define BCRYPT_EFFECTIVE_KEY_LENGTH      \
	(const WCHAR*)"E\x00"                \
	              "f\x00"                \
	              "f\x00"                \
	              "e\x00"                \
	              "c\x00t\x00i\x00v\x00" \
	              "e\x00K\x00"           \
	              "e\x00y\x00L\x00"      \
	              "e\x00n\x00g"          \
	              "\x00t\x00h\x00\x00"
#define BCRYPT_HASH_LENGTH                \
	(const WCHAR*)"H\x00"                 \
	              "a\x00s\x00h\x00"       \
	              "D\x00i\x00g\x00"       \
	              "e\x00s\x00t\x00L\x00"  \
	              "e\x00n\x00g\x00t\x00h" \
	              "\x00\x00"
#define BCRYPT_HASH_OID_LIST                  \
	(const WCHAR*)"H\x00"                     \
	              "a\x00s\x00h\x00O\x00I\x00" \
	              "D\x00L\x00i\x00s\x00t\x00\x00"
#define BCRYPT_PADDING_SCHEMES                \
	(const WCHAR*)"P\x00"                     \
	              "a\x00"                     \
	              "d\x00"                     \
	              "d\x00i\x00n\x00g\x00S\x00" \
	              "c\x00h\x00"                \
	              "e\x00m\x00"                \
	              "e\x00s\x00\x00"
#define BCRYPT_SIGNATURE_LENGTH               \
	(const WCHAR*)"S\x00i\x00g\x00n\x00"      \
	              "a\x00t\x00u\x00r\x00"      \
	              "e\x00L\x00"                \
	              "e\x00n\x00g\x00t\x00h\x00" \
	              "\x00"
#define BCRYPT_HASH_BLOCK_LENGTH              \
	(const WCHAR*)"H\x00"                     \
	              "a\x00s\x00h\x00"           \
	              "B\x00l\x00o\x00"           \
	              "c\x00k\x00L\x00"           \
	              "e\x00n\x00g\x00t\x00h\x00" \
	              "\x00"
#define BCRYPT_AUTH_TAG_LENGTH                \
	(const WCHAR*)"A\x00u\x00t\x00h\x00T\x00" \
	              "a\x00g\x00L\x00"           \
	              "e\x00n\x00g\x00t\x00h\x00\x00"
#define BCRYPT_PRIMITIVE_TYPE                                \
	(const WCHAR*)"P\x00r\x00i\x00m\x00i\x00t\x00i\x00v\x00" \
	              "e\x00T\x00y\x00p\x00"                     \
	              "e\x00\x00"
#define BCRYPT_IS_KEYED_HASH        \
	(const WCHAR*)"I\x00s\x00K\x00" \
	              "e\x00y\x00"      \
	              "e\x00"           \
	              "d\x00H\x00"      \
	              "a\x00s\x00h\x00\x00"

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
