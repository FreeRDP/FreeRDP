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

#include <freerdp/crypto/crypto.h>
#include <winpr/crypto.h>
#include "../utils/cJSON/cJSON.h"

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
#include <openssl/core_names.h>
#endif

#include "transport.h"

#include "aad.h"

#define TAG FREERDP_TAG("aad")
#define LOG_ERROR_AND_GOTO(label, ...) \
	do                                 \
	{                                  \
		WLog_ERR(TAG, __VA_ARGS__);    \
		goto label;                    \
	} while (0);
#define LOG_ERROR_AND_RETURN(ret, ...) \
	do                                 \
	{                                  \
		WLog_ERR(TAG, __VA_ARGS__);    \
		return ret;                    \
	} while (0);
#define XFREE(x)  \
	do            \
	{             \
		free(x);  \
		x = NULL; \
	} while (0);

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

struct rdp_aad
{
	enum AAD_STATE state;
	rdpContext* rdpcontext;
	rdpTransport* transport;
	char* access_token;
	EVP_PKEY* pop_key;
	char* kid;
	char* nonce;
	char* hostname;
};

static int alloc_sprintf(char** s, const char* template, ...);
static BOOL get_encoded_rsa_params(EVP_PKEY* pkey, char** e, char** n);
static BOOL generate_pop_key(rdpAad* aad);
static BOOL read_http_message(BIO* bio, long* status_code, char** content, size_t* content_length);

static int print_error(const char* str, size_t len, void* u)
{
	WLog_ERR(TAG, "%s", str);
	return 1;
}

rdpAad* aad_new(rdpContext* context, rdpTransport* transport)
{
	WINPR_ASSERT(transport);
	WINPR_ASSERT(context);

	rdpAad* aad = (rdpAad*)calloc(1, sizeof(rdpAad));

	if (!aad)
		return NULL;

	aad->rdpcontext = context;
	aad->transport = transport;

	return aad;
}

