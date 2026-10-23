/**
 * WinPR: Windows Portable Runtime
 * TSSSP Security Package (Remote Credential Guard)
 *
 * The TSSSP package drives the CredSSP exchange for Remote Credential Guard.
 * On Windows it forwards every call to the native secur32 "TSSSP" provider so
 * that core/tsssp.c has one code path for all platforms. On other platforms it
 * proxies to the per-user lsa4posix daemon, which runs the CredSSP exchange and
 * services the redirected [MS-RDPEAR] calls on the client's behalf.
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
#include <winpr/platform.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/library.h>
#include <winpr/sspi.h>

#include "tsssp.h"

#include "../sspi.h"
#include "../../log.h"

#define TAG WINPR_TAG("sspi.TSSSP")

#if defined(_WIN32) && !defined(_UWP) && defined(WITH_NATIVE_SSPI)
#define TSSSP_FORWARD_NATIVE 1
#endif

#if defined(TSSSP_FORWARD_NATIVE)

/* A WinPR credential/context handle wraps the native secur32 handle. */
typedef struct
{
	CredHandle native;
} TSSSP_CREDENTIALS;

typedef struct
{
	CtxtHandle native;
} TSSSP_CONTEXT;

static INIT_ONCE g_NativeOnce = INIT_ONCE_STATIC_INIT;
static HMODULE g_NativeModule = nullptr;
static PSecurityFunctionTableW g_NativeW = nullptr;
static PSecurityFunctionTableA g_NativeA = nullptr;

static BOOL CALLBACK tsssp_native_init(WINPR_ATTR_UNUSED PINIT_ONCE once,
                                       WINPR_ATTR_UNUSED PVOID param, WINPR_ATTR_UNUSED PVOID* ctx)
{
	g_NativeModule = LoadLibraryW(L"secur32.dll");
	if (!g_NativeModule)
		g_NativeModule = LoadLibraryW(L"sspicli.dll");
	if (!g_NativeModule)
	{
		WLog_ERR(TAG, "failed to load the native SSPI module");
		return TRUE;
	}

	INIT_SECURITY_INTERFACE_W pInitW =
	    GetProcAddressAs(g_NativeModule, "InitSecurityInterfaceW", INIT_SECURITY_INTERFACE_W);
	INIT_SECURITY_INTERFACE_A pInitA =
	    GetProcAddressAs(g_NativeModule, "InitSecurityInterfaceA", INIT_SECURITY_INTERFACE_A);

	if (pInitW)
		g_NativeW = pInitW();
	if (pInitA)
		g_NativeA = pInitA();

	return TRUE;
}

static PSecurityFunctionTableW tsssp_native_W(void)
{
	(void)InitOnceExecuteOnce(&g_NativeOnce, tsssp_native_init, nullptr, nullptr);
	return g_NativeW;
}

static PSecurityFunctionTableA tsssp_native_A(void)
{
	(void)InitOnceExecuteOnce(&g_NativeOnce, tsssp_native_init, nullptr, nullptr);
	return g_NativeA;
}

/* Replace any output SECBUFFER_TOKEN that secur32 allocated (ISC_REQ_ALLOCATE_MEMORY)
 * with a copy owned by the WinPR context-buffer allocator, so the caller can free it
 * through the WinPR FreeContextBuffer. */
