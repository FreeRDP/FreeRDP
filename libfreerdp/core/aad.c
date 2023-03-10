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

#ifdef WITH_CJSON
#include <cjson/cJSON.h>
#endif

#include <winpr/crypto.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#include "transport.h"

#include "aad.h"

#ifdef WITH_CJSON
#if CJSON_VERSION_MAJOR == 1
#if CJSON_VERSION_MINOR <= 7
#if CJSON_VERSION_PATCH < 13
#define USE_CJSON_COMPAT
#endif
#endif
#endif
#endif

struct rdp_aad
{
	AAD_STATE state;
	rdpContext* rdpcontext;
	rdpTransport* transport;
	char* access_token;
	EVP_PKEY* pop_key;
	char* kid;
	char* nonce;
	char* hostname;
	wLog* log;
};

#ifdef WITH_CJSON
#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/core_names.h>
#else
static int BIO_get_line(BIO* bio, char* buf, int size)
{
	int pos = 0;

	if (size <= 1)
		return 0;

	while (pos < size - 1)
	{
		char c = '\0';
		const int rc = BIO_read(bio, &c, sizeof(c));
		if (rc < 0)
			return rc;
		if (rc > 0)
		{
			buf[pos++] = c;
			if (c == '\n')
				break;
		}
	}

	buf[pos] = '\0';
	return pos;
}
#endif

#define OAUTH2_CLIENT_ID "5177bc73-fd99-4c77-a90c-76844c9b6999"
static const char* auth_server = "login.microsoftonline.com";

static const char nonce_http_request[] = ""
                                         "POST /common/oauth2/token HTTP/1.1\r\n"
                                         "Host: login.microsoftonline.com\r\n"
                                         "Content-Type: application/x-www-form-urlencoded\r\n"
                                         "Content-Length: 24\r\n"
                                         "\r\n"
                                         "grant_type=srv_challenge"
                                         "\r\n\r\n";

static const char token_http_request_header[] =
    ""
    "POST /common/oauth2/v2.0/token HTTP/1.1\r\n"
    "Host: login.microsoftonline.com\r\n"
    "Content-Type: application/x-www-form-urlencoded\r\n"
    "Content-Length: %lu\r\n"
    "\r\n";
static const char token_http_request_body[] =
    ""
    "client_id=" OAUTH2_CLIENT_ID "&grant_type=authorization_code"
    "&code=%s"
    "&scope=ms-device-service%%3A%%2F%%2Ftermsrv.wvd.microsoft.com%%2Fname%%2F%s%%2Fuser_"
    "impersonation"
    "&req_cnf=%s"
    "&redirect_uri=ms-appx-web%%3a%%2f%%2fMicrosoft.AAD.BrokerPlugin%%2f5177bc73-fd99-4c77-a90c-"
    "76844c9b6999"
    "\r\n\r\n";

static BOOL get_encoded_rsa_params(wLog* wlog, EVP_PKEY* pkey, char** e, char** n);
static BOOL generate_pop_key(rdpAad* aad);
static BOOL read_http_message(rdpAad* aad, BIO* bio, long* status_code, char** content,
                              size_t* content_length);

static int alloc_sprintf(char** s, size_t* slen, const char* template, ...)
{
	va_list ap;

	WINPR_ASSERT(s);
	WINPR_ASSERT(slen);
	*s = NULL;
	*slen = 0;

	va_start(ap, template);
	const int length = vsnprintf(NULL, 0, template, ap);
	va_end(ap);
	if (length < 0)
		return length;

	char* str = calloc((size_t)length + 1ul, sizeof(char));
	if (!str)
		return -1;

	va_start(ap, template);
	const int plen = vsprintf(str, template, ap);
	va_end(ap);

	WINPR_ASSERT(length == plen);
	*s = str;
	*slen = (size_t)length;
	return length;
}

