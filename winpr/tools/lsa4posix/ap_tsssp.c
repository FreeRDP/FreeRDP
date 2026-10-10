/**
 * WinPR: Windows Portable Runtime
 * lsa4posix: TSSSP authentication package servicing
 *
 * Runs the CredSSP exchange on a client's behalf: the daemon owns the CredSSP
 * credential and context; the client proxies AcquireCredentialsHandle /
 * InitializeSecurityContext over the SSPI wire messages.
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

#include <winpr/crt.h>
#include <winpr/wlog.h>
#include <winpr/nt.h>
#include <winpr/sspi.h>
#include <winpr/credssp.h>
#include <winpr/endian.h>
#include <winpr/stream.h>

#include <pthread.h>

#include <krb5.h>
#include <rdpear-common/rdpear_kerb.h>

#include "ap_tsssp.h"
#include "lsa4posix.h"

#define TAG LSA4POSIX_TAG

#define AP_TSSSP_MAX_CREDS 16
#define AP_TSSSP_MAX_CTX 16

static const WCHAR CREDSSP_NAMEW[] = { 'C', 'R', 'E', 'D', 'S', 'S', 'P', '\0' };

typedef struct
{
	BOOL used;
	UINT64 id;
	UINT64 owner; /* connection that owns this entry; only the owner frees it */
	CredHandle cred;
	/* identity backing store, kept alive for the credential's lifetime */
	WCHAR* user;
	WCHAR* domain;
	WCHAR* password;
	SEC_WINNT_AUTH_IDENTITY_W identity;
	CREDSSP_CRED_EX credEx;
} ApCred;

typedef struct
{
	BOOL used;
	UINT64 id;
	UINT64 owner;
	UINT64 credId;
	CtxtHandle ctx;
	BOOL haveCtx;
} ApCtx;

/* The credential and context tables are process-global so that a later same-uid connection (for
 * example the AuthRedirection channel servicing a redirected call) can reach a context created on
 * the connection that authenticated. Entries are tagged with their owning connection and freed
 * when that connection closes. The peer uid is already checked when the connection is accepted. */
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static PSecurityFunctionTableW g_table = nullptr;
static UINT64 g_nextId = 1;
static UINT64 g_nextOwner = 1;
static ApCred g_creds[AP_TSSSP_MAX_CREDS];
static ApCtx g_ctxs[AP_TSSSP_MAX_CTX];

struct ap_tsssp_state
{
	UINT64 owner;
};

ApTssspState* ap_tsssp_new(void)
{
	ApTssspState* state = (ApTssspState*)calloc(1, sizeof(ApTssspState));
	if (!state)
		return nullptr;
	pthread_mutex_lock(&g_lock);
	if (!g_table)
		g_table = InitSecurityInterfaceExW(SSPI_INTERFACE_WINPR);
	state->owner = g_nextOwner++;
	pthread_mutex_unlock(&g_lock);
	return state;
}

/* g_lock must be held. */
static void ap_cred_clear(ApCred* c)
{
	if (c->used && g_table && g_table->FreeCredentialsHandle)
		(void)g_table->FreeCredentialsHandle(&c->cred);
	free(c->user);
	free(c->domain);
	if (c->password)
		SecureZeroMemory(c->password, (c->identity.PasswordLength + 1) * sizeof(WCHAR));
	free(c->password);
	memset(c, 0, sizeof(*c));
}

/* g_lock must be held. */
static void ap_ctx_clear(ApCtx* c)
{
	if (c->used && c->haveCtx && g_table && g_table->DeleteSecurityContext)
		(void)g_table->DeleteSecurityContext(&c->ctx);
	memset(c, 0, sizeof(*c));
}

void ap_tsssp_free(ApTssspState* state)
{
	if (!state)
		return;
	pthread_mutex_lock(&g_lock);
	for (size_t i = 0; i < AP_TSSSP_MAX_CTX; i++)
		if (g_ctxs[i].used && (g_ctxs[i].owner == state->owner))
			ap_ctx_clear(&g_ctxs[i]);
	for (size_t i = 0; i < AP_TSSSP_MAX_CREDS; i++)
		if (g_creds[i].used && (g_creds[i].owner == state->owner))
			ap_cred_clear(&g_creds[i]);
	pthread_mutex_unlock(&g_lock);
	free(state);
}

/* g_lock must be held. */
static ApCred* ap_cred_find(UINT64 id)
{
	for (size_t i = 0; i < AP_TSSSP_MAX_CREDS; i++)
		if (g_creds[i].used && (g_creds[i].id == id))
			return &g_creds[i];
	return nullptr;
}

