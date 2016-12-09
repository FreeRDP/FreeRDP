/**
 * WinPR: Windows Portable Runtime
 * Cryptography API (CryptoAPI)
 *
 * Copyright 2012-2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crypto.h>

/**
 * CryptAcquireCertificatePrivateKey
 * CryptBinaryToStringA
 * CryptBinaryToStringW
 * CryptCloseAsyncHandle
 * CryptCreateAsyncHandle
 * CryptCreateKeyIdentifierFromCSP
 * CryptDecodeMessage
 * CryptDecodeObject
 * CryptDecodeObjectEx
 * CryptDecryptAndVerifyMessageSignature
 * CryptDecryptMessage
 * CryptEncodeObject
 * CryptEncodeObjectEx
 * CryptEncryptMessage
 * CryptEnumKeyIdentifierProperties
 * CryptEnumOIDFunction
 * CryptEnumOIDInfo
 * CryptExportPKCS8
 * CryptExportPublicKeyInfo
 * CryptExportPublicKeyInfoEx
 * CryptExportPublicKeyInfoFromBCryptKeyHandle
 * CryptFindCertificateKeyProvInfo
 * CryptFindLocalizedName
 * CryptFindOIDInfo
 * CryptFormatObject
 * CryptFreeOIDFunctionAddress
 * CryptGetAsyncParam
 * CryptGetDefaultOIDDllList
 * CryptGetDefaultOIDFunctionAddress
 * CryptGetKeyIdentifierProperty
 * CryptGetMessageCertificates
 * CryptGetMessageSignerCount
 * CryptGetOIDFunctionAddress
 * CryptGetOIDFunctionValue
 * CryptHashCertificate
 * CryptHashCertificate2
 * CryptHashMessage
 * CryptHashPublicKeyInfo
 * CryptHashToBeSigned
 * CryptImportPKCS8
 * CryptImportPublicKeyInfo
 * CryptImportPublicKeyInfoEx
 * CryptImportPublicKeyInfoEx2
 * CryptInitOIDFunctionSet
 * CryptInstallDefaultContext
 * CryptInstallOIDFunctionAddress
 * CryptLoadSip
 * CryptMemAlloc
 * CryptMemFree
 * CryptMemRealloc
 * CryptMsgCalculateEncodedLength
 * CryptMsgClose
 * CryptMsgControl
 * CryptMsgCountersign
 * CryptMsgCountersignEncoded
 * CryptMsgDuplicate
 * CryptMsgEncodeAndSignCTL
 * CryptMsgGetAndVerifySigner
 * CryptMsgGetParam
 * CryptMsgOpenToDecode
 * CryptMsgOpenToEncode
 * CryptMsgSignCTL
 * CryptMsgUpdate
 * CryptMsgVerifyCountersignatureEncoded
 * CryptMsgVerifyCountersignatureEncodedEx
 * CryptQueryObject
 * CryptRegisterDefaultOIDFunction
 * CryptRegisterOIDFunction
 * CryptRegisterOIDInfo
 * CryptRetrieveTimeStamp
 * CryptSetAsyncParam
 * CryptSetKeyIdentifierProperty
 * CryptSetOIDFunctionValue
 * CryptSignAndEncodeCertificate
 * CryptSignAndEncryptMessage
 * CryptSignCertificate
 * CryptSignMessage
 * CryptSignMessageWithKey
 * CryptSIPAddProvider
 * CryptSIPCreateIndirectData
 * CryptSIPGetCaps
 * CryptSIPGetSignedDataMsg
 * CryptSIPLoad
 * CryptSIPPutSignedDataMsg
 * CryptSIPRemoveProvider
 * CryptSIPRemoveSignedDataMsg
 * CryptSIPRetrieveSubjectGuid
 * CryptSIPRetrieveSubjectGuidForCatalogFile
 * CryptSIPVerifyIndirectData
 * CryptUninstallDefaultContext
 * CryptUnregisterDefaultOIDFunction
 * CryptUnregisterOIDFunction
 * CryptUnregisterOIDInfo
 * CryptUpdateProtectedState
 * CryptVerifyCertificateSignature
 * CryptVerifyCertificateSignatureEx
 * CryptVerifyDetachedMessageHash
 * CryptVerifyDetachedMessageSignature
 * CryptVerifyMessageHash
 * CryptVerifyMessageSignature
 * CryptVerifyMessageSignatureWithKey
 * CryptVerifyTimeStampSignature
 * DbgInitOSS
 * DbgPrintf
 * PFXExportCertStore
 * PFXExportCertStore2
 * PFXExportCertStoreEx
 * PFXImportCertStore
 * PFXIsPFXBlob
 * PFXVerifyPassword
 */

#ifndef _WIN32

#include "crypto.h"

#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/collections.h>

static wListDictionary* g_ProtectedMemoryBlocks = NULL;

