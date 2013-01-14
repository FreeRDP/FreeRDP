/**
 * WinPR: Windows Portable Runtime
 * Cryptography API (CryptoAPI)
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crypto.h>

/**
 * CertOpenStore
 * CertCloseStore
 * CertControlStore
 * CertDuplicateStore
 * CertSaveStore
 * CertRegisterPhysicalStore
 * CertRegisterSystemStore
 * CertAddStoreToCollection
 * CertRemoveStoreFromCollection
 * CertOpenSystemStoreA
 * CertOpenSystemStoreW
 * CertEnumPhysicalStore
 * CertEnumSystemStore
 * CertEnumSystemStoreLocation
 * CertSetStoreProperty
 * CertUnregisterPhysicalStore
 * CertUnregisterSystemStore
 *
 * CertAddCertificateContextToStore
 * CertAddCertificateLinkToStore
 * CertAddCRLContextToStore
 * CertAddCRLLinkToStore
 * CertAddCTLContextToStore
 * CertAddCTLLinkToStore
 * CertAddEncodedCertificateToStore
 * CertAddEncodedCertificateToSystemStoreA
 * CertAddEncodedCertificateToSystemStoreW
 * CertAddEncodedCRLToStore
 * CertAddEncodedCTLToStore
 * CertAddSerializedElementToStore
 * CertDeleteCertificateFromStore
 * CertDeleteCRLFromStore
 * CertDeleteCTLFromStore
 * CertGetCRLFromStore
 * CertEnumCertificatesInStore
 * CertEnumCRLsInStore
 * CertEnumCTLsInStore
 * CertFindCertificateInStore
 * CertFindChainInStore
 * CertFindCRLInStore
 * CertFindCTLInStore
 * CertGetIssuerCertificateFromStore
 * CertGetStoreProperty
 * CertGetSubjectCertificateFromStore
 * CertSerializeCertificateStoreElement
 * CertSerializeCRLStoreElement
 * CertSerializeCTLStoreElement
 *
 * CertAddEnhancedKeyUsageIdentifier
 * CertAddRefServerOcspResponse
 * CertAddRefServerOcspResponseContext
 * CertAlgIdToOID
 * CertCloseServerOcspResponse
 * CertCompareCertificate
 * CertCompareCertificateName
 * CertCompareIntegerBlob
 * CertComparePublicKeyInfo
 * CertCreateCertificateChainEngine
 * CertCreateCertificateContext
 * CertCreateContext
 * CertCreateCRLContext
 * CertCreateCTLContext
 * CertCreateCTLEntryFromCertificateContextProperties
 * CertCreateSelfSignCertificate
 * CertDuplicateCertificateChain
 * CertDuplicateCertificateContext
 * CertDuplicateCRLContext
 * CertDuplicateCTLContext
 * CertEnumCertificateContextProperties
 * CertEnumCRLContextProperties
 * CertEnumCTLContextProperties
 * CertEnumSubjectInSortedCTL
 * CertFindAttribute
 * CertFindCertificateInCRL
 * CertFindExtension
 * CertFindRDNAttr
 * CertFindSubjectInCTL
 * CertFindSubjectInSortedCTL
 * CertFreeCertificateChain
 * CertFreeCertificateChainEngine
 * CertFreeCertificateChainList
 * CertFreeCertificateContext
 * CertFreeCRLContext
 * CertFreeCTLContext
 * CertFreeServerOcspResponseContext
 * CertGetCertificateChain
 * CertGetCertificateContextProperty
 * CertGetCRLContextProperty
 * CertGetCTLContextProperty
 * CertGetEnhancedKeyUsage
 * CertGetIntendedKeyUsage
 * CertGetNameStringA
 * CertGetNameStringW
 * CertGetPublicKeyLength
 * CertGetServerOcspResponseContext
 * CertGetValidUsages
 * CertIsRDNAttrsInCertificateName
 * CertIsStrongHashToSign
 * CertIsValidCRLForCertificate
 * CertNameToStrA
 * CertNameToStrW
 * CertOIDToAlgId
 * CertOpenServerOcspResponse
 * CertRDNValueToStrA
 * CertRDNValueToStrW
 * CertRemoveEnhancedKeyUsageIdentifier
 * CertResyncCertificateChainEngine
 * CertRetrieveLogoOrBiometricInfo
 * CertSelectCertificateChains
 * CertSetCertificateContextPropertiesFromCTLEntry
 * CertSetCertificateContextProperty
 * CertSetCRLContextProperty
 * CertSetCTLContextProperty
 * CertSetEnhancedKeyUsage
 * CertStrToNameA
 * CertStrToNameW
 * CertVerifyCertificateChainPolicy
 * CertVerifyCRLRevocation
 * CertVerifyCRLTimeValidity
 * CertVerifyCTLUsage
 * CertVerifyRevocation
 * CertVerifySubjectCertificateContext
 * CertVerifyTimeValidity
 * CertVerifyValidityNesting
 */

#ifndef _WIN32

HCERTSTORE CertOpenSystemStoreW(HCRYPTPROV_LEGACY hProv, LPCWSTR szSubsystemProtocol)
{
	return (HCERTSTORE) 1;
}

HCERTSTORE CertOpenSystemStoreA(HCRYPTPROV_LEGACY hProv, LPCSTR szSubsystemProtocol)
{
	return CertOpenSystemStoreW(hProv, NULL);
}

BOOL CertCloseStore(HCERTSTORE hCertStore, DWORD dwFlags)
{
	return TRUE;
}

PCCERT_CONTEXT CertFindCertificateInStore(HCERTSTORE hCertStore, DWORD dwCertEncodingType,
		DWORD dwFindFlags, DWORD dwFindType, const void* pvFindPara, PCCERT_CONTEXT pPrevCertContext)
{
	return (PCCERT_CONTEXT) 1;
}

PCCERT_CONTEXT CertEnumCertificatesInStore(HCERTSTORE hCertStore, PCCERT_CONTEXT pPrevCertContext)
{
	return (PCCERT_CONTEXT) NULL;
}

DWORD CertGetNameStringW(PCCERT_CONTEXT pCertContext, DWORD dwType,
		DWORD dwFlags, void* pvTypePara, LPWSTR pszNameString, DWORD cchNameString)
{
	return 0;
}

DWORD CertGetNameStringA(PCCERT_CONTEXT pCertContext, DWORD dwType,
		DWORD dwFlags, void* pvTypePara, LPSTR pszNameString, DWORD cchNameString)
{
	return 0;
}

#endif

