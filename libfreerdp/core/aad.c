/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Network Level Authentication (NLA)
 *
 * Copyright 2023 Isaac Klein <fifthdegree@protonmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *		 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <freerdp/config.h>

#include <stdio.h>
#include <string.h>

#include <freerdp/crypto/crypto.h>
#include <freerdp/crypto/privatekey.h>
#include "../crypto/privatekey.h"
#include <freerdp/utils/http.h>
#include <freerdp/utils/aad.h>

#include <winpr/crypto.h>
#include <winpr/json.h>

#include "transport.h"

#include "aad.h"

struct rdp_aad
{
	AAD_STATE state;
	rdpContext* rdpcontext;
	rdpTransport* transport;
	char* access_token;
	rdpPrivateKey* key;
	char* kid;
	char* nonce;
	char* hostname;
	char* scope;
	wLog* log;
};

#ifdef WITH_AAD

static BOOL get_encoded_rsa_params(wLog* wlog, rdpPrivateKey* key, char** e, char** n);
static BOOL generate_pop_key(rdpAad* aad);

WINPR_ATTR_FORMAT_ARG(2, 3)
static SSIZE_T stream_sprintf(wStream* s, WINPR_FORMAT_ARG const char* fmt, ...)
{
	va_list ap = { 0 };
	va_start(ap, fmt);
	const int rc = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);

	if (rc < 0)
		return rc;

	if (!Stream_EnsureRemainingCapacity(s, (size_t)rc + 1))
		return -1;

	char* ptr = Stream_PointerAs(s, char);
	va_start(ap, fmt);
	const int rc2 = vsnprintf(ptr, rc + 1, fmt, ap);
	va_end(ap);
	if (rc != rc2)
		return -23;
	if (!Stream_SafeSeek(s, (size_t)rc2))
		return -3;
	return rc2;
}

static BOOL json_get_object(wLog* wlog, WINPR_JSON* json, const char* key, WINPR_JSON** obj)
{
	WINPR_ASSERT(json);
	WINPR_ASSERT(key);

	if (!WINPR_JSON_HasObjectItem(json, key))
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] does not contain a key '%s'", key);
		return FALSE;
	}

	WINPR_JSON* prop = WINPR_JSON_GetObjectItem(json, key);
	if (!prop)
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is NULL", key);
		return FALSE;
	}
	*obj = prop;
	return TRUE;
}

static BOOL json_get_number(wLog* wlog, WINPR_JSON* json, const char* key, double* result)
{
	BOOL rc = FALSE;
	WINPR_JSON* prop = NULL;
	if (!json_get_object(wlog, json, key, &prop))
		return FALSE;

	if (!WINPR_JSON_IsNumber(prop))
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is NOT a NUMBER", key);
		goto fail;
	}

	*result = WINPR_JSON_GetNumberValue(prop);

	rc = TRUE;
fail:
	return rc;
}

static BOOL json_get_const_string(wLog* wlog, WINPR_JSON* json, const char* key,
                                  const char** result)
{
	BOOL rc = FALSE;
	WINPR_ASSERT(result);

	*result = NULL;

	WINPR_JSON* prop = NULL;
	if (!json_get_object(wlog, json, key, &prop))
		return FALSE;

	if (!WINPR_JSON_IsString(prop))
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is NOT a STRING", key);
		goto fail;
	}

	const char* str = WINPR_JSON_GetStringValue(prop);
	if (!str)
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is NULL", key);
	*result = str;
	rc = str != NULL;

fail:
	return rc;
}

static BOOL json_get_string_alloc(wLog* wlog, WINPR_JSON* json, const char* key, char** result)
{
	const char* str = NULL;
	if (!json_get_const_string(wlog, json, key, &str))
		return FALSE;
	free(*result);
	*result = _strdup(str);
	if (!*result)
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' strdup is NULL", key);
	return *result != NULL;
}

