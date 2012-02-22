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
#define SEC_E_SECPKG_NOT_FOUND			0x80090305

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

SECURITY_STATUS EnumerateSecurityPackages(uint32* pcPackages, SEC_PKG_INFO** ppPackageInfo);
SECURITY_FUNCTION_TABLE* InitSecurityInterface(void);
SECURITY_STATUS QuerySecurityPackageInfo(char* pszPackageName, SEC_PKG_INFO** ppPackageInfo);

/* Credential Management */

SECURITY_STATUS AcquireCredentialsHandle(char* pszPrincipal, char* pszPackage,
		uint32 fCredentialUse, void* pvLogonID, void* pAuthData, void* pGetKeyFn,
		void* pvGetKeyArgument, CRED_HANDLE* phCredential, SEC_TIMESTAMP* ptsExpiry);

SECURITY_STATUS ExportSecurityContext(CTXT_HANDLE* phContext, uint32 fFlags, SEC_BUFFER* pPackedContext, void* pToken);
SECURITY_STATUS FreeCredentialsHandle(CRED_HANDLE* phCredential);
SECURITY_STATUS ImportSecurityContext(char* pszPackage, SEC_BUFFER* pPackedContext, void* pToken, CTXT_HANDLE* phContext);
SECURITY_STATUS QueryCredentialsAttributes(CRED_HANDLE* phCredential, uint32 ulAttribute, void* pBuffer);

/* Context Management */

SECURITY_STATUS AcceptSecurityContext(CRED_HANDLE* phCredential, CTXT_HANDLE* phContext,
		SEC_BUFFER_DESC* pInput, uint32 fContextReq, uint32 TargetDataRep, CTXT_HANDLE* phNewContext,
		SEC_BUFFER_DESC* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsTimeStamp);

SECURITY_STATUS ApplyControlToken(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pInput);
SECURITY_STATUS CompleteAuthToken(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pToken);
SECURITY_STATUS DeleteSecurityContext(CTXT_HANDLE* phContext);
SECURITY_STATUS FreeContextBuffer(void* pvContextBuffer);
SECURITY_STATUS ImpersonateSecurityContext(CTXT_HANDLE* phContext);

SECURITY_STATUS InitializeSecurityContext(CRED_HANDLE* phCredential, CTXT_HANDLE* phContext,
		char* pszTargetName, uint32 fContextReq, uint32 Reserved1, uint32 TargetDataRep,
		SEC_BUFFER_DESC* pInput, uint32 Reserved2, CTXT_HANDLE* phNewContext,
		SEC_BUFFER_DESC* pOutput, uint32* pfContextAttr, SEC_TIMESTAMP* ptsExpiry);

SECURITY_STATUS QueryContextAttributes(CTXT_HANDLE* phContext, uint32 ulAttribute, void* pBuffer);
SECURITY_STATUS QuerySecurityContextToken(CTXT_HANDLE* phContext, void* phToken);
SECURITY_STATUS SetContextAttributes(CTXT_HANDLE* phContext, uint32 ulAttribute, void* pBuffer, uint32 cbBuffer);
SECURITY_STATUS RevertSecurityContext(CTXT_HANDLE* phContext);

/* Message Support */

SECURITY_STATUS DecryptMessage(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo, uint32* pfQOP);
SECURITY_STATUS EncryptMessage(CTXT_HANDLE* phContext, uint32 fQOP, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo);
SECURITY_STATUS MakeSignature(CTXT_HANDLE* phContext, uint32 fQOP, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo);
SECURITY_STATUS VerifySignature(CTXT_HANDLE* phContext, SEC_BUFFER_DESC* pMessage, uint32 MessageSeqNo, uint32* pfQOP);

#endif /* FREERDP_AUTH_SSPI_H */