int aad_client_begin(rdpAad* aad)
{
	int ret = -1;
	SSL_CTX* ssl_ctx = NULL;
	BIO* bio = NULL;
	char* auth_code = NULL;
	char *buffer = NULL, *req_header = NULL, *req_body = NULL;
	size_t length = 0;
	const char* hostname = NULL;
	char* p = NULL;
	long status_code;
	cJSON* json = NULL;
	cJSON* prop = NULL;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(aad->rdpcontext);

	rdpSettings* settings = aad->rdpcontext->settings;
	WINPR_ASSERT(settings);

	freerdp* instance = aad->rdpcontext->instance;
	WINPR_ASSERT(instance);

	/* Get the host part of the hostname */
	hostname = freerdp_settings_get_string(settings, FreeRDP_ServerHostname);
	if (!hostname || !(aad->hostname = _strdup(hostname)))
		LOG_ERROR_AND_GOTO(fail, "Unable to get hostname");
	if ((p = strchr(aad->hostname, '.')))
		*p = '\0';

	if (!generate_pop_key(aad))
		LOG_ERROR_AND_GOTO(fail, "Unable to generate pop key");

	/* Obtain an oauth authorization code */
	if (!instance->GetAadAuthCode || !instance->GetAadAuthCode(aad->hostname, &auth_code))
		LOG_ERROR_AND_GOTO(fail, "Unable to obtain authorization code");

	/* Set up an ssl connection to the authorization server */
	if (!(ssl_ctx = SSL_CTX_new(TLS_client_method())))
		LOG_ERROR_AND_GOTO(fail, "Error setting up SSL context");
	SSL_CTX_set_default_verify_paths(ssl_ctx);
	SSL_CTX_set_mode(ssl_ctx, SSL_MODE_AUTO_RETRY);

	if (!(bio = BIO_new_ssl_connect(ssl_ctx)))
		LOG_ERROR_AND_GOTO(fail, "Error setting up connection");
	BIO_set_conn_hostname(bio, auth_server);
	BIO_set_conn_port(bio, "https");

	/* Construct and send the token request message */
	length = alloc_sprintf(&req_body, token_http_request_body, auth_code, aad->hostname, aad->kid);
	if (length < 0)
		goto fail;
	if (alloc_sprintf(&req_header, token_http_request_header, length) < 0)
		goto fail;

	WLog_DBG(TAG, "HTTP access token request: %s%s", req_header, req_body);

	ERR_clear_error();
	if (BIO_write(bio, req_header, strlen(req_header)) < 0)
	{
		ERR_print_errors_cb(print_error, NULL);
		goto fail;
	}

	ERR_clear_error();
	if (BIO_write(bio, req_body, strlen(req_body)) < 0)
	{
		ERR_print_errors_cb(print_error, NULL);
		goto fail;
	}

	/* Read in the response */
	if (!read_http_message(bio, &status_code, &buffer, &length))
		LOG_ERROR_AND_GOTO(fail, "Unable to read access token HTTP response");
	WLog_DBG(TAG, "HTTP access token response: %s", buffer);

	if (status_code != 200)
		LOG_ERROR_AND_GOTO(fail, "Received status code: %li", status_code);

	/* Extract the access token from the JSON response */
	if (!(json = cJSON_Parse(buffer)))
		LOG_ERROR_AND_GOTO(fail, "Failed to parse JSON response");
	if (!(prop = cJSON_GetObjectItem(json, "access_token")) || !cJSON_IsString(prop))
		LOG_ERROR_AND_GOTO(fail, "Could not find \"access_token\" property in JSON response");
	if (!(aad->access_token = _strdup(cJSON_GetStringValue(prop))))
		goto fail;

	XFREE(buffer);
	cJSON_Delete(json);
	json = NULL;

	/* Send the nonce request message */
	WLog_DBG(TAG, "HTTP nonce request: %s", nonce_http_request);

	ERR_clear_error();
	if (BIO_write(bio, nonce_http_request, strlen(nonce_http_request)) < 0)
	{
		ERR_print_errors_cb(print_error, NULL);
		goto fail;
	}

	/* Read in the response */
	if (!read_http_message(bio, &status_code, &buffer, &length))
		LOG_ERROR_AND_GOTO(fail, "Unable to read HTTP response");
	WLog_DBG(TAG, "HTTP nonce response: %s", buffer);

	if (status_code != 200)
		LOG_ERROR_AND_GOTO(fail, "Received status code: %li", status_code);

	/* Extract the nonce from the response */
	if (!(json = cJSON_Parse(buffer)))
		LOG_ERROR_AND_GOTO(fail, "Failed to parse JSON response");
	if (!(prop = cJSON_GetObjectItem(json, "Nonce")) || !cJSON_IsString(prop))
		LOG_ERROR_AND_GOTO(fail, "Could not find \"Nonce\" property in JSON response");
	if (!(aad->nonce = _strdup(cJSON_GetStringValue(prop))))
		goto fail;

	ret = 1;

fail:
	cJSON_Delete(json);
	free(buffer);
	free(req_body);
	free(req_header);
	BIO_free_all(bio);
	SSL_CTX_free(ssl_ctx);
	free(auth_code);
	return ret;
}