static SSIZE_T stream_sprintf(wStream* s, const char* fmt, ...)
{
	va_list ap;
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

static int print_error(const char* str, size_t len, void* u)
{
	wLog* wlog = (wLog*)u;
	WLog_Print(wlog, WLOG_ERROR, "%s [%" PRIuz "]", str, len);
	return 1;
}

static BOOL json_get_object(wLog* wlog, cJSON* json, const char* key, cJSON** obj)
{
	WINPR_ASSERT(json);
	WINPR_ASSERT(key);

	if (!cJSON_HasObjectItem(json, key))
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] does not contain a key '%s'", key);
		return FALSE;
	}

	cJSON* prop = cJSON_GetObjectItem(json, key);
	if (!prop)
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is NULL", key);
		return FALSE;
	}
	*obj = prop;
	return TRUE;
}

static BOOL json_get_number(wLog* wlog, cJSON* json, const char* key, double* result)
{
	BOOL rc = FALSE;
	cJSON* prop = NULL;
	if (!json_get_object(wlog, json, key, &prop))
		return FALSE;

	if (!cJSON_IsNumber(prop))
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is NOT a NUMBER", key);
		goto fail;
	}
#if defined(USE_CJSON_COMPAT)
	if (!cJSON_IsNumber(prop))
		goto fail;
	char* val = cJSON_GetStringValue(prop);
	if (!val)
		goto fail;

	errno = 0;
	char* endptr = NULL;
	double dval = strtod(val, &endptr);
	if (val == endptr)
		goto fail;
	if (endptr != NULL)
		goto fail;
	if (errno != 0)
		goto fail;
	*result = dval;
#else
	*result = cJSON_GetNumberValue(prop);
#endif
	rc = TRUE;
fail:
	return rc;
}

static BOOL json_get_const_string(wLog* wlog, cJSON* json, const char* key, const char** result)
{
	BOOL rc = FALSE;
	WINPR_ASSERT(result);

	*result = NULL;

	cJSON* prop = NULL;
	if (!json_get_object(wlog, json, key, &prop))
		return FALSE;

	if (!cJSON_IsString(prop))
	{
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is NOT a STRING", key);
		goto fail;
	}

	const char* str = cJSON_GetStringValue(prop);
	if (!str)
		WLog_Print(wlog, WLOG_ERROR, "[json] object for key '%s' is NULL", key);
	*result = str;
	rc = str != NULL;

fail:
	return rc;
}

static BOOL json_get_string_alloc(wLog* wlog, cJSON* json, const char* key, char** result)
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

static BIO* aad_connect_https(rdpAad* aad, SSL_CTX* ssl_ctx)
{
	WINPR_ASSERT(aad);
	WINPR_ASSERT(ssl_ctx);

	const int vprc = SSL_CTX_set_default_verify_paths(ssl_ctx);
	if (vprc != 1)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Error setting verify paths");
		return NULL;
	}
	const long mrc = SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);

	BIO* bio = BIO_new_ssl_connect(ssl_ctx);
	if (!bio)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Error setting up connection");
		return NULL;
	}
	const long chrc = BIO_set_conn_hostname(bio, auth_server);
	if (chrc != 1)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Error setting BIO hostname");
		BIO_free(bio);
		return NULL;
	}
	const long cprc = BIO_set_conn_port(bio, "https");
	if (cprc != 1)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Error setting BIO port");
		BIO_free(bio);
		return NULL;
	}
	return bio;
}

static BOOL aad_logging_bio_write(rdpAad* aad, BIO* bio, const char* str)
{
	WINPR_ASSERT(aad);
	WINPR_ASSERT(bio);
	WINPR_ASSERT(str);

	const size_t size = strlen(str);
	if (size > INT_MAX)
		return FALSE;
	ERR_clear_error();
	if (BIO_write(bio, str, (int)size) < 0)
	{
		ERR_print_errors_cb(print_error, aad->log);
		return FALSE;
	}
	return TRUE;
}

static char* aad_read_response(rdpAad* aad, BIO* bio, size_t* plen, const char* what)
{
	WINPR_ASSERT(plen);

	long status_code;
	char* buffer = NULL;
	size_t length = 0;

	*plen = 0;
	if (!read_http_message(aad, bio, &status_code, &buffer, &length))
	{
		WLog_Print(aad->log, WLOG_ERROR, "Unable to read %s HTTP response", what);
		return NULL;
	}
	WLog_Print(aad->log, WLOG_DEBUG, "%s HTTP response: %s", what, buffer);

	if (status_code != 200)
	{
		WLog_Print(aad->log, WLOG_ERROR, "%s HTTP status code: %li", what, status_code);
		free(buffer);
		return NULL;
	}
	*plen = length;
	return buffer;
}