static INLINE const char* aad_auth_result_to_string(DWORD code)
{
#define ERROR_CASE(cd, x)   \
	if ((cd) == (DWORD)(x)) \
		return #x;

	ERROR_CASE(code, S_OK)
	ERROR_CASE(code, SEC_E_INVALID_TOKEN)
	ERROR_CASE(code, E_ACCESSDENIED)
	ERROR_CASE(code, STATUS_LOGON_FAILURE)
	ERROR_CASE(code, STATUS_NO_LOGON_SERVERS)
	ERROR_CASE(code, STATUS_INVALID_LOGON_HOURS)
	ERROR_CASE(code, STATUS_INVALID_WORKSTATION)
	ERROR_CASE(code, STATUS_PASSWORD_EXPIRED)
	ERROR_CASE(code, STATUS_ACCOUNT_DISABLED)
	return "Unknown error";
}

static BOOL aad_get_nonce(rdpAad* aad)
{
	BOOL ret = FALSE;
	BYTE* response = NULL;
	long resp_code = 0;
	size_t response_length = 0;
	WINPR_JSON* json = NULL;

	if (!freerdp_http_request("https://login.microsoftonline.com/common/oauth2/v2.0/token",
	                          "grant_type=srv_challenge", &resp_code, &response, &response_length))
	{
		WLog_Print(aad->log, WLOG_ERROR, "nonce request failed");
		goto fail;
	}

	if (resp_code != HTTP_STATUS_OK)
	{
		WLog_Print(aad->log, WLOG_ERROR,
		           "Server unwilling to provide nonce; returned status code %li", resp_code);
		if (response_length > 0)
			WLog_Print(aad->log, WLOG_ERROR, "[status message] %s", response);
		goto fail;
	}

	json = WINPR_JSON_ParseWithLength((const char*)response, response_length);
	if (!json)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Failed to parse nonce response");
		goto fail;
	}

	if (!json_get_string_alloc(aad->log, json, "Nonce", &aad->nonce))
		goto fail;

	ret = TRUE;

fail:
	free(response);
	WINPR_JSON_Delete(json);
	return ret;
}

int aad_client_begin(rdpAad* aad)
{
	size_t size = 0;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(aad->rdpcontext);

	rdpSettings* settings = aad->rdpcontext->settings;
	WINPR_ASSERT(settings);

	freerdp* instance = aad->rdpcontext->instance;
	WINPR_ASSERT(instance);

	/* Get the host part of the hostname */
	const char* hostname = freerdp_settings_get_string(settings, FreeRDP_AadServerHostname);
	if (!hostname)
		hostname = freerdp_settings_get_string(settings, FreeRDP_ServerHostname);
	if (!hostname)
	{
		WLog_Print(aad->log, WLOG_ERROR, "FreeRDP_ServerHostname == NULL");
		return -1;
	}

	aad->hostname = _strdup(hostname);
	if (!aad->hostname)
	{
		WLog_Print(aad->log, WLOG_ERROR, "_strdup(FreeRDP_ServerHostname) == NULL");
		return -1;
	}

	char* p = strchr(aad->hostname, '.');
	if (p)
		*p = '\0';

	if (winpr_asprintf(&aad->scope, &size,
	                   "ms-device-service%%3A%%2F%%2Ftermsrv.wvd.microsoft.com%%2Fname%%2F%s%%"
	                   "2Fuser_impersonation",
	                   aad->hostname) <= 0)
		return -1;

	if (!generate_pop_key(aad))
		return -1;

	/* Obtain an oauth authorization code */
	if (!instance->GetAccessToken)
	{
		WLog_Print(aad->log, WLOG_ERROR, "instance->GetAccessToken == NULL");
		return -1;
	}
	const BOOL arc = instance->GetAccessToken(instance, ACCESS_TOKEN_TYPE_AAD, &aad->access_token,
	                                          2, aad->scope, aad->kid);
	if (!arc)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Unable to obtain access token");
		return -1;
	}

	/* Send the nonce request message */
	if (!aad_get_nonce(aad))
	{
		WLog_Print(aad->log, WLOG_ERROR, "Unable to obtain nonce");
		return -1;
	}

	return 1;
}