static int aad_send_auth_request(rdpAad* aad, const char* ts_nonce)
{
	int ret = -1;
	char* jws_header = NULL;
	char* jws_payload = NULL;
	char* jws_signature = NULL;
	char* buffer = NULL;
	wStream* s = NULL;
	time_t ts = time(NULL);
	char *e = NULL, *n = NULL;
	size_t length = 0;
	EVP_MD_CTX* md_ctx = NULL;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(ts_nonce);

	/* Construct the base64url encoded JWS header */
	if ((length = alloc_sprintf(&buffer, "{\"alg\":\"RS256\",\"kid\":\"%s\"}", aad->kid)) < 0)
		goto fail;
	if (!(jws_header = crypto_base64url_encode((BYTE*)buffer, strlen(buffer))))
		goto fail;
	XFREE(buffer);

	if (!get_encoded_rsa_params(aad->pop_key, &e, &n))
		LOG_ERROR_AND_GOTO(fail, "Error getting RSA key params");

	/* Construct the base64url encoded JWS payload */
	length = alloc_sprintf(&buffer,
	                       "{"
	                       "\"ts\":\"%li\","
	                       "\"at\":\"%s\","
	                       "\"u\":\"ms-device-service://termsrv.wvd.microsoft.com/name/%s\","
	                       "\"nonce\":\"%s\","
	                       "\"cnf\":{\"jwk\":{\"kty\":\"RSA\",\"e\":\"%s\",\"n\":\"%s\"}},"
	                       "\"client_claims\":\"{\\\"aad_nonce\\\":\\\"%s\\\"}\""
	                       "}",
	                       ts, aad->access_token, aad->hostname, ts_nonce, e, n, aad->nonce);
	if (length < 0)
		goto fail;
	if (!(jws_payload = crypto_base64url_encode((BYTE*)buffer, strlen(buffer))))
		goto fail;
	XFREE(buffer);

	/* Sign the JWS with the pop key */
	if (!(md_ctx = EVP_MD_CTX_new()))
		goto fail;

	if (!(EVP_DigestSignInit(md_ctx, NULL, EVP_sha256(), NULL, aad->pop_key)))
		LOG_ERROR_AND_GOTO(fail, "Error while initializing signature context");

	if (!(EVP_DigestSignUpdate(md_ctx, jws_header, strlen(jws_header))))
		LOG_ERROR_AND_GOTO(fail, "Error while signing data");
	if (!(EVP_DigestSignUpdate(md_ctx, ".", 1)))
		LOG_ERROR_AND_GOTO(fail, "Error while signing data");
	if (!(EVP_DigestSignUpdate(md_ctx, jws_payload, strlen(jws_payload))))
		LOG_ERROR_AND_GOTO(fail, "Error while signing data");

	if (!(EVP_DigestSignFinal(md_ctx, NULL, &length)))
		LOG_ERROR_AND_GOTO(fail, "Error while signing data");

	if (!(buffer = malloc(length)))
		goto fail;
	if (!(EVP_DigestSignFinal(md_ctx, (BYTE*)buffer, &length)))
		LOG_ERROR_AND_GOTO(fail, "Error while signing data");

	if (!(jws_signature = crypto_base64url_encode((BYTE*)buffer, length)))
		goto fail;

	/* Construct the Authentication Request PDU with the JWS as the RDP Assertion */
	length = _snprintf(NULL, 0, "{\"rdp_assertion\":\"%s.%s.%s\"}", jws_header, jws_payload,
	                   jws_signature) +
	         1;
	if (length < 0)
		goto fail;

	if (!(s = Stream_New(NULL, length)))
		goto fail;
	_snprintf(Stream_PointerAs(s, char), length, "{\"rdp_assertion\":\"%s.%s.%s\"}", jws_header,
	          jws_payload, jws_signature);
	Stream_Seek(s, length);

	if (transport_write(aad->transport, s) < 0)
		LOG_ERROR_AND_GOTO(fail, "Failed to send Authentication Request PDU");

	ret = 1;
	aad->state = AAD_STATE_AUTH;

fail:
	Stream_Free(s, TRUE);
	free(e);
	free(n);
	free(buffer);
	free(jws_header);
	free(jws_payload);
	free(jws_signature);
	EVP_MD_CTX_free(md_ctx);
	return ret;
}

