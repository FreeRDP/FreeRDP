/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Security Support Provider Interface (SSPI)
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

#ifndef FREERDP_AUTH_SSPI_H
#define FREERDP_AUTH_SSPI_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#define SECPKG_ID_NONE				0xFFFF

#define SECPKG_FLAG_INTEGRITY			0x00000001
#define SECPKG_FLAG_PRIVACY			0x00000002
#define SECPKG_FLAG_TOKEN_ONLY			0x00000004
#define SECPKG_FLAG_DATAGRAM			0x00000008
#define SECPKG_FLAG_CONNECTION			0x00000010
#define SECPKG_FLAG_MULTI_REQUIRED		0x00000020
#define SECPKG_FLAG_CLIENT_ONLY			0x00000040
#define SECPKG_FLAG_EXTENDED_ERROR		0x00000080
#define SECPKG_FLAG_IMPERSONATION		0x00000100
#define SECPKG_FLAG_ACCEPT_WIN32_NAME		0x00000200
#define SECPKG_FLAG_STREAM			0x00000400
#define SECPKG_FLAG_NEGOTIABLE			0x00000800
#define SECPKG_FLAG_GSS_COMPATIBLE		0x00001000
#define SECPKG_FLAG_LOGON			0x00002000
#define SECPKG_FLAG_ASCII_BUFFERS		0x00004000
#define SECPKG_FLAG_FRAGMENT			0x00008000
#define SECPKG_FLAG_MUTUAL_AUTH			0x00010000
#define SECPKG_FLAG_DELEGATION			0x00020000
#define SECPKG_FLAG_READONLY_WITH_CHECKSUM	0x00040000
#define SECPKG_FLAG_RESTRICTED_TOKENS		0x00080000
#define SECPKG_FLAG_NEGO_EXTENDER		0x00100000
#define SECPKG_FLAG_NEGOTIABLE2			0x00200000

typedef uint32 SECURITY_STATUS;

