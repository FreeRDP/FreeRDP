#include <stdio.h>

#include <freerdp/freerdp.h>
#include <freerdp/settings.h>
#include <freerdp/utils/aad.h>
#include <freerdp/utils/helpers.h>

#include <winpr/crt.h>
#include <winpr/crypto.h>
#include <winpr/environment.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/string.h>

#if defined(BUILD_TESTING_INTERNAL)
#include "../aad.h"
#endif

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

/** The directory XDG_CONFIG_HOME points at while the test runs. */
static char* config_home = nullptr;

/** The user aad-clouds.json inside it, in whatever layout the build resolves it to. */
static char* config_file = nullptr;

static char* append(const char* fmt, ...)
{
	va_list ap = WINPR_C_ARRAY_INIT;

	va_start(ap, fmt);
	char* str = nullptr;
	size_t len = 0;
	const int rc = winpr_vasprintf(&str, &len, fmt, ap);
	va_end(ap);

	if (rc <= 0)
		return nullptr;
	return str;
}

/** Points XDG_CONFIG_HOME at an empty directory, so that the test neither reads the
 *  configuration of the user running it nor writes into it. */
static BOOL config_setup(void)
{
	UINT64 id = 0;
	if (winpr_RAND(&id, sizeof(id)) < 0)
		return FALSE;

	char* tmp = GetKnownPath(KNOWN_PATH_TEMP);
	if (!tmp)
		return FALSE;

	config_home = append("%s/aad-cloud-test-%" PRIx64, tmp, id);
	free(tmp);

	if (!config_home || !winpr_PathMakePath(config_home, nullptr))
		return FALSE;

	if (!SetEnvironmentVariableA("XDG_CONFIG_HOME", config_home))
		return FALSE;

	config_file = freerdp_GetConfigFilePath(FALSE, "aad-clouds.json");
	if (!config_file)
		return FALSE;

	/* Which subdirectory of the configuration home holds the file depends on the build
	 * options, so the directory to create is taken from the path the library resolves. */
	char* sep = strrchr(config_file, '/');
#if defined(_WIN32)
	char* winsep = strrchr(config_file, '\\');
	if (winsep > sep)
		sep = winsep;
#endif
	if (!sep)
		return FALSE;

	const char separator = *sep;
	*sep = '\0';
	const BOOL rc = winpr_PathMakePath(config_file, nullptr);
	*sep = separator;
	return rc;
}

static void config_cleanup(void)
{
	if (config_home)
		winpr_RemoveDirectory_RecursiveA(config_home);
	free(config_home);
	free(config_file);
	config_home = nullptr;
	config_file = nullptr;
}

#if defined(BUILD_TESTING_INTERNAL)
/** Writes the configuration file and drops the table, which is otherwise loaded once. */
static BOOL config_write(const char* content)
{
	FILE* fp = winpr_fopen(config_file, "w");
	if (!fp)
		return FALSE;

	const size_t len = strlen(content);
	const BOOL rc = fwrite(content, 1, len, fp) == len;
	(void)fclose(fp);

	freerdp_utils_aad_cloud_table_reset();
	return rc;
}

static void config_remove(void)
{
	(void)winpr_DeleteFile(config_file);
	freerdp_utils_aad_cloud_table_reset();
}

static BOOL check_cloud(const char* name, const char* authority, const char* suffix)
{
	const FreeRDP_AadCloud* cloud = freerdp_utils_aad_cloud_by_name(name);
	if (!cloud)
	{
		(void)fprintf(stderr, "cloud '%s' not found\n", name);
		return FALSE;
	}

	const char* actual = freerdp_utils_aad_cloud_get_authority(cloud);
	if (!actual || (strcmp(actual, authority) != 0))
	{
		(void)fprintf(stderr, "cloud '%s' has authority '%s', expected '%s'\n", name,
		              actual ? actual : "", authority);
		return FALSE;
	}

	const char* gateway = freerdp_utils_aad_cloud_get_gateway_suffix(cloud);
	if (suffix)
	{
		if (!gateway || (strcmp(gateway, suffix) != 0))
		{
			(void)fprintf(stderr, "cloud '%s' has gateway suffix '%s', expected '%s'\n", name,
			              gateway ? gateway : "", suffix);
			return FALSE;
		}
	}
	else if (gateway)
	{
		(void)fprintf(stderr, "cloud '%s' has gateway suffix '%s', expected none\n", name, gateway);
		return FALSE;
	}

	return TRUE;
}