static SECURITY_STATUS tsssp_reown_output(PSecurityFunctionTableW nat, PSecBufferDesc pOutput)
{
	if (!pOutput || !pOutput->pBuffers)
		return SEC_E_OK;

	for (ULONG i = 0; i < pOutput->cBuffers; i++)
	{
		SecBuffer* b = &pOutput->pBuffers[i];
		if ((b->BufferType != SECBUFFER_TOKEN) || !b->pvBuffer || (b->cbBuffer == 0))
			continue;

		void* copy = sspi_ContextBufferAlloc(InitializeSecurityContextIndex, b->cbBuffer);
		if (!copy)
			return SEC_E_INSUFFICIENT_MEMORY;

		CopyMemory(copy, b->pvBuffer, b->cbBuffer);
		if (nat && nat->FreeContextBuffer)
			(void)nat->FreeContextBuffer(b->pvBuffer);
		b->pvBuffer = copy;
	}

	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY tsssp_AcquireCredentialsHandleW(
    SEC_WCHAR* pszPrincipal, SEC_WCHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	PSecurityFunctionTableW nat = tsssp_native_W();
	if (!nat || !nat->AcquireCredentialsHandleW)
		return SEC_E_SECPKG_NOT_FOUND;

	TSSSP_CREDENTIALS* creds = (TSSSP_CREDENTIALS*)calloc(1, sizeof(TSSSP_CREDENTIALS));
	if (!creds)
		return SEC_E_INSUFFICIENT_MEMORY;

	const SECURITY_STATUS status = nat->AcquireCredentialsHandleW(
	    pszPrincipal, pszPackage, fCredentialUse, pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument,
	    &creds->native, ptsExpiry);
	if (status != SEC_E_OK)
	{
		free(creds);
		return status;
	}

	sspi_SecureHandleSetLowerPointer(phCredential, creds);
	sspi_SecureHandleSetPackageId(phCredential, SSPI_PACKAGE_TSSSP);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY tsssp_AcquireCredentialsHandleA(
    SEC_CHAR* pszPrincipal, SEC_CHAR* pszPackage, ULONG fCredentialUse, void* pvLogonID,
    void* pAuthData, SEC_GET_KEY_FN pGetKeyFn, void* pvGetKeyArgument, PCredHandle phCredential,
    PTimeStamp ptsExpiry)
{
	PSecurityFunctionTableA nat = tsssp_native_A();
	if (!nat || !nat->AcquireCredentialsHandleA)
		return SEC_E_SECPKG_NOT_FOUND;

	TSSSP_CREDENTIALS* creds = (TSSSP_CREDENTIALS*)calloc(1, sizeof(TSSSP_CREDENTIALS));
	if (!creds)
		return SEC_E_INSUFFICIENT_MEMORY;

	const SECURITY_STATUS status = nat->AcquireCredentialsHandleA(
	    pszPrincipal, pszPackage, fCredentialUse, pvLogonID, pAuthData, pGetKeyFn, pvGetKeyArgument,
	    &creds->native, ptsExpiry);
	if (status != SEC_E_OK)
	{
		free(creds);
		return status;
	}

	sspi_SecureHandleSetLowerPointer(phCredential, creds);
	sspi_SecureHandleSetPackageId(phCredential, SSPI_PACKAGE_TSSSP);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY tsssp_FreeCredentialsHandle(PCredHandle phCredential)
{
	if (!phCredential)
		return SEC_E_INVALID_HANDLE;

	TSSSP_CREDENTIALS* creds = (TSSSP_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);
	sspi_SecureHandleInvalidate(phCredential);
	if (!creds)
		return SEC_E_INVALID_HANDLE;

	PSecurityFunctionTableW nat = tsssp_native_W();
	SECURITY_STATUS status = SEC_E_OK;
	if (nat && nat->FreeCredentialsHandle)
		status = nat->FreeCredentialsHandle(&creds->native);

	free(creds);
	return status;
}

static SECURITY_STATUS tsssp_initialize_context(
    BOOL unicode, PCredHandle phCredential, PCtxtHandle phContext, void* pszTargetName,
    ULONG fContextReq, ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	PSecurityFunctionTableW natW = tsssp_native_W();
	PSecurityFunctionTableA natA = tsssp_native_A();
	if (unicode && (!natW || !natW->InitializeSecurityContextW))
		return SEC_E_SECPKG_NOT_FOUND;
	if (!unicode && (!natA || !natA->InitializeSecurityContextA))
		return SEC_E_SECPKG_NOT_FOUND;

	TSSSP_CREDENTIALS* creds = (TSSSP_CREDENTIALS*)sspi_SecureHandleGetLowerPointer(phCredential);
	if (!creds)
		return SEC_E_INVALID_HANDLE;

	TSSSP_CONTEXT* context =
	    phContext ? (TSSSP_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext) : nullptr;
	const BOOL fresh = (context == nullptr);
	if (fresh)
	{
		context = (TSSSP_CONTEXT*)calloc(1, sizeof(TSSSP_CONTEXT));
		if (!context)
			return SEC_E_INSUFFICIENT_MEMORY;
	}

	PCtxtHandle inNative = fresh ? nullptr : &context->native;

	SECURITY_STATUS status = 0;
	if (unicode)
		status = natW->InitializeSecurityContextW(
		    &creds->native, inNative, (SEC_WCHAR*)pszTargetName, fContextReq, Reserved1,
		    TargetDataRep, pInput, Reserved2, &context->native, pOutput, pfContextAttr, ptsExpiry);
	else
		status = natA->InitializeSecurityContextA(
		    &creds->native, inNative, (SEC_CHAR*)pszTargetName, fContextReq, Reserved1,
		    TargetDataRep, pInput, Reserved2, &context->native, pOutput, pfContextAttr, ptsExpiry);

	if ((status != SEC_E_OK) && (status != SEC_I_CONTINUE_NEEDED) &&
	    (status != SEC_I_COMPLETE_NEEDED) && (status != SEC_I_COMPLETE_AND_CONTINUE))
	{
		if (fresh)
			free(context);
		return status;
	}

	const SECURITY_STATUS reown = tsssp_reown_output(natW, pOutput);
	if (reown != SEC_E_OK)
	{
		if (fresh)
		{
			if (natW && natW->DeleteSecurityContext)
				(void)natW->DeleteSecurityContext(&context->native);
			free(context);
		}
		return reown;
	}

	sspi_SecureHandleSetLowerPointer(phNewContext, context);
	sspi_SecureHandleSetPackageId(phNewContext, SSPI_PACKAGE_TSSSP);
	return status;
}

static SECURITY_STATUS SEC_ENTRY tsssp_InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	return tsssp_initialize_context(TRUE, phCredential, phContext, pszTargetName, fContextReq,
	                                Reserved1, TargetDataRep, pInput, Reserved2, phNewContext,
	                                pOutput, pfContextAttr, ptsExpiry);
}

static SECURITY_STATUS SEC_ENTRY tsssp_InitializeSecurityContextA(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_CHAR* pszTargetName, ULONG fContextReq,
    ULONG Reserved1, ULONG TargetDataRep, PSecBufferDesc pInput, ULONG Reserved2,
    PCtxtHandle phNewContext, PSecBufferDesc pOutput, PULONG pfContextAttr, PTimeStamp ptsExpiry)
{
	return tsssp_initialize_context(FALSE, phCredential, phContext, pszTargetName, fContextReq,
	                                Reserved1, TargetDataRep, pInput, Reserved2, phNewContext,
	                                pOutput, pfContextAttr, ptsExpiry);
}

static SECURITY_STATUS tsssp_query_context(BOOL unicode, PCtxtHandle phContext, ULONG ulAttribute,
                                           void* pBuffer)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	TSSSP_CONTEXT* context = (TSSSP_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (ulAttribute == SECPKG_ATTR_WINPR_TSPKG_CONTEXT)
	{
		if (!pBuffer)
			return SEC_E_INSUFFICIENT_MEMORY;

		/* The native TSSSP keeps the TsPkgContext handle in the context's upper word. */
		*((UINT64*)pBuffer) = (UINT64)context->native.dwUpper;
		return SEC_E_OK;
	}

	PSecurityFunctionTableW natW = tsssp_native_W();
	PSecurityFunctionTableA natA = tsssp_native_A();
	if (unicode && natW && natW->QueryContextAttributesW)
		return natW->QueryContextAttributesW(&context->native, ulAttribute, pBuffer);
	if (!unicode && natA && natA->QueryContextAttributesA)
		return natA->QueryContextAttributesA(&context->native, ulAttribute, pBuffer);
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY tsssp_QueryContextAttributesW(PCtxtHandle phContext,
                                                               ULONG ulAttribute, void* pBuffer)
{
	return tsssp_query_context(TRUE, phContext, ulAttribute, pBuffer);
}

static SECURITY_STATUS SEC_ENTRY tsssp_QueryContextAttributesA(PCtxtHandle phContext,
                                                               ULONG ulAttribute, void* pBuffer)
{
	return tsssp_query_context(FALSE, phContext, ulAttribute, pBuffer);
}

static SECURITY_STATUS SEC_ENTRY tsssp_DeleteSecurityContext(PCtxtHandle phContext)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;

	TSSSP_CONTEXT* context = (TSSSP_CONTEXT*)sspi_SecureHandleGetLowerPointer(phContext);
	sspi_SecureHandleInvalidate(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	PSecurityFunctionTableW nat = tsssp_native_W();
	SECURITY_STATUS status = SEC_E_OK;
	if (nat && nat->DeleteSecurityContext)
		status = nat->DeleteSecurityContext(&context->native);

	free(context);
	return status;
}

#else /* !TSSSP_FORWARD_NATIVE: proxy to the lsa4posix daemon */

#include <winpr/credssp.h>

#include "../../sspicli/lsa_proto.h"
#include "../../sspicli/lsa_transport.h"

/* A credential owns the daemon connection; the context rides the same socket. */
typedef struct
{
	int fd;
	UINT64 credId;
	UINT32 reqId;
} TSSSP_PROXY_CRED;

typedef struct
{
	int fd;
	UINT64 ctxId;
	UINT32 reqId;
} TSSSP_PROXY_CTX;

static SECURITY_STATUS SEC_ENTRY tsssp_AcquireCredentialsHandleW(
    WINPR_ATTR_UNUSED SEC_WCHAR* pszPrincipal, WINPR_ATTR_UNUSED SEC_WCHAR* pszPackage,
    ULONG fCredentialUse, WINPR_ATTR_UNUSED void* pvLogonID, void* pAuthData,
    WINPR_ATTR_UNUSED SEC_GET_KEY_FN pGetKeyFn, WINPR_ATTR_UNUSED void* pvGetKeyArgument,
    PCredHandle phCredential, WINPR_ATTR_UNUSED PTimeStamp ptsExpiry)
{
	if (fCredentialUse != SECPKG_CRED_OUTBOUND)
		return SEC_E_UNSUPPORTED_FUNCTION;

	const int fd = lsa4posix_client_connect();
	if (fd < 0)
	{
		WLog_ERR(TAG, "the lsa4posix daemon is not available");
		return SEC_E_NO_CREDENTIALS;
	}

	DWORD flags = 0;
	SEC_WINNT_AUTH_IDENTITY_W* identity = nullptr;
	if (pAuthData)
	{
		CREDSSP_CRED_EX* credEx = (CREDSSP_CRED_EX*)pAuthData;
		flags = credEx->Flags;
		identity = (SEC_WINNT_AUTH_IDENTITY_W*)credEx->Cred.pSpnegoCred;
	}

	const UINT32 reqId = 1;
	LSA_MSG_HDR hdr = { SSPI_MSG_ACQUIRE_CRED, 0, reqId };
	LsaProtoWriter w = { 0 };
	wStream* request = nullptr;
	BOOL built = lsa_proto_writer_init(&w, &hdr) && lsa_proto_write_u32(&w, flags) &&
	             lsa_proto_write_u16(&w, identity ? 1 : 0);
	if (built && identity)
		built = lsa_proto_write_blob(&w, (const BYTE*)identity->User,
		                             identity->UserLength * sizeof(WCHAR)) &&
		        lsa_proto_write_blob(&w, (const BYTE*)identity->Domain,
		                             identity->DomainLength * sizeof(WCHAR)) &&
		        lsa_proto_write_blob(&w, (const BYTE*)identity->Password,
		                             identity->PasswordLength * sizeof(WCHAR));
	if (!built || !lsa_proto_writer_finish(&w, &request))
	{
		lsa_proto_writer_abort(&w);
		lsa4posix_client_close(fd);
		return SEC_E_INTERNAL_ERROR;
	}

	LsaProtoReader r = { 0 };
	LSA_MSG_HDR rh = { 0 };
	wStream* response = nullptr;
	if (lsa4posix_transact(fd, SSPI_MSG_ACQUIRE_CRED, request, reqId, &r, &rh, &response) !=
	    STATUS_SUCCESS)
	{
		lsa4posix_client_close(fd);
		return SEC_E_NO_CREDENTIALS;
	}

	UINT32 status = 0;
	UINT64 credId = 0;
	const BOOL ok = lsa_proto_read_u32(&r, &status) && lsa_proto_read_u64(&r, &credId);
	lsa_proto_reader_done(&r);
	Stream_Free(response, TRUE);
	if (!ok || (status != SEC_E_OK))
	{
		lsa4posix_client_close(fd);
		return ok ? (SECURITY_STATUS)status : SEC_E_INTERNAL_ERROR;
	}

	TSSSP_PROXY_CRED* creds = (TSSSP_PROXY_CRED*)calloc(1, sizeof(TSSSP_PROXY_CRED));
	if (!creds)
	{
		lsa4posix_client_close(fd);
		return SEC_E_INSUFFICIENT_MEMORY;
	}
	creds->fd = fd;
	creds->credId = credId;
	creds->reqId = 2;
	sspi_SecureHandleSetLowerPointer(phCredential, creds);
	sspi_SecureHandleSetPackageId(phCredential, SSPI_PACKAGE_TSSSP);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY tsssp_AcquireCredentialsHandleA(
    WINPR_ATTR_UNUSED SEC_CHAR* pszPrincipal, WINPR_ATTR_UNUSED SEC_CHAR* pszPackage,
    WINPR_ATTR_UNUSED ULONG fCredentialUse, WINPR_ATTR_UNUSED void* pvLogonID,
    WINPR_ATTR_UNUSED void* pAuthData, WINPR_ATTR_UNUSED SEC_GET_KEY_FN pGetKeyFn,
    WINPR_ATTR_UNUSED void* pvGetKeyArgument, WINPR_ATTR_UNUSED PCredHandle phCredential,
    WINPR_ATTR_UNUSED PTimeStamp ptsExpiry)
{
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY tsssp_FreeCredentialsHandle(PCredHandle phCredential)
{
	if (!phCredential)
		return SEC_E_INVALID_HANDLE;
	TSSSP_PROXY_CRED* creds = (TSSSP_PROXY_CRED*)sspi_SecureHandleGetLowerPointer(phCredential);
	sspi_SecureHandleInvalidate(phCredential);
	if (!creds)
		return SEC_E_INVALID_HANDLE;

	LSA_MSG_HDR hdr = { SSPI_MSG_FREE_CRED, 0, creds->reqId++ };
	LsaProtoWriter w = { 0 };
	wStream* request = nullptr;
	if (lsa_proto_writer_init(&w, &hdr) && lsa_proto_write_u64(&w, creds->credId) &&
	    lsa_proto_writer_finish(&w, &request))
	{
		LsaProtoReader r = { 0 };
		LSA_MSG_HDR rh = { 0 };
		wStream* response = nullptr;
		if (lsa4posix_transact(creds->fd, SSPI_MSG_FREE_CRED, request, hdr.requestId, &r, &rh,
		                       &response) == STATUS_SUCCESS)
		{
			lsa_proto_reader_done(&r);
			Stream_Free(response, TRUE);
		}
	}
	else
		lsa_proto_writer_abort(&w);

	lsa4posix_client_close(creds->fd);
	free(creds);
	return SEC_E_OK;
}

static SECURITY_STATUS SEC_ENTRY tsssp_InitializeSecurityContextW(
    PCredHandle phCredential, PCtxtHandle phContext, SEC_WCHAR* pszTargetName, ULONG fContextReq,
    WINPR_ATTR_UNUSED ULONG Reserved1, WINPR_ATTR_UNUSED ULONG TargetDataRep, PSecBufferDesc pInput,
    WINPR_ATTR_UNUSED ULONG Reserved2, PCtxtHandle phNewContext, PSecBufferDesc pOutput,
    WINPR_ATTR_UNUSED PULONG pfContextAttr, WINPR_ATTR_UNUSED PTimeStamp ptsExpiry)
{
	TSSSP_PROXY_CRED* creds = (TSSSP_PROXY_CRED*)sspi_SecureHandleGetLowerPointer(phCredential);
	if (!creds)
		return SEC_E_INVALID_HANDLE;

	TSSSP_PROXY_CTX* context =
	    phContext ? (TSSSP_PROXY_CTX*)sspi_SecureHandleGetLowerPointer(phContext) : nullptr;
	const UINT64 ctxId = context ? context->ctxId : 0;

	const SecBuffer* inToken = nullptr;
	const SecBuffer* publicKey = nullptr;
	if (pInput)
	{
		for (ULONG i = 0; i < pInput->cBuffers; i++)
		{
			const SecBuffer* b = &pInput->pBuffers[i];
			if ((b->BufferType & ~SECBUFFER_ATTRMASK) != SECBUFFER_TOKEN)
				continue;
			if (b->BufferType & SECBUFFER_READONLY)
				publicKey = b;
			else if (b->cbBuffer > 0)
				inToken = b;
		}
	}

	/* The target is a UTF-16 SPN (for example TERMSRV/host); send its bytes without the NUL. */
	const UINT32 targetLen = pszTargetName ? (UINT32)(_wcslen(pszTargetName) * sizeof(WCHAR)) : 0;

	const UINT32 reqId = creds->reqId++;
	LSA_MSG_HDR hdr = { SSPI_MSG_ISC, 0, reqId };
	LsaProtoWriter w = { 0 };
	wStream* request = nullptr;
	if (!lsa_proto_writer_init(&w, &hdr) || !lsa_proto_write_u64(&w, creds->credId) ||
	    !lsa_proto_write_u64(&w, ctxId) || !lsa_proto_write_u32(&w, fContextReq) ||
	    !lsa_proto_write_blob(&w, (const BYTE*)pszTargetName, targetLen) ||
	    !lsa_proto_write_blob(&w, inToken ? (const BYTE*)inToken->pvBuffer : nullptr,
	                          inToken ? inToken->cbBuffer : 0) ||
	    !lsa_proto_write_blob(&w, publicKey ? (const BYTE*)publicKey->pvBuffer : nullptr,
	                          publicKey ? publicKey->cbBuffer : 0) ||
	    !lsa_proto_writer_finish(&w, &request))
	{
		lsa_proto_writer_abort(&w);
		return SEC_E_INTERNAL_ERROR;
	}

	LsaProtoReader r = { 0 };
	LSA_MSG_HDR rh = { 0 };
	wStream* response = nullptr;
	if (lsa4posix_transact(creds->fd, SSPI_MSG_ISC, request, reqId, &r, &rh, &response) !=
	    STATUS_SUCCESS)
		return SEC_E_INTERNAL_ERROR;

	UINT32 status = 0;
	UINT64 newCtxId = 0;
	BYTE* outToken = nullptr;
	UINT32 outLen = 0;
	const BOOL ok = lsa_proto_read_u32(&r, &status) && lsa_proto_read_u64(&r, &newCtxId) &&
	                lsa_proto_read_blob(&r, &outToken, &outLen);
	lsa_proto_reader_done(&r);
	Stream_Free(response, TRUE);
	if (!ok)
	{
		free(outToken);
		return SEC_E_INTERNAL_ERROR;
	}

	const SECURITY_STATUS sstatus = (SECURITY_STATUS)status;
	if ((sstatus == SEC_E_OK) || (sstatus == SEC_I_CONTINUE_NEEDED))
	{
		if (!context)
		{
			context = (TSSSP_PROXY_CTX*)calloc(1, sizeof(TSSSP_PROXY_CTX));
			if (!context)
			{
				free(outToken);
				return SEC_E_INSUFFICIENT_MEMORY;
			}
			context->fd = creds->fd;
			context->reqId = 1;
		}
		context->ctxId = newCtxId;
		sspi_SecureHandleSetLowerPointer(phNewContext, context);
		sspi_SecureHandleSetPackageId(phNewContext, SSPI_PACKAGE_TSSSP);

		if (pOutput && (pOutput->cBuffers >= 1))
		{
			SecBuffer* out = &pOutput->pBuffers[0];
			out->BufferType = SECBUFFER_TOKEN;
			out->pvBuffer = nullptr;
			out->cbBuffer = 0;
			if (outLen > 0)
			{
				void* buf = sspi_ContextBufferAlloc(InitializeSecurityContextIndex, outLen);
				if (!buf)
				{
					free(outToken);
					return SEC_E_INSUFFICIENT_MEMORY;
				}
				CopyMemory(buf, outToken, outLen);
				out->pvBuffer = buf;
				out->cbBuffer = outLen;
			}
		}
	}

	free(outToken);
	return sstatus;
}

static SECURITY_STATUS SEC_ENTRY tsssp_InitializeSecurityContextA(
    WINPR_ATTR_UNUSED PCredHandle phCredential, WINPR_ATTR_UNUSED PCtxtHandle phContext,
    WINPR_ATTR_UNUSED SEC_CHAR* pszTargetName, WINPR_ATTR_UNUSED ULONG fContextReq,
    WINPR_ATTR_UNUSED ULONG Reserved1, WINPR_ATTR_UNUSED ULONG TargetDataRep,
    WINPR_ATTR_UNUSED PSecBufferDesc pInput, WINPR_ATTR_UNUSED ULONG Reserved2,
    WINPR_ATTR_UNUSED PCtxtHandle phNewContext, WINPR_ATTR_UNUSED PSecBufferDesc pOutput,
    WINPR_ATTR_UNUSED PULONG pfContextAttr, WINPR_ATTR_UNUSED PTimeStamp ptsExpiry)
{
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY tsssp_QueryContextAttributesW(PCtxtHandle phContext,
                                                               ULONG ulAttribute, void* pBuffer)
{
	TSSSP_PROXY_CTX* context = (TSSSP_PROXY_CTX*)sspi_SecureHandleGetLowerPointer(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	if (ulAttribute == SECPKG_ATTR_WINPR_TSPKG_CONTEXT)
	{
		if (!pBuffer)
			return SEC_E_INSUFFICIENT_MEMORY;
		*((UINT64*)pBuffer) = context->ctxId;
		return SEC_E_OK;
	}
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY
tsssp_QueryContextAttributesA(WINPR_ATTR_UNUSED PCtxtHandle phContext,
                              WINPR_ATTR_UNUSED ULONG ulAttribute, WINPR_ATTR_UNUSED void* pBuffer)
{
	return SEC_E_UNSUPPORTED_FUNCTION;
}

static SECURITY_STATUS SEC_ENTRY tsssp_DeleteSecurityContext(PCtxtHandle phContext)
{
	if (!phContext)
		return SEC_E_INVALID_HANDLE;
	TSSSP_PROXY_CTX* context = (TSSSP_PROXY_CTX*)sspi_SecureHandleGetLowerPointer(phContext);
	sspi_SecureHandleInvalidate(phContext);
	if (!context)
		return SEC_E_INVALID_HANDLE;

	LSA_MSG_HDR hdr = { SSPI_MSG_DELETE_CTX, 0, context->reqId++ };
	LsaProtoWriter w = { 0 };
	wStream* request = nullptr;
	if (lsa_proto_writer_init(&w, &hdr) && lsa_proto_write_u64(&w, context->ctxId) &&
	    lsa_proto_writer_finish(&w, &request))
	{
		LsaProtoReader r = { 0 };
		LSA_MSG_HDR rh = { 0 };
		wStream* response = nullptr;
		if (lsa4posix_transact(context->fd, SSPI_MSG_DELETE_CTX, request, hdr.requestId, &r, &rh,
		                       &response) == STATUS_SUCCESS)
		{
			lsa_proto_reader_done(&r);
			Stream_Free(response, TRUE);
		}
	}
	else
		lsa_proto_writer_abort(&w);

	free(context);
	return SEC_E_OK;
}

#endif /* TSSSP_FORWARD_NATIVE */

const SecurityFunctionTableA TSSSP_SecurityFunctionTableA = {
	3,                                /* dwVersion */
	nullptr,                          /* EnumerateSecurityPackages */
	nullptr,                          /* QueryCredentialsAttributes */
	tsssp_AcquireCredentialsHandleA,  /* AcquireCredentialsHandle */
	tsssp_FreeCredentialsHandle,      /* FreeCredentialsHandle */
	nullptr,                          /* Reserved2 */
	tsssp_InitializeSecurityContextA, /* InitializeSecurityContext */
	nullptr,                          /* AcceptSecurityContext */
	nullptr,                          /* CompleteAuthToken */
	tsssp_DeleteSecurityContext,      /* DeleteSecurityContext */
	nullptr,                          /* ApplyControlToken */
	tsssp_QueryContextAttributesA,    /* QueryContextAttributes */
	nullptr,                          /* ImpersonateSecurityContext */
	nullptr,                          /* RevertSecurityContext */
	nullptr,                          /* MakeSignature */
	nullptr,                          /* VerifySignature */
	nullptr,                          /* FreeContextBuffer */
	nullptr,                          /* QuerySecurityPackageInfo */
	nullptr,                          /* Reserved3 */
	nullptr,                          /* Reserved4 */
	nullptr,                          /* ExportSecurityContext */
	nullptr,                          /* ImportSecurityContext */
	nullptr,                          /* AddCredentials */
	nullptr,                          /* Reserved8 */
	nullptr,                          /* QuerySecurityContextToken */
	nullptr,                          /* EncryptMessage */
	nullptr,                          /* DecryptMessage */
	nullptr,                          /* SetContextAttributes */
	nullptr,                          /* SetCredentialsAttributes */
};

const SecurityFunctionTableW TSSSP_SecurityFunctionTableW = {
	3,                                /* dwVersion */
	nullptr,                          /* EnumerateSecurityPackages */
	nullptr,                          /* QueryCredentialsAttributes */
	tsssp_AcquireCredentialsHandleW,  /* AcquireCredentialsHandle */
	tsssp_FreeCredentialsHandle,      /* FreeCredentialsHandle */
	nullptr,                          /* Reserved2 */
	tsssp_InitializeSecurityContextW, /* InitializeSecurityContext */
	nullptr,                          /* AcceptSecurityContext */
	nullptr,                          /* CompleteAuthToken */
	tsssp_DeleteSecurityContext,      /* DeleteSecurityContext */
	nullptr,                          /* ApplyControlToken */
	tsssp_QueryContextAttributesW,    /* QueryContextAttributes */
	nullptr,                          /* ImpersonateSecurityContext */
	nullptr,                          /* RevertSecurityContext */
	nullptr,                          /* MakeSignature */
	nullptr,                          /* VerifySignature */
	nullptr,                          /* FreeContextBuffer */
	nullptr,                          /* QuerySecurityPackageInfo */
	nullptr,                          /* Reserved3 */
	nullptr,                          /* Reserved4 */
	nullptr,                          /* ExportSecurityContext */
	nullptr,                          /* ImportSecurityContext */
	nullptr,                          /* AddCredentials */
	nullptr,                          /* Reserved8 */
	nullptr,                          /* QuerySecurityContextToken */
	nullptr,                          /* EncryptMessage */
	nullptr,                          /* DecryptMessage */
	nullptr,                          /* SetContextAttributes */
	nullptr,                          /* SetCredentialsAttributes */
};

const SecPkgInfoA TSSSP_SecPkgInfoA = {
	0x00010000,                             /* fCapabilities */
	1,                                      /* wVersion */
	0xFFFF,                                 /* wRPCID */
	0x000090A8,                             /* cbMaxToken */
	"TSSSP",                                /* Name */
	"WinPR TSSSP (Remote Credential Guard)" /* Comment */
};

static WCHAR TSSSP_SecPkgInfoW_NameBuffer[128] = WINPR_C_ARRAY_INIT;
static WCHAR TSSSP_SecPkgInfoW_CommentBuffer[128] = WINPR_C_ARRAY_INIT;

const SecPkgInfoW TSSSP_SecPkgInfoW = {
	0x00010000,                     /* fCapabilities */
	1,                              /* wVersion */
	0xFFFF,                         /* wRPCID */
	0x000090A8,                     /* cbMaxToken */
	TSSSP_SecPkgInfoW_NameBuffer,   /* Name */
	TSSSP_SecPkgInfoW_CommentBuffer /* Comment */
};

BOOL TSSSP_init(void)
{
	InitializeConstWCharFromUtf8(TSSSP_SecPkgInfoA.Name, TSSSP_SecPkgInfoW_NameBuffer,
	                             ARRAYSIZE(TSSSP_SecPkgInfoW_NameBuffer));
	InitializeConstWCharFromUtf8(TSSSP_SecPkgInfoA.Comment, TSSSP_SecPkgInfoW_CommentBuffer,
	                             ARRAYSIZE(TSSSP_SecPkgInfoW_CommentBuffer));
	return TRUE;
}
