#include <freerdp/client.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/settings.h>
#include <winpr/cmdline.h>
#include <winpr/spec.h>
#include <winpr/strlst.h>
#include <winpr/collections.h>


typedef BOOL (*validate_settings_pr)(rdpSettings* settings);

#define printref() printf("%s:%d: in function %-40s:", __FILE__, __LINE__, __FUNCTION__)

#define ERROR(format, ...)                                              \
	do{                                                             \
		fprintf(stderr, format, ##__VA_ARGS__);                 \
		printref();                                             \
		printf(format, ##__VA_ARGS__);                          \
		fflush(stdout);                                         \
	}while(0)

#define FAILURE(format, ...)                                            \
	do{                                                             \
		printref();                                             \
		printf(" FAILURE ");                                    \
		printf(format, ##__VA_ARGS__);                          \
		fflush(stdout);                                         \
	}while(0)


static void print_test_title(int argc, char** argv)
{
	int i;
	printf("Running test:");

	for (i = 0; i < argc; i ++)
	{
		printf(" %s", argv[i]);
	}

	printf("\n");
}

static INLINE BOOL testcase(const char* name, char** argv, size_t argc,
                            int expected_return, validate_settings_pr validate_settings)
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

BOOL LinkedList_ContainsString(wLinkedList* list, const char* string);

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

static const char* smartcard_device_name_one = "Xiring";
static const char* smartcard_device_name_two = "NeoWave";

void LinkedList_PrintStrings(wLinkedList* list);

static BOOL expect_smartcard_device_filter_contains(rdpSettings* settings, const char* filter)
{
	RDPDR_SMARTCARD* device = (RDPDR_SMARTCARD*)freerdp_device_collection_find_type(settings,
	                          RDPDR_DTYP_SMARTCARD);

	if (!device)
	{
		FAILURE("Expected to find smartcard device record, but found none!\n");
		return FALSE;
	}

	if (!device->deviceFilter)
	{
		FAILURE("Device filter list not initialized!\n");
		return FALSE;
	}

	if (!LinkedList_ContainsString(device->deviceFilter, filter))
	{
		FAILURE("Device filter list does not contain \"%s\"\n", filter);
		FAILURE("deviceFilter = ");
		LinkedList_PrintStrings(device->deviceFilter);
		return FALSE;
	}

	return TRUE;
}

static BOOL expect_one_smartcard_device(rdpSettings* settings)
{
	UINT32 count;
	BOOL result = TRUE;
	count = freerdp_device_collection_count_type(settings, RDPDR_DTYP_SMARTCARD);

	if (1 != count)
	{
		FAILURE("Expected ONE smartcard device, but found %d!\n", count);
		result = FALSE;
	}

	if (1 <= count)
	{
		if (!freerdp_device_collection_find_type(settings, RDPDR_DTYP_SMARTCARD))
		{
			FAILURE("Expected ONE smartcard device record, but found none!\n");
			result = FALSE;
		}
	}

	return result;
}


static BOOL check_settings_smartcard_redirect_all(rdpSettings* settings)
{
	BOOL result = TRUE;

	if (!settings->RedirectSmartCards)
	{
		FAILURE("Expected RedirectSmartCards = TRUE,  but RedirectSmartCards = FALSE!\n");
		result = FALSE;
	}

	if (!expect_one_smartcard_device(settings))
	{
		result = FALSE;
	}

	if (!expect_smartcard_device_filter_contains(settings, ""))
	{
		result = FALSE;
	}

	return result;
}

static BOOL check_settings_smartcard_redirect_one(rdpSettings* settings)
{
	BOOL result = TRUE;

	if (!settings->RedirectSmartCards)
	{
		FAILURE("Expected RedirectSmartCards = TRUE,  but RedirectSmartCards = FALSE!\n");
		result = FALSE;
	}

	if (!expect_one_smartcard_device(settings))
	{
		result = FALSE;
	}

	if (!expect_smartcard_device_filter_contains(settings, smartcard_device_name_one))
	{
		result = FALSE;
	}

	return result;
}

static BOOL check_settings_smartcard_redirect_two(rdpSettings* settings)
{
	BOOL result = TRUE;

	if (!settings->RedirectSmartCards)
	{
		FAILURE("Expected RedirectSmartCards = TRUE,  but RedirectSmartCards = FALSE!\n");
		result = FALSE;
	}

	if (!expect_one_smartcard_device(settings))
	{
		result = FALSE;
	}

	if (!expect_smartcard_device_filter_contains(settings, smartcard_device_name_one))
	{
		result = FALSE;
	}

	if (!expect_smartcard_device_filter_contains(settings, smartcard_device_name_two))
	{
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
		const char*   expected_value;
	} modified_arguments[8];
} test;

static test tests[] =
{
	{
		COMMAND_LINE_STATUS_PRINT_HELP, check_settings_smartcard_no_redirection,
		{"xfreerdp", "--help", 0},
		{{0}}
	},
	{
		COMMAND_LINE_STATUS_PRINT_HELP, check_settings_smartcard_no_redirection,
		{"xfreerdp", "/help", 0},
		{{0}}
	},
	{
		COMMAND_LINE_STATUS_PRINT_HELP, check_settings_smartcard_no_redirection,
		{"xfreerdp", "-help", 0},
		{{0}}
	},
	{
		COMMAND_LINE_STATUS_PRINT_VERSION, check_settings_smartcard_no_redirection,
		{"xfreerdp", "--version", 0},
		{{0}}
	},
	{
		COMMAND_LINE_STATUS_PRINT_VERSION, check_settings_smartcard_no_redirection,
		{"xfreerdp", "/version", 0},
		{{0}}
	},
	{
		COMMAND_LINE_STATUS_PRINT_VERSION, check_settings_smartcard_no_redirection,
		{"xfreerdp", "-version", 0},
		{{0}}
	},
	{
		0, check_settings_smartcard_no_redirection,
		{"xfreerdp", "test.freerdp.com", 0},
		{{0}}
	},
	{
		0, check_settings_smartcard_no_redirection,
		{"xfreerdp", "-v", "test.freerdp.com", 0},
		{{0}}
	},
	{
		0, check_settings_smartcard_no_redirection,
		{"xfreerdp", "--v", "test.freerdp.com", 0},
		{{0}}
	},
	{
		0, check_settings_smartcard_no_redirection,
		{"xfreerdp", "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0, check_settings_smartcard_no_redirection,
		{"xfreerdp", "--plugin", "rdpsnd", "--plugin", "rdpdr", "--data", "disk:media:"DRIVE_REDIRECT_PATH, "--", "test.freerdp.com", 0},
		{{0}}
	},
	{
		0, check_settings_smartcard_no_redirection,
		{"xfreerdp", "/sound", "/drive:media,"DRIVE_REDIRECT_PATH, "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0, check_settings_smartcard_no_redirection,
		{"xfreerdp", "-u", "test", "-p", "test", "test.freerdp.com", 0},
		{{4, "****"}, {0}}
	},
	{
		0, check_settings_smartcard_no_redirection,
		{"xfreerdp", "-u", "test", "-p", "test", "-v", "test.freerdp.com", 0},
		{{4, "****"}, {0}}
	},
	{
		0, check_settings_smartcard_no_redirection,
		{"xfreerdp", "/u:test", "/p:test", "/v:test.freerdp.com", 0},
		{{2, "/p:****"}, {0}}
	},
	{
		COMMAND_LINE_ERROR_NO_KEYWORD, check_settings_smartcard_no_redirection,
		{"xfreerdp", "-invalid", 0},
		{{0}}
	},
	{
		COMMAND_LINE_ERROR_NO_KEYWORD, check_settings_smartcard_no_redirection,
		{"xfreerdp", "--invalid", 0},
		{{0}}
	},
	{
		COMMAND_LINE_STATUS_PRINT, check_settings_smartcard_no_redirection,
		{"xfreerdp", "/kbd-list", 0},
		{{0}}
	},
	{
		COMMAND_LINE_STATUS_PRINT, check_settings_smartcard_no_redirection,
		{"xfreerdp", "/monitor-list", 0},
		{{0}}
	},
	{
		COMMAND_LINE_ERROR, check_settings_smartcard_no_redirection,
		{"xfreerdp", "/sound", "/drive:media:"DRIVE_REDIRECT_PATH, "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		COMMAND_LINE_ERROR, check_settings_smartcard_no_redirection,
		{"xfreerdp", "/sound", "/drive:media,/foo/bar/blabla", "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0,  check_settings_smartcard_redirect_all,
		{"xfreerdp", "/smartcard", "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0, check_settings_smartcard_redirect_all,
		{"xfreerdp", "/smartcard", "/smartcard:Xiring",  "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0, check_settings_smartcard_redirect_all,
		{"xfreerdp", "/smartcard:,Xiring",  "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0, check_settings_smartcard_redirect_all,
		{"xfreerdp",  "/smartcard:Xiring", "/smartcard", "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0, check_settings_smartcard_redirect_all,
		{"xfreerdp", "/smartcard:Xiring,",  "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0,  check_settings_smartcard_redirect_one,
		{"xfreerdp", "/smartcard:Xiring",  "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0,  check_settings_smartcard_redirect_one,
		{"xfreerdp", "/smartcard:Xiring", "/smartcard:Xiring",  "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0,  check_settings_smartcard_redirect_one,
		{"xfreerdp", "/smartcard:Xiring,Xiring",  "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0,  check_settings_smartcard_redirect_two,
		{"xfreerdp", "/smartcard:Xiring", "/smartcard:NeoWave" ,  "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0,  check_settings_smartcard_redirect_two,
		{"xfreerdp", "/smartcard:Xiring,NeoWave" ,  "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0,  check_settings_smartcard_redirect_two,
		{"xfreerdp", "/smartcard:Xiring", "/smartcard:NeoWave", "/smartcard:Xiring", "/smartcard:NeoWave", "/v:test.freerdp.com", 0},
		{{0}}
	},
	{
		0,  check_settings_smartcard_redirect_two,
		{"xfreerdp", "/smartcard:Xiring,NeoWave,Xiring,NeoWave", "/v:test.freerdp.com", 0},
		{{0}}
	},

#if 0
	{
		COMMAND_LINE_STATUS_PRINT, check_settings_smartcard_no_redirection,
		{"xfreerdp", "-z", "--plugin", "cliprdr", "--plugin", "rdpsnd", "--data", "alsa", "latency:100", "--", "--plugin", "rdpdr", "--data", "disk:w7share:/home/w7share", "--", "--plugin", "drdynvc", "--data", "tsmf:decoder:gstreamer", "--", "-u", "test", "host.example.com", 0},
		{{0}}
	},
#endif
};


void check_modified_arguments(test* test, char** command_line, int* rc)
{
	int k;
	const char*   expected_argument;

	for (k = 0; (expected_argument = test->modified_arguments[k].expected_value); k ++)
	{
		int index = test->modified_arguments[k].index;
		char* actual_argument = command_line[index];

		if (0 != strcmp(actual_argument, expected_argument))
		{
			FAILURE("Failure: overridden argument %d is %s but it should be %s\n",
			        index, actual_argument, expected_argument);
			fflush(stdout);
			* rc = -1;
		}
	}
}

int TestClientCmdLine(int argc, char* argv[])
{
	int rc = 0;
	int i;

	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i ++)
	{
		int failure = 0;
		char** command_line = string_list_copy(tests[i].command_line);

		if (!testcase(__FUNCTION__,
		              command_line, string_list_length((const char * const*)command_line),
		              tests[i].expected_status, tests[i].validate_settings))
		{
			FAILURE("parsing arguments.\n");
			failure = 1;
		}

		check_modified_arguments(& tests[i], command_line, & failure);

		if (failure)
		{
			string_list_print(stdout, (const char * const*)command_line);
			rc = -1;
		}

		string_list_free(command_line);
	}

	return rc;
}
