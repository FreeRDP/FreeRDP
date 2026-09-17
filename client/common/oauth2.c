/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * OAuth2 state handling
 *
 * Copyright 2026 Armin Novak <anovak@thincast.com>
 * Copyright 2026 Thincast Technologies GmbH
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
#include "oauth2.h"

#include <winpr/string.h>
#include <winpr/crypto.h>
#include <winpr/print.h>
#include <freerdp/crypto/crypto.h>

#include <freerdp/log.h>

#define TAG CLIENT_TAG("common.oauth2")

enum pkcs_state
{
	INVALID_STATE = 0,
	INITIAL_CHALLENGE,
	TOKEN_VERIFIER
};

struct rdp_client_oauth2
{
	wLog* log;
	const char* pkcsChallengeMethod;
	enum pkcs_state pkcsstate;
	bool valid;
	char* state;
	size_t state_len;
	char* code_verifier;
	size_t code_verifier_len;
	char* code_challenge;
	size_t code_challenge_len;
};

WINPR_ATTR_MALLOC(free, 1)
static char* rfc7636_generate_code_verifier(size_t* plen)
{
	WINPR_ASSERT(plen);
	*plen = 0;

	BYTE random[32] = WINPR_C_ARRAY_INIT;
	if (winpr_RAND(random, sizeof(random)) < 0)
		return nullptr;
	char* str = crypto_base64url_encode_len(random, sizeof(random), plen);
	if (!str)
		return nullptr;
	return str;
}

WINPR_ATTR_MALLOC(free, 1)
static char* rfc7636_generate_code_challenge(const char* method, const char* code_verifier,
                                             size_t len, size_t* plen)
{
	WINPR_ASSERT(plen);
	*plen = 0;

	if (!code_verifier || (len == 0) || !method)
		return nullptr;

	if (strcmp("plain", method) == 0)
	{
		char* str = strndup(code_verifier, len);
		if (!str)
			return nullptr;
		*plen = len;
		return str;
	}

	if (strcmp("S256", method) == 0)
	{
		BYTE hash[WINPR_SHA256_DIGEST_LENGTH] = WINPR_C_ARRAY_INIT;
		if (!winpr_Digest(WINPR_MD_SHA256, code_verifier, len, hash, sizeof(hash)))
			return nullptr;
		char* str = crypto_base64url_encode_len(hash, sizeof(hash), plen);
		if (!str)
			return nullptr;
		return str;
	}

	return nullptr;
}

WINPR_ATTR_MALLOC(free, 1)
static char* rfc6749_generate_state(size_t* plen)
{
	WINPR_ASSERT(plen);
	*plen = 0;
	BYTE random[32] = WINPR_C_ARRAY_INIT;
	if (winpr_RAND(random, sizeof(random)) < 0)
		return nullptr;
	char* str = winpr_BinToHexString(random, sizeof(random), FALSE);
	if (str)
		*plen = strlen(str);
	return str;
}

static void oauth2_free(rdpClientOAuth2* oauth2)
{
	if (!oauth2)
		return;

	oauth2->valid = false;

	oauth2->state_len = 0;
	free(oauth2->state);
	oauth2->state = nullptr;

	oauth2->code_verifier_len = 0;
	free(oauth2->code_verifier);
	oauth2->code_verifier = nullptr;

	oauth2->code_challenge_len = 0;
	free(oauth2->code_challenge);
	oauth2->code_challenge = nullptr;
}

void freerdp_oauth2_free(rdpClientOAuth2* oauth2)
{
	oauth2_free(oauth2);
	free(oauth2);
}

rdpClientOAuth2* freerdp_oauth2_new(void)
{
	rdpClientOAuth2* oauth2 = calloc(1, sizeof(rdpClientOAuth2));
	if (!oauth2)
		return nullptr;
	oauth2->log = WLog_Get(TAG);
	oauth2->pkcsChallengeMethod = "S256"; // alternatively "plain"
	WINPR_ASSERT(oauth2->log);

	return oauth2;
}

