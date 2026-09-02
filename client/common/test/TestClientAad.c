/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Tests for the client common AAD helpers
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

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/string.h>

#include <freerdp/client.h>
#include <freerdp/settings.h>

/* The field taken out of the reserved padding must not have changed the size of the struct. */
WINPR_STATIC_ASSERT(sizeof(rdpClientContext) - offsetof(rdpClientContext, aad_oauth) ==
                    (129 - 16) * sizeof(UINT64));

/* Every case below needs a cached OpenID configuration to stay offline, and the only way to
 * install one is freerdp_utils_aad_set_wellknown(), which is core-private (FREERDP_LOCAL) and
 * therefore only linkable when BUILD_TESTING_INTERNAL forces EXPORT_ALL_SYMBOLS. A normal
 * BUILD_TESTING=ON build skips the whole body; CI enables BUILD_TESTING_INTERNAL. */
#if defined(WITH_AAD) && defined(BUILD_TESTING_INTERNAL)

#include <winpr/custom-crypto.h>

#include <freerdp/crypto/crypto.h>
#include <freerdp/utils/aad.h>

#include "../../../libfreerdp/core/aad.h"

/* A synthetic OpenID configuration, so no test needs to reach the network. */
static const char wellknown[] =
    "{\"authorization_endpoint\":\"https://login.contoso.example/common/oauth2/v2.0/"
    "authorize\",\"token_endpoint\":\"https://login.contoso.example/common/oauth2/v2.0/token\"}";

/* The redirect URI the tests configure, percent encoded as the settings hold it. */
static const char redirect_fmt[] = "https%%3A%%2F%%2Fcontoso.example%%2Fcallback";

static rdpContext* test_context_new(void)
{
	RDP_CLIENT_ENTRY_POINTS entry = WINPR_C_ARRAY_INIT;

	entry.Size = sizeof(entry);
	entry.Version = RDP_CLIENT_INTERFACE_VERSION;
	entry.ContextSize = sizeof(rdpClientContext);

	rdpContext* context = freerdp_client_context_new(&entry);
	if (!context)
	{
		(void)fprintf(stderr, "failed to create a client context\n");
		return nullptr;
	}

	if (!freerdp_utils_aad_set_wellknown(context, wellknown))
	{
		(void)fprintf(stderr, "failed to install the wellknown document\n");
		freerdp_client_context_free(context);
		return nullptr;
	}

	if (!freerdp_settings_set_string(context->settings, FreeRDP_GatewayAvdAccessTokenFormat,
	                                 redirect_fmt))
	{
		(void)fprintf(stderr, "failed to set the redirect URI format\n");
		freerdp_client_context_free(context);
		return nullptr;
	}

	return context;
}

/** Returns the value of parameter @p name of the query of @p url, or of @p url itself if it
 *  is a request body without a query separator. */
static char* query_value(const char* url, const char* name)
{
	const size_t namelen = strlen(name);
	const char* query = strchr(url, '?');
	const char* pos = query ? query + 1 : url;

	while (pos)
	{
		if ((strncmp(pos, name, namelen) == 0) && (pos[namelen] == '='))
		{
			pos += namelen + 1;
			const char* end = strchr(pos, '&');
			const size_t len = end ? (size_t)(end - pos) : strlen(pos);
			return strndup(pos, len);
		}

		pos = strchr(pos, '&');
		if (pos)
			pos++;
	}

	return nullptr;
}

static BOOL is_base64url(const char* what, const char* str, size_t expected)
{
	if (!str)
	{
		(void)fprintf(stderr, "%s is missing\n", what);
		return FALSE;
	}

	const size_t len = strlen(str);
	if (len != expected)
	{
		(void)fprintf(stderr, "%s: expected %" PRIuz " characters, got %" PRIuz "\n", what,
		              expected, len);
		return FALSE;
	}

	for (size_t x = 0; x < len; x++)
	{
		const char cur = str[x];
		const BOOL ok = ((cur >= 'a') && (cur <= 'z')) || ((cur >= 'A') && (cur <= 'Z')) ||
		                ((cur >= '0') && (cur <= '9')) || (cur == '-') || (cur == '_');
		if (!ok)
		{
			(void)fprintf(stderr, "%s: unexpected character\n", what);
			return FALSE;
		}
	}

	return TRUE;
}