static char* aad_create_jws_header(rdpAad* aad)
{
	WINPR_ASSERT(aad);

	/* Construct the base64url encoded JWS header */
	char* buffer = NULL;
	size_t bufferlen = 0;
	const int length =
	    winpr_asprintf(&buffer, &bufferlen, "{\"alg\":\"RS256\",\"kid\":\"%s\"}", aad->kid);
	if (length < 0)
		return NULL;

	char* jws_header = crypto_base64url_encode((const BYTE*)buffer, bufferlen);
	free(buffer);
	return jws_header;
}

static char* aad_create_jws_payload(rdpAad* aad, const char* ts_nonce)
{
	const time_t ts = time(NULL);

	WINPR_ASSERT(aad);

	char* e = NULL;
	char* n = NULL;
	if (!get_encoded_rsa_params(aad->log, aad->key, &e, &n))
		return NULL;

	/* Construct the base64url encoded JWS payload */
	char* buffer = NULL;
	size_t bufferlen = 0;
	const int length =
	    winpr_asprintf(&buffer, &bufferlen,
	                   "{"
	                   "\"ts\":\"%li\","
	                   "\"at\":\"%s\","
	                   "\"u\":\"ms-device-service://termsrv.wvd.microsoft.com/name/%s\","
	                   "\"nonce\":\"%s\","
	                   "\"cnf\":{\"jwk\":{\"kty\":\"RSA\",\"e\":\"%s\",\"n\":\"%s\"}},"
	                   "\"client_claims\":\"{\\\"aad_nonce\\\":\\\"%s\\\"}\""
	                   "}",
	                   ts, aad->access_token, aad->hostname, ts_nonce, e, n, aad->nonce);
	free(e);
	free(n);

	if (length < 0)
		return NULL;

	char* jws_payload = crypto_base64url_encode((BYTE*)buffer, bufferlen);
	free(buffer);
	return jws_payload;
}

static BOOL aad_update_digest(rdpAad* aad, WINPR_DIGEST_CTX* ctx, const char* what)
{
	WINPR_ASSERT(aad);
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(what);

	const BOOL dsu1 = winpr_DigestSign_Update(ctx, what, strlen(what));
	if (!dsu1)
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_DigestSign_Update [%s] failed", what);
		return FALSE;
	}
	return TRUE;
}

static char* aad_final_digest(rdpAad* aad, WINPR_DIGEST_CTX* ctx)
{
	char* jws_signature = NULL;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(ctx);

	size_t siglen = 0;
	const int dsf = winpr_DigestSign_Final(ctx, NULL, &siglen);
	if (dsf <= 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_DigestSign_Final failed with %d", dsf);
		return FALSE;
	}

	char* buffer = calloc(siglen + 1, sizeof(char));
	if (!buffer)
	{
		WLog_Print(aad->log, WLOG_ERROR, "calloc %" PRIuz " bytes failed", siglen + 1);
		goto fail;
	}

	size_t fsiglen = siglen;
	const int dsf2 = winpr_DigestSign_Final(ctx, (BYTE*)buffer, &fsiglen);
	if (dsf2 <= 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_DigestSign_Final failed with %d", dsf2);
		goto fail;
	}

	if (siglen != fsiglen)
	{
		WLog_Print(aad->log, WLOG_ERROR,
		           "winpr_DigestSignFinal returned different sizes, first %" PRIuz " then %" PRIuz,
		           siglen, fsiglen);
		goto fail;
	}
	jws_signature = crypto_base64url_encode((const BYTE*)buffer, fsiglen);
fail:
	free(buffer);
	return jws_signature;
}