BOOL CryptProtectMemory(LPVOID pData, DWORD cbData, DWORD dwFlags)
{
	BYTE* pCipherText;
	size_t cbOut, cbFinal;
	WINPR_CIPHER_CTX* enc = NULL;
	BYTE randomKey[256];
	WINPR_PROTECTED_MEMORY_BLOCK* pMemBlock;

	if (dwFlags != CRYPTPROTECTMEMORY_SAME_PROCESS)
		return FALSE;

	if (!g_ProtectedMemoryBlocks)
	{
		g_ProtectedMemoryBlocks = ListDictionary_New(TRUE);

		if (!g_ProtectedMemoryBlocks)
			return FALSE;
	}

	pMemBlock = (WINPR_PROTECTED_MEMORY_BLOCK*) calloc(1, sizeof(WINPR_PROTECTED_MEMORY_BLOCK));

	if (!pMemBlock)
		return FALSE;

	pMemBlock->pData = pData;
	pMemBlock->cbData = cbData;
	pMemBlock->dwFlags = dwFlags;

	winpr_RAND(pMemBlock->salt, 8);
	winpr_RAND(randomKey, sizeof(randomKey));

	winpr_openssl_BytesToKey(WINPR_CIPHER_AES_256_CBC, WINPR_MD_SHA1,
			pMemBlock->salt, randomKey, sizeof(randomKey), 4, pMemBlock->key, pMemBlock->iv);

	SecureZeroMemory(randomKey, sizeof(randomKey));

	cbOut = pMemBlock->cbData + 16 - 1;
	pCipherText = (BYTE*) malloc(cbOut);

	if (!pCipherText)
		goto out;

	if ((enc = winpr_Cipher_New(WINPR_CIPHER_AES_256_CBC, WINPR_ENCRYPT,
				    pMemBlock->key, pMemBlock->iv)) == NULL)
		goto out;
	if (!winpr_Cipher_Update(enc, pMemBlock->pData, pMemBlock->cbData, pCipherText, &cbOut))
		goto out;
	if (!winpr_Cipher_Final(enc, pCipherText + cbOut, &cbFinal))
		goto out;
	winpr_Cipher_Free(enc);

	CopyMemory(pMemBlock->pData, pCipherText, pMemBlock->cbData);
	free(pCipherText);

	return ListDictionary_Add(g_ProtectedMemoryBlocks, pData, pMemBlock);
out:
	free (pMemBlock);
	free (pCipherText);
	winpr_Cipher_Free(enc);

	return FALSE;
}

BOOL CryptUnprotectMemory(LPVOID pData, DWORD cbData, DWORD dwFlags)
{
	BYTE* pPlainText = NULL;
	size_t cbOut, cbFinal;
	WINPR_CIPHER_CTX* dec = NULL;
	WINPR_PROTECTED_MEMORY_BLOCK* pMemBlock = NULL;

	if (dwFlags != CRYPTPROTECTMEMORY_SAME_PROCESS)
		return FALSE;

	if (!g_ProtectedMemoryBlocks)
		return FALSE;

	pMemBlock = (WINPR_PROTECTED_MEMORY_BLOCK*) ListDictionary_GetItemValue(g_ProtectedMemoryBlocks, pData);

	if (!pMemBlock)
		goto out;

	cbOut = pMemBlock->cbData + 16 - 1;

	pPlainText = (BYTE*) malloc(cbOut);

	if (!pPlainText)
		goto out;

	if ((dec = winpr_Cipher_New(WINPR_CIPHER_AES_256_CBC, WINPR_DECRYPT,
				    pMemBlock->key, pMemBlock->iv)) == NULL)
		goto out;
	if (!winpr_Cipher_Update(dec, pMemBlock->pData, pMemBlock->cbData, pPlainText, &cbOut))
		goto out;
	if (!winpr_Cipher_Final(dec, pPlainText + cbOut, &cbFinal))
		goto out;
	winpr_Cipher_Free(dec);

	CopyMemory(pMemBlock->pData, pPlainText, pMemBlock->cbData);
	SecureZeroMemory(pPlainText, pMemBlock->cbData);
	free(pPlainText);

	ListDictionary_Remove(g_ProtectedMemoryBlocks, pData);

	free(pMemBlock);

	return TRUE;

out:
	free(pPlainText);
	free(pMemBlock);
	winpr_Cipher_Free(dec);
	return FALSE;
}

BOOL CryptProtectData(DATA_BLOB* pDataIn, LPCWSTR szDataDescr, DATA_BLOB* pOptionalEntropy,
		PVOID pvReserved, CRYPTPROTECT_PROMPTSTRUCT* pPromptStruct, DWORD dwFlags, DATA_BLOB* pDataOut)
{
	return TRUE;
}

BOOL CryptUnprotectData(DATA_BLOB* pDataIn, LPWSTR* ppszDataDescr, DATA_BLOB* pOptionalEntropy,
		PVOID pvReserved, CRYPTPROTECT_PROMPTSTRUCT* pPromptStruct, DWORD dwFlags, DATA_BLOB* pDataOut)
{
	return TRUE;
}

BOOL CryptStringToBinaryW(LPCWSTR pszString, DWORD cchString, DWORD dwFlags, BYTE* pbBinary,
		DWORD* pcbBinary, DWORD* pdwSkip, DWORD* pdwFlags)
{
	return TRUE;
}

BOOL CryptStringToBinaryA(LPCSTR pszString, DWORD cchString, DWORD dwFlags, BYTE* pbBinary,
		DWORD* pcbBinary, DWORD* pdwSkip, DWORD* pdwFlags)
{
	return TRUE;
}

BOOL CryptBinaryToStringW(CONST BYTE* pbBinary, DWORD cbBinary, DWORD dwFlags, LPWSTR pszString, DWORD* pcchString)
{
	return TRUE;
}

BOOL CryptBinaryToStringA(CONST BYTE* pbBinary, DWORD cbBinary, DWORD dwFlags, LPSTR pszString, DWORD* pcchString)
{
	return TRUE;
}

#endif
