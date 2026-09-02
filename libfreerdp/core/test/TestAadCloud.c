#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/utils/aad.h>

#include <winpr/crt.h>

static const char commercial_scope[] =
    "https%3A%2F%2Fwww.wvd.microsoft.com%2F.default%20openid%20profile%20offline_access";
static const char commercial_redirect[] = "https%%3A%%2F%%2F%s%%2F%s%%2Foauth2%%2Fnativeclient";
static const char usgov_scope[] =
    "https%3A%2F%2Fwww.wvd.azure.us%2F.default%20openid%20profile%20offline_access";
static const char usgov_redirect[] =
    "https%%3A%%2F%%2Flogin.microsoftonline.com%%2Fcommon%%2Foauth2%%2Fnativeclient";

static BOOL check_string(const rdpSettings* settings, FreeRDP_Settings_Keys_String key,
                         const char* expected)
{
	const char* actual = freerdp_settings_get_string(settings, key);
	return actual && (strcmp(actual, expected) == 0);
}

static BOOL test_lookup(void)
{
	const FreeRDP_AadCloud* commercial = freerdp_utils_aad_cloud_by_name("commercial");
	const FreeRDP_AadCloud* usgov = freerdp_utils_aad_cloud_by_name("usgov");

	if (!commercial || !usgov || (commercial == usgov) ||
	    (freerdp_utils_aad_cloud_by_name("USGOV") != usgov) ||
	    freerdp_utils_aad_cloud_by_name(nullptr) || freerdp_utils_aad_cloud_by_name("unknown"))
		return FALSE;

	/* Only a cloud with a gateway suffix takes part in the hostname lookup. */
	if (freerdp_utils_aad_cloud_get_gateway_suffix(commercial) ||
	    freerdp_utils_aad_cloud_for_gateway(nullptr) ||
	    freerdp_utils_aad_cloud_for_gateway("gateway.wvd.azure.com") ||
	    freerdp_utils_aad_cloud_for_gateway("xwvd.azure.us") ||
	    (freerdp_utils_aad_cloud_for_gateway("gateway.WVD.AZURE.US") != usgov) ||
	    (freerdp_utils_aad_cloud_for_gateway("gateway.wvd.azure.us") != usgov))
		return FALSE;

	if (strcmp(freerdp_utils_aad_cloud_get_name(commercial), "commercial") ||
	    strcmp(freerdp_utils_aad_cloud_get_authority(commercial), "login.microsoftonline.com") ||
	    strcmp(freerdp_utils_aad_cloud_get_avd_scope(commercial), commercial_scope) ||
	    strcmp(freerdp_utils_aad_cloud_get_avd_redirect_format(commercial), commercial_redirect))
		return FALSE;

	if (strcmp(freerdp_utils_aad_cloud_get_name(usgov), "usgov") ||
	    strcmp(freerdp_utils_aad_cloud_get_gateway_suffix(usgov), ".wvd.azure.us") ||
	    strcmp(freerdp_utils_aad_cloud_get_authority(usgov), "login.microsoftonline.us") ||
	    strcmp(freerdp_utils_aad_cloud_get_avd_scope(usgov), usgov_scope) ||
	    strcmp(freerdp_utils_aad_cloud_get_avd_redirect_format(usgov), usgov_redirect))
		return FALSE;

	return !freerdp_utils_aad_cloud_get_name(nullptr) &&
	       !freerdp_utils_aad_cloud_get_gateway_suffix(nullptr) &&
	       !freerdp_utils_aad_cloud_get_authority(nullptr) &&
	       !freerdp_utils_aad_cloud_get_avd_scope(nullptr) &&
	       !freerdp_utils_aad_cloud_get_avd_redirect_format(nullptr);
}

static BOOL test_apply(void)
{
	const FreeRDP_AadCloud* commercial = freerdp_utils_aad_cloud_by_name("commercial");
	const FreeRDP_AadCloud* usgov = freerdp_utils_aad_cloud_by_name("usgov");
	rdpSettings* settings = freerdp_settings_new(0);
	BOOL rc = FALSE;

	if (!commercial || !usgov || !settings)
		goto fail;

	/* The defaults of a fresh settings instance are the commercial cloud. */
	if (!check_string(settings, FreeRDP_GatewayAzureActiveDirectory, "login.microsoftonline.com") ||
	    !check_string(settings, FreeRDP_GatewayAvdScope, commercial_scope) ||
	    !check_string(settings, FreeRDP_GatewayAvdAccessAadFormat, commercial_redirect))
		goto fail;

	/* Applying a cloud writes exactly the three cloud-owned strings, unconditionally. */
	if (!freerdp_settings_set_string(settings, FreeRDP_GatewayAvdScope, "custom-scope") ||
	    !freerdp_settings_set_bool(settings, FreeRDP_GatewayAvdUseTenantid, TRUE) ||
	    !freerdp_utils_aad_apply_cloud(settings, usgov) ||
	    !check_string(settings, FreeRDP_GatewayAzureActiveDirectory, "login.microsoftonline.us") ||
	    !check_string(settings, FreeRDP_GatewayAvdScope, usgov_scope) ||
	    !check_string(settings, FreeRDP_GatewayAvdAccessAadFormat, usgov_redirect) ||
	    !freerdp_settings_get_bool(settings, FreeRDP_GatewayAvdUseTenantid))
		goto fail;

	if (!freerdp_utils_aad_apply_cloud(settings, commercial) ||
	    !check_string(settings, FreeRDP_GatewayAzureActiveDirectory, "login.microsoftonline.com") ||
	    !check_string(settings, FreeRDP_GatewayAvdScope, commercial_scope) ||
	    !check_string(settings, FreeRDP_GatewayAvdAccessAadFormat, commercial_redirect) ||
	    !freerdp_settings_get_bool(settings, FreeRDP_GatewayAvdUseTenantid))
		goto fail;

	if (freerdp_utils_aad_apply_cloud(nullptr, usgov) ||
	    freerdp_utils_aad_apply_cloud(settings, nullptr))
		goto fail;

	rc = TRUE;
fail:
	freerdp_settings_free(settings);
	return rc;
}

int TestAadCloud(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!test_lookup())
		return -1;
	if (!test_apply())
		return -1;
	return 0;
}
