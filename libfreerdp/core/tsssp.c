/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Credential Guard via the native TSSSP package (tspkg.dll) over FreeRDP's TLS
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

#include <freerdp/config.h>

#include <winpr/assert.h>

#include "tsssp.h"

#if defined(TSSSP_SUPPORTED)

#include <winpr/crt.h>
#include <winpr/string.h>
#include <winpr/stream.h>
#include <winpr/endian.h>
#include <winpr/sspi.h>

#include <freerdp/log.h>
#include <freerdp/error.h>
#include <freerdp/crypto/certificate.h>

#include <credssp.h>
#define SCHANNEL_USE_BLACKLISTS
#include <schannel.h>

/* mingw-w64 before 8.0 lacks SCH_CREDENTIALS from schannel.h. Only dwVersion
 * is used; the layout matches the SDK. */
#if defined(__MINGW32__) && (__MINGW64_VERSION_MAJOR < 8)
typedef struct
{
	DWORD dwVersion;
	DWORD dwCredFormat;
	DWORD cCreds;
	PCCERT_CONTEXT* paCred;
	HCERTSTORE hRootStore;
	DWORD cMappers;
	struct _HMAPPER** aphMappers;
	DWORD dwSessionLifespan;
	DWORD dwFlags;
	DWORD cTlsParameters;
	void* pTlsParameters;
} SCH_CREDENTIALS;
#define SCH_CREDENTIALS_VERSION 0x00000005
#endif

#include "../crypto/tls.h"

#define TAG FREERDP_TAG("core.tsssp")

#define TSSSP_PACKAGE_NAMEW L"TSSSP"

/* InitializeSecurityContext request flags for the CredSSP exchange. */
#define TSSSP_ISC_REQ_FLAGS                                                                        \
	(ISC_REQ_STREAM | ISC_REQ_EXTENDED_ERROR | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY | \
	 ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_MUTUAL_AUTH)

/* [MS-RDPBCGR] Early User Authorization Result PDU */
#define TSSSP_AUTHZ_SUCCESS 0x00000000
#define TSSSP_AUTHZ_ACCESS_DENIED 0x00000005

/* CREDSSP_CRED_EX from credssp.h, declared locally because mingw-w64 lacks it.
 * The layout matches the SDK. */
#define TSSSP_CREDSSP_CRED_EX_TYPE 100 /* CredsspCredEx */
#define TSSSP_CREDSSP_CRED_EX_VERSION 0
#define TSSSP_CREDSSP_FLAG_REDIRECT 0x00000001

typedef struct
{
	CREDSPP_SUBMIT_TYPE Type;
	DWORD Version;
	DWORD Flags;
	DWORD Reserved;
	CREDSSP_CRED Cred;
} tsssp_cred_ex;

struct rdp_tsssp
{
	rdpContext* context;
	rdpTransport* transport;
	SecurityFunctionTableW* table;
	CredHandle credentials;
	BOOL haveCredentials;
	CtxtHandle ctx;
	BOOL haveContext;
	WCHAR* targetName;
	BYTE* certBuffer;
	DWORD certBufferLength;
	TSSSP_STATE state;
};

static const char* tsssp_get_state_str(TSSSP_STATE state)
{
	switch (state)
	{
		case TSSSP_STATE_INITIAL:
			return "TSSSP_STATE_INITIAL";
		case TSSSP_STATE_TOKEN:
			return "TSSSP_STATE_TOKEN";
		case TSSSP_STATE_EARLY_USER_AUTH:
			return "TSSSP_STATE_EARLY_USER_AUTH";
		case TSSSP_STATE_FINAL:
			return "TSSSP_STATE_FINAL";
		default:
			return "TSSSP_STATE_UNKNOWN";
	}
}

static void tsssp_set_state(rdpTsssp* tsssp, TSSSP_STATE state)
{
	WINPR_ASSERT(tsssp);
	WLog_DBG(TAG, "-- %s\t--> %s", tsssp_get_state_str(tsssp->state), tsssp_get_state_str(state));
	tsssp->state = state;
}

TSSSP_STATE tsssp_get_state(const rdpTsssp* tsssp)
{
	WINPR_ASSERT(tsssp);
	return tsssp->state;
}