/* Item 2: the authorization request carries a state value and a PKCE S256 challenge, the token
 * request the matching verifier. */
static BOOL test_pkce(void)
{
	BOOL rc = FALSE;
	char* url = nullptr;
	char* token = nullptr;
	char* state = nullptr;
	char* challenge = nullptr;
	char* method = nullptr;
	char* verifier = nullptr;
	char* expected = nullptr;
	BYTE hash[WINPR_SHA256_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;

	rdpContext* context = test_context_new();
	if (!context)
		return FALSE;

	url =
	    freerdp_client_get_aad_url((rdpClientContext*)context, FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST);
	if (!url)
	{
		(void)fprintf(stderr, "no authorization request\n");
		goto fail;
	}

	state = query_value(url, "state");
	challenge = query_value(url, "code_challenge");
	method = query_value(url, "code_challenge_method");

	if (!is_base64url("state", state, 43) || !is_base64url("code_challenge", challenge, 43))
		goto fail;

	if (!method || (strcmp(method, "S256") != 0))
	{
		(void)fprintf(stderr, "code_challenge_method is not S256\n");
		goto fail;
	}

	token = freerdp_client_get_aad_url((rdpClientContext*)context,
	                                   FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST, "a+b c");
	if (!token)
	{
		(void)fprintf(stderr, "no token request\n");
		goto fail;
	}

	verifier = query_value(token, "code_verifier");
	if (!is_base64url("code_verifier", verifier, 43))
		goto fail;

	/* The challenge must be the base64url encoded SHA-256 of the verifier. */
	if (!winpr_Digest(WINPR_MD_SHA256, verifier, strlen(verifier), hash, sizeof(hash)))
		goto fail;

	expected = crypto_base64url_encode(hash, sizeof(hash));
	if (!expected || (strcmp(expected, challenge) != 0))
	{
		(void)fprintf(stderr, "the challenge is not the SHA-256 of the verifier\n");
		goto fail;
	}

	/* The authorization code is escaped before it is put into the request body. */
	if (!strstr(token, "code=a%2Bb%20c&"))
	{
		(void)fprintf(stderr, "the authorization code was not escaped\n");
		goto fail;
	}

	rc = TRUE;
fail:
	free(url);
	free(token);
	free(state);
	free(challenge);
	free(method);
	free(verifier);
	free(expected);
	freerdp_client_context_free(context);
	return rc;
}

/* Item 2: every authorization request replaces the previous transaction, the verifier is sent
 * with one token request, and a token request built without a transaction carries none. */
static BOOL test_transaction_lifetime(void)
{
	BOOL rc = FALSE;
	char* first = nullptr;
	char* second = nullptr;
	char* state1 = nullptr;
	char* state2 = nullptr;
	char* token = nullptr;

	rdpContext* context = test_context_new();
	if (!context)
		return FALSE;

	/* A caller that builds the authorization URL itself gets an unchanged token request. */
	token = freerdp_client_get_aad_url((rdpClientContext*)context,
	                                   FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST, "code");
	if (!token || strstr(token, "code_verifier"))
	{
		(void)fprintf(stderr, "a token request without a transaction has a verifier\n");
		goto fail;
	}

	first =
	    freerdp_client_get_aad_url((rdpClientContext*)context, FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST);
	second =
	    freerdp_client_get_aad_url((rdpClientContext*)context, FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST);
	if (!first || !second)
		goto fail;

	state1 = query_value(first, "state");
	state2 = query_value(second, "state");
	if (!state1 || !state2 || (strcmp(state1, state2) == 0))
	{
		(void)fprintf(stderr, "two authorization requests share a state value\n");
		goto fail;
	}

	/* The token request of the transaction carries the verifier, a second one does not: the
	 * authorization code it belongs to is redeemed once. */
	free(token);
	token = freerdp_client_get_aad_url((rdpClientContext*)context,
	                                   FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST, "code");
	if (!token || !strstr(token, "code_verifier"))
	{
		(void)fprintf(stderr, "the token request of a transaction has no verifier\n");
		goto fail;
	}

	free(token);
	token = freerdp_client_get_aad_url((rdpClientContext*)context,
	                                   FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST, "code");
	if (!token || strstr(token, "code_verifier"))
	{
		(void)fprintf(stderr, "a verifier was sent with a second token request\n");
		goto fail;
	}

	/* After a reset no verifier is sent any more. */
	freerdp_client_aad_reset((rdpClientContext*)context);
	free(token);
	token = freerdp_client_get_aad_url((rdpClientContext*)context,
	                                   FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST, "code");
	if (!token || strstr(token, "code_verifier"))
	{
		(void)fprintf(stderr, "a token request after a reset has a verifier\n");
		goto fail;
	}

	rc = TRUE;
fail:
	free(first);
	free(second);
	free(state1);
	free(state2);
	free(token);
	freerdp_client_context_free(context);
	return rc;
}

/* Item 2: a token request that could not be built leaves the transaction in place, so the
 * verifier is still there when the caller tries again. */
static BOOL test_token_build_failure(void)
{
	BOOL rc = FALSE;
	char* url = nullptr;
	char* token = nullptr;
	const char* nocode = nullptr;

	rdpContext* context = test_context_new();
	if (!context)
		return FALSE;

	url =
	    freerdp_client_get_aad_url((rdpClientContext*)context, FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST);
	if (!url)
		goto fail;

	/* Neither request can be built without an authorization code. */
	token = freerdp_client_get_aad_url((rdpClientContext*)context, FREERDP_CLIENT_AAD_TOKEN_REQUEST,
	                                   "scope", nocode, "cnf");
	if (token)
	{
		(void)fprintf(stderr, "an AAD token request without an authorization code was built\n");
		goto fail;
	}

	token = freerdp_client_get_aad_url((rdpClientContext*)context,
	                                   FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST, nocode);
	if (token)
	{
		(void)fprintf(stderr, "an AVD token request without an authorization code was built\n");
		goto fail;
	}

	/* Nothing was sent, so the verifier of the transaction is still available. */
	token = freerdp_client_get_aad_url((rdpClientContext*)context,
	                                   FREERDP_CLIENT_AAD_AVD_TOKEN_REQUEST, "code");
	if (!token || !strstr(token, "code_verifier"))
	{
		(void)fprintf(stderr, "a token request that failed to build lost the code verifier\n");
		goto fail;
	}

	rc = TRUE;
fail:
	free(url);
	free(token);
	freerdp_client_context_free(context);
	return rc;
}

/* Item 1: the redirect URI format strings are printf formats taken from settings. */
static BOOL test_redirect_format(void)
{
	static const struct
	{
		const char* fmt;
		BOOL valid;
	} tests[] = {
		{ "https%%3A%%2F%%2Fcontoso.example%%2Fcallback%%2F%s", TRUE }, /* one conversion */
		{ "https%%3A%%2F%%2Fcontoso.example%%2Fcallback", TRUE },       /* fixed URI */
		{ "%s", TRUE },
		{ "%s%s", FALSE },                     /* more arguments than the caller pushes */
		{ "%s%s%s", FALSE },                   /* ... */
		{ "%d", FALSE },                       /* wrong conversion */
		{ "%n", FALSE },                       /* writes through the argument */
		{ "%1$s", FALSE },                     /* positional argument */
		{ "%ls", FALSE },                      /* length modifier */
		{ "%.100s", FALSE },                   /* precision */
		{ "%p", FALSE },                       /* wrong conversion */
		{ "https://contoso.example/%", FALSE } /* trailing '%' */
	};

	BOOL rc = FALSE;
	rdpContext* context = test_context_new();
	if (!context)
		return FALSE;

	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		if (!freerdp_settings_set_string(context->settings, FreeRDP_GatewayAvdAccessTokenFormat,
		                                 tests[x].fmt))
			goto fail;

		char* url = freerdp_client_get_aad_url((rdpClientContext*)context,
		                                       FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST);
		const BOOL got = url != nullptr;
		free(url);

		if (got != tests[x].valid)
		{
			(void)fprintf(stderr, "format [%" PRIuz "] '%s': expected %s, got %s\n", x,
			              tests[x].fmt, tests[x].valid ? "accepted" : "rejected",
			              got ? "accepted" : "rejected");
			goto fail;
		}
	}

	rc = TRUE;
fail:
	freerdp_client_context_free(context);
	return rc;
}