static char* aad_create_jws_signature(rdpAad* aad, const char* jws_header, const char* jws_payload)
{
	char* jws_signature = NULL;

	WINPR_ASSERT(aad);

	WINPR_DIGEST_CTX* md_ctx = freerdp_key_digest_sign(aad->key, WINPR_MD_SHA256);
	if (!md_ctx)
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_Digest_New failed");
		goto fail;
	}

	if (!aad_update_digest(aad, md_ctx, jws_header))
		goto fail;
	if (!aad_update_digest(aad, md_ctx, "."))
		goto fail;
	if (!aad_update_digest(aad, md_ctx, jws_payload))
		goto fail;

	jws_signature = aad_final_digest(aad, md_ctx);
fail:
	winpr_Digest_Free(md_ctx);
	return jws_signature;
}

static int aad_send_auth_request(rdpAad* aad, const char* ts_nonce)
{
	int ret = -1;
	char* jws_header = NULL;
	char* jws_payload = NULL;
	char* jws_signature = NULL;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(ts_nonce);

	wStream* s = Stream_New(NULL, 1024);
	if (!s)
		goto fail;

	/* Construct the base64url encoded JWS header */
	jws_header = aad_create_jws_header(aad);
	if (!jws_header)
		goto fail;

	/* Construct the base64url encoded JWS payload */
	jws_payload = aad_create_jws_payload(aad, ts_nonce);
	if (!jws_payload)
		goto fail;

	/* Sign the JWS with the pop key */
	jws_signature = aad_create_jws_signature(aad, jws_header, jws_payload);
	if (!jws_signature)
		goto fail;

	/* Construct the Authentication Request PDU with the JWS as the RDP Assertion */
	if (stream_sprintf(s, "{\"rdp_assertion\":\"%s.%s.%s\"}", jws_header, jws_payload,
	                   jws_signature) < 0)
		goto fail;

	/* Include null terminator in PDU */
	Stream_Write_UINT8(s, 0);

	Stream_SealLength(s);

	if (transport_write(aad->transport, s) < 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "transport_write [%" PRIdz " bytes] failed",
		           Stream_Length(s));
	}
	else
	{
		ret = 1;
		aad->state = AAD_STATE_AUTH;
	}
fail:
	Stream_Free(s, TRUE);
	free(jws_header);
	free(jws_payload);
	free(jws_signature);

	return ret;
}

static int aad_parse_state_initial(rdpAad* aad, wStream* s)
{
	const char* jstr = Stream_PointerAs(s, char);
	const size_t jlen = Stream_GetRemainingLength(s);
	const char* ts_nonce = NULL;
	int ret = -1;
	WINPR_JSON* json = NULL;

	if (!Stream_SafeSeek(s, jlen))
		goto fail;

	json = WINPR_JSON_ParseWithLength(jstr, jlen);
	if (!json)
		goto fail;

	if (!json_get_const_string(aad->log, json, "ts_nonce", &ts_nonce))
		goto fail;

	ret = aad_send_auth_request(aad, ts_nonce);
fail:
	WINPR_JSON_Delete(json);
	return ret;
}

static int aad_parse_state_auth(rdpAad* aad, wStream* s)
{
	int rc = -1;
	double result = 0;
	DWORD error_code = 0;
	WINPR_JSON* json = NULL;
	const char* jstr = Stream_PointerAs(s, char);
	const size_t jlength = Stream_GetRemainingLength(s);

	if (!Stream_SafeSeek(s, jlength))
		goto fail;

	json = WINPR_JSON_ParseWithLength(jstr, jlength);
	if (!json)
		goto fail;

	if (!json_get_number(aad->log, json, "authentication_result", &result))
		goto fail;
	error_code = (DWORD)result;

	if (error_code != S_OK)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Authentication result: %s (0x%08" PRIx32 ")",
		           aad_auth_result_to_string(error_code), error_code);
		goto fail;
	}
	aad->state = AAD_STATE_FINAL;
	rc = 1;
fail:
	WINPR_JSON_Delete(json);
	return rc;
}