rdpTsssp* tsssp_new(rdpContext* context, rdpTransport* transport)
{
	char* spn = nullptr;

	WINPR_ASSERT(context);
	WINPR_ASSERT(transport);

	rdpTsssp* tsssp = calloc(1, sizeof(rdpTsssp));
	if (!tsssp)
		return nullptr;

	tsssp->context = context;
	tsssp->transport = transport;
	tsssp->state = TSSSP_STATE_INITIAL;

	/* TSSSP only exists in the native SSPI. WinPR returns its own SSPI table if
	 * the native one is disabled at runtime (WINPR_NATIVE_SSPI=0), so check that
	 * the package is available. */
	tsssp->table = InitSecurityInterfaceExW(SSPI_INTERFACE_NATIVE);
	if (!tsssp->table)
	{
		WLog_ERR(TAG, "the native SSPI interface is not available");
		goto fail;
	}

	PSecPkgInfoW info = nullptr;
	const SECURITY_STATUS status =
	    tsssp->table->QuerySecurityPackageInfoW(TSSSP_PACKAGE_NAMEW, &info);
	if (info)
		(void)tsssp->table->FreeContextBuffer(info);
	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG,
		         "the TSSSP package is not available (0x%08" PRIx32
		         "), Remote Credential Guard needs the native SSPI",
		         (UINT32)status);
		goto fail;
	}

	const char* hostname = freerdp_settings_get_server_name(context->settings);
	if (!hostname)
		goto fail;

	size_t spnLength = 0;
	if (winpr_asprintf(&spn, &spnLength, "TERMSRV/%s", hostname) < 0)
		goto fail;

	tsssp->targetName = ConvertUtf8ToWCharAlloc(spn, nullptr);
	if (!tsssp->targetName)
		goto fail;

	free(spn);
	return tsssp;

fail:
	free(spn);
	tsssp_free(tsssp);
	return nullptr;
}

void tsssp_free(rdpTsssp* tsssp)
{
	if (!tsssp)
		return;

	if (tsssp->haveContext)
		(void)tsssp->table->DeleteSecurityContext(&tsssp->ctx);
	if (tsssp->haveCredentials)
		(void)tsssp->table->FreeCredentialsHandle(&tsssp->credentials);

	free(tsssp->targetName);
	free(tsssp->certBuffer);
	free(tsssp);
}

BOOL tsssp_get_package_context(const rdpTsssp* tsssp, UINT64* pContext)
{
	WINPR_ASSERT(pContext);

	if (!tsssp || (tsssp->state != TSSSP_STATE_FINAL))
		return FALSE;

	*pContext = (UINT64)tsssp->ctx.dwUpper;
	return TRUE;
}

/* TSSSP expects the server leaf certificate as [UINT32 length][DER], little
 * endian, without tag or padding. It binds pubKeyAuth to the public key of this
 * certificate, so it must be the one negotiated in the TLS handshake. */
static BOOL tsssp_build_cert_buffer(rdpTsssp* tsssp)
{
	BOOL rc = FALSE;
	size_t derLength = 0;
	BYTE* der = nullptr;

	WINPR_ASSERT(tsssp);

	rdpTls* tls = transport_get_tls(tsssp->transport);
	if (!tls)
	{
		WLog_ERR(TAG, "no TLS context available for the server certificate");
		return FALSE;
	}

	rdpCertificate* cert = freerdp_tls_get_certificate(tls, TRUE);
	if (!cert)
		return FALSE;

	der = freerdp_certificate_get_der(cert, &derLength);
	if (!der || (derLength == 0) || (derLength > UINT32_MAX - sizeof(UINT32)))
	{
		WLog_ERR(TAG, "failed to encode the server certificate");
		goto out;
	}

	const size_t length = derLength + sizeof(UINT32);
	BYTE* buffer = malloc(length);
	if (!buffer)
		goto out;

	winpr_Data_Write_UINT32(buffer, (UINT32)derLength);
	memcpy(&buffer[sizeof(UINT32)], der, derLength);

	tsssp->certBuffer = buffer;
	tsssp->certBufferLength = (DWORD)length;
	rc = TRUE;

out:
	free(der);
	freerdp_certificate_free(cert);
	return rc;
}

