#include <freerdp/client.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/settings.h>
#include <winpr/cmdline.h>
#include <winpr/spec.h>
#include <winpr/strlst.h>
#include <winpr/collections.h>

typedef BOOL (*validate_settings_pr)(rdpSettings* settings);

#define printref() printf("%s:%d: in function %-40s:", __FILE__, __LINE__, __func__)

#define TEST_ERROR(format, ...)                       \
	do                                                \
	{                                                 \
		(void)fprintf(stderr, format, ##__VA_ARGS__); \
		printref();                                   \
		(void)printf(format, ##__VA_ARGS__);          \
		(void)fflush(stdout);                         \
	} while (0)

#define TEST_FAILURE(format, ...)            \
	do                                       \
	{                                        \
		printref();                          \
		(void)printf(" FAILURE ");           \
		(void)printf(format, ##__VA_ARGS__); \
		(void)fflush(stdout);                \
	} while (0)

static void print_test_title(int argc, char** argv)
{
	printf("Running test:");

	for (int i = 0; i < argc; i++)
	{
		printf(" %s", argv[i]);
	}

	printf("\n");
}

static inline BOOL testcase(const char* name, char** argv, size_t argc, int expected_return,
                            validate_settings_pr validate_settings)
{
	int status = 0;
	BOOL valid_settings = TRUE;
	rdpSettings* settings = freerdp_settings_new(0);

	WINPR_ASSERT(argc <= INT_MAX);

	print_test_title((int)argc, argv);

	if (!settings)
	{
		TEST_ERROR("Test %s could not allocate settings!\n", name);
		return FALSE;
	}

	status = freerdp_client_settings_parse_command_line(settings, (int)argc, argv, FALSE);

	if (validate_settings)
	{
		valid_settings = validate_settings(settings);
	}

	freerdp_settings_free(settings);

	if (status == expected_return)
	{
		if (!valid_settings)
		{
			return FALSE;
		}
	}
	else
	{
		TEST_FAILURE("Expected status %d,  got status %d\n", expected_return, status);
		return FALSE;
	}

	return TRUE;
}

#if defined(_WIN32)
#define DRIVE_REDIRECT_PATH "c:\\Windows"
#else
#define DRIVE_REDIRECT_PATH "/tmp"
#endif

static BOOL check_settings_smartcard_no_redirection(rdpSettings* settings)
{
	BOOL result = TRUE;

	if (freerdp_settings_get_bool(settings, FreeRDP_RedirectSmartCards))
	{
		TEST_FAILURE("Expected RedirectSmartCards = FALSE,  but RedirectSmartCards = TRUE!\n");
		result = FALSE;
	}

	if (freerdp_device_collection_find_type(settings, RDPDR_DTYP_SMARTCARD))
	{
		TEST_FAILURE("Expected no SMARTCARD device, but found at least one!\n");
		result = FALSE;
	}

	return result;
}

static const char avd_commercial_scope[] =
    "https%3A%2F%2Fwww.wvd.microsoft.com%2F.default%20openid%20profile%20offline_access";
static const char avd_commercial_redirect[] = "https%%3A%%2F%%2F%s%%2F%s%%2Foauth2%%2Fnativeclient";
static const char avd_usgov_scope[] =
    "https%3A%2F%2Fwww.wvd.azure.us%2F.default%20openid%20profile%20offline_access";
static const char avd_usgov_redirect[] =
    "https%%3A%%2F%%2Flogin.microsoftonline.com%%2Fcommon%%2Foauth2%%2Fnativeclient";

static BOOL check_avd_cloud(rdpSettings* settings, const char* authority, const char* scope,
                            const char* redirect, BOOL useTenantid)
{
	const struct
	{
		FreeRDP_Settings_Keys_String key;
		const char* expected;
	} strings[] = { { FreeRDP_GatewayAzureActiveDirectory, authority },
		            { FreeRDP_GatewayAvdScope, scope },
		            { FreeRDP_GatewayAvdAccessAadFormat, redirect } };

	for (size_t x = 0; x < ARRAYSIZE(strings); x++)
	{
		const char* val = freerdp_settings_get_string(settings, strings[x].key);
		if (!val || (strcmp(val, strings[x].expected) != 0))
		{
			TEST_FAILURE("Expected %s = %s, but got %s!\n",
			             freerdp_settings_get_name_for_key(strings[x].key), strings[x].expected,
			             val ? val : "(null)");
			return FALSE;
		}
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_GatewayAvdUseTenantid) != useTenantid)
	{
		TEST_FAILURE("Expected GatewayAvdUseTenantid = %d!\n", useTenantid);
		return FALSE;
	}

	return TRUE;
}

static BOOL check_settings_avd_commercial(rdpSettings* settings)
{
	return check_avd_cloud(settings, "login.microsoftonline.com", avd_commercial_scope,
	                       avd_commercial_redirect, FALSE);
}

static BOOL check_settings_avd_usgov(rdpSettings* settings)
{
	return check_avd_cloud(settings, "login.microsoftonline.us", avd_usgov_scope,
	                       avd_usgov_redirect, FALSE);
}

static BOOL check_settings_avd_usgov_tenantid(rdpSettings* settings)
{
	return check_avd_cloud(settings, "login.microsoftonline.us", avd_usgov_scope,
	                       avd_usgov_redirect, TRUE);
}

static BOOL check_settings_avd_usgov_custom_scope(rdpSettings* settings)
{
	return check_avd_cloud(settings, "login.microsoftonline.us", "custom-scope", avd_usgov_redirect,
	                       FALSE);
}

static BOOL check_settings_avd_custom_authority(rdpSettings* settings)
{
	return check_avd_cloud(settings, "login.contoso.invalid", avd_commercial_scope,
	                       avd_commercial_redirect, FALSE);
}

typedef struct
{
	int expected_status;
	validate_settings_pr validate_settings;
	const char* command_line[128];
	struct
	{
		int index;
		const char* expected_value;
	} modified_arguments[8];
} test;

// NOLINTBEGIN(bugprone-suspicious-missing-comma)
static const test tests[] = {
	{ COMMAND_LINE_STATUS_PRINT_HELP,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "--help", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_STATUS_PRINT_HELP,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/help", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_STATUS_PRINT_HELP,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "-help", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_STATUS_PRINT_VERSION,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "--version", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_STATUS_PRINT_VERSION,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/version", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_STATUS_PRINT_VERSION,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "-version", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "-v", "test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "--v", "test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/sound", "/drive:media," DRIVE_REDIRECT_PATH, "/v:test.freerdp.com",
	    nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "-u", "test", "-p", "test", "-v", "test.freerdp.com", nullptr },
	  { { 4, "****" }, WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/u:test", "/p:test", "/v:test.freerdp.com", nullptr },
	  { { 2, "/p:****" }, WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_ERROR_NO_KEYWORD,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "-invalid", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_ERROR_NO_KEYWORD,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "--invalid", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	/* An ARM gateway of a sovereign cloud selects that cloud, unless the values it owns were
	 * customized or /gateway:cloud: named a cloud explicitly. */
	{ 0,
	  check_settings_avd_usgov,
	  { "testfreerdp", "/gateway:g:gw.wvd.azure.us,type:arm", "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_avd_usgov_tenantid,
	  { "testfreerdp", "/gateway:g:gw.WVD.AZURE.US,type:arm",
	    "/azure:tenantid:00000000-0000-0000-0000-000000000000", "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_avd_usgov,
	  { "testfreerdp", "/gateway:g:gw.wvd.azure.us,type:arm",
	    "/azure:tenantid:00000000-0000-0000-0000-000000000000,use-tenantid:off",
	    "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_avd_usgov_custom_scope,
	  { "testfreerdp", "/gateway:g:gw.wvd.azure.us,type:arm", "/azure:avd-scope:custom-scope",
	    "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_avd_commercial,
	  { "testfreerdp", "/gateway:g:gw.wvd.azure.us", "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_avd_usgov,
	  { "testfreerdp", "/gateway:g:gw.contoso.com,type:arm,cloud:usgov", "/v:test.freerdp.com",
	    nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_avd_commercial,
	  { "testfreerdp", "/gateway:g:gw.wvd.azure.us,type:arm,cloud:commercial",
	    "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_ERROR,
	  nullptr,
	  { "testfreerdp", "/gateway:g:gw.contoso.com,type:arm,cloud:unknown", "/v:test.freerdp.com",
	    nullptr },
	  { WINPR_C_ARRAY_INIT } },
	/* An explicit cloud gets the same tenant handling as the automatic selection. */
	{ 0,
	  check_settings_avd_usgov_tenantid,
	  { "testfreerdp", "/gateway:g:gw.contoso.com,type:arm,cloud:usgov",
	    "/azure:tenantid:00000000-0000-0000-0000-000000000000", "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_avd_usgov,
	  { "testfreerdp", "/gateway:g:gw.contoso.com,type:arm,cloud:usgov",
	    "/azure:tenantid:00000000-0000-0000-0000-000000000000,use-tenantid:off",
	    "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	/* The cloud strings are applied where cloud: is parsed, so the last option wins. */
	{ 0,
	  check_settings_avd_usgov_custom_scope,
	  { "testfreerdp", "/gateway:g:gw.contoso.com,type:arm,cloud:usgov",
	    "/azure:avd-scope:custom-scope", "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_avd_usgov,
	  { "testfreerdp", "/azure:avd-scope:custom-scope",
	    "/gateway:g:gw.contoso.com,type:arm,cloud:usgov", "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	/* A .rdp file is loaded without selecting a cloud, so the cloud is selected once, from the
	 * settings the options left behind. */
	{ 0,
	  check_settings_avd_usgov_tenantid,
	  { "testfreerdp", TEST_SOURCE_DIR "/rdp-avd/avd-usgov.rdp", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_avd_commercial,
	  { "testfreerdp", TEST_SOURCE_DIR "/rdp-avd/avd-usgov.rdp",
	    "/gateway:g:gw.wvd.microsoft.com,type:arm", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_avd_custom_authority,
	  { "testfreerdp", TEST_SOURCE_DIR "/rdp-avd/avd-usgov.rdp", "/azure:ad:login.contoso.invalid",
	    nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_avd_usgov,
	  { "testfreerdp", TEST_SOURCE_DIR "/rdp-avd/avd-usgov.rdp", "/azure:tenantid:common",
	    nullptr },
	  { WINPR_C_ARRAY_INIT } },
#if defined(WITH_FREERDP_DEPRECATED_CMDLINE)
	{ COMMAND_LINE_STATUS_PRINT,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/kbd-list", 0 },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_STATUS_PRINT,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/monitor-list", 0 },
	  { WINPR_C_ARRAY_INIT } },
#endif
	{ COMMAND_LINE_STATUS_PRINT,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/list:kbd", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_STATUS_PRINT,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/list:monitor", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/sound", "/drive:media:" DRIVE_REDIRECT_PATH, "/v:test.freerdp.com",
	    nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/sound", "/drive:media,/foo/bar/blabla", "/v:test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
};
// NOLINTEND(bugprone-suspicious-missing-comma)

static void check_modified_arguments(const test* test, char** command_line, int* rc)
{
	const char* expected_argument = nullptr;

	for (int k = 0; (expected_argument = test->modified_arguments[k].expected_value); k++)
	{
		int index = test->modified_arguments[k].index;
		char* actual_argument = command_line[index];

		if (0 != strcmp(actual_argument, expected_argument))
		{
			printref();
			printf("Failure: overridden argument %d is %s but it should be %s\n", index,
			       actual_argument, expected_argument);
			(void)fflush(stdout);
			*rc = -1;
		}
	}
}

int TestClientCmdLine(int argc, char* argv[])
{
	int rc = 0;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	for (size_t i = 0; i < ARRAYSIZE(tests); i++)
	{
		const test* current = &tests[i];
		int failure = 0;
		char** command_line = string_list_copy(current->command_line);

		const int len = string_list_length((const char* const*)command_line);
		if (!testcase(__func__, command_line, WINPR_ASSERTING_INT_CAST(size_t, len),
		              current->expected_status, current->validate_settings))
		{
			TEST_FAILURE("parsing arguments.\n");
			failure = 1;
		}

		check_modified_arguments(current, command_line, &failure);

		if (failure)
		{
			string_list_print(stdout, (const char* const*)command_line);
			rc = -1;
		}

		string_list_free(command_line);
	}

	return rc;
}