int aad_recv(rdpAad* aad, wStream* s)
{
	WINPR_ASSERT(aad);
	WINPR_ASSERT(s);

	switch (aad->state)
	{
		case AAD_STATE_INITIAL:
			return aad_parse_state_initial(aad, s);
		case AAD_STATE_AUTH:
			return aad_parse_state_auth(aad, s);
		case AAD_STATE_FINAL:
		default:
			WLog_Print(aad->log, WLOG_ERROR, "Invalid AAD_STATE %d", aad->state);
			return -1;
	}
}

static BOOL generate_rsa_2048(rdpAad* aad)
{
	WINPR_ASSERT(aad);
	return freerdp_key_generate(aad->key, 2048);
}

static char* generate_rsa_digest_base64_str(rdpAad* aad, const char* input, size_t ilen)
{
	char* b64 = NULL;
	WINPR_DIGEST_CTX* digest = winpr_Digest_New();
	if (!digest)
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_Digest_New failed");
		goto fail;
	}

	if (!winpr_Digest_Init(digest, WINPR_MD_SHA256))
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_Digest_Init(WINPR_MD_SHA256) failed");
		goto fail;
	}

	if (!winpr_Digest_Update(digest, (const BYTE*)input, ilen))
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_Digest_Update(%" PRIuz ") failed", ilen);
		goto fail;
	}

	BYTE hash[WINPR_SHA256_DIGEST_LENGTH] = { 0 };
	if (!winpr_Digest_Final(digest, hash, sizeof(hash)))
	{
		WLog_Print(aad->log, WLOG_ERROR, "winpr_Digest_Final(%" PRIuz ") failed", sizeof(hash));
		goto fail;
	}

	/* Base64url encode the hash */
	b64 = crypto_base64url_encode(hash, sizeof(hash));

fail:
	winpr_Digest_Free(digest);
	return b64;
}

static BOOL generate_json_base64_str(rdpAad* aad, const char* b64_hash)
{
	WINPR_ASSERT(aad);

	char* buffer = NULL;
	size_t blen = 0;
	const int length = winpr_asprintf(&buffer, &blen, "{\"kid\":\"%s\"}", b64_hash);
	if (length < 0)
		return FALSE;

	/* Finally, base64url encode the JSON text to form the kid */
	free(aad->kid);
	aad->kid = crypto_base64url_encode((const BYTE*)buffer, (size_t)length);
	free(buffer);

	if (!aad->kid)
	{
		return FALSE;
	}
	return TRUE;
}

BOOL generate_pop_key(rdpAad* aad)
{
	BOOL ret = FALSE;
	char* buffer = NULL;
	char* b64_hash = NULL;
	char* e = NULL;
	char* n = NULL;

	WINPR_ASSERT(aad);

	/* Generate a 2048-bit RSA key pair */
	if (!generate_rsa_2048(aad))
		goto fail;

	/* Encode the public key as a JWK */
	if (!get_encoded_rsa_params(aad->log, aad->key, &e, &n))
		goto fail;

	size_t blen = 0;
	const int alen =
	    winpr_asprintf(&buffer, &blen, "{\"e\":\"%s\",\"kty\":\"RSA\",\"n\":\"%s\"}", e, n);
	if (alen < 0)
		goto fail;

	/* Hash the encoded public key */
	b64_hash = generate_rsa_digest_base64_str(aad, buffer, blen);
	if (!b64_hash)
		goto fail;

	/* Encode a JSON object with a single property "kid" whose value is the encoded hash */
	ret = generate_json_base64_str(aad, b64_hash);

fail:
	free(b64_hash);
	free(buffer);
	free(e);
	free(n);
	return ret;
}

static char* bn_to_base64_url(wLog* wlog, rdpPrivateKey* key, enum FREERDP_KEY_PARAM param)
{
	WINPR_ASSERT(wlog);
	WINPR_ASSERT(key);

	size_t len = 0;
	BYTE* bn = freerdp_key_get_param(key, param, &len);
	if (!bn)
		return NULL;

	char* b64 = crypto_base64url_encode(bn, len);
	free(bn);

	if (!b64)
		WLog_Print(wlog, WLOG_ERROR, "failed  base64 url encode BIGNUM");

	return b64;
}

