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

#define BCRYPT_RSA_ALGORITHM u"RSA"
#define BCRYPT_RSA_SIGN_ALGORITHM u"RSA_SIGN"
#define BCRYPT_DH_ALGORITHM u"DH"
#define BCRYPT_DSA_ALGORITHM u"DSA"
#define BCRYPT_RC2_ALGORITHM u"RC2"
#define BCRYPT_RC4_ALGORITHM u"RC4"
#define BCRYPT_AES_ALGORITHM u"AES"
#define BCRYPT_DES_ALGORITHM u"DES"
#define BCRYPT_DESX_ALGORITHM u"DESX"
#define BCRYPT_3DES_ALGORITHM u"3DES"
#define BCRYPT_3DES_112_ALGORITHM u"3DES_112"
#define BCRYPT_MD2_ALGORITHM u"MD2"
#define BCRYPT_MD4_ALGORITHM u"MD4"
#define BCRYPT_MD5_ALGORITHM u"MD5"
#define BCRYPT_SHA1_ALGORITHM u"SHA1"
#define BCRYPT_SHA256_ALGORITHM u"SHA256"
#define BCRYPT_SHA384_ALGORITHM u"SHA384"
#define BCRYPT_SHA512_ALGORITHM u"SHA512"
#define BCRYPT_AES_GMAC_ALGORITHM u"AES-GMAC"
#define BCRYPT_AES_CMAC_ALGORITHM u"AES-CMAC"
#define BCRYPT_ECDSA_P256_ALGORITHM u"ECDSA_P256"
#define BCRYPT_ECDSA_P384_ALGORITHM u"ECDSA_P384"
#define BCRYPT_ECDSA_P521_ALGORITHM u"ECDSA_P521"
#define BCRYPT_ECDH_P256_ALGORITHM u"ECDSA_P256"
#define BCRYPT_ECDH_P384_ALGORITHM u"ECDSA_P384"
#define BCRYPT_ECDH_P521_ALGORITHM u"ECDSA_P521"
#define BCRYPT_RNG_ALGORITHM u"RNG"
#define BCRYPT_RNG_FIPS186_DSA_ALGORITHM u"FIPS186DSARNG"
#define BCRYPT_RNG_DUAL_EC_ALGORITHM u"DUALECRNG"

#define BCRYPT_ECDSA_ALGORITHM u"ECDSA"
#define BCRYPT_ECDH_ALGORITHM u"ECDH"
#define BCRYPT_XTS_AES_ALGORITHM u"XTS-AES"

#define MS_PRIMITIVE_PROVIDER u"Microsoft Primitive Provider"
#define MS_PLATFORM_CRYPTO_PROVIDER u"Microsoft Platform Crypto Provider"

#define BCRYPT_ALG_HANDLE_HMAC_FLAG 0x00000008
#define BCRYPT_PROV_DISPATCH 0x00000001

#define BCRYPT_OBJECT_LENGTH u"ObjectLength"
#define BCRYPT_ALGORITHM_NAME u"AlgorithmName"
#define BCRYPT_PROVIDER_HANDLE u"ProviderHandle"
#define BCRYPT_CHAINING_MODE u"ChainingMode"
#define BCRYPT_BLOCK_LENGTH u"BlockLength"
#define BCRYPT_KEY_LENGTH u"KeyLength"
#define BCRYPT_KEY_OBJECT_LENGTH u"KeyObjectLength"
#define BCRYPT_KEY_STRENGTH u"KeyStrength"
#define BCRYPT_KEY_LENGTHS u"KeyLengths"
#define BCRYPT_BLOCK_SIZE_LIST u"BlockSizeList"
#define BCRYPT_EFFECTIVE_KEY_LENGTH u"EffectiveKeyLength"
#define BCRYPT_HASH_LENGTH u"HashDigestLength"
#define BCRYPT_HASH_OID_LIST u"HashOIDList"
#define BCRYPT_PADDING_SCHEMES u"PaddingSchemes"
#define BCRYPT_SIGNATURE_LENGTH u"SignatureLength"
#define BCRYPT_HASH_BLOCK_LENGTH u"HashBlockLength"
#define BCRYPT_AUTH_TAG_LENGTH u"AuthTagLength"
#define BCRYPT_PRIMITIVE_TYPE u"PrimitiveType"
#define BCRYPT_IS_KEYED_HASH u"IsKeyedHash"
#define BCRYPT_KEY_DATA_BLOB u"KeyDataBlob"

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
