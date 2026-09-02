#include <string.h>

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

#ifndef TEST_SOURCE_DIR
#error "TEST_SOURCE_DIR must be defined to the test source directory"
#endif
#define DISABLED_DISPLAY_OPTIONS_RDP TEST_SOURCE_DIR "/rdp-cmdline/disabled-display-options.rdp"

static BOOL check_settings_multimon_disabled(rdpSettings* settings)
{
	BOOL result = TRUE;

	if (freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
	{
		TEST_FAILURE("Expected UseMultimon = FALSE, but UseMultimon = TRUE!\n");
		result = FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_ForceMultimon))
	{
		TEST_FAILURE("Expected ForceMultimon = FALSE, but ForceMultimon = TRUE!\n");
		result = FALSE;
	}

	if (freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds) != 0)
	{
		TEST_FAILURE("Expected NumMonitorIds = 0, but NumMonitorIds = %u!\n",
		             (unsigned)freerdp_settings_get_uint32(settings, FreeRDP_NumMonitorIds));
		result = FALSE;
	}

	if (!freerdp_settings_get_bool(settings, FreeRDP_SmartSizing))
	{
		TEST_FAILURE("Expected SmartSizing = TRUE, but SmartSizing = FALSE!\n");
		result = FALSE;
	}

	return result;
}

static BOOL check_settings_smart_sizing_disabled(rdpSettings* settings)
{
	BOOL result = TRUE;

	if (freerdp_settings_get_bool(settings, FreeRDP_SmartSizing))
	{
		TEST_FAILURE("Expected SmartSizing = FALSE, but SmartSizing = TRUE!\n");
		result = FALSE;
	}

	if (!freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate))
	{
		TEST_FAILURE(
		    "Expected DynamicResolutionUpdate = TRUE, but DynamicResolutionUpdate = FALSE!\n");
		result = FALSE;
	}

	if (!freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
	{
		TEST_FAILURE("Expected UseMultimon = TRUE, but UseMultimon = FALSE!\n");
		result = FALSE;
	}

	return result;
}

static BOOL check_settings_dynamic_resolution_disabled(rdpSettings* settings)
{
	BOOL result = TRUE;

	if (freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate))
	{
		TEST_FAILURE(
		    "Expected DynamicResolutionUpdate = FALSE, but DynamicResolutionUpdate = TRUE!\n");
		result = FALSE;
	}

	if (freerdp_settings_get_bool(settings, FreeRDP_SupportDisplayControl))
	{
		TEST_FAILURE("Expected SupportDisplayControl = FALSE, but SupportDisplayControl = TRUE!\n");
		result = FALSE;
	}

	if (!freerdp_settings_get_bool(settings, FreeRDP_SmartSizing))
	{
		TEST_FAILURE("Expected SmartSizing = TRUE, but SmartSizing = FALSE!\n");
		result = FALSE;
	}

	return result;
}

static BOOL check_settings_multimon_force(rdpSettings* settings)
{
	BOOL result = TRUE;

	if (!freerdp_settings_get_bool(settings, FreeRDP_UseMultimon))
	{
		TEST_FAILURE("Expected UseMultimon = TRUE, but UseMultimon = FALSE!\n");
		result = FALSE;
	}

	if (!freerdp_settings_get_bool(settings, FreeRDP_ForceMultimon))
	{
		TEST_FAILURE("Expected ForceMultimon = TRUE, but ForceMultimon = FALSE!\n");
		result = FALSE;
	}

	return result;
}

static BOOL check_settings_smart_sizing_size(rdpSettings* settings)
{
	BOOL result = TRUE;

	if (!freerdp_settings_get_bool(settings, FreeRDP_SmartSizing))
	{
		TEST_FAILURE("Expected SmartSizing = TRUE, but SmartSizing = FALSE!\n");
		result = FALSE;
	}

	if (freerdp_settings_get_uint32(settings, FreeRDP_SmartSizingWidth) != 1024)
	{
		TEST_FAILURE("Expected SmartSizingWidth = 1024, but SmartSizingWidth = %u!\n",
		             (unsigned)freerdp_settings_get_uint32(settings, FreeRDP_SmartSizingWidth));
		result = FALSE;
	}

	if (freerdp_settings_get_uint32(settings, FreeRDP_SmartSizingHeight) != 768)
	{
		TEST_FAILURE("Expected SmartSizingHeight = 768, but SmartSizingHeight = %u!\n",
		             (unsigned)freerdp_settings_get_uint32(settings, FreeRDP_SmartSizingHeight));
		result = FALSE;
	}

	return result;
}

static BOOL check_settings_dynamic_resolution_posix_not_swallowed(rdpSettings* settings)
{
	BOOL result = TRUE;

	if (!freerdp_settings_get_bool(settings, FreeRDP_DynamicResolutionUpdate))
	{
		TEST_FAILURE(
		    "Expected DynamicResolutionUpdate = TRUE, but DynamicResolutionUpdate = FALSE!\n");
		result = FALSE;
	}

	if (strcmp(freerdp_settings_get_string(settings, FreeRDP_ServerHostname), "test.freerdp.com") !=
	    0)
	{
		TEST_FAILURE("Expected ServerHostname = test.freerdp.com, but ServerHostname = %s!\n",
		             freerdp_settings_get_string(settings, FreeRDP_ServerHostname));
		result = FALSE;
	}

	return result;
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
	{ 0,
	  check_settings_multimon_disabled,
	  { "testfreerdp", DISABLED_DISPLAY_OPTIONS_RDP, "/multimon:force", "/multimon:off", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_multimon_disabled,
	  { "testfreerdp", DISABLED_DISPLAY_OPTIONS_RDP, "/multimon:force", "-multimon", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_smart_sizing_disabled,
	  { "testfreerdp", DISABLED_DISPLAY_OPTIONS_RDP, "-smart-sizing", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_smart_sizing_disabled,
	  { "testfreerdp", DISABLED_DISPLAY_OPTIONS_RDP, "/smart-sizing:off", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_dynamic_resolution_disabled,
	  { "testfreerdp", DISABLED_DISPLAY_OPTIONS_RDP, "-dynamic-resolution", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ -1,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "--dynamic-resolution", "server.example", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_dynamic_resolution_posix_not_swallowed,
	  { "testfreerdp", "--dynamic-resolution", "-v", "test.freerdp.com", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_multimon_force,
	  { "testfreerdp", "--multimon", "force", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ 0,
	  check_settings_smart_sizing_size,
	  { "testfreerdp", "--smart-sizing", "1024x768", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_ERROR_UNEXPECTED_VALUE,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/multimon:garbage", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_ERROR_UNEXPECTED_VALUE,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "--multimon", "server.example", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_ERROR_UNEXPECTED_VALUE,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/smart-sizing:garbage", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_ERROR_UNEXPECTED_VALUE,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/multimon:banana", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_ERROR,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/dynamic-resolution:off", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_ERROR,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/dynamic-resolution:on", nullptr },
	  { WINPR_C_ARRAY_INIT } },
	{ COMMAND_LINE_ERROR,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/dynamic-resolution:banana", nullptr },
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
