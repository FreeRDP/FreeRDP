/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Tests for the HTTP debug log redaction
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

#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>

#include "../http.h"

/* The shapes below are the ones observed against Azure Virtual Desktop: the token request of
 * client_common_get_access_token(), the token response it parses, the nonce exchange of
 * aad_get_nonce() and the OpenID discovery document of freerdp_utils_aad_get_wellknown(). */

typedef struct
{
	const char* name;
	const char* in;
	const char* expected;
} redact_test;

static const redact_test tests[] = {
	/* Authorization code redemption. The code and the PKCE verifier are credentials, the grant
	 * type is not, even though its value ends in 'code'. */
	{ "token request",
	  "grant_type=authorization_code&code=0.AXkAq2ZDeF_Aut&client_id=a85cf173-4192-42f8-81fa-"
	  "777a763e6e2c&redirect_uri=https%3A%2F%2Flogin.microsoftonline.com%2Fcommon%2Foauth2%"
	  "2Fnativeclient&code_verifier=dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk",
	  "grant_type=authorization_code&code=<redacted, 16 bytes>&client_id=a85cf173-4192-42f8-81fa-"
	  "777a763e6e2c&redirect_uri=https%3A%2F%2Flogin.microsoftonline.com%2Fcommon%2Foauth2%"
	  "2Fnativeclient&code_verifier=<redacted, 43 bytes>" },
	/* Refresh, client secret and assertion grants use the same body shape. */
	{ "refresh request",
	  "grant_type=refresh_token&refresh_token=0.AXkAbQ&client_secret=Xy7~8Q&assertion=eyJhbGci",
	  "grant_type=refresh_token&refresh_token=<redacted, 8 bytes>&client_secret=<redacted, 6 "
	  "bytes>&assertion=<redacted, 8 bytes>" },
	/* Token response. token_type, scope and the expiry stay readable. */
	{ "token response",
	  "{\"token_type\":\"Bearer\",\"scope\":\"https://www.wvd.azure.us/.default\",\"expires_in\":"
	  "3599,\"ext_expires_in\":3599,\"access_token\":\"eyJ0eXAiOiJKV1Qi\",\"refresh_token\":\"0."
	  "AXkAq2ZDeF\",\"id_token\":\"eyJhbGciOiJub25l\"}",
	  "{\"token_type\":\"Bearer\",\"scope\":\"https://www.wvd.azure.us/.default\",\"expires_in\":"
	  "3599,\"ext_expires_in\":3599,\"access_token\":\"<redacted, 16 bytes>\",\"refresh_token\":"
	  "\"<redacted, 12 bytes>\",\"id_token\":\"<redacted, 16 bytes>\"}" },
	/* Whitespace between name, colon and value, and a value that is not a string. */
	{ "token response, pretty printed",
	  "{\n\t\"access_token\" : \"eyJ0\",\n\t\"expires_in\" : 3599,\n\t\"id_token\" : null\n}",
	  "{\n\t\"access_token\" : \"<redacted, 4 bytes>\",\n\t\"expires_in\" : 3599,\n\t\"id_token\" "
	  ": <redacted, 4 bytes>\n}" },
	/* A quote escaped inside a token value does not end it. */
	{ "escaped quote", "{\"access_token\":\"ey\\\"J0\",\"scope\":\"user_impersonation\"}",
	  "{\"access_token\":\"<redacted, 6 bytes>\",\"scope\":\"user_impersonation\"}" },
	/* Nonce exchange, no credential in either direction. */
	{ "nonce request", "grant_type=srv_challenge", "grant_type=srv_challenge" },
	{ "nonce response", "{\"Nonce\":\"AwABAAAAAAACAOz_BAD0_ykWQ\"}",
	  "{\"Nonce\":\"AwABAAAAAAACAOz_BAD0_ykWQ\"}" },
	/* Public metadata people need for troubleshooting stays intact. The field names below are
	 * prefixed by or contain a redacted name, and 'code' appears as a value, not as a field. */
	{ "discovery document",
	  "{\"token_endpoint\":\"https://login.microsoftonline.us/common/oauth2/v2.0/token\","
	  "\"response_types_supported\":[\"code\",\"id_token\",\"code id_token\"],"
	  "\"code_challenge_methods_supported\":[\"plain\",\"S256\"],"
	  "\"id_token_signing_alg_values_supported\":[\"RS256\"],\"claims_supported\":[\"sub\"]}",
	  "{\"token_endpoint\":\"https://login.microsoftonline.us/common/oauth2/v2.0/token\","
	  "\"response_types_supported\":[\"code\",\"id_token\",\"code id_token\"],"
	  "\"code_challenge_methods_supported\":[\"plain\",\"S256\"],"
	  "\"id_token_signing_alg_values_supported\":[\"RS256\"],\"claims_supported\":[\"sub\"]}" },
	/* A field name is only one when it is the whole name. */
	{ "prefix of a field name", "code_challenge=E9Melhoa2Ow&codes=1",
	  "code_challenge=E9Melhoa2Ow&codes=1" },
	{ "field name in a value", "state=code&scope=refresh_token", "state=code&scope=refresh_token" },
	/* Empty and truncated bodies must not confuse the scanner. */
	{ "empty value", "code=&code_verifier=x",
	  "code=<redacted, 0 bytes>&code_verifier=<redacted, 1 bytes>" },
	{ "truncated", "{\"access_token\":\"eyJ", "{\"access_token\":\"<redacted, 3 bytes>" },
	{ "name without value", "code", "code" },
	{ "empty body", "", "" }
};

/* Nothing that was replaced may survive anywhere in the output. */
static const char* const secrets[] = {
	"0.AXkAq2ZDeF_Aut", "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk",
	"eyJ0eXAiOiJKV1Qi", "eyJhbGciOiJub25l",
	"eyJhbGci",         "Xy7~8Q"
};

int TestHttpRedact(int argc, char* argv[])
{
	int rc = 0;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	for (size_t x = 0; x < ARRAYSIZE(tests); x++)
	{
		const redact_test* test = &tests[x];
		char* actual = freerdp_http_redact_for_log(test->in, strlen(test->in));
		if (!actual)
		{
			(void)fprintf(stderr, "[%s] redaction failed\n", test->name);
			rc = -1;
			continue;
		}

		if (strcmp(actual, test->expected) != 0)
		{
			(void)fprintf(stderr, "[%s]\nexpected %s\nactual   %s\n", test->name, test->expected,
			              actual);
			rc = -1;
		}

		for (size_t y = 0; y < ARRAYSIZE(secrets); y++)
		{
			if (strstr(actual, secrets[y]))
			{
				(void)fprintf(stderr, "[%s] leaked '%s'\n", test->name, secrets[y]);
				rc = -1;
			}
		}

		free(actual);
	}

	/* An empty response is passed in as a nullptr body. */
	char* empty = freerdp_http_redact_for_log(nullptr, 0);
	if (!empty || (strlen(empty) != 0))
	{
		(void)fprintf(stderr, "[nullptr] expected an empty string\n");
		rc = -1;
	}
	free(empty);

	return rc;
}