BOOL freerdp_oauth2_reset(rdpClientOAuth2* oauth2)
{
	WINPR_ASSERT(oauth2);
	oauth2_free(oauth2);

	oauth2->state = rfc6749_generate_state(&oauth2->state_len);
	if (!oauth2->state || (oauth2->state_len == 0))
		return FALSE;

	oauth2->code_verifier = rfc7636_generate_code_verifier(&oauth2->code_verifier_len);
	if (!oauth2->code_verifier || (oauth2->code_verifier_len == 0))
		return FALSE;
	oauth2->code_challenge =
	    rfc7636_generate_code_challenge(oauth2->pkcsChallengeMethod, oauth2->code_verifier,
	                                    oauth2->code_verifier_len, &oauth2->code_challenge_len);
	if (!oauth2->code_challenge || (oauth2->code_challenge_len == 0))
		return FALSE;

	oauth2->pkcsstate = INITIAL_CHALLENGE;
	return TRUE;
}

BOOL freerdp_oauth2_check_return_valid(rdpClientOAuth2* oauth2, const char* response, size_t len)
{
	WINPR_ASSERT(oauth2);
	if (!oauth2->valid)
	{
		WLog_Print(oauth2->log, WLOG_WARN,
		           "No OAuth2 request generated, but we have a response. "
		           "Discarding response.");
		return FALSE;
	}

	// We did use a state parameter, so check it is there in the response.
	if (oauth2->state && (oauth2->state_len > 0))
	{
		WLog_Print(oauth2->log, WLOG_DEBUG,
		           "OAuth2 state parameter used in request, checking response for mirrored value");
		// Check if there is a state argument and it must match the one in the request.
		const char* state = winpr_strnstr(response, "state=", len);
		if (!state)
		{
			WLog_Print(oauth2->log, WLOG_WARN,
			           "OAuth2 state parameter used in request, but missing in response. "
			           "Discarding response.");
			return FALSE;
		}
		if (strncmp(oauth2->state, &state[6], oauth2->state_len) != 0)
		{
			WLog_Print(oauth2->log, WLOG_WARN,
			           "OAuth2 state parameter used in request, but does not match parameter value "
			           "in response. Discarding response.");
			return FALSE;
		}
	}
	else
		WLog_Print(oauth2->log, WLOG_DEBUG,
		           "OAuth2 state parameter was not used in request, skipping response check.");

	return TRUE;
}

char* freerdp_oauth2_append_state(rdpClientOAuth2* oauth2, const char* url, size_t len,
                                  size_t* plen)
{
	WINPR_ASSERT(oauth2);

	if (plen)
		*plen = 0;
	if (strnlen(url, len + 1) > len)
		return nullptr;

	char* safeurl = nullptr;
	size_t safeurllen = 0;
	if (oauth2->pkcsChallengeMethod)
	{
		switch (oauth2->pkcsstate)
		{
			case INITIAL_CHALLENGE:
			{
				winpr_asprintf(&safeurl, &safeurllen,
				               "%s&state=%s&code_challenge=%s&code_challenge_method=%s", url,
				               oauth2->state, oauth2->code_challenge, oauth2->pkcsChallengeMethod);
				oauth2->pkcsstate = TOKEN_VERIFIER;
			}
			break;
			case TOKEN_VERIFIER:
			{
				winpr_asprintf(&safeurl, &safeurllen, "%s&state=%s&code_verifier=%s", url,
				               oauth2->state, oauth2->code_verifier);
				oauth2->pkcsstate = INVALID_STATE;
			}
			break;
			default:
				WLog_Print(
				    oauth2->log, WLOG_ERROR,
				    "Invalid pkcs state. OAuth2 call sequence of your client is wrong. Aborting.");
				return nullptr;
		}
	}
	else
		winpr_asprintf(&safeurl, &safeurllen, "%s&state=%s", url, oauth2->state);

	oauth2->valid = true;
	if (plen)
		*plen = safeurllen;
	return safeurl;
}

char* freerdp_oauth2_extract_code(rdpClientOAuth2* oauth2, const char* response, size_t len)
{
	if (!freerdp_oauth2_check_return_valid(oauth2, response, len))
		return nullptr;

	const char* token = winpr_strnstr(response, "code=", len);
	if (!token)
		return nullptr;

	const char* start = &token[5];
	const size_t olen = WINPR_ASSERTING_INT_CAST(size_t, start - response);
	if (olen > len)
		return nullptr;
	const size_t rlen = len - olen;
	char* str = strndup(start, rlen);
	if (!str)
		return nullptr;
	char* end = winpr_strnstr(str, "&", rlen);
	if (end)
		*end = '\0';
	return str;
}
