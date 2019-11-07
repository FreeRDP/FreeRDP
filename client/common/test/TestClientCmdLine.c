#include <freerdp/client.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/settings.h>
#include <winpr/cmdline.h>
#include <winpr/spec.h>
#include <winpr/strlst.h>
#include <winpr/collections.h>

typedef BOOL (*validate_settings_pr)(rdpSettings* settings);

#define printref() printf("%s:%d: in function %-40s:", __FILE__, __LINE__, __FUNCTION__)

#define ERROR(format, ...)                      \
	do                                          \
	{                                           \
		fprintf(stderr, format, ##__VA_ARGS__); \
		printref();                             \
		printf(format, ##__VA_ARGS__);          \
		fflush(stdout);                         \
	} while (0)

#define FAILURE(format, ...)           \
	do                                 \
	{                                  \
		printref();                    \
		printf(" FAILURE ");           \
		printf(format, ##__VA_ARGS__); \
		fflush(stdout);                \
	} while (0)

static void print_test_title(int argc, char** argv)
{
	int i;
	printf("Running test:");

	for (i = 0; i < argc; i++)
	{
		printf(" %s", argv[i]);
	}

	printf("\n");
}

static INLINE BOOL testcase(const char* name, char** argv, size_t argc, int expected_return,
                            validate_settings_pr validate_settings)
{
	int status;
	BOOL valid_settings = TRUE;
	rdpSettings* settings = freerdp_settings_new(0);
	print_test_title(argc, argv);

	if (!settings)
	{
		ERROR("Test %s could not allocate settings!\n", name);
		return FALSE;
	}

	status = freerdp_client_settings_parse_command_line(settings, argc, argv, FALSE);

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
		FAILURE("Expected status %d,  got status %d\n", expected_return, status);
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

	if (settings->RedirectSmartCards)
	{
		FAILURE("Expected RedirectSmartCards = FALSE,  but RedirectSmartCards = TRUE!\n");
		result = FALSE;
	}

	if (freerdp_device_collection_find_type(settings, RDPDR_DTYP_SMARTCARD))
	{
		FAILURE("Expected no SMARTCARD device, but found at least one!\n");
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

static test tests[] = {
	{ COMMAND_LINE_STATUS_PRINT_HELP,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "--help", 0 },
	  { { 0 } } },
	{ COMMAND_LINE_STATUS_PRINT_HELP,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/help", 0 },
	  { { 0 } } },
	{ COMMAND_LINE_STATUS_PRINT_HELP,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "-help", 0 },
	  { { 0 } } },
	{ COMMAND_LINE_STATUS_PRINT_VERSION,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "--version", 0 },
	  { { 0 } } },
	{ COMMAND_LINE_STATUS_PRINT_VERSION,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/version", 0 },
	  { { 0 } } },
	{ COMMAND_LINE_STATUS_PRINT_VERSION,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "-version", 0 },
	  { { 0 } } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "test.freerdp.com", 0 },
	  { { 0 } } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "-v", "test.freerdp.com", 0 },
	  { { 0 } } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "--v", "test.freerdp.com", 0 },
	  { { 0 } } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/v:test.freerdp.com", 0 },
	  { { 0 } } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "--plugin", "rdpsnd", "--plugin", "rdpdr", "--data",
	    "disk:media:" DRIVE_REDIRECT_PATH, "--", "test.freerdp.com", 0 },
	  { { 0 } } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/sound", "/drive:media," DRIVE_REDIRECT_PATH, "/v:test.freerdp.com", 0 },
	  { { 0 } } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "-u", "test", "-p", "test", "test.freerdp.com", 0 },
	  { { 4, "****" }, { 0 } } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "-u", "test", "-p", "test", "-v", "test.freerdp.com", 0 },
	  { { 4, "****" }, { 0 } } },
	{ 0,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/u:test", "/p:test", "/v:test.freerdp.com", 0 },
	  { { 2, "/p:****" }, { 0 } } },
	{ COMMAND_LINE_ERROR_NO_KEYWORD,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "-invalid", 0 },
	  { { 0 } } },
	{ COMMAND_LINE_ERROR_NO_KEYWORD,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "--invalid", 0 },
	  { { 0 } } },
	{ COMMAND_LINE_STATUS_PRINT,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/kbd-list", 0 },
	  { { 0 } } },
	{ COMMAND_LINE_STATUS_PRINT,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/monitor-list", 0 },
	  { { 0 } } },
	{ COMMAND_LINE_ERROR,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/sound", "/drive:media:" DRIVE_REDIRECT_PATH, "/v:test.freerdp.com", 0 },
	  { { 0 } } },
	{ COMMAND_LINE_ERROR,
	  check_settings_smartcard_no_redirection,
	  { "testfreerdp", "/sound", "/drive:media,/foo/bar/blabla", "/v:test.freerdp.com", 0 },
	  { { 0 } } },

#if 0
	{
		COMMAND_LINE_STATUS_PRINT, check_settings_smartcard_no_redirection,
		{"testfreerdp", "-z", "--plugin", "cliprdr", "--plugin", "rdpsnd", "--data", "alsa", "latency:100", "--", "--plugin", "rdpdr", "--data", "disk:w7share:/home/w7share", "--", "--plugin", "drdynvc", "--data", "tsmf:decoder:gstreamer", "--", "-u", "test", "host.example.com", 0},
		{{0}}
	},
#endif
};

void check_modified_arguments(test* test, char** command_line, int* rc)
{
	int k;
	const char* expected_argument;

	for (k = 0; (expected_argument = test->modified_arguments[k].expected_value); k++)
	{
		int index = test->modified_arguments[k].index;
		char* actual_argument = command_line[index];

		if (0 != strcmp(actual_argument, expected_argument))
		{
			printref();
			printf("Failure: overridden argument %d is %s but it should be %s\n", index,
			       actual_argument, expected_argument);
			fflush(stdout);
			*rc = -1;
		}
	}
}

int TestClientCmdLine(int argc, char* argv[])
{
	int rc = 0;
	size_t i;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
	{
		int failure = 0;
		char** command_line = string_list_copy(tests[i].command_line);

		if (!testcase(__FUNCTION__, command_line,
		              string_list_length((const char* const*)command_line),
		              tests[i].expected_status, tests[i].validate_settings))
		{
			FAILURE("parsing arguments.\n");
			failure = 1;
		}

		check_modified_arguments(&tests[i], command_line, &failure);

		if (failure)
		{
			string_list_print(stdout, (const char* const*)command_line);
			rc = -1;
		}

		string_list_free(command_line);
	}

	return rc;
}