#define SEC_E_OK				0x00000000
#define SEC_E_INSUFFICIENT_MEMORY		0x80090300
#define SEC_E_INVALID_HANDLE			0x80090301
#define SEC_E_UNSUPPORTED_FUNCTION		0x80090302
#define SEC_E_TARGET_UNKNOWN			0x80090303
#define SEC_E_INTERNAL_ERROR			0x80090304
#define SEC_E_SECPKG_NOT_FOUND			0x80090305
#define SEC_E_NOT_OWNER				0x80090306
#define SEC_E_CANNOT_INSTALL			0x80090307
#define SEC_E_INVALID_TOKEN			0x80090308
#define SEC_E_CANNOT_PACK			0x80090309
#define SEC_E_QOP_NOT_SUPPORTED			0x8009030A
#define SEC_E_NO_IMPERSONATION			0x8009030B
#define SEC_E_LOGON_DENIED			0x8009030C
#define SEC_E_UNKNOWN_CREDENTIALS		0x8009030D
#define SEC_E_NO_CREDENTIALS			0x8009030E
#define SEC_E_MESSAGE_ALTERED			0x8009030F
#define SEC_E_OUT_OF_SEQUENCE			0x80090310
#define SEC_E_NO_AUTHENTICATING_AUTHORITY	0x80090311
#define SEC_E_BAD_PKGID				0x80090316
#define SEC_E_CONTEXT_EXPIRED			0x80090317
#define SEC_E_INCOMPLETE_MESSAGE		0x80090318
#define SEC_E_INCOMPLETE_CREDENTIALS		0x80090320
#define SEC_E_BUFFER_TOO_SMALL			0x80090321
#define SEC_E_WRONG_PRINCIPAL			0x80090322
#define SEC_E_TIME_SKEW				0x80090324
#define SEC_E_UNTRUSTED_ROOT			0x80090325
#define SEC_E_ILLEGAL_MESSAGE			0x80090326
#define SEC_E_CERT_UNKNOWN			0x80090327
#define SEC_E_CERT_EXPIRED			0x80090328
#define SEC_E_ENCRYPT_FAILURE			0x80090329
#define SEC_E_DECRYPT_FAILURE			0x80090330
#define SEC_E_ALGORITHM_MISMATCH		0x80090331
#define SEC_E_SECURITY_QOS_FAILED		0x80090332
#define SEC_E_UNFINISHED_CONTEXT_DELETED	0x80090333
#define SEC_E_NO_TGT_REPLY			0x80090334
#define SEC_E_NO_IP_ADDRESSES			0x80090335
#define SEC_E_WRONG_CREDENTIAL_HANDLE		0x80090336
#define SEC_E_CRYPTO_SYSTEM_INVALID		0x80090337
#define SEC_E_MAX_REFERRALS_EXCEEDED		0x80090338
#define SEC_E_MUST_BE_KDC			0x80090339
#define SEC_E_STRONG_CRYPTO_NOT_SUPPORTED	0x8009033A
#define SEC_E_TOO_MANY_PRINCIPALS		0x8009033B
#define SEC_E_NO_PA_DATA			0x8009033C
#define SEC_E_PKINIT_NAME_MISMATCH		0x8009033D
#define SEC_E_SMARTCARD_LOGON_REQUIRED		0x8009033E
#define SEC_E_SHUTDOWN_IN_PROGRESS		0x8009033F
#define SEC_E_KDC_INVALID_REQUEST		0x80090340
#define SEC_E_KDC_UNABLE_TO_REFER		0x80090341
#define SEC_E_KDC_UNKNOWN_ETYPE			0x80090342
#define SEC_E_UNSUPPORTED_PREAUTH		0x80090343
#define SEC_E_DELEGATION_REQUIRED		0x80090345
#define SEC_E_BAD_BINDINGS			0x80090346
#define SEC_E_MULTIPLE_ACCOUNTS			0x80090347
#define SEC_E_NO_KERB_KEY			0x80090348
#define SEC_E_CERT_WRONG_USAGE			0x80090349
#define SEC_E_DOWNGRADE_DETECTED		0x80090350
#define SEC_E_SMARTCARD_CERT_REVOKED		0x80090351
#define SEC_E_ISSUING_CA_UNTRUSTED		0x80090352
#define SEC_E_REVOCATION_OFFLINE_C		0x80090353
#define SEC_E_PKINIT_CLIENT_FAILURE		0x80090354
#define SEC_E_SMARTCARD_CERT_EXPIRED		0x80090355
#define SEC_E_NO_S4U_PROT_SUPPORT		0x80090356
#define SEC_E_CROSSREALM_DELEGATION_FAILURE	0x80090357
#define SEC_E_REVOCATION_OFFLINE_KDC		0x80090358
#define SEC_E_ISSUING_CA_UNTRUSTED_KDC		0x80090359
#define SEC_E_KDC_CERT_EXPIRED			0x8009035A
#define SEC_E_KDC_CERT_REVOKED			0x8009035B
#define SEC_E_INVALID_PARAMETER			0x8009035D
#define SEC_E_DELEGATION_POLICY			0x8009035E
#define SEC_E_POLICY_NLTM_ONLY			0x8009035F
#define SEC_E_NO_CONTEXT			0x80090361
#define SEC_E_PKU2U_CERT_FAILURE		0x80090362
#define SEC_E_MUTUAL_AUTH_FAILED		0x80090363

#define SEC_I_CONTINUE_NEEDED			0x00090312
#define SEC_I_COMPLETE_NEEDED			0x00090313
#define SEC_I_COMPLETE_AND_CONTINUE		0x00090314
#define SEC_I_LOCAL_LOGON			0x00090315
#define SEC_I_CONTEXT_EXPIRED			0x00090317
#define SEC_I_INCOMPLETE_CREDENTIALS		0x00090320
#define SEC_I_RENEGOTIATE			0x00090321
#define SEC_I_NO_LSA_CONTEXT			0x00090323
#define SEC_I_SIGNATURE_NEEDED			0x0009035C
#define SEC_I_NO_RENEGOTIATION			0x00090360