/* Item 1: the tenant identifier is expanded into a URL. */
static BOOL test_tenantid(void)
{
	static const struct
	{
		const char* tenantid;
		BOOL valid;
	} tests[] = {
		{ "00000000-0000-0000-0000-000000000000", TRUE },
		{ "contoso.example", TRUE },
		{ "common", TRUE },
		{ "", FALSE },
		{ "contoso example", FALSE },   /* space */
		{ "contoso%2Fexample", FALSE }, /* percent escape */
		{ "contoso/example", FALSE },
		{ "contoso?example", FALSE },
		{ "contoso#example", FALSE },
		{ "contoso\xc3\xa9example", FALSE }, /* not ASCII, isalnum() is locale dependent */
		{ ".", FALSE },                      /* a path element of the URL the tenant goes into */
		{ "..", FALSE },
		{ "0123456789012345678901234567890123456789012345678901234567890123456789012345678901234567"
		  "8901234567890123456789012345678901234567890",
		  FALSE } /* 129 characters */
	};

	BOOL rc = FALSE;
	rdpContext* context = test_context_new();
	if (!context)
		return FALSE;

	/* The stdio callbacks select the tenant based redirect URI format. */
	if (!freerdp_settings_set_bool(context->settings, FreeRDP_UseCommonStdioCallbacks, TRUE))
		goto fail;
	if (!freerdp_settings_set_bool(context->settings, FreeRDP_GatewayAvdUseTenantid, TRUE))
		goto fail;

	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		if (!freerdp_settings_set_string(context->settings, FreeRDP_GatewayAvdAadtenantid,
		                                 tests[x].tenantid))
			goto fail;

		char* url = freerdp_client_get_aad_url((rdpClientContext*)context,
		                                       FREERDP_CLIENT_AAD_AVD_AUTH_REQUEST);
		const BOOL got = url != nullptr;
		free(url);

		if (got != tests[x].valid)
		{
			(void)fprintf(stderr, "tenantid [%" PRIuz "]: expected %s, got %s\n", x,
			              tests[x].valid ? "accepted" : "rejected", got ? "accepted" : "rejected");
			goto fail;
		}
	}

	rc = TRUE;
fail:
	freerdp_client_context_free(context);
	return rc;
}

#endif /* WITH_AAD && BUILD_TESTING_INTERNAL */

int TestClientAad(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#if !defined(WITH_AAD)
	(void)fprintf(stderr, "Build does not support AAD authentication, skipping\n");
	return 0;
#elif !defined(BUILD_TESTING_INTERNAL)
	(void)fprintf(stderr, "Build is not BUILD_TESTING_INTERNAL, skipping\n");
	return 0;
#else
	if (!test_redirect_format())
		return -1;
	if (!test_tenantid())
		return -1;
	if (!test_pkce())
		return -1;
	if (!test_transaction_lifetime())
		return -1;
	if (!test_token_build_failure())
		return -1;
	return 0;
#endif
}
