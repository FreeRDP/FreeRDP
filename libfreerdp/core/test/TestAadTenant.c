#include <stdio.h>

#include <freerdp/freerdp.h>
#include <freerdp/utils/aad.h>

#include <winpr/crt.h>
#include <winpr/json.h>

/* freerdp_utils_aad_get_wellknown() interpolates base and tenant id into the OpenID discovery
 * URL. Every case below has to be rejected before any request is made, so this test runs
 * offline; a valid tenant id would reach the network and is therefore not covered here. */
static const char* rejected_tenants[] = {
	nullptr, "", ".", "..", "a/b", "a?b", "a#b", "a@b", "a:b", "a b", "tenant\x01",
	/* 129 characters, one more than the accepted maximum */
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
	"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
};

int TestAadTenant(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	for (size_t x = 0; x < ARRAYSIZE(rejected_tenants); x++)
	{
		WINPR_JSON* json = freerdp_utils_aad_get_wellknown(nullptr, "login.microsoftonline.com",
		                                                   rejected_tenants[x]);
		if (json)
		{
			WINPR_JSON_Delete(json);
			(void)fprintf(stderr, "tenant id [%" PRIuz "] was not rejected\n", x);
			return -1;
		}
	}

	/* A missing authority is rejected as well, instead of being asserted on. */
	{
		WINPR_JSON* json = freerdp_utils_aad_get_wellknown(nullptr, nullptr, "common");
		if (json)
		{
			WINPR_JSON_Delete(json);
			(void)fprintf(stderr, "a missing authority was not rejected\n");
			return -1;
		}
	}

	return 0;
}
