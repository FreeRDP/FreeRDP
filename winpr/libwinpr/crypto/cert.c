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

#include <winpr/config.h>

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

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/wincrypt.h>

#ifndef _WIN32

#include "crypto.h"

HCERTSTORE CertOpenStore(LPCSTR lpszStoreProvider, DWORD dwMsgAndCertEncodingType,
                         WINPR_ATTR_UNUSED HCRYPTPROV_LEGACY hCryptProv,
                         WINPR_ATTR_UNUSED DWORD dwFlags, WINPR_ATTR_UNUSED const void* pvPara)
{
	WINPR_CERTSTORE* certstore = NULL;

	certstore = (WINPR_CERTSTORE*)calloc(1, sizeof(WINPR_CERTSTORE));

	if (certstore)
	{
		certstore->lpszStoreProvider = lpszStoreProvider;
		certstore->dwMsgAndCertEncodingType = dwMsgAndCertEncodingType;
	}

	return (HCERTSTORE)certstore;
}

HCERTSTORE CertOpenSystemStoreW(HCRYPTPROV_LEGACY hProv,
                                WINPR_ATTR_UNUSED LPCWSTR szSubsystemProtocol)
{
	HCERTSTORE hCertStore = NULL;

	hCertStore = CertOpenStore(CERT_STORE_PROV_FILE, X509_ASN_ENCODING, hProv, 0, NULL);

	return hCertStore;
}

HCERTSTORE CertOpenSystemStoreA(HCRYPTPROV_LEGACY hProv,
                                WINPR_ATTR_UNUSED LPCSTR szSubsystemProtocol)
{
	return CertOpenSystemStoreW(hProv, NULL);
}

BOOL CertCloseStore(HCERTSTORE hCertStore, WINPR_ATTR_UNUSED DWORD dwFlags)
{
	WINPR_CERTSTORE* certstore = NULL;

	certstore = (WINPR_CERTSTORE*)hCertStore;

	free(certstore);

	return TRUE;
}

PCCERT_CONTEXT CertFindCertificateInStore(WINPR_ATTR_UNUSED HCERTSTORE hCertStore,
                                          WINPR_ATTR_UNUSED DWORD dwCertEncodingType,
                                          WINPR_ATTR_UNUSED DWORD dwFindFlags,
                                          WINPR_ATTR_UNUSED DWORD dwFindType,
                                          WINPR_ATTR_UNUSED const void* pvFindPara,
                                          WINPR_ATTR_UNUSED PCCERT_CONTEXT pPrevCertContext)
{
	WLog_ERR("TODO", "TODO: Implement");
	return (PCCERT_CONTEXT)1;
}

PCCERT_CONTEXT CertEnumCertificatesInStore(WINPR_ATTR_UNUSED HCERTSTORE hCertStore,
                                           WINPR_ATTR_UNUSED PCCERT_CONTEXT pPrevCertContext)
{
	WLog_ERR("TODO", "TODO: Implement");
	return (PCCERT_CONTEXT)NULL;
}

DWORD CertGetNameStringW(WINPR_ATTR_UNUSED PCCERT_CONTEXT pCertContext,
                         WINPR_ATTR_UNUSED DWORD dwType, WINPR_ATTR_UNUSED DWORD dwFlags,
                         WINPR_ATTR_UNUSED void* pvTypePara, WINPR_ATTR_UNUSED LPWSTR pszNameString,
                         WINPR_ATTR_UNUSED DWORD cchNameString)
{
	WLog_ERR("TODO", "TODO: Implement");
	return 0;
}

DWORD CertGetNameStringA(WINPR_ATTR_UNUSED PCCERT_CONTEXT pCertContext,
                         WINPR_ATTR_UNUSED DWORD dwType, WINPR_ATTR_UNUSED DWORD dwFlags,
                         WINPR_ATTR_UNUSED void* pvTypePara, WINPR_ATTR_UNUSED LPSTR pszNameString,
                         WINPR_ATTR_UNUSED DWORD cchNameString)
{
	WLog_ERR("TODO", "TODO: Implement");
	return 0;
}

#endif