int aad_recv(rdpAad* aad, wStream* s)
{
	cJSON* json;
	cJSON* prop;

	WINPR_ASSERT(aad);
	WINPR_ASSERT(s);

	if (aad->state == AAD_STATE_INITIAL)
	{
		int ret = 0;

		if (!(json = cJSON_Parse(Stream_PointerAs(s, char))))
			LOG_ERROR_AND_RETURN(-1, "Failed to parse Server Nonce PDU");

		if (!(prop = cJSON_GetObjectItem(json, "ts_nonce")) || !cJSON_IsString(prop))
		{
			cJSON_Delete(json);
			WLog_ERR(TAG, "Failed to find ts_nonce in PDU");
			return -1;
		}

		Stream_Seek(s, Stream_Length(s));

		ret = aad_send_auth_request(aad, cJSON_GetStringValue(prop));
		cJSON_Delete(json);
		return ret;
	}
	else if (aad->state == AAD_STATE_AUTH)
	{
		double result = 0;

		if (!(json = cJSON_Parse(Stream_PointerAs(s, char))))
			LOG_ERROR_AND_RETURN(-1, "Failed to parse Authentication Result PDU");

		if (!(prop = cJSON_GetObjectItem(json, "authentication_result")) || !cJSON_IsNumber(prop))
		{
			cJSON_Delete(json);
			WLog_ERR(TAG, "Failed to find authentication_result in PDU");
			return -1;
		}
		result = cJSON_GetNumberValue(prop);

		cJSON_Delete(json);

		Stream_Seek(s, Stream_Length(s));

		if (result != 0)
			LOG_ERROR_AND_RETURN(-1, "Authentication result: %d", (int)result);

		aad->state = AAD_STATE_FINAL;
		return 1;
	}
	else
		LOG_ERROR_AND_RETURN(-1, "Invalid state");
}

enum AAD_STATE aad_get_state(rdpAad* aad)
{
	if (!aad)
		return AAD_STATE_FINAL;
	return aad->state;
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

static BOOL read_http_message(BIO* bio, long* status_code, char** content, size_t* content_length)
{
	char buffer[1024] = { 0 };

	WINPR_ASSERT(status_code);
	WINPR_ASSERT(content);
	WINPR_ASSERT(content_length);

	if (BIO_get_line(bio, buffer, sizeof(buffer)) <= 0)
		LOG_ERROR_AND_RETURN(FALSE, "Error reading HTTP response");

	if (sscanf(buffer, "HTTP/%*u.%*u %li %*[^\r\n]\r\n", status_code) < 1)
		LOG_ERROR_AND_RETURN(FALSE, "Invalid HTTP response status line");

	do
	{
		char* name = NULL;
		char* val = NULL;

		if (BIO_get_line(bio, buffer, sizeof(buffer)) <= 0)
			LOG_ERROR_AND_RETURN(FALSE, "Error reading HTTP response");

		name = strtok_r(buffer, ":", &val);
		if (name && _stricmp(name, "content-length") == 0)
			*content_length = strtoul(val, NULL, 10);
	} while (strcmp(buffer, "\r\n") != 0);

	if (*content_length == 0)
		return TRUE;

	if (!(*content = malloc(*content_length + 1)))
		return FALSE;
	(*content)[*content_length] = '\0';

	if (BIO_read(bio, *content, *content_length) < *content_length)
	{
		free(*content);
		LOG_ERROR_AND_RETURN(FALSE, "Error reading HTTP response body");
	}

	return TRUE;
}

static BOOL generate_pop_key(rdpAad* aad)
{
	EVP_PKEY_CTX* ctx = NULL;
	BOOL ret = FALSE;
	size_t length = 0;
	char* buffer = NULL;
	char *e = NULL, *n = NULL;
	WINPR_DIGEST_CTX* digest = NULL;
	BYTE hash[WINPR_SHA256_DIGEST_LENGTH] = { 0 };

	WINPR_ASSERT(aad);

	/* Generate a 2048-bit RSA key pair */
	if (!(ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL)))
		return FALSE;