static cJSON* compat_cJSON_ParseWithLength(const char* value, size_t buffer_length)
{
#if defined(USE_CJSON_COMPAT)
	// Check for string '\0' termination.
	const size_t slen = strnlen(value, buffer_length);
	if (slen >= buffer_length)
		return NULL;
	return cJSON_Parse(value);
#else
	return cJSON_ParseWithLength(value, buffer_length);
#endif
}
static BOOL aad_read_and_extract_token_from_json(rdpAad* aad, BIO* bio)
{
	BOOL rc = FALSE;
	size_t blen = 0;
	char* buffer = aad_read_response(aad, bio, &blen, "access token");
	if (!buffer)
		return FALSE;

	cJSON* json = compat_cJSON_ParseWithLength(buffer, blen);
	if (!json)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Failed to parse JSON response");
		goto fail;
	}

	if (!json_get_string_alloc(aad->log, json, "access_token", &aad->access_token))
	{
		WLog_Print(aad->log, WLOG_ERROR,
		           "Could not find \"access_token\" property in JSON response");
		goto fail;
	}

	rc = TRUE;
fail:
	free(buffer);
	cJSON_Delete(json);
	return rc;
}

static BOOL aad_read_and_extrace_nonce_from_json(rdpAad* aad, BIO* bio)
{
	BOOL rc = FALSE;
	size_t blen = 0;
	char* buffer = aad_read_response(aad, bio, &blen, "Nonce");
	if (!buffer)
		return FALSE;

	/* Extract the nonce from the response */
	cJSON* json = compat_cJSON_ParseWithLength(buffer, blen);
	if (!json)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Failed to parse JSON response");
		goto fail;
	}

	if (!json_get_string_alloc(aad->log, json, "Nonce", &aad->nonce))
	{
		WLog_Print(aad->log, WLOG_ERROR, "Could not find \"Nonce\" property in JSON response");
		goto fail;
	}
	rc = TRUE;
fail:
	free(buffer);
	cJSON_Delete(json);
	return rc;
}

static BOOL aad_send_token_request(rdpAad* aad, BIO* bio, const char* auth_code)
{
	BOOL rc = FALSE;

	char* req_body = NULL;
	char* req_header = NULL;
	size_t req_body_len = 0;
	size_t req_header_len = 0;
	const int trc = alloc_sprintf(&req_body, &req_body_len, token_http_request_body, auth_code,
	                              aad->hostname, aad->kid);
	if (trc < 0)
		goto fail;
	const int trh = alloc_sprintf(&req_header, &req_header_len, token_http_request_header, trc);
	if (trh < 0)
		goto fail;

	WLog_Print(aad->log, WLOG_DEBUG, "HTTP access token request: %s%s", req_header, req_body);

	if (!aad_logging_bio_write(aad, bio, req_header))
		goto fail;
	if (!aad_logging_bio_write(aad, bio, req_body))
		goto fail;
	rc = TRUE;
fail:
	free(req_body);
	free(req_header);
	return rc;
}

int aad_client_begin(rdpAad* aad)
{
	int ret = -1;
	SSL_CTX* ssl_ctx = NULL;
	BIO* bio = NULL;
	char* auth_code = NULL;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(aad->rdpcontext);

	rdpSettings* settings = aad->rdpcontext->settings;
	WINPR_ASSERT(settings);

	freerdp* instance = aad->rdpcontext->instance;
	WINPR_ASSERT(instance);

	/* Get the host part of the hostname */
	const char* hostname = freerdp_settings_get_string(settings, FreeRDP_ServerHostname);
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

	if (!generate_pop_key(aad))
		goto fail;

	/* Obtain an oauth authorization code */
	if (!instance->GetAadAuthCode)
	{
		WLog_Print(aad->log, WLOG_ERROR, "instance->GetAadAuthCode == NULL");
		goto fail;
	}
	const BOOL arc = instance->GetAadAuthCode(instance, aad->hostname, &auth_code);
	if (!arc)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Unable to obtain authorization code");
		goto fail;
	}

	/* Set up an ssl connection to the authorization server */
	ssl_ctx = SSL_CTX_new(TLS_client_method());
	if (!ssl_ctx)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Error setting up SSL context");
		goto fail;
	}

	bio = aad_connect_https(aad, ssl_ctx);
	if (!bio)
		goto fail;

	/* Construct and send the token request message */
	if (!aad_send_token_request(aad, bio, auth_code))
		goto fail;

	/* Extract the access token from the JSON response */
	if (!aad_read_and_extract_token_from_json(aad, bio))
		goto fail;

	/* Send the nonce request message */
	WLog_Print(aad->log, WLOG_DEBUG, "HTTP nonce request: %s", nonce_http_request);

	if (!aad_logging_bio_write(aad, bio, nonce_http_request))
		goto fail;

	/* Read in the response */
	if (!aad_read_and_extrace_nonce_from_json(aad, bio))
		goto fail;

	ret = 1;

