#include <freerdp/client.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/settings.h>
#include <winpr/cmdline.h>
#include <winpr/spec.h>

#define TESTCASE(cmd, expected_return) status = freerdp_client_settings_parse_command_line(settings, ARRAYSIZE(cmd), cmd); \
   if (status != expected_return) { \
      printf("Test argument %s failed\n", #cmd); \
       return -1; \
    }

#define TESTCASE_SUCCESS(cmd) status = freerdp_client_settings_parse_command_line(settings, ARRAYSIZE(cmd), cmd); \
   if (status < 0) { \
     printf("Test argument %s failed\n", #cmd); \
     return -1; \
   }

int TestClientCmdLine(int argc, char* argv[])
{
	int status;
	rdpSettings* settings = freerdp_settings_new(0);

	char* cmd1[] = {"xfreerdp", "--help"};
	TESTCASE(cmd1, COMMAND_LINE_STATUS_PRINT_HELP);

	char* cmd2[] = {"xfreerdp", "/help"};
	TESTCASE(cmd2, COMMAND_LINE_STATUS_PRINT_HELP);

	char* cmd3[] = {"xfreerdp", "-help"};
	TESTCASE(cmd3, COMMAND_LINE_STATUS_PRINT_HELP);

	char* cmd4[] = {"xfreerdp", "--version"};
	TESTCASE(cmd4, COMMAND_LINE_STATUS_PRINT_VERSION);

	char* cmd5[] = {"xfreerdp", "/version"};
	TESTCASE(cmd5, COMMAND_LINE_STATUS_PRINT_VERSION);

	char* cmd6[] = {"xfreerdp", "-version"};
	TESTCASE(cmd6, COMMAND_LINE_STATUS_PRINT_VERSION);

	char* cmd7[] = {"xfreerdp", "test.freerdp.com"};
	TESTCASE_SUCCESS(cmd7);

	char* cmd8[] = {"xfreerdp", "-v", "test.freerdp.com"};
	TESTCASE_SUCCESS(cmd8);

	char* cmd9[] = {"xfreerdp", "--v", "test.freerdp.com"};
	TESTCASE_SUCCESS(cmd9);

	char* cmd10[] = {"xfreerdp", "/v:test.freerdp.com"};
	TESTCASE_SUCCESS(cmd10);

	char* cmd11[] = {"xfreerdp", "--plugin", "rdpsnd", "--plugin", "rdpdr", "--data", "disk:media:/tmp", "--", "test.freerdp.com" };
	TESTCASE_SUCCESS(cmd11);

	char* cmd12[] = {"xfreerdp", "/sound", "/drive:media:/tmp", "/v:test.freerdp.com" };
	TESTCASE_SUCCESS(cmd12);

	// password gets overwritten therefore it need to be writeable
	char* cmd13[6] = {"xfreerdp", "-u", "test", "-p", "test", "test.freerdp.com"};
	cmd13[4] = malloc(5);
	strncpy(cmd13[4], "test", 4);
	TESTCASE_SUCCESS(cmd13);
	free(cmd13[4]);

	char* cmd14[] = {"xfreerdp", "-u", "test", "-p", "test", "-v", "test.freerdp.com"};
	cmd14[4] = malloc(5);
	strncpy(cmd14[4], "test", 4);
	TESTCASE_SUCCESS(cmd14);
	free(cmd14[4]);

	char* cmd15[] = {"xfreerdp", "/u:test", "/p:test", "/v:test.freerdp.com"};
	cmd15[2] = malloc(7);
	strncpy(cmd15[2], "/p:test", 6);
	TESTCASE_SUCCESS(cmd15);
	free(cmd15[2]);

	char* cmd16[] = {"xfreerdp", "-invalid"};
	TESTCASE(cmd16, COMMAND_LINE_ERROR_NO_KEYWORD);

	char* cmd17[] = {"xfreerdp", "--invalid"};
	TESTCASE(cmd17, COMMAND_LINE_ERROR_NO_KEYWORD);

	char* cmd18[] = {"xfreerdp", "/kbd-list"};
	TESTCASE(cmd18, COMMAND_LINE_STATUS_PRINT);

	char* cmd19[] = {"xfreerdp", "/monitor-list"};
	TESTCASE(cmd19, COMMAND_LINE_STATUS_PRINT);

	return 0;
}