BOOL get_encoded_rsa_params(wLog* wlog, rdpPrivateKey* key, char** pe, char** pn)
{
	BOOL rc = FALSE;
	char* e = NULL;
	char* n = NULL;

	WINPR_ASSERT(wlog);
	WINPR_ASSERT(key);
	WINPR_ASSERT(pe);
	WINPR_ASSERT(pn);

	*pe = NULL;
	*pn = NULL;

	e = bn_to_base64_url(wlog, key, FREERDP_KEY_PARAM_RSA_E);
	if (!e)
	{
		WLog_Print(wlog, WLOG_ERROR, "failed  base64 url encode RSA E");
		goto fail;
	}

	n = bn_to_base64_url(wlog, key, FREERDP_KEY_PARAM_RSA_N);
	if (!n)
	{
		WLog_Print(wlog, WLOG_ERROR, "failed  base64 url encode RSA N");
		goto fail;
	}

	rc = TRUE;
fail:
	if (!rc)
	{
		free(e);
		free(n);
	}
	else
	{
		*pe = e;
		*pn = n;
	}
	return rc;
}
#else
int aad_client_begin(rdpAad* aad)
{
	WINPR_ASSERT(aad);
	WLog_Print(aad->log, WLOG_ERROR, "AAD security not compiled in, aborting!");
	return -1;
}
int aad_recv(rdpAad* aad, wStream* s)
{
	WINPR_ASSERT(aad);
	WLog_Print(aad->log, WLOG_ERROR, "AAD security not compiled in, aborting!");
	return -1;
}
#endif

rdpAad* aad_new(rdpContext* context, rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	WINPR_ASSERT(context);

	rdpAad* aad = (rdpAad*)calloc(1, sizeof(rdpAad));

	if (!aad)
		return NULL;

	aad->log = WLog_Get(FREERDP_TAG("aad"));
	aad->key = freerdp_key_new();
	if (!aad->key)
		goto fail;
	aad->rdpcontext = context;
	aad->transport = transport;

	return aad;
fail:
	WINPR_PRAGMA_DIAG_PUSH
	WINPR_PRAGMA_DIAG_IGNORED_MISMATCHED_DEALLOC
	aad_free(aad);
	WINPR_PRAGMA_DIAG_POP
	return NULL;
}

void aad_free(rdpAad* aad)
{
	if (!aad)
		return;

	free(aad->hostname);
	free(aad->scope);
	free(aad->nonce);
	free(aad->access_token);
	free(aad->kid);
	freerdp_key_free(aad->key);

	free(aad);
}

AAD_STATE aad_get_state(rdpAad* aad)
{
	WINPR_ASSERT(aad);
	return aad->state;
}

BOOL aad_is_supported(void)
{
#ifdef WITH_AAD
	return TRUE;
#else
	return FALSE;
#endif
}

#ifdef WITH_AAD
char* freerdp_utils_aad_get_access_token(wLog* log, const char* data, size_t length)
{
	char* token = NULL;
	WINPR_JSON* access_token_prop = NULL;
	const char* access_token_str = NULL;

	WINPR_JSON* json = WINPR_JSON_ParseWithLength(data, length);
	if (!json)
	{
		WLog_Print(log, WLOG_ERROR, "Failed to parse access token response [got %" PRIuz " bytes",
		           length);
		goto cleanup;
	}

	access_token_prop = WINPR_JSON_GetObjectItem(json, "access_token");
	if (!access_token_prop)
	{
		WLog_Print(log, WLOG_ERROR, "Response has no \"access_token\" property");
		goto cleanup;
	}

	access_token_str = WINPR_JSON_GetStringValue(access_token_prop);
	if (!access_token_str)
	{
		WLog_Print(log, WLOG_ERROR, "Invalid value for \"access_token\"");
		goto cleanup;
	}

	token = _strdup(access_token_str);

cleanup:
	WINPR_JSON_Delete(json);
	return token;
}
#endif