fail:
	BIO_free_all(bio);
	SSL_CTX_free(ssl_ctx);
	free(auth_code);
	return ret;
}

static char* aad_create_jws_header(rdpAad* aad)
{
	WINPR_ASSERT(aad);

	/* Construct the base64url encoded JWS header */
	char* buffer = NULL;
	size_t bufferlen = 0;
	const int length =
	    alloc_sprintf(&buffer, &bufferlen, "{\"alg\":\"RS256\",\"kid\":\"%s\"}", aad->kid);
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
	if (!get_encoded_rsa_params(aad->log, aad->pop_key, &e, &n))
		return NULL;

	/* Construct the base64url encoded JWS payload */
	char* buffer = NULL;
	size_t bufferlen = 0;
	const int length =
	    alloc_sprintf(&buffer, &bufferlen,
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

static BOOL aad_update_digest(rdpAad* aad, EVP_MD_CTX* ctx, const char* what)
{
	WINPR_ASSERT(aad);
	WINPR_ASSERT(ctx);
	WINPR_ASSERT(what);

	const int dsu1 = EVP_DigestSignUpdate(ctx, what, strlen(what));
	if (dsu1 <= 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "EVP_DigestSignUpdate [%s] failed with %d", what, dsu1);
		return FALSE;
	}
	return TRUE;
}

static char* aad_final_digest(rdpAad* aad, EVP_MD_CTX* ctx)
{
	char* jws_signature = NULL;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(ctx);

	size_t siglen = 0;
	const int dsf = EVP_DigestSignFinal(ctx, NULL, &siglen);
	if (dsf <= 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "EVP_DigestSignFinal failed with %d", dsf);
		return FALSE;
	}

	char* buffer = calloc(siglen + 1, sizeof(char));
	if (!buffer)
	{
		WLog_Print(aad->log, WLOG_ERROR, "calloc %" PRIuz " bytes failed", siglen + 1);
		goto fail;
	}

	size_t fsiglen = siglen;
	const int dsf2 = EVP_DigestSignFinal(ctx, (BYTE*)buffer, &fsiglen);
	if (dsf2 <= 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "EVP_DigestSignFinal failed with %d", dsf2);
		goto fail;
	}

	if (siglen != fsiglen)
	{
		WLog_Print(aad->log, WLOG_ERROR,
		           "EVP_DigestSignFinal returned different sizes, first %" PRIuz " then %" PRIuz,
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

	EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
	if (!md_ctx)
	{
		WLog_Print(aad->log, WLOG_ERROR, "EVP_MD_CTX_new failed");
		goto fail;
	}

	const int rdsi = EVP_DigestSignInit(md_ctx, NULL, EVP_sha256(), NULL, aad->pop_key);
	if (rdsi <= 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "EVP_DigestSignInit failed with %d", rdsi);
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
	EVP_MD_CTX_free(md_ctx);
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
	cJSON* json = NULL;

	if (!Stream_SafeSeek(s, jlen))
		goto fail;

	json = compat_cJSON_ParseWithLength(jstr, jlen);
	if (!json)
		goto fail;

	if (!json_get_const_string(aad->log, json, "ts_nonce", &ts_nonce))
		goto fail;

	ret = aad_send_auth_request(aad, ts_nonce);
fail:
	cJSON_Delete(json);
	return ret;
}

static int aad_parse_state_auth(rdpAad* aad, wStream* s)
{
	int rc = -1;
	double result = 0;
	cJSON* json = NULL;
	const char* jstr = Stream_PointerAs(s, char);
	const size_t jlength = Stream_GetRemainingLength(s);

	if (!Stream_SafeSeek(s, jlength))
		goto fail;

	json = compat_cJSON_ParseWithLength(jstr, jlength);
	if (!json)
		goto fail;

	if (!json_get_number(aad->log, json, "authentication_result", &result))
		goto fail;

	if (result != 0.0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Authentication result: %lf", result);
		goto fail;
	}
	aad->state = AAD_STATE_FINAL;
	rc = 1;
fail:
	cJSON_Delete(json);
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

static BOOL read_http_message(rdpAad* aad, BIO* bio, long* status_code, char** content,
                              size_t* content_length)
{
	char buffer[1024] = { 0 };

	WINPR_ASSERT(aad);
	WINPR_ASSERT(status_code);
	WINPR_ASSERT(content);
	WINPR_ASSERT(content_length);

	*content_length = 0;
	const int rb = BIO_get_line(bio, buffer, sizeof(buffer));
	if (rb <= 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Error reading HTTP response [BIO_get_line %d]", rb);
		return FALSE;
	}

	if (sscanf(buffer, "HTTP/%*u.%*u %li %*[^\r\n]\r\n", status_code) < 1)
	{
		WLog_Print(aad->log, WLOG_ERROR, "Invalid HTTP response status line");
		return FALSE;
	}

	do
	{
		const int rbb = BIO_get_line(bio, buffer, sizeof(buffer));
		if (rbb <= 0)
		{
			WLog_Print(aad->log, WLOG_ERROR, "Error reading HTTP response [BIO_get_line %d]", rbb);
			return FALSE;
		}

		char* val = NULL;
		char* name = strtok_s(buffer, ":", &val);
		if (name && (_stricmp(name, "content-length") == 0))
		{
			errno = 0;
			*content_length = strtoul(val, NULL, 10);
			switch (errno)
			{
				case 0:
					break;
				default:
					WLog_Print(aad->log, WLOG_ERROR, "strtoul(%s) returned %s [%d]", val,
					           strerror(errno), errno);
					return FALSE;
			}
		}
	} while (strcmp(buffer, "\r\n") != 0);

	if (*content_length == 0)
		return TRUE;

	*content = calloc(*content_length + 1, sizeof(char));
	if (!*content)
		return FALSE;

	size_t offset = 0;
	while (offset < *content_length)
	{
		const size_t diff = *content_length - offset;
		int len = (int)diff;
		if (diff > INT_MAX)
			len = INT_MAX;
		const int brc = BIO_read(bio, &(*content)[offset], len);
		if (brc <= 0)
		{
			free(*content);
			WLog_Print(aad->log, WLOG_ERROR,
			           "Error reading HTTP response body (BIO_read returned %d)", brc);
			return FALSE;
		}
		offset += (size_t)brc;
	}

	return TRUE;
}

static BOOL generate_rsa_2048(rdpAad* aad)
{
	BOOL rc = FALSE;
	WINPR_ASSERT(aad);

	EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
	if (!aad)
	{
		WLog_Print(aad->log, WLOG_ERROR, "EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL) failed");
		goto fail;
	}

	const int rki = EVP_PKEY_keygen_init(ctx);
	if (rki <= 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "EVP_PKEY_keygen_init failed with %d", rki);
		goto fail;
	}

	const int key_bits = 2048;
	const int rkb = EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, key_bits);
	if (rkb <= 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "EVP_PKEY_CTX_set_rsa_keygen_bits(%d) failed with %d",
		           key_bits, rkb);
		goto fail;
	}

	const int rkg = EVP_PKEY_keygen(ctx, &aad->pop_key);
	if (rkg <= 0)
	{
		WLog_Print(aad->log, WLOG_ERROR, "EVP_PKEY_keygen failed with %d", rkg);
		goto fail;
	}

	rc = TRUE;