/* Explicit identity from the settings, if a user name is set. Otherwise TSSSP
 * uses the credentials of the current logon. */
static BOOL tsssp_build_identity(const rdpSettings* settings, SEC_WINNT_AUTH_IDENTITY_W* identity,
                                 BOOL* pHaveIdentity)
{
	WINPR_ASSERT(identity);
	WINPR_ASSERT(pHaveIdentity);

	*pHaveIdentity = FALSE;

	const char* user = freerdp_settings_get_string(settings, FreeRDP_Username);
	if (!user || (strlen(user) == 0))
		return TRUE;

	const char* domain = freerdp_settings_get_string(settings, FreeRDP_Domain);
	const char* password = freerdp_settings_get_string(settings, FreeRDP_Password);

	identity->Flags = SEC_WINNT_AUTH_IDENTITY_UNICODE;

	size_t len = 0;
	identity->User = (UINT16*)ConvertUtf8ToWCharAlloc(user, &len);
	if (!identity->User)
		return FALSE;
	identity->UserLength = (ULONG)len;

	if (domain && (strlen(domain) > 0))
	{
		identity->Domain = (UINT16*)ConvertUtf8ToWCharAlloc(domain, &len);
		if (!identity->Domain)
			return FALSE;
		identity->DomainLength = (ULONG)len;
	}

	if (password && (strlen(password) > 0))
	{
		identity->Password = (UINT16*)ConvertUtf8ToWCharAlloc(password, &len);
		if (!identity->Password)
			return FALSE;
		identity->PasswordLength = (ULONG)len;
	}

	*pHaveIdentity = TRUE;
	return TRUE;
}

static void tsssp_free_identity(SEC_WINNT_AUTH_IDENTITY_W* identity)
{
	WINPR_ASSERT(identity);

	free(identity->User);
	if (identity->Password)
		SecureZeroMemory(identity->Password, identity->PasswordLength * sizeof(WCHAR));
	free(identity->Password);
	free(identity->Domain);
	memset(identity, 0, sizeof(*identity));
}

static BOOL tsssp_acquire_credentials(rdpTsssp* tsssp)
{
	WINPR_ASSERT(tsssp);

	SEC_WINNT_AUTH_IDENTITY_W identity = WINPR_C_ARRAY_INIT;
	BOOL haveIdentity = FALSE;
	if (!tsssp_build_identity(tsssp->context->settings, &identity, &haveIdentity))
	{
		tsssp_free_identity(&identity);
		return FALSE;
	}

	SCH_CREDENTIALS schannelCred = WINPR_C_ARRAY_INIT;
	schannelCred.dwVersion = SCH_CREDENTIALS_VERSION;

	const CREDSSP_CRED cred = { .Type = CredsspSubmitBufferBothOld,
		                        .pSchannelCred = &schannelCred,
		                        .pSpnegoCred = haveIdentity ? &identity : nullptr };
	tsssp_cred_ex authData = { .Type = (CREDSPP_SUBMIT_TYPE)TSSSP_CREDSSP_CRED_EX_TYPE,
		                       .Version = TSSSP_CREDSSP_CRED_EX_VERSION,
		                       .Flags = TSSSP_CREDSSP_FLAG_REDIRECT,
		                       .Reserved = 0,
		                       .Cred = cred };

	const SECURITY_STATUS status = tsssp->table->AcquireCredentialsHandleW(
	    nullptr, TSSSP_PACKAGE_NAMEW, SECPKG_CRED_OUTBOUND, nullptr, &authData, nullptr, nullptr,
	    &tsssp->credentials, nullptr);
	tsssp_free_identity(&identity);
	if (status != SEC_E_OK)
	{
		WLog_ERR(TAG, "AcquireCredentialsHandle(TSSSP) failed: 0x%08" PRIx32, (UINT32)status);
		return FALSE;
	}

	tsssp->haveCredentials = TRUE;
	return TRUE;
}

/* Send one ISC output token as a TSRequest PDU and release the SSPI buffer.
 * An empty token is not an error. */