#define SECPKG_CRED_INBOUND			0x00000001
#define SECPKG_CRED_OUTBOUND			0x00000002
#define SECPKG_CRED_BOTH			0x00000003
#define SECPKG_CRED_AUTOLOGON_RESTRICTED	0x00000010
#define SECPKG_CRED_PROCESS_POLICY_ONLY		0x00000020

struct _SEC_PKG_INFO
{
	uint32 fCapabilities;
	uint16 wVersion;
	uint16 wRPCID;
	uint32 cbMaxToken;
	char* Name;
	char* Comment;
};
typedef struct _SEC_PKG_INFO SEC_PKG_INFO;

#define SEC_AUTH_IDENTITY_ANSI			0x1
#define SEC_AUTH_IDENTITY_UNICODE		0x2

struct _SEC_AUTH_IDENTITY
{
	uint16* User;
	uint32 UserLength;
	uint16* Domain;
	uint32 DomainLength;
	uint16* Password;
	uint32 PasswordLength;
	uint32 Flags;
};
typedef struct _SEC_AUTH_IDENTITY SEC_AUTH_IDENTITY;

struct _SEC_HANDLE
{
	uint32* dwLower;
	uint32* dwUpper;
};
typedef struct _SEC_HANDLE SEC_HANDLE;

typedef SEC_HANDLE CRED_HANDLE;
typedef SEC_HANDLE CTXT_HANDLE;

struct _SEC_INTEGER
{
	uint32 LowPart;
	sint32 HighPart;
};
typedef struct _SEC_INTEGER SEC_INTEGER;

typedef SEC_INTEGER SEC_TIMESTAMP;

struct _SEC_BUFFER
{
	uint32 cbBuffer;
	uint32 BufferType;
	void* pvBuffer;
};
typedef struct _SEC_BUFFER SEC_BUFFER;

struct _SEC_BUFFER_DESC
{
	uint32 ulVersion;
	uint32 cBuffers;
	SEC_BUFFER* pBuffers;
};
typedef struct _SEC_BUFFER_DESC SEC_BUFFER_DESC;

typedef SECURITY_STATUS (*ENUMERATE_SECURITY_PACKAGES_FN)(uint32* pcPackages, SEC_PKG_INFO** ppPackageInfo);

typedef SECURITY_STATUS (*QUERY_CREDENTIAL_ATTRIBUTES_FN)(CRED_HANDLE* phCredential, uint32 ulAttribute, void* pBuffer);

typedef SECURITY_STATUS (*ACQUIRE_CREDENTIALS_HANDLE_FN)(char* pszPrincipal, char* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, CRED_HANDLE* phCredential, SEC_TIMESTAMP* ptsExpiry);

typedef SECURITY_STATUS (*FREE_CREDENTIALS_HANDLE_FN)(CRED_HANDLE* phCredential);

typedef SECURITY_STATUS (*INITIALIZE_SECURITY_CONTEXT_FN)(CRED_HANDLE* phCredential, CTXT_HANDLE* phContext,
		char* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		SEC_BUFFER_DESC* pInput, uint32 Reserved2, CTXT_HANDLE* phNewContext,
		SEC_BUFFER_DESC* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsExpiry);

typedef SECURITY_STATUS (*ACCEPT_SECURITY_CONTEXT_FN)(CRED_HANDLE* phCredential, CTXT_HANDLE* phContext,
		SEC_BUFFER_DESC* pInput, uint32 fContextReq, uint32 TargetDataRep, CTXT_HANDLE* phNewContext,
		SEC_BUFFER_DESC* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsTimeStamp);

typedef SECURITY_STATUS (*COMPLETE_AUTH_TOKEN_FN)(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pToken);

typedef SECURITY_STATUS (*DELETE_SECURITY_CONTEXT_FN)(CTXT_HANDLE* phContext);

typedef SECURITY_STATUS (*APPLY_CONTROL_TOKEN_FN)(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pInput);