fail:

	EVP_PKEY_CTX_free(ctx);
	return rc;
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
	const int length = alloc_sprintf(&buffer, &blen, "{\"kid\":\"%s\"}", b64_hash);
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
	char *e = NULL, *n = NULL;

	WINPR_ASSERT(aad);

	/* Generate a 2048-bit RSA key pair */
	if (!generate_rsa_2048(aad))
		goto fail;

	/* Encode the public key as a JWK */
	if (!get_encoded_rsa_params(aad->log, aad->pop_key, &e, &n))
		goto fail;

	size_t blen = 0;
	const int alen =
	    alloc_sprintf(&buffer, &blen, "{\"e\":\"%s\",\"kty\":\"RSA\",\"n\":\"%s\"}", e, n);
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

static char* bn_to_base64_url(wLog* wlog, BIGNUM* bn)
{
	WINPR_ASSERT(wlog);
	WINPR_ASSERT(bn);

	const int length = BN_num_bytes(bn);
	if (length < 0)
	{
		WLog_Print(wlog, WLOG_ERROR, "BN_num_bytes failed with %d", length);
		return NULL;
	}

	const size_t alloc_size = (size_t)length + 1ull;
	BYTE* buf = calloc(alloc_size, sizeof(BYTE));
	if (!buf)
	{
		return NULL;
	}

	const int bnlen = BN_bn2bin(bn, buf);
	if (bnlen != length)
	{
		free(buf);
		WLog_Print(wlog, WLOG_ERROR, "BN_bn2bin returned %d, expected result %d", bnlen, length);
		return NULL;
	}
	char* b64 = crypto_base64url_encode(buf, (size_t)length);
	free(buf);

	if (!b64)
		WLog_Print(wlog, WLOG_ERROR, "failed  base64 url encode BIGNUM");

	return b64;
}