/* g_lock must be held. */
static ApCred* ap_cred_alloc(void)
{
	for (size_t i = 0; i < AP_TSSSP_MAX_CREDS; i++)
		if (!g_creds[i].used)
			return &g_creds[i];
	return nullptr;
}

/* g_lock must be held. */
static ApCtx* ap_ctx_find(UINT64 id)
{
	for (size_t i = 0; i < AP_TSSSP_MAX_CTX; i++)
		if (g_ctxs[i].used && (g_ctxs[i].id == id))
			return &g_ctxs[i];
	return nullptr;
}

/* g_lock must be held. */
static ApCtx* ap_ctx_alloc(void)
{
	for (size_t i = 0; i < AP_TSSSP_MAX_CTX; i++)
		if (!g_ctxs[i].used)
			return &g_ctxs[i];
	return nullptr;
}

/* Read a UTF-16 string blob into a NUL-terminated WCHAR buffer; *outChars excludes the NUL. */
static BOOL read_wstring(LsaProtoReader* r, WCHAR** out, ULONG* outChars)
{
	BYTE* data = nullptr;
	UINT32 len = 0;
	if (!lsa_proto_read_blob(r, &data, &len))
		return FALSE;
	const UINT32 chars = len / sizeof(WCHAR);
	WCHAR* buf = (WCHAR*)calloc(chars + 1, sizeof(WCHAR));
	if (!buf)
	{
		free(data);
		return FALSE;
	}
	if (len)
		memcpy(buf, data, chars * sizeof(WCHAR));
	free(data);
	*out = buf;
	*outChars = chars;
	return TRUE;
}

BOOL ap_tsssp_acquire(ApTssspState* state, LsaProtoReader* r, LsaProtoWriter* w)
{
	UINT32 flags = 0;
	UINT16 haveIdentity = 0;
	if (!lsa_proto_read_u32(r, &flags) || !lsa_proto_read_u16(r, &haveIdentity))
		return FALSE;

	NTSTATUS status = STATUS_SUCCESS;
	UINT64 credId = 0;

	pthread_mutex_lock(&g_lock);
	ApCred* c = ap_cred_alloc();
	if (!c)
		status = STATUS_INSUFFICIENT_RESOURCES;

	if (c)
	{
		c->owner = state->owner;
		ULONG ulen = 0;
		ULONG dlen = 0;
		ULONG plen = 0;
		if (haveIdentity)
		{
			if (!read_wstring(r, &c->user, &ulen) || !read_wstring(r, &c->domain, &dlen) ||
			    !read_wstring(r, &c->password, &plen))
			{
				ap_cred_clear(c);
				pthread_mutex_unlock(&g_lock);
				return FALSE;
			}
			c->identity.User = (UINT16*)c->user;
			c->identity.UserLength = ulen;
			c->identity.Domain = (UINT16*)c->domain;
			c->identity.DomainLength = dlen;
			c->identity.Password = (UINT16*)c->password;
			c->identity.PasswordLength = plen;
			c->identity.Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;
		}

		c->credEx.Type = CredsspCredEx;
		c->credEx.Version = CREDSSP_CRED_EX_VERSION;
		c->credEx.Flags = flags;
		c->credEx.Cred.Type = CredsspSubmitBufferBothOld;
		c->credEx.Cred.pSpnegoCred = haveIdentity ? &c->identity : nullptr;

		TimeStamp expiry = { 0 };
		status = g_table
		             ? g_table->AcquireCredentialsHandleW(nullptr, (SEC_WCHAR*)CREDSSP_NAMEW,
		                                                  SECPKG_CRED_OUTBOUND, nullptr, &c->credEx,
		                                                  nullptr, nullptr, &c->cred, &expiry)
		             : SEC_E_SECPKG_NOT_FOUND;
		if (status == SEC_E_OK)
		{
			c->used = TRUE;
			c->id = g_nextId++;
			credId = c->id;
		}
		else
		{
			WLog_ERR(TAG, "CredSSP AcquireCredentialsHandle failed: 0x%08" PRIx32, (UINT32)status);
			ap_cred_clear(c);
		}
	}
	pthread_mutex_unlock(&g_lock);

	return lsa_proto_write_u32(w, (UINT32)status) && lsa_proto_write_u64(w, credId);
}

