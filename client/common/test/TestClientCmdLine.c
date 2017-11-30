#include <freerdp/client.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/settings.h>
#include <winpr/cmdline.h>
#include <winpr/spec.h>

#define TESTCASE(argv, ret_val) TESTCASE_impl(#argv, argv, ARRAYSIZE(argv), ret_val)
#define TESTCASE_SUCCESS(argv) TESTCASE_impl(#argv, argv, ARRAYSIZE(argv), 0)
static INLINE BOOL TESTCASE_impl(const char* name, char** argv, size_t argc,
                                 int expected_return)
{
	int status;
	rdpSettings* settings = freerdp_settings_new(0);
	printf("Running test %s\n", name);

	if (!settings)
	{
		fprintf(stderr, "Test %s could not allocate settings!\n", name);
		return FALSE;
	}

	status = freerdp_client_settings_parse_command_line(settings, argc, argv, FALSE);
	freerdp_settings_free(settings);

	if (status != expected_return)
	{
		fprintf(stderr, "Test %s failed!\n", name);
		return FALSE;
	}

	return TRUE;
}

#if defined(_WIN32)
#define DRIVE_REDIRECT_PATH "c:\\Windows"
#else
#define DRIVE_REDIRECT_PATH "/tmp"
#endif
int TestClientCmdLine(int argc, char* argv[])
{
	int rc = -1;
	char* cmd1[] = {"xfreerdp", "--help"};
	char* cmd2[] = {"xfreerdp", "/help"};
	char* cmd3[] = {"xfreerdp", "-help"};
	char* cmd4[] = {"xfreerdp", "--version"};
	char* cmd5[] = {"xfreerdp", "/version"};
	char* cmd6[] = {"xfreerdp", "-version"};
	char* cmd7[] = {"xfreerdp", "test.freerdp.com"};
	char* cmd8[] = {"xfreerdp", "-v", "test.freerdp.com"};
	char* cmd9[] = {"xfreerdp", "--v", "test.freerdp.com"};
	char* cmd10[] = {"xfreerdp", "/v:test.freerdp.com"};
	char* cmd11[] = {"xfreerdp", "--plugin", "rdpsnd", "--plugin", "rdpdr", "--data", "disk:media:"DRIVE_REDIRECT_PATH, "--", "test.freerdp.com" };
	char* cmd12[] = {"xfreerdp", "/sound", "/drive:media,"DRIVE_REDIRECT_PATH, "/v:test.freerdp.com" };
	char* cmd13[] = {"xfreerdp", "-u", "test", "-p", "test", "test.freerdp.com"};
	char* cmd14[] = {"xfreerdp", "-u", "test", "-p", "test", "-v", "test.freerdp.com"};
	char* cmd15[] = {"xfreerdp", "/u:test", "/p:test", "/v:test.freerdp.com"};
	char* cmd16[] = {"xfreerdp", "-invalid"};
	char* cmd17[] = {"xfreerdp", "--invalid"};
	char* cmd18[] = {"xfreerdp", "/kbd-list"};
	char* cmd19[] = {"xfreerdp", "/monitor-list"};
	char* cmd20[] = {"xfreerdp", "/sound", "/drive:media:"DRIVE_REDIRECT_PATH, "/v:test.freerdp.com" };
	char* cmd21[] = {"xfreerdp", "/sound", "/drive:media,/foo/bar/blabla", "/v:test.freerdp.com" };

	if (!TESTCASE(cmd1, COMMAND_LINE_STATUS_PRINT_HELP))
		goto fail;

	if (!TESTCASE(cmd2, COMMAND_LINE_STATUS_PRINT_HELP))
		goto fail;

	if (!TESTCASE(cmd3, COMMAND_LINE_STATUS_PRINT_HELP))
		goto fail;

	if (!TESTCASE(cmd4, COMMAND_LINE_STATUS_PRINT_VERSION))
		goto fail;

	if (!TESTCASE(cmd5, COMMAND_LINE_STATUS_PRINT_VERSION))
		goto fail;

	if (!TESTCASE(cmd6, COMMAND_LINE_STATUS_PRINT_VERSION))
		goto fail;

	if (!TESTCASE_SUCCESS(cmd7))
		goto fail;

	if (!TESTCASE_SUCCESS(cmd8))
		goto fail;

	if (!TESTCASE_SUCCESS(cmd9))
		goto fail;

	if (!TESTCASE_SUCCESS(cmd10))
		goto fail;

	if (!TESTCASE_SUCCESS(cmd11))
		goto fail;

	if (!TESTCASE_SUCCESS(cmd12))
		goto fail;

	// password gets overwritten therefore it need to be writeable
	cmd13[4] = _strdup("test");
	cmd14[4] = _strdup("test");
	cmd15[2] = _strdup("/p:test");

	if (!cmd13[4] || !cmd14[4] || !cmd15[2])
		goto free_arg;

	if (!TESTCASE_SUCCESS(cmd13))
		goto free_arg;

	if (memcmp(cmd13[4], "****", 4) != 0)
		goto free_arg;

	if (!TESTCASE_SUCCESS(cmd14))
		goto free_arg;

	if (memcmp(cmd14[4], "****", 4) != 0)
		goto free_arg;

	if (!TESTCASE_SUCCESS(cmd15))
		goto free_arg;

	if (memcmp(cmd15[2], "/p:****", 7) != 0)
		goto free_arg;

	if (!TESTCASE(cmd16, COMMAND_LINE_ERROR_NO_KEYWORD))
		goto free_arg;

	if (!TESTCASE(cmd17, COMMAND_LINE_ERROR_NO_KEYWORD))
		goto free_arg;

	if (!TESTCASE(cmd18, COMMAND_LINE_STATUS_PRINT))
		goto free_arg;

	if (!TESTCASE(cmd19, COMMAND_LINE_STATUS_PRINT))
		goto free_arg;

	if (!TESTCASE(cmd20, COMMAND_LINE_ERROR))
		goto free_arg;

	if (!TESTCASE(cmd21, COMMAND_LINE_ERROR))
		goto free_arg;

	rc = 0;
free_arg:
	free(cmd13[4]);
	free(cmd14[4]);
	free(cmd15[2]);
#if 0
	char* cmd20[] = {"-z --plugin cliprdr --plugin rdpsnd --data alsa latency:100 -- --plugin rdpdr --data disk:w7share:/home/w7share -- --plugin drdynvc --data tsmf:decoder:gstreamer -- -u test host.example.com"};
	TESTCASE(cmd20, COMMAND_LINE_STATUS_PRINT);
#endif
fail:
	return rc;
}