/** The compiled clouds, which every case that does not override them expects to find. */
static BOOL check_defaults(void)
{
	return check_cloud("commercial", "login.microsoftonline.com", nullptr) &&
	       check_cloud("usgov", "login.microsoftonline.us", ".wvd.azure.us");
}

/** A valid entry adding a cloud, as a literal: it is an argument of append(), not a format. */
static const char valid_entry[] =
    "\t{\n"
    "\t\t\"name\": \"example\",\n"
    "\t\t\"active_directory\": \"login.example.test\",\n"
    "\t\t\"gateway_suffix\": \".wvd.example.test\",\n"
    "\t\t\"avd_scope\": \"https%3A%2F%2Fwww.wvd.example.test%2F.default%20openid\",\n"
    "\t\t\"avd_redirect_format\": \"https%%3A%%2F%%2F%s%%2F%s%%2Foauth2%%2Fnativeclient\"\n"
    "\t}";

static BOOL test_config_none(void)
{
	/* Nothing was written yet, so this is what a missing configuration file looks like. */
	freerdp_utils_aad_cloud_table_reset();
	return check_defaults() && !freerdp_utils_aad_cloud_by_name("example") &&
	       !freerdp_utils_aad_cloud_for_gateway("host.wvd.example.test");
}

static BOOL test_config_override(void)
{
	BOOL rc = FALSE;
	const FreeRDP_AadCloud* example = nullptr;
	rdpSettings* settings = nullptr;
	char* content = append("[\n"
	                       "\t{\n"
	                       "\t\t\"name\": \"usgov\",\n"
	                       "\t\t\"active_directory\": \"login.microsoftonline.test\",\n"
	                       "\t\t\"gateway_suffix\": \".wvd.azure.us\",\n"
	                       "\t\t\"avd_scope\": \"https%%3A%%2F%%2Fwww.wvd.azure.us%%2F.default\",\n"
	                       "\t\t\"avd_redirect_format\": \"https%%%%3A%%%%2F%%%%2F%%s%%%%2F%%s"
	                       "%%%%2Foauth2%%%%2Fnativeclient\",\n"
	                       "\t\t\"resource_manager\": \"https://management.usgovcloudapi.net/\"\n"
	                       "\t},\n"
	                       "%s\n"
	                       "]\n",
	                       valid_entry);
	if (!content)
		return FALSE;

	if (!config_write(content))
		goto fail;

	/* The entry naming a compiled cloud replaces it, the other one is added. */
	if (!check_cloud("commercial", "login.microsoftonline.com", nullptr) ||
	    !check_cloud("usgov", "login.microsoftonline.test", ".wvd.azure.us") ||
	    !check_cloud("example", "login.example.test", ".wvd.example.test"))
		goto fail;

	/* A configured cloud is selected by a gateway hostname and applies like a compiled one. */
	example = freerdp_utils_aad_cloud_by_name("example");
	if ((freerdp_utils_aad_cloud_for_gateway("gateway.WVD.example.TEST") != example) ||
	    (freerdp_utils_aad_cloud_for_gateway("host.wvd.azure.us") !=
	     freerdp_utils_aad_cloud_by_name("usgov")))
		goto fail;

	settings = freerdp_settings_new(0);
	if (!settings)
		goto fail;

	rc = freerdp_utils_aad_apply_cloud(settings, example) &&
	     check_string(settings, FreeRDP_GatewayAzureActiveDirectory, "login.example.test");

fail:
	freerdp_settings_free(settings);
	free(content);
	return rc;
}

static BOOL test_config_malformed(void)
{
	BOOL rc = FALSE;
	char* content = append("[\n"
	                       "\t\"not an object\",\n"
	                       "\t{ \"name\": \"BAD NAME\" },\n"
	                       "\t{\n"
	                       "\t\t\"name\": \"scheme\",\n"
	                       "\t\t\"active_directory\": \"https://login.example.test/common\",\n"
	                       "\t\t\"avd_scope\": \"https%%3A%%2F%%2Fwww.example.test%%2F.default\",\n"
	                       "\t\t\"avd_redirect_format\": \"https%%%%3A%%%%2F%%%%2F%%s\"\n"
	                       "\t},\n"
	                       "\t{\n"
	                       "\t\t\"name\": \"noboundary\",\n"
	                       "\t\t\"active_directory\": \"login.example.test\",\n"
	                       "\t\t\"gateway_suffix\": \"wvd.example.test\",\n"
	                       "\t\t\"avd_scope\": \"https%%3A%%2F%%2Fwww.example.test%%2F.default\",\n"
	                       "\t\t\"avd_redirect_format\": \"https%%%%3A%%%%2F%%%%2F%%s\"\n"
	                       "\t},\n"
	                       "%s\n"
	                       "]\n",
	                       valid_entry);
	if (!content)
		return FALSE;

	if (!config_write(content))
		goto fail;

	/* Every malformed entry is skipped, its valid siblings and the defaults are kept. */
	rc = check_defaults() && check_cloud("example", "login.example.test", ".wvd.example.test") &&
	     !freerdp_utils_aad_cloud_by_name("scheme") &&
	     !freerdp_utils_aad_cloud_by_name("noboundary") &&
	     !freerdp_utils_aad_cloud_by_name("BAD NAME");

fail:
	free(content);
	return rc;
}