static BOOL tsssp_write_token(rdpTsssp* tsssp, SecBuffer* out)
{
	BOOL rc = TRUE;

	WINPR_ASSERT(tsssp);
	WINPR_ASSERT(out);

	if ((out->cbBuffer > 0) && out->pvBuffer)
	{
		rc = FALSE;

		wStream* s = Stream_New(nullptr, out->cbBuffer);
		if (s)
		{
			Stream_Write(s, out->pvBuffer, out->cbBuffer);
			rc = (transport_write(tsssp->transport, s) >= 0);
			Stream_Free(s, TRUE);
		}
	}

	if (out->pvBuffer)
		(void)tsssp->table->FreeContextBuffer(out->pvBuffer);
	out->pvBuffer = nullptr;
	out->cbBuffer = 0;
	return rc;
}

/* Send the output of InitializeSecurityContext and advance the state. Once the
 * package is done, the server sends the PROTOCOL_HYBRID_EX Early User
 * Authorization Result PDU, which is not a TSRequest. */
static int tsssp_handle_isc_result(rdpTsssp* tsssp, SECURITY_STATUS status, SecBuffer* out)
{
	WINPR_ASSERT(tsssp);

	if ((status != SEC_I_CONTINUE_NEEDED) && (status != SEC_E_OK))
	{
		WLog_ERR(TAG, "InitializeSecurityContext failed: 0x%08" PRIx32, (UINT32)status);
		(void)tsssp_write_token(tsssp, out);
		return -1;
	}

	if (!tsssp_write_token(tsssp, out))
		return -1;

	if (status == SEC_E_OK)
	{
		transport_set_nla_mode(tsssp->transport, FALSE);
		transport_set_early_user_auth_mode(tsssp->transport, TRUE);
		tsssp_set_state(tsssp, TSSSP_STATE_EARLY_USER_AUTH);
	}
	else
		tsssp_set_state(tsssp, TSSSP_STATE_TOKEN);

	return 1;
}

int tsssp_client_begin(rdpTsssp* tsssp)
{
	WINPR_ASSERT(tsssp);

	if (tsssp->state != TSSSP_STATE_INITIAL)
	{
		WLog_ERR(TAG, "TSSSP in invalid state %s", tsssp_get_state_str(tsssp->state));
		return -1;
	}

	if (!tsssp_acquire_credentials(tsssp))
		return -1;

	/* The server leaf certificate is the only data passed from TLS to TSSSP. */
	if (!tsssp_build_cert_buffer(tsssp))
		return -1;

	DWORD outFlags = 0;
	TimeStamp expiry = WINPR_C_ARRAY_INIT;
	SecBuffer outBuffer = { 0, SECBUFFER_TOKEN, nullptr };
	SecBufferDesc outDesc = { SECBUFFER_VERSION, 1, &outBuffer };

	const SECURITY_STATUS status = tsssp->table->InitializeSecurityContextW(
	    &tsssp->credentials, nullptr, tsssp->targetName, TSSSP_ISC_REQ_FLAGS, 0,
	    SECURITY_NETWORK_DREP, nullptr, 0, &tsssp->ctx, &outDesc, &outFlags, &expiry);
	if ((status == SEC_I_CONTINUE_NEEDED) || (status == SEC_E_OK))
		tsssp->haveContext = TRUE;

	return tsssp_handle_isc_result(tsssp, status, &outBuffer);
}

/* One TSRequest from the server, framed by the transport in NLA mode. The
 * certificate is passed with the first server token only. */
