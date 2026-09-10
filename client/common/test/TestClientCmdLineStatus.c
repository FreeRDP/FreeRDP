#include <freerdp/client.h>
#include <freerdp/client/cmdline.h>
#include <freerdp/settings.h>
#include <winpr/cmdline.h>
#include <winpr/strlst.h>

int TestClientCmdLineStatus(int argc, char* argv[])
{
	const struct
	{
		const char* args[6];
		int expected_status;
		BOOL list_monitors;
	} tests[] = {
		{ { "testfreerdp", "/list:kbd", nullptr }, 0, FALSE },
		{ { "testfreerdp", "--list", "kbd", nullptr }, 0, FALSE },
		{ { "testfreerdp", "-list", "kbd", nullptr }, 0, FALSE },
		{ { "testfreerdp", "/list:monitor", nullptr }, 0, TRUE },
		{ { "testfreerdp", "--list", "monitor", nullptr }, 0, TRUE },
		{ { "testfreerdp", "-list", "monitor", nullptr }, 0, TRUE },
		{ { "testfreerdp", "/log-level:ERROR", "/list:monitor", nullptr }, 0, TRUE },
		{ { "testfreerdp", "--log-level", "ERROR", "--list", "monitor", nullptr }, 0, TRUE },
		{ { "testfreerdp", "/list:invalid", nullptr }, COMMAND_LINE_ERROR, FALSE },
		{ { "testfreerdp", "--list", "invalid", nullptr }, COMMAND_LINE_ERROR, FALSE },
	};
	int rc = 0;

	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	for (size_t i = 0; i < ARRAYSIZE(tests); i++)
	{
		rdpSettings* settings = freerdp_settings_new(0);
		char** args = string_list_copy(tests[i].args);
		if (!settings || !args)
		{
			freerdp_settings_free(settings);
			string_list_free(args);
			return -1;
		}

		const int count = string_list_length((const char* const*)args);
		int status = freerdp_client_settings_parse_command_line(settings, count, args, FALSE);
		if (status != COMMAND_LINE_STATUS_PRINT)
		{
			fprintf(stderr, "Test %zu: expected print status, got %d\n", i, status);
			rc = -1;
		}
		else
		{
			status =
			    freerdp_client_settings_command_line_status_print(settings, status, count, args);
			if (status != tests[i].expected_status)
			{
				fprintf(stderr, "Test %zu: expected status %d, got %d\n", i,
				        tests[i].expected_status, status);
				rc = -1;
			}
			if (freerdp_settings_get_bool(settings, FreeRDP_ListMonitors) != tests[i].list_monitors)
			{
				fprintf(stderr, "Test %zu: unexpected ListMonitors setting\n", i);
				rc = -1;
			}
		}

		freerdp_settings_free(settings);
		string_list_free(args);
	}

	return rc;
}