BOOL ap_tsssp_isc(ApTssspState* state, LsaProtoReader* r, LsaProtoWriter* w)
{
	UINT64 credId = 0;
	UINT64 ctxId = 0;
	UINT32 contextReq = 0;
	BYTE* target = nullptr;
	UINT32 targetLen = 0;
	BYTE* inToken = nullptr;
	UINT32 inLen = 0;
	BYTE* publicKey = nullptr;
	UINT32 pkLen = 0;

	if (!lsa_proto_read_u64(r, &credId) || !lsa_proto_read_u64(r, &ctxId) ||
	    !lsa_proto_read_u32(r, &contextReq) || !lsa_proto_read_blob(r, &target, &targetLen) ||
	    !lsa_proto_read_blob(r, &inToken, &inLen) || !lsa_proto_read_blob(r, &publicKey, &pkLen))
	{
		free(target);
		free(inToken);
		free(publicKey);
		return FALSE;
	}

	NTSTATUS status = SEC_E_INVALID_HANDLE;
	UINT64 outCtxId = 0;
	BYTE* outToken = nullptr;
	UINT32 outLen = 0;

	/* Rebuild the NUL-terminated UTF-16 target SPN from the blob the client sent. */
	WCHAR* targetName = nullptr;
	if (targetLen > 0)
	{
		const UINT32 chars = targetLen / sizeof(WCHAR);
		targetName = (WCHAR*)calloc(chars + 1, sizeof(WCHAR));
		if (targetName)
			memcpy(targetName, target, chars * sizeof(WCHAR));
	}

	pthread_mutex_lock(&g_lock);
	ApCred* cred = ap_cred_find(credId);
	ApCtx* ctx = ctxId ? ap_ctx_find(ctxId) : ap_ctx_alloc();
	if (cred && ctx && g_table)
	{
		SecBuffer inBuffers[3] = { 0 };
		inBuffers[0].BufferType = SECBUFFER_TOKEN;
		inBuffers[0].pvBuffer = inToken;
		inBuffers[0].cbBuffer = inLen;
		ULONG inCount = 1;
		if (pkLen > 0)
		{
			inBuffers[1].BufferType = SECBUFFER_READONLY | SECBUFFER_TOKEN;
			inBuffers[1].pvBuffer = publicKey;
			inBuffers[1].cbBuffer = pkLen;
			inCount = 2;
		}
		SecBufferDesc inDesc = { SECBUFFER_VERSION, inCount, inBuffers };

		SecBuffer outBuffer = { 0, SECBUFFER_TOKEN, nullptr };
		SecBufferDesc outDesc = { SECBUFFER_VERSION, 1, &outBuffer };
		ULONG attr = 0;
		TimeStamp expiry = { 0 };
		CtxtHandle* inCtx = ctx->haveCtx ? &ctx->ctx : nullptr;

		status = g_table->InitializeSecurityContextW(
		    &cred->cred, inCtx, targetName, contextReq | ISC_REQ_ALLOCATE_MEMORY, 0,
		    SECURITY_NATIVE_DREP, (inLen > 0) ? &inDesc : nullptr, 0, &ctx->ctx, &outDesc, &attr,
		    &expiry);

		if ((status == SEC_E_OK) || (status == SEC_I_CONTINUE_NEEDED))
		{
			if (!ctx->used)
			{
				ctx->used = TRUE;
				ctx->owner = state->owner;
				ctx->id = g_nextId++;
				ctx->credId = credId;
			}
			ctx->haveCtx = TRUE;
			outCtxId = ctx->id;
			if (outBuffer.cbBuffer && outBuffer.pvBuffer)
			{
				outToken = (BYTE*)malloc(outBuffer.cbBuffer);
				if (outToken)
				{
					memcpy(outToken, outBuffer.pvBuffer, outBuffer.cbBuffer);
					outLen = outBuffer.cbBuffer;
				}
			}
		}
		if (outBuffer.pvBuffer && g_table->FreeContextBuffer)
			(void)g_table->FreeContextBuffer(outBuffer.pvBuffer);
	}
	pthread_mutex_unlock(&g_lock);

	free(targetName);
	free(target);
	free(inToken);
	free(publicKey);

	const BOOL rc = lsa_proto_write_u32(w, (UINT32)status) && lsa_proto_write_u64(w, outCtxId) &&
	                lsa_proto_write_blob(w, outToken, outLen);
	free(outToken);
	return rc;
}

