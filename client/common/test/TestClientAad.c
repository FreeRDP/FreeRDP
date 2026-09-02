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

#include <stdio.h>
#include <string.h>

#include <winpr/crt.h>
#include <winpr/string.h>

#include <freerdp/client.h>
#include <freerdp/settings.h>

/* Every case below needs a cached OpenID configuration to stay offline, and the only way to
 * install one is freerdp_utils_aad_set_wellknown(), which is core-private (FREERDP_LOCAL) and
 * therefore only linkable when BUILD_TESTING_INTERNAL forces EXPORT_ALL_SYMBOLS. A normal
 * BUILD_TESTING=ON build skips the whole body; CI enables BUILD_TESTING_INTERNAL. */
#if defined(WITH_AAD) && defined(BUILD_TESTING_INTERNAL)

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
	return 0;
#endif
}