	if (EVP_PKEY_keygen_init(ctx) <= 0)
		LOG_ERROR_AND_GOTO(fail, "Error initializing keygen");
	if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0)
		LOG_ERROR_AND_GOTO(fail, "Error setting RSA keygen bits");
	if (EVP_PKEY_keygen(ctx, &aad->pop_key) <= 0)
		LOG_ERROR_AND_GOTO(fail, "Error generating RSA pop token key");

	/* Encode the public key as a JWK */
	if (!get_encoded_rsa_params(aad->pop_key, &e, &n))
		LOG_ERROR_AND_GOTO(fail, "Error getting RSA key params");

	if ((length = alloc_sprintf(&buffer, "{\"e\":\"%s\",\"kty\":\"RSA\",\"n\":\"%s\"}", e, n)) < 0)
		goto fail;

	/* Hash the encoded public key */
	if (!(digest = winpr_Digest_New()))
		goto fail;

	if (!winpr_Digest_Init(digest, WINPR_MD_SHA256))
		LOG_ERROR_AND_GOTO(fail, "Error initializing SHA256 digest");
	if (!winpr_Digest_Update(digest, (BYTE*)buffer, length))
		LOG_ERROR_AND_GOTO(fail, "Unable to get hash of JWK");
	if (!winpr_Digest_Final(digest, hash, WINPR_SHA256_DIGEST_LENGTH))
		LOG_ERROR_AND_GOTO(fail, "Unable to get hash of JWK");

	XFREE(buffer);

	/* Base64url encode the hash */
	if (!(buffer = crypto_base64url_encode(hash, WINPR_SHA256_DIGEST_LENGTH)))
		goto fail;

	/* Encode a JSON object with a single property "kid" whose value is the encoded hash */
	{
		char* buf2 = NULL;
		if ((length = alloc_sprintf(&buf2, "{\"kid\":\"%s\"}", buffer)) < 0)
			goto fail;
		free(buffer);
		buffer = buf2;
	}

	/* Finally, base64url encode the JSON text to form the kid */
	if (!(aad->kid = crypto_base64url_encode((BYTE*)buffer, length)))
		LOG_ERROR_AND_GOTO(fail, "Error base64url encoding kid");

	ret = TRUE;

fail:
	free(buffer);
	free(e);
	free(n);
	winpr_Digest_Free(digest);
	EVP_PKEY_CTX_free(ctx);
	return ret;
}

static BOOL get_encoded_rsa_params(EVP_PKEY* pkey, char** e, char** n)
{
	BIGNUM *bn_e = NULL, *bn_n = NULL;
	BYTE buf[2048];
	size_t length = 0;

	*e = NULL;
	*n = NULL;

#if OPENSSL_VERSION_NUMBER >= 0x30000000L
	if (!EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_E, &bn_e))
		goto fail;
	if (!EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_RSA_N, &bn_n))
		goto fail;
#else
	{
		const RSA* rsa = NULL;

		if (!(rsa = EVP_PKEY_get0_RSA(pkey)))
			goto fail;
		if (!(bn_e = BN_dup(RSA_get0_e(rsa))))
			goto fail;
		if (!(bn_n = BN_dup(RSA_get0_n(rsa))))
			goto fail;
	}
#endif

	length = BN_num_bytes(bn_e);
	if (length > sizeof(buf))
		goto fail;
	if (BN_bn2bin(bn_e, buf) < length)
		goto fail;
	*e = crypto_base64url_encode(buf, length);

	length = BN_num_bytes(bn_n);
	if (length > sizeof(buf))
		goto fail;
	if (BN_bn2bin(bn_n, buf) < length)
		goto fail;
	*n = crypto_base64url_encode(buf, length);

fail:
	BN_free(bn_e);
	BN_free(bn_n);
	if (!(*e) || !(*n))
	{
		free(*e);
		free(*n);
		return FALSE;
	}
	return TRUE;
}

static int alloc_sprintf(char** s, const char* template, ...)
{
	int length;
	va_list ap;

	WINPR_ASSERT(s);
	*s = NULL;

	va_start(ap, template);
	length = vsnprintf(NULL, 0, template, ap);
	va_end(ap);

	if (!(*s = malloc(length + 1)))
		return -1;

	va_start(ap, template);
	vsprintf(*s, template, ap);
	va_end(ap);

	return length;
}