BOOL ap_tsssp_delete_ctx(ApTssspState* state, LsaProtoReader* r, LsaProtoWriter* w)
{
	UINT64 ctxId = 0;
	if (!lsa_proto_read_u64(r, &ctxId))
		return FALSE;
	pthread_mutex_lock(&g_lock);
	ApCtx* ctx = ap_ctx_find(ctxId);
	/* Only the owning connection may free a context. */
	if (ctx && (ctx->owner == state->owner))
		ap_ctx_clear(ctx);
	pthread_mutex_unlock(&g_lock);
	return lsa_proto_write_u32(w, STATUS_SUCCESS);
}

BOOL ap_tsssp_free_cred(ApTssspState* state, LsaProtoReader* r, LsaProtoWriter* w)
{
	UINT64 credId = 0;
	if (!lsa_proto_read_u64(r, &credId))
		return FALSE;
	pthread_mutex_lock(&g_lock);
	ApCred* cred = ap_cred_find(credId);
	if (cred && (cred->owner == state->owner))
		ap_cred_clear(cred);
	pthread_mutex_unlock(&g_lock);
	return lsa_proto_write_u32(w, STATUS_SUCCESS);
}

/* [MS-RDPEAR] TSRemoteGuardPacket: ProtocolMagic, Length, Version, Reserved, TsPkgContext. */
#define RDPEAR_PROTOCOL_MAGIC 0x4EACC3C8
#define RDPEAR_HEADER_LENGTH 24
#define RDPEAR_CONTEXT_OFFSET 16