typedef SECURITY_STATUS (*QUERY_CONTEXT_ATTRIBUTES_FN)(CTXT_HANDLE* phContext, uint32 ulAttribute, void* pBuffer);

typedef SECURITY_STATUS (*IMPERSONATE_SECURITY_CONTEXT_FN)(CTXT_HANDLE* phContext);

typedef SECURITY_STATUS (*REVERT_SECURITY_CONTEXT_FN)(CTXT_HANDLE* phContext);

typedef SECURITY_STATUS (*MAKE_SIGNATURE_FN)(CTXT_HANDLE* phContext, uint32 fQOP, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo);

typedef SECURITY_STATUS (*VERIFY_SIGNATURE_FN)(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo, uint32* pfQOP);

typedef SECURITY_STATUS (*FREE_CONTEXT_BUFFER_FN)(void* pvContextBuffer);

typedef SECURITY_STATUS (*QUERY_SECURITY_PACKAGE_INFO_FN)(char* pszPackageName, SEC_PKG_INFO** ppPackageInfo);

typedef SECURITY_STATUS (*EXPORT_SECURITY_CONTEXT_FN)(CTXT_HANDLE* phContext, uint32 fFlags, SEC_BUFFER* pPackedContext, void* pToken);

typedef SECURITY_STATUS (*IMPORT_SECURITY_CONTEXT_FN)(char* pszPackage, SEC_BUFFER* pPackedContext, void* pToken, CTXT_HANDLE* phContext);

typedef SECURITY_STATUS (*ADD_CREDENTIALS_FN)(void);

typedef SECURITY_STATUS (*QUERY_SECURITY_CONTEXT_TOKEN_FN)(CTXT_HANDLE* phContext, void* phToken);

typedef SECURITY_STATUS (*ENCRYPT_MESSAGE_FN)(CTXT_HANDLE* phContext, uint32 fQOP, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo);

typedef SECURITY_STATUS (*DECRYPT_MESSAGE_FN)(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo, uint32* pfQOP);

typedef SECURITY_STATUS (*SET_CONTEXT_ATTRIBUTES_FN)(CTXT_HANDLE* phContext, uint32 ulAttribute, void* pBuffer, uint32 cbBuffer);

struct _SECURITY_FUNCTION_TABLE
{
	uint32 dwVersion;
	ENUMERATE_SECURITY_PACKAGES_FN EnumerateSecurityPackages;
	void* Reserved1;
	QUERY_CREDENTIAL_ATTRIBUTES_FN QueryCredentialsAttributes;
	ACQUIRE_CREDENTIALS_HANDLE_FN AcquireCredentialsHandle;
	FREE_CREDENTIALS_HANDLE_FN FreeCredentialsHandle;
	void* Reserved2;
	INITIALIZE_SECURITY_CONTEXT_FN InitializeSecurityContext;
	ACCEPT_SECURITY_CONTEXT_FN AcceptSecurityContext;
	COMPLETE_AUTH_TOKEN_FN CompleteAuthToken;
	DELETE_SECURITY_CONTEXT_FN DeleteSecurityContext;
	APPLY_CONTROL_TOKEN_FN ApplyControlToken;
	QUERY_CONTEXT_ATTRIBUTES_FN QueryContextAttributes;
	IMPERSONATE_SECURITY_CONTEXT_FN ImpersonateSecurityContext;
	REVERT_SECURITY_CONTEXT_FN RevertSecurityContext;
	MAKE_SIGNATURE_FN MakeSignature;
	VERIFY_SIGNATURE_FN VerifySignature;
	FREE_CONTEXT_BUFFER_FN FreeContextBuffer;
	QUERY_SECURITY_PACKAGE_INFO_FN QuerySecurityPackageInfo;
	void* Reserved3;
	void* Reserved4;
	EXPORT_SECURITY_CONTEXT_FN ExportSecurityContext;
	IMPORT_SECURITY_CONTEXT_FN ImportSecurityContext;
	ADD_CREDENTIALS_FN AddCredentials;
	void* Reserved8;
	QUERY_SECURITY_CONTEXT_TOKEN_FN QuerySecurityContextToken;
	ENCRYPT_MESSAGE_FN EncryptMessage;
	DECRYPT_MESSAGE_FN DecryptMessage;
	SET_CONTEXT_ATTRIBUTES_FN SetContextAttributes;
};
typedef struct _SECURITY_FUNCTION_TABLE SECURITY_FUNCTION_TABLE;