static int tsssp_recv_token(rdpTsssp* tsssp, wStream* s)
{
	WINPR_ASSERT(tsssp);
	WINPR_ASSERT(s);

	const size_t length = Stream_GetRemainingLength(s);
	if ((length == 0) || (length > ULONG_MAX))
		return -1;

	SecBuffer inBuffers[3] = WINPR_C_ARRAY_INIT;
	inBuffers[0].BufferType = SECBUFFER_TOKEN;
	inBuffers[0].pvBuffer = Stream_Pointer(s);
	inBuffers[0].cbBuffer = (ULONG)length;

	ULONG inCount = 2;
	if (tsssp->certBuffer)
	{
		inBuffers[1].BufferType = SECBUFFER_READONLY | SECBUFFER_TOKEN;
		inBuffers[1].pvBuffer = tsssp->certBuffer;
		inBuffers[1].cbBuffer = tsssp->certBufferLength;
		inBuffers[2].BufferType = SECBUFFER_EMPTY;
		inCount = 3;
	}
	else
		inBuffers[1].BufferType = SECBUFFER_EMPTY;

	SecBufferDesc inDesc = { SECBUFFER_VERSION, inCount, inBuffers };

	DWORD outFlags = 0;
	TimeStamp expiry = WINPR_C_ARRAY_INIT;
	SecBuffer outBuffer = { 0, SECBUFFER_TOKEN, nullptr };
	SecBufferDesc outDesc = { SECBUFFER_VERSION, 1, &outBuffer };

	const SECURITY_STATUS status = tsssp->table->InitializeSecurityContextW(
	    &tsssp->credentials, &tsssp->ctx, tsssp->targetName, TSSSP_ISC_REQ_FLAGS, 0,
	    SECURITY_NETWORK_DREP, &inDesc, 0, &tsssp->ctx, &outDesc, &outFlags, &expiry);
	Stream_Seek(s, length);

	free(tsssp->certBuffer);
	tsssp->certBuffer = nullptr;
	tsssp->certBufferLength = 0;

	return tsssp_handle_isc_result(tsssp, status, &outBuffer);
}

/* [MS-RDPBCGR] With PROTOCOL_HYBRID_EX the server sends a 4-byte Early User
 * Authorization Result PDU after the CredSSP exchange. See nla_recv_pdu for the
 * NLA path. */
static int tsssp_recv_early_user_auth(rdpTsssp* tsssp, wStream* s)
{
	WINPR_ASSERT(tsssp);
	WINPR_ASSERT(s);

	transport_set_early_user_auth_mode(tsssp->transport, FALSE);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return -1;

	const UINT32 authzResult = Stream_Get_UINT32(s);
	if (authzResult != TSSSP_AUTHZ_SUCCESS)
	{
		UINT32 code = FREERDP_ERROR_AUTHENTICATION_FAILED;

		/* The user authenticated but is not allowed to log on to this host. */
		if (authzResult == TSSSP_AUTHZ_ACCESS_DENIED)
			code = FREERDP_ERROR_CONNECT_ACCESS_DENIED;

		WLog_ERR(TAG, "Early User Auth active: FAILURE authorization result 0x%08" PRIX32 "",
		         authzResult);
		freerdp_set_last_error_log(tsssp->context, code);
		return -1;
	}

	WLog_DBG(TAG, "Early User Auth active: SUCCESS");
	tsssp_set_state(tsssp, TSSSP_STATE_FINAL);
	return 1;
}

int tsssp_recv(rdpTsssp* tsssp, wStream* s)
{
	int rc = -1;

	WINPR_ASSERT(tsssp);
	WINPR_ASSERT(s);

	switch (tsssp->state)
	{
		case TSSSP_STATE_TOKEN:
			rc = tsssp_recv_token(tsssp, s);
			break;
		case TSSSP_STATE_EARLY_USER_AUTH:
			rc = tsssp_recv_early_user_auth(tsssp, s);
			break;
		default:
			WLog_ERR(TAG, "TSSSP in invalid receive state %s", tsssp_get_state_str(tsssp->state));
			break;
	}

	if (rc < 0)
		freerdp_set_last_error_if_not(tsssp->context, FREERDP_ERROR_AUTHENTICATION_FAILED);
	return rc;
}

#else

void tsssp_free(rdpTsssp* tsssp)
{
	free(tsssp);
}

int tsssp_recv(rdpTsssp* tsssp, wStream* s)
{
	WINPR_UNUSED(tsssp);
	WINPR_UNUSED(s);
	return -1;
}

TSSSP_STATE tsssp_get_state(const rdpTsssp* tsssp)
{
	WINPR_UNUSED(tsssp);
	return TSSSP_STATE_INITIAL;
}

BOOL tsssp_get_package_context(const rdpTsssp* tsssp, UINT64* pContext)
{
	WINPR_UNUSED(tsssp);
	WINPR_ASSERT(pContext);
	return FALSE;
}

#endif
