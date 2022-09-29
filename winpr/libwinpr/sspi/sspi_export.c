/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Security Support Provider Interface (SSPI)
 *
 * Copyright 2012-2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifdef _WIN32
#define SEC_ENTRY __stdcall
#define SSPI_EXPORT __declspec(dllexport)
#else
#include <winpr/winpr.h>
#define SEC_ENTRY
#define SSPI_EXPORT WINPR_API
#endif

#ifdef _WIN32
typedef long LONG;
typedef unsigned long ULONG;
#endif
typedef LONG SECURITY_STATUS;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-prototypes"
#endif

#ifdef SSPI_DLL

/**
 * Standard SSPI API
 */

/* Package Management */

extern SECURITY_STATUS SEC_ENTRY sspi_EnumerateSecurityPackagesW(void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesW(void* pcPackages,
                                                                 void* ppPackageInfo)
{
	return sspi_EnumerateSecurityPackagesW(pcPackages, ppPackageInfo);
}

extern SECURITY_STATUS SEC_ENTRY sspi_EnumerateSecurityPackagesA(void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY EnumerateSecurityPackagesA(void* pcPackages,
                                                                 void* ppPackageInfo)
{
	return sspi_EnumerateSecurityPackagesA(pcPackages, ppPackageInfo);
}

extern void* SEC_ENTRY sspi_InitSecurityInterfaceW(void);

SSPI_EXPORT void* SEC_ENTRY InitSecurityInterfaceW(void)
{
	return sspi_InitSecurityInterfaceW();
}

extern void* SEC_ENTRY sspi_InitSecurityInterfaceA(void);

SSPI_EXPORT void* SEC_ENTRY InitSecurityInterfaceA(void)
{
	return sspi_InitSecurityInterfaceA();
}

extern SECURITY_STATUS SEC_ENTRY sspi_QuerySecurityPackageInfoW(void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY QuerySecurityPackageInfoW(void* pszPackageName,
                                                                void* ppPackageInfo)
{
	return sspi_QuerySecurityPackageInfoW(pszPackageName, ppPackageInfo);
}

extern SECURITY_STATUS SEC_ENTRY sspi_QuerySecurityPackageInfoA(void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY QuerySecurityPackageInfoA(void* pszPackageName,
                                                                void* ppPackageInfo)
{
	return sspi_QuerySecurityPackageInfoA(pszPackageName, ppPackageInfo);
}

/* Credential Management */

extern SECURITY_STATUS SEC_ENTRY sspi_AcquireCredentialsHandleW(void*, void*, ULONG, void*, void*,
                                                                void*, void*, void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleW(
    void* pszPrincipal, void* pszPackage, ULONG fCredentialUse, void* pvLogonID, void* pAuthData,
    void* pGetKeyFn, void* pvGetKeyArgument, void* phCredential, void* ptsExpiry)
{
	return sspi_AcquireCredentialsHandleW(pszPrincipal, pszPackage, fCredentialUse, pvLogonID,
	                                      pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential,
	                                      ptsExpiry);
}

extern SECURITY_STATUS SEC_ENTRY sspi_AcquireCredentialsHandleA(void*, void*, ULONG, void*, void*,
                                                                void*, void*, void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY AcquireCredentialsHandleA(
    void* pszPrincipal, void* pszPackage, ULONG fCredentialUse, void* pvLogonID, void* pAuthData,
    void* pGetKeyFn, void* pvGetKeyArgument, void* phCredential, void* ptsExpiry)
{
	return sspi_AcquireCredentialsHandleA(pszPrincipal, pszPackage, fCredentialUse, pvLogonID,
	                                      pAuthData, pGetKeyFn, pvGetKeyArgument, phCredential,
	                                      ptsExpiry);
}

extern SECURITY_STATUS SEC_ENTRY sspi_ExportSecurityContext(void*, ULONG, void*, void**);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY ExportSecurityContext(void* phContext, ULONG fFlags,
                                                            void* pPackedContext, void** pToken)
{
	return sspi_ExportSecurityContext(phContext, fFlags, pPackedContext, pToken);
}

extern SECURITY_STATUS SEC_ENTRY sspi_FreeCredentialsHandle(void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY FreeCredentialsHandle(void* phCredential)
{
	return sspi_FreeCredentialsHandle(phCredential);
}

extern SECURITY_STATUS SEC_ENTRY sspi_ImportSecurityContextW(void*, void*, void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY ImportSecurityContextW(void* pszPackage, void* pPackedContext,
                                                             void* pToken, void* phContext)
{
	return sspi_ImportSecurityContextW(pszPackage, pPackedContext, pToken, phContext);
}

extern SECURITY_STATUS SEC_ENTRY sspi_ImportSecurityContextA(void*, void*, void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY ImportSecurityContextA(void* pszPackage, void* pPackedContext,
                                                             void* pToken, void* phContext)
{
	return sspi_ImportSecurityContextA(pszPackage, pPackedContext, pToken, phContext);
}

extern SECURITY_STATUS SEC_ENTRY sspi_QueryCredentialsAttributesW(void*, ULONG, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesW(void* phCredential,
                                                                  ULONG ulAttribute, void* pBuffer)
{
	return sspi_QueryCredentialsAttributesW(phCredential, ulAttribute, pBuffer);
}

extern SECURITY_STATUS SEC_ENTRY sspi_QueryCredentialsAttributesA(void*, ULONG, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY QueryCredentialsAttributesA(void* phCredential,
                                                                  ULONG ulAttribute, void* pBuffer)
{
	return sspi_QueryCredentialsAttributesA(phCredential, ulAttribute, pBuffer);
}

/* Context Management */

extern SECURITY_STATUS SEC_ENTRY sspi_AcceptSecurityContext(void*, void*, void*, ULONG, ULONG,
                                                            void*, void*, void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY AcceptSecurityContext(void* phCredential, void* phContext,
                                                            void* pInput, ULONG fContextReq,
                                                            ULONG TargetDataRep, void* phNewContext,
                                                            void* pOutput, void* pfContextAttr,
                                                            void* ptsTimeStamp)
{
	return sspi_AcceptSecurityContext(phCredential, phContext, pInput, fContextReq, TargetDataRep,
	                                  phNewContext, pOutput, pfContextAttr, ptsTimeStamp);
}

extern SECURITY_STATUS SEC_ENTRY sspi_ApplyControlToken(void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY ApplyControlToken(void* phContext, void* pInput)
{
	return sspi_ApplyControlToken(phContext, pInput);
}

extern SECURITY_STATUS SEC_ENTRY sspi_CompleteAuthToken(void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY CompleteAuthToken(void* phContext, void* pToken)
{
	return sspi_CompleteAuthToken(phContext, pToken);
}

extern SECURITY_STATUS SEC_ENTRY sspi_DeleteSecurityContext(void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY DeleteSecurityContext(void* phContext)
{
	return sspi_DeleteSecurityContext(phContext);
}

extern SECURITY_STATUS SEC_ENTRY sspi_FreeContextBuffer(void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY FreeContextBuffer(void* pvContextBuffer)
{
	return sspi_FreeContextBuffer(pvContextBuffer);
}

extern SECURITY_STATUS SEC_ENTRY sspi_ImpersonateSecurityContext(void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY ImpersonateSecurityContext(void* phContext)
{
	return sspi_ImpersonateSecurityContext(phContext);
}

extern SECURITY_STATUS SEC_ENTRY sspi_InitializeSecurityContextW(void*, void*, void*, ULONG, ULONG,
                                                                 ULONG, void*, ULONG, void*, void*,
                                                                 void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY InitializeSecurityContextW(
    void* phCredential, void* phContext, void* pszTargetName, ULONG fContextReq, ULONG Reserved1,
    ULONG TargetDataRep, void* pInput, ULONG Reserved2, void* phNewContext, void* pOutput,
    void* pfContextAttr, void* ptsExpiry)
{
	return sspi_InitializeSecurityContextW(phCredential, phContext, pszTargetName, fContextReq,
	                                       Reserved1, TargetDataRep, pInput, Reserved2,
	                                       phNewContext, pOutput, pfContextAttr, ptsExpiry);
}

extern SECURITY_STATUS SEC_ENTRY sspi_InitializeSecurityContextA(void*, void*, void*, ULONG, ULONG,
                                                                 ULONG, void*, ULONG, void*, void*,
                                                                 void*, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY InitializeSecurityContextA(
    void* phCredential, void* phContext, void* pszTargetName, ULONG fContextReq, ULONG Reserved1,
    ULONG TargetDataRep, void* pInput, ULONG Reserved2, void* phNewContext, void* pOutput,
    void* pfContextAttr, void* ptsExpiry)
{
	return sspi_InitializeSecurityContextA(phCredential, phContext, pszTargetName, fContextReq,
	                                       Reserved1, TargetDataRep, pInput, Reserved2,
	                                       phNewContext, pOutput, pfContextAttr, ptsExpiry);
}

extern SECURITY_STATUS SEC_ENTRY sspi_QueryContextAttributesW(void*, ULONG, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY QueryContextAttributesW(void* phContext, ULONG ulAttribute,
                                                              void* pBuffer)
{
	return sspi_QueryContextAttributesW(phContext, ulAttribute, pBuffer);
}

extern SECURITY_STATUS SEC_ENTRY sspi_QueryContextAttributesA(void*, ULONG, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY QueryContextAttributesA(void* phContext, ULONG ulAttribute,
                                                              void* pBuffer)
{
	return sspi_QueryContextAttributesA(phContext, ulAttribute, pBuffer);
}

extern SECURITY_STATUS SEC_ENTRY sspi_QuerySecurityContextToken(void*, void**);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY QuerySecurityContextToken(void* phContext, void** phToken)
{
	return sspi_QuerySecurityContextToken(phContext, phToken);
}

extern SECURITY_STATUS SEC_ENTRY sspi_SetContextAttributesW(void*, ULONG, void*, ULONG);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY SetContextAttributesW(void* phContext, ULONG ulAttribute,
                                                            void* pBuffer, ULONG cbBuffer)
{
	return sspi_SetContextAttributesW(phContext, ulAttribute, pBuffer, cbBuffer);
}

extern SECURITY_STATUS SEC_ENTRY sspi_SetContextAttributesA(void*, ULONG, void*, ULONG);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY SetContextAttributesA(void* phContext, ULONG ulAttribute,
                                                            void* pBuffer, ULONG cbBuffer)
{
	return sspi_SetContextAttributesA(phContext, ulAttribute, pBuffer, cbBuffer);
}

extern SECURITY_STATUS SEC_ENTRY sspi_SetCredentialsAttributesW(void*, ULONG, void*, ULONG);

static SECURITY_STATUS SEC_ENTRY SetCredentialsAttributesW(void* phCredential, ULONG ulAttribute,
                                                           void* pBuffer, ULONG cbBuffer)
{
	return sspi_SetCredentialsAttributesW(phCredential, ulAttribute, pBuffer, cbBuffer);
}

extern SECURITY_STATUS SEC_ENTRY sspi_SetCredentialsAttributesA(void*, ULONG, void*, ULONG);

static SECURITY_STATUS SEC_ENTRY SetCredentialsAttributesA(void* phCredential, ULONG ulAttribute,
                                                           void* pBuffer, ULONG cbBuffer)
{
	return sspi_SetCredentialsAttributesA(phCredential, ulAttribute, pBuffer, cbBuffer);
}

extern SECURITY_STATUS SEC_ENTRY sspi_RevertSecurityContext(void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY RevertSecurityContext(void* phContext)
{
	return sspi_RevertSecurityContext(phContext);
}

/* Message Support */

extern SECURITY_STATUS SEC_ENTRY sspi_DecryptMessage(void*, void*, ULONG, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY DecryptMessage(void* phContext, void* pMessage,
                                                     ULONG MessageSeqNo, void* pfQOP)
{
	return sspi_DecryptMessage(phContext, pMessage, MessageSeqNo, pfQOP);
}

extern SECURITY_STATUS SEC_ENTRY sspi_EncryptMessage(void*, ULONG, void*, ULONG);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY EncryptMessage(void* phContext, ULONG fQOP, void* pMessage,
                                                     ULONG MessageSeqNo)
{
	return sspi_EncryptMessage(phContext, fQOP, pMessage, MessageSeqNo);
}

extern SECURITY_STATUS SEC_ENTRY sspi_MakeSignature(void*, ULONG, void*, ULONG);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY MakeSignature(void* phContext, ULONG fQOP, void* pMessage,
                                                    ULONG MessageSeqNo)
{
	return sspi_MakeSignature(phContext, fQOP, pMessage, MessageSeqNo);
}

extern SECURITY_STATUS SEC_ENTRY sspi_VerifySignature(void*, void*, ULONG, void*);

SSPI_EXPORT SECURITY_STATUS SEC_ENTRY VerifySignature(void* phContext, void* pMessage,
                                                      ULONG MessageSeqNo, void* pfQOP)
{
	return sspi_VerifySignature(phContext, pMessage, MessageSeqNo, pfQOP);
}

#endif /* SSPI_DLL */

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