/* Package Management */

FREERDP_API SECURITY_STATUS EnumerateSecurityPackages(uint32* pcPackages, SEC_PKG_INFO** ppPackageInfo);
FREERDP_API SECURITY_FUNCTION_TABLE* InitSecurityInterface(void);
FREERDP_API SECURITY_STATUS QuerySecurityPackageInfo(char* pszPackageName, SEC_PKG_INFO** ppPackageInfo);

/* Credential Management */

FREERDP_API SECURITY_STATUS AcquireCredentialsHandle(char* pszPrincipal, char* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, CRED_HANDLE* phCredential, SEC_TIMESTAMP* ptsExpiry);

FREERDP_API SECURITY_STATUS ExportSecurityContext(CTXT_HANDLE* phContext, uint32 fFlags, SEC_BUFFER* pPackedContext, void* pToken);
FREERDP_API SECURITY_STATUS FreeCredentialsHandle(CRED_HANDLE* phCredential);
FREERDP_API SECURITY_STATUS ImportSecurityContext(char* pszPackage, SEC_BUFFER* pPackedContext, void* pToken, CTXT_HANDLE* phContext);
FREERDP_API SECURITY_STATUS QueryCredentialsAttributes(CRED_HANDLE* phCredential, uint32 ulAttribute, void* pBuffer);

/* Context Management */

FREERDP_API SECURITY_STATUS AcceptSecurityContext(CRED_HANDLE* phCredential, CTXT_HANDLE* phContext,
		SEC_BUFFER_DESC* pInput, uint32 fContextReq, uint32 TargetDataRep, CTXT_HANDLE* phNewContext,
		SEC_BUFFER_DESC* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsTimeStamp);

FREERDP_API SECURITY_STATUS ApplyControlToken(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pInput);
FREERDP_API SECURITY_STATUS CompleteAuthToken(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pToken);
FREERDP_API SECURITY_STATUS DeleteSecurityContext(CTXT_HANDLE* phContext);
FREERDP_API SECURITY_STATUS FreeContextBuffer(void* pvContextBuffer);
FREERDP_API SECURITY_STATUS ImpersonateSecurityContext(CTXT_HANDLE* phContext);

FREERDP_API SECURITY_STATUS InitializeSecurityContext(CRED_HANDLE* phCredential, CTXT_HANDLE* phContext,
		char* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		SEC_BUFFER_DESC* pInput, uint32 Reserved2, CTXT_HANDLE* phNewContext,
		SEC_BUFFER_DESC* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsExpiry);

FREERDP_API SECURITY_STATUS QueryContextAttributes(CTXT_HANDLE* phContext, uint32 ulAttribute, void* pBuffer);
FREERDP_API SECURITY_STATUS QuerySecurityContextToken(CTXT_HANDLE* phContext, void* phToken);
FREERDP_API SECURITY_STATUS SetContextAttributes(CTXT_HANDLE* phContext, uint32 ulAttribute, void* pBuffer, uint32 cbBuffer);
FREERDP_API SECURITY_STATUS RevertSecurityContext(CTXT_HANDLE* phContext);

/* Message Support */

FREERDP_API SECURITY_STATUS DecryptMessage(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo, uint32* pfQOP);
FREERDP_API SECURITY_STATUS EncryptMessage(CTXT_HANDLE* phContext, uint32 fQOP, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo);
FREERDP_API SECURITY_STATUS MakeSignature(CTXT_HANDLE* phContext, uint32 fQOP, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo);
FREERDP_API SECURITY_STATUS VerifySignature(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo, uint32* pfQOP);

#endif /* FREERDP_AUTH_SSPI_H */