BOOL ap_tsssp_call_package(const BYTE* submit, UINT32 submitLen, NTSTATUS* protocolStatus,
                           BYTE** ret, UINT32* retLen)
{
	*ret = nullptr;
	*retLen = 0;
	*protocolStatus = STATUS_NOT_IMPLEMENTED;

	if (submitLen < RDPEAR_HEADER_LENGTH)
	{
		*protocolStatus = STATUS_INVALID_PARAMETER;
		return FALSE;
	}
	if (winpr_Data_Get_UINT32(submit) != RDPEAR_PROTOCOL_MAGIC)
	{
		*protocolStatus = STATUS_INVALID_PARAMETER;
		return FALSE;
	}
	const UINT32 bodyLen = winpr_Data_Get_UINT32(&submit[4]);
	const UINT64 ctxId = winpr_Data_Get_UINT64(&submit[RDPEAR_CONTEXT_OFFSET]);
	if ((UINT64)RDPEAR_HEADER_LENGTH + bodyLen > submitLen)
	{
		*protocolStatus = STATUS_INVALID_PARAMETER;
		return FALSE;
	}

	pthread_mutex_lock(&g_lock);
	ApCtx* ctx = ap_ctx_find(ctxId);
	if (!ctx || !ctx->haveCtx || !g_table || !g_table->QueryContextAttributesW ||
	    !g_table->DecryptMessage)
	{
		pthread_mutex_unlock(&g_lock);
		WLog_ERR(TAG, "rdpear: no TSSSP context 0x%016" PRIx64, ctxId);
		*protocolStatus = STATUS_INVALID_HANDLE;
		return FALSE;
	}

	SecPkgContext_Sizes sizes = { 0 };
	if (g_table->QueryContextAttributesW(&ctx->ctx, SECPKG_ATTR_SIZES, &sizes) != SEC_E_OK)
	{
		pthread_mutex_unlock(&g_lock);
		*protocolStatus = STATUS_INTERNAL_ERROR;
		return FALSE;
	}

	/* The body is [signature (cbSecurityTrailer)][encrypted payload]. */
	if (bodyLen < sizes.cbSecurityTrailer)
	{
		pthread_mutex_unlock(&g_lock);
		*protocolStatus = STATUS_INVALID_PARAMETER;
		return FALSE;
	}
	const UINT32 dataLen = bodyLen - sizes.cbSecurityTrailer;
	BYTE* data = (BYTE*)malloc(dataLen ? dataLen : 1);
	if (!data)
	{
		pthread_mutex_unlock(&g_lock);
		*protocolStatus = STATUS_NO_MEMORY;
		return FALSE;
	}
	memcpy(data, &submit[RDPEAR_HEADER_LENGTH + sizes.cbSecurityTrailer], dataLen);

	SecBuffer buffers[2] = { 0 };
	buffers[0].BufferType = SECBUFFER_TOKEN;
	buffers[0].pvBuffer = (void*)&submit[RDPEAR_HEADER_LENGTH];
	buffers[0].cbBuffer = sizes.cbSecurityTrailer;
	buffers[1].BufferType = SECBUFFER_DATA;
	buffers[1].pvBuffer = data;
	buffers[1].cbBuffer = dataLen;
	SecBufferDesc desc = { SECBUFFER_VERSION, 2, buffers };
	ULONG qop = 0;
	const SECURITY_STATUS st = g_table->DecryptMessage(&ctx->ctx, &desc, 0, &qop);
	if (st != SEC_E_OK)
	{
		pthread_mutex_unlock(&g_lock);
		WLog_ERR(TAG, "rdpear DecryptMessage failed: 0x%08" PRIx32, (UINT32)st);
		free(data);
		*protocolStatus = STATUS_INTERNAL_ERROR;
		return FALSE;
	}

	/* Dispatch the redirected call to the krb5 handlers (the keys ride in the request, so a
	 * throwaway krb5 context suffices) and get the plaintext response payload. */
	krb5_context krb = nullptr;
	wStream* respPlain = nullptr;
	if (krb5_init_context(&krb) == 0)
	{
		respPlain = rdpear_kerb_service(krb, buffers[1].pvBuffer, buffers[1].cbBuffer);
		krb5_free_context(krb);
	}
	free(data);
	if (!respPlain)
	{
		pthread_mutex_unlock(&g_lock);
		*protocolStatus = STATUS_INTERNAL_ERROR;
		return FALSE;
	}

	/* Encrypt the response with the same context ([signature][ciphertext]) and build the reply
	 * TSRemoteGuardPacket. The sequence number continues from the request, so keep the lock. */
	const ULONG plainLen = (ULONG)Stream_GetPosition(respPlain);
	BYTE* encBuf = (BYTE*)calloc(1, (size_t)plainLen + sizes.cbSecurityTrailer);
	BOOL built = FALSE;
	if (encBuf)
	{
		SecBuffer encBuffers[2] = { 0 };
		encBuffers[0].BufferType = SECBUFFER_TOKEN;
		encBuffers[0].pvBuffer = encBuf;
		encBuffers[0].cbBuffer = sizes.cbSecurityTrailer;
		encBuffers[1].BufferType = SECBUFFER_DATA;
		encBuffers[1].pvBuffer = encBuf + sizes.cbSecurityTrailer;
		encBuffers[1].cbBuffer = plainLen;
		memcpy(encBuffers[1].pvBuffer, Stream_Buffer(respPlain), plainLen);
		SecBufferDesc encDesc = { SECBUFFER_VERSION, 2, encBuffers };
		if (g_table->EncryptMessage &&
		    (g_table->EncryptMessage(&ctx->ctx, 0, &encDesc, 0) == SEC_E_OK))
		{
			/* The signature may be shorter than cbSecurityTrailer; pack the buffers contiguously.
			 */
			if (encBuffers[0].cbBuffer < sizes.cbSecurityTrailer)
				memmove(encBuf + encBuffers[0].cbBuffer, encBuffers[1].pvBuffer,
				        encBuffers[1].cbBuffer);
			const UINT32 wrappedLen = encBuffers[0].cbBuffer + encBuffers[1].cbBuffer;
			BYTE* pkt = (BYTE*)malloc((size_t)RDPEAR_HEADER_LENGTH + wrappedLen);
			if (pkt)
			{
				winpr_Data_Write_UINT32(&pkt[0], RDPEAR_PROTOCOL_MAGIC);
				winpr_Data_Write_UINT32(&pkt[4], wrappedLen);
				winpr_Data_Write_UINT32(&pkt[8], 0);  /* Version */
				winpr_Data_Write_UINT32(&pkt[12], 0); /* Reserved */
				winpr_Data_Write_UINT64(&pkt[16], 0); /* TsPkgContext (zero on the wire) */
				memcpy(&pkt[RDPEAR_HEADER_LENGTH], encBuf, wrappedLen);
				*ret = pkt;
				*retLen = RDPEAR_HEADER_LENGTH + wrappedLen;
				built = TRUE;
			}
		}
		free(encBuf);
	}
	pthread_mutex_unlock(&g_lock);
	Stream_Free(respPlain, TRUE);

	if (!built)
	{
		*protocolStatus = STATUS_INTERNAL_ERROR;
		return FALSE;
	}
	WLog_DBG(TAG, "rdpear serviced: response %" PRIu32 " bytes", *retLen);
	*protocolStatus = STATUS_SUCCESS;
	return TRUE;
}