static BOOL test_config_format(void)
{
	/* '%n' writes through whatever winpr_asprintf finds on the argument list and three '%s'
	 * read past its end, so both are rejected, and so is a trailing '%'. */
	const char* formats[] = { "https%%3A%%2F%%2F%n", "https%%3A%%2F%%2F%s%%2F%s%%2F%s",
		                      "https%%3A%%2F%%2F%" };

	for (size_t x = 0; x < ARRAYSIZE(formats); x++)
	{
		char* content = append("[\n"
		                       "\t{\n"
		                       "\t\t\"name\": \"example\",\n"
		                       "\t\t\"active_directory\": \"login.example.test\",\n"
		                       "\t\t\"avd_scope\": \"https%%3A%%2F%%2Fwww.example.test%%2F"
		                       ".default\",\n"
		                       "\t\t\"avd_redirect_format\": \"%s\"\n"
		                       "\t}\n"
		                       "]\n",
		                       formats[x]);
		if (!content)
			return FALSE;

		const BOOL written = config_write(content);
		free(content);
		if (!written)
			return FALSE;

		if (freerdp_utils_aad_cloud_by_name("example"))
		{
			(void)fprintf(stderr, "redirect format '%s' was accepted\n", formats[x]);
			return FALSE;
		}
		if (!check_defaults())
			return FALSE;
	}

	return TRUE;
}

static BOOL test_config_broken_file(void)
{
	/* A file that does not parse, and one that is not an array, are ignored as a whole. */
	if (!config_write("this is not JSON") || !check_defaults())
		return FALSE;

	return config_write("{ \"name\": \"example\" }\n") && check_defaults() &&
	       !freerdp_utils_aad_cloud_by_name("example");
}

static BOOL test_config_sample(void)
{
	/* The sample shipped with the documentation has to stay loadable. */
	const char sample[] = TESTING_SRC_DIRECTORY "/docs/aad-clouds.example.json";
	BOOL rc = FALSE;
	char* content = nullptr;
	FILE* fp = winpr_fopen(sample, "rb");
	if (!fp)
	{
		(void)fprintf(stderr, "failed to open '%s'\n", sample);
		return FALSE;
	}

	if (fseek(fp, 0, SEEK_END) != 0)
		goto fail;

	const long size = ftell(fp);
	if ((size <= 0) || (fseek(fp, 0, SEEK_SET) != 0))
		goto fail;

	content = calloc((size_t)size + 1, sizeof(char));
	if (!content || (fread(content, 1, (size_t)size, fp) != (size_t)size))
		goto fail;

	if (!config_write(content))
		goto fail;

	/* Its usgov entry repeats the compiled values, its example entry adds a cloud. */
	rc = check_defaults() && check_cloud("example", "login.example.test", ".wvd.example.test");

fail:
	(void)fclose(fp);
	free(content);
	return rc;
}

static BOOL test_config(void)
{
	const BOOL rc = test_config_none() && test_config_override() && test_config_malformed() &&
	                test_config_format() && test_config_broken_file() && test_config_sample();

	/* Leave the process with the compiled table, whatever happened above. */
	config_remove();
	return rc;
}
#endif

int TestAadCloud(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	int rc = -1;

	/* Before the first lookup, so that neither the configuration of the user running the test
	 * nor a file this test writes is read by accident. */
	if (!config_setup())
		goto fail;

	if (!test_lookup())
		goto fail;
	if (!test_apply())
		goto fail;
#if defined(BUILD_TESTING_INTERNAL)
	if (!test_config())
		goto fail;
#endif

	rc = 0;
fail:
	config_cleanup();
	return rc;
}
