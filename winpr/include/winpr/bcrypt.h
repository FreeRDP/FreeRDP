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

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

typedef PVOID BCRYPT_HANDLE;
typedef PVOID BCRYPT_ALG_HANDLE;
typedef PVOID BCRYPT_KEY_HANDLE;
typedef PVOID BCRYPT_HASH_HANDLE;
typedef PVOID BCRYPT_SECRET_HANDLE;

#define BCRYPT_RSA_ALGORITHM L"RSA"
#define BCRYPT_RSA_SIGN_ALGORITHM L"RSA_SIGN"
#define BCRYPT_DH_ALGORITHM L"DH"
#define BCRYPT_DSA_ALGORITHM L"DSA"
#define BCRYPT_RC2_ALGORITHM L"RC2"
#define BCRYPT_RC4_ALGORITHM L"RC4"
#define BCRYPT_AES_ALGORITHM L"AES"
#define BCRYPT_DES_ALGORITHM L"DES"
#define BCRYPT_DESX_ALGORITHM L"DESX"
#define BCRYPT_3DES_ALGORITHM L"3DES"
#define BCRYPT_3DES_112_ALGORITHM L"3DES_112"
#define BCRYPT_MD2_ALGORITHM L"MD2"
#define BCRYPT_MD4_ALGORITHM L"MD4"
#define BCRYPT_MD5_ALGORITHM L"MD5"
#define BCRYPT_SHA1_ALGORITHM L"SHA1"
#define BCRYPT_SHA256_ALGORITHM L"SHA256"
#define BCRYPT_SHA384_ALGORITHM L"SHA384"
#define BCRYPT_SHA512_ALGORITHM L"SHA512"
#define BCRYPT_AES_GMAC_ALGORITHM L"AES-GMAC"
#define BCRYPT_ECDSA_P256_ALGORITHM L"ECDSA_P256"
#define BCRYPT_ECDSA_P384_ALGORITHM L"ECDSA_P384"
#define BCRYPT_ECDSA_P521_ALGORITHM L"ECDSA_P521"
#define BCRYPT_ECDH_P256_ALGORITHM L"ECDH_P256"
#define BCRYPT_ECDH_P384_ALGORITHM L"ECDH_P384"
#define BCRYPT_ECDH_P521_ALGORITHM L"ECDH_P521"
#define BCRYPT_RNG_ALGORITHM L"RNG"
#define BCRYPT_RNG_FIPS186_DSA_ALGORITHM L"FIPS186DSARNG"
#define BCRYPT_RNG_DUAL_EC_ALGORITHM L"DUALECRNG"

#define MS_PRIMITIVE_PROVIDER L"Microsoft Primitive Provider"

#define BCRYPT_ALG_HANDLE_HMAC_FLAG 0x00000008
#define BCRYPT_PROV_DISPATCH 0x00000001

#define BCRYPT_OBJECT_LENGTH L"ObjectLength"
#define BCRYPT_ALGORITHM_NAME L"AlgorithmName"
#define BCRYPT_PROVIDER_HANDLE L"ProviderHandle"
#define BCRYPT_CHAINING_MODE L"ChainingMode"
#define BCRYPT_BLOCK_LENGTH L"BlockLength"
#define BCRYPT_KEY_LENGTH L"KeyLength"
#define BCRYPT_KEY_OBJECT_LENGTH L"KeyObjectLength"
#define BCRYPT_KEY_STRENGTH L"KeyStrength"
#define BCRYPT_KEY_LENGTHS L"KeyLengths"
#define BCRYPT_BLOCK_SIZE_LIST L"BlockSizeList"
#define BCRYPT_EFFECTIVE_KEY_LENGTH L"EffectiveKeyLength"
#define BCRYPT_HASH_LENGTH L"HashDigestLength"
#define BCRYPT_HASH_OID_LIST L"HashOIDList"
#define BCRYPT_PADDING_SCHEMES L"PaddingSchemes"
#define BCRYPT_SIGNATURE_LENGTH L"SignatureLength"
#define BCRYPT_HASH_BLOCK_LENGTH L"HashBlockLength"
#define BCRYPT_AUTH_TAG_LENGTH L"AuthTagLength"
#define BCRYPT_PRIMITIVE_TYPE L"PrimitiveType"
#define BCRYPT_IS_KEYED_HASH L"IsKeyedHash"

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
#endif /* WINPR_BCRYPT_MEMORY_H */