BOOL get_encoded_rsa_params(wLog* wlog, EVP_PKEY* pkey, char** pe, char** pn)
{
	BOOL rc = FALSE;
	BIGNUM *bn_e = NULL, *bn_n = NULL;
	char* e = NULL;
	char* n = NULL;

	WINPR_ASSERT(wlog);
	WINPR_ASSERT(pkey);
	WINPR_ASSERT(pe);
	WINPR_ASSERT(pn);

	*pe = NULL;
	*pn = NULL;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	if (!EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_E, &bn_e))
	{
		WLog_Print(wlog, WLOG_ERROR, "failed to get RSA E");
		goto fail;
	}
	if (!EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_N, &bn_n))
	{
		WLog_Print(wlog, WLOG_ERROR, "failed to get RSA N");
		goto fail;
	}
#else
	{
		const RSA* rsa = NULL;

		if (!(rsa = EVP_PKEY_get0_RSA(pkey)))
		{
			WLog_Print(wlog, WLOG_ERROR, "failed to get RSA");
			goto fail;
		}
		if (!(bn_e = BN_dup(RSA_get0_e(rsa))))
		{
			WLog_Print(wlog, WLOG_ERROR, "failed to get RSA E");
			goto fail;
		}
		if (!(bn_n = BN_dup(RSA_get0_n(rsa))))
		{
			WLog_Print(wlog, WLOG_ERROR, "failed to get RSA N");
			goto fail;
		}
	}
#endif

	e = bn_to_base64_url(wlog, bn_e);
	if (!e)
	{
		WLog_Print(wlog, WLOG_ERROR, "failed  base64 url encode RSA E");
		goto fail;
	}

	n = bn_to_base64_url(wlog, bn_n);
	if (!n)
	{
		WLog_Print(wlog, WLOG_ERROR, "failed  base64 url encode RSA N");
		goto fail;
	}

	rc = TRUE;
fail:
	BN_free(bn_e);
	BN_free(bn_n);
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
	aad->rdpcontext = context;
	aad->transport = transport;

	return aad;
}

void aad_free(rdpAad* aad)
{
	if (!aad)
		return;

	free(aad->hostname);
	free(aad->nonce);
	free(aad->access_token);
	free(aad->kid);
	EVP_PKEY_free(aad->pop_key);

	free(aad);
}

AAD_STATE aad_get_state(rdpAad* aad)
{
	WINPR_ASSERT(aad);
	return aad->state;
}

BOOL aad_is_supported(void)
{
#ifdef WITH_CJSON
	return TRUE;
#else
	return FALSE;
#endif
}
