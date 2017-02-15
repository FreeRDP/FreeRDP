#include <freerdp/client.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/settings.h>
#include <winpr/cmdline.h>
#include <winpr/spec.h>

#define TESTCASE(cmd, expected_return) \
{ \
	rdpSettings* settings = freerdp_settings_new(0); \
	status = freerdp_client_settings_parse_command_line(settings, ARRAYSIZE(cmd), cmd, FALSE); \
	freerdp_settings_free(settings); \
	if (status != expected_return) { \
		printf("Test argument %s failed\n", #cmd); \
		return -1; \
	} \
}

#define TESTCASE_SUCCESS(cmd) \
{ \
	rdpSettings* settings = freerdp_settings_new(0); \
	status = freerdp_client_settings_parse_command_line(settings, ARRAYSIZE(cmd), cmd, FALSE); \
	freerdp_settings_free(settings); \
	if (status < 0) { \
		printf("Test argument %s failed\n", #cmd); \
		return -1; \
	} \
}

int TestClientCmdLine(int argc, char* argv[])
{
	int status;
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
	char* cmd11[] = {"xfreerdp", "--plugin", "rdpsnd", "--plugin", "rdpdr", "--data", "disk:media:/tmp", "--", "test.freerdp.com" };
	char* cmd12[] = {"xfreerdp", "/sound", "/drive:media:/tmp", "/v:test.freerdp.com" };
	char* cmd13[] = {"xfreerdp", "-u", "test", "-p", "test", "test.freerdp.com"};
	char* cmd14[] = {"xfreerdp", "-u", "test", "-p", "test", "-v", "test.freerdp.com"};
	char* cmd15[] = {"xfreerdp", "/u:test", "/p:test", "/v:test.freerdp.com"};
	char* cmd16[] = {"xfreerdp", "-invalid"};
	char* cmd17[] = {"xfreerdp", "--invalid"};
	char* cmd18[] = {"xfreerdp", "/kbd-list"};
	char* cmd19[] = {"xfreerdp", "/monitor-list"};

	TESTCASE(cmd1, COMMAND_LINE_STATUS_PRINT_HELP);

	TESTCASE(cmd2, COMMAND_LINE_STATUS_PRINT_HELP);

	TESTCASE(cmd3, COMMAND_LINE_STATUS_PRINT_HELP);

	TESTCASE(cmd4, COMMAND_LINE_STATUS_PRINT_VERSION);

	TESTCASE(cmd5, COMMAND_LINE_STATUS_PRINT_VERSION);

	TESTCASE(cmd6, COMMAND_LINE_STATUS_PRINT_VERSION);

	TESTCASE_SUCCESS(cmd7);

	TESTCASE_SUCCESS(cmd8);

	TESTCASE_SUCCESS(cmd9);

	TESTCASE_SUCCESS(cmd10);

	TESTCASE_SUCCESS(cmd11);

	TESTCASE_SUCCESS(cmd12);

	// password gets overwritten therefore it need to be writeable
	cmd13[4] = calloc(5, sizeof(char));
	strncpy(cmd13[4], "test", 4);
	TESTCASE_SUCCESS(cmd13);
	free(cmd13[4]);

	cmd14[4] = calloc(5, sizeof(char));
	strncpy(cmd14[4], "test", 4);
	TESTCASE_SUCCESS(cmd14);
	free(cmd14[4]);

	cmd15[2] = calloc(7, sizeof(char));
	strncpy(cmd15[2], "/p:test", 6);
	TESTCASE_SUCCESS(cmd15);
	free(cmd15[2]);

	TESTCASE(cmd16, COMMAND_LINE_ERROR_NO_KEYWORD);

	TESTCASE(cmd17, COMMAND_LINE_ERROR_NO_KEYWORD);

	TESTCASE(cmd18, COMMAND_LINE_STATUS_PRINT);

	TESTCASE(cmd19, COMMAND_LINE_STATUS_PRINT);

#if 0
	char* cmd20[] = {"-z --plugin cliprdr --plugin rdpsnd --data alsa latency:100 -- --plugin rdpdr --data disk:w7share:/home/w7share -- --plugin drdynvc --data tsmf:decoder:gstreamer -- -u test host.example.com"};
	TESTCASE(cmd20, COMMAND_LINE_STATUS_PRINT);
#endif

	return 0;
}

