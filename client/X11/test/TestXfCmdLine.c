/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 command-line tests
 *
 * Copyright 2026 Tony Dursun <oraturk75@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <freerdp/client.h>
#include <freerdp/settings.h>

#include <winpr/cmdline.h>
#include <winpr/crt.h>

#include "xf_cmdline.h"
#include "xfreerdp.h"

static int parse(xfContext* xfc, int argc, char** argv)
{
	size_t count = 0;
	COMMAND_LINE_ARGUMENT_A* arguments = xf_command_line_arguments(&count);
	(void)CommandLineClearArgumentsA(arguments);
	return freerdp_client_settings_parse_command_line_ex(xfc->common.context.settings, argc, argv,
	                                                     FALSE, arguments, count,
	                                                     xf_command_line_handle_option, xfc);
}

static BOOL test_help_contains_option(void)
{
	BOOL rc = FALSE;
	int saved = -1;
	FILE* capture = nullptr;
	rdpSettings* settings = freerdp_settings_new(0);
	if (!settings)
		return FALSE;
	xfContext xfc = WINPR_C_ARRAY_INIT;
	xfc.common.context.settings = settings;
	char* argv[] = { "xfreerdp", "/help", nullptr };
	const int status = parse(&xfc, 2, argv);
	if (status != COMMAND_LINE_STATUS_PRINT_HELP)
		goto out;

	capture = tmpfile();
	if (!capture)
		goto out;
	saved = dup(STDOUT_FILENO);
	if ((saved < 0) || (fflush(stdout) != 0) || (dup2(fileno(capture), STDOUT_FILENO) < 0))
		goto out;
	size_t count = 0;
	COMMAND_LINE_ARGUMENT_A* arguments = xf_command_line_arguments(&count);
	WINPR_UNUSED(count);
	if (freerdp_client_settings_command_line_status_print_ex(settings, status, 2, argv,
	                                                         arguments) != 0)
		goto restore;
	if ((fflush(stdout) != 0) || (fseek(capture, 0, SEEK_SET) != 0))
		goto restore;
	char line[1024] = WINPR_C_ARRAY_INIT;
	while (fgets(line, sizeof(line), capture))
	{
		if (strstr(line, "rail-multi-exec"))
		{
			rc = TRUE;
			break;
		}
	}

restore:
	(void)fflush(stdout);
	if (saved >= 0)
	{
		(void)dup2(saved, STDOUT_FILENO);
		close(saved);
		saved = -1;
	}

out:
	if (saved >= 0)
		close(saved);
	if (capture)
		fclose(capture);
	freerdp_settings_free(settings);
	return rc;
}

int main(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);
	rdpSettings* settings = freerdp_settings_new(0);
	if (!settings)
		return 1;
	xfContext xfc = WINPR_C_ARRAY_INIT;
	xfc.common.context.settings = settings;

	char* defaults[] = { "xfreerdp", "/v:test.example.invalid", nullptr };
	if ((parse(&xfc, 2, defaults) != 0) || xfc.railMultiExec)
	{
		fprintf(stderr, "default-off case failed\n");
		goto fail;
	}

	char* port[] = { "xfreerdp", "/v:test.example.invalid", "/port:3390", nullptr };
	if ((parse(&xfc, 3, port) != 0) ||
	    (freerdp_settings_get_uint32(settings, FreeRDP_ServerPort) != 3390))
	{
		fprintf(stderr, "global-option passthrough case failed\n");
		goto fail;
	}

	char* enable[] = { "xfreerdp", "+rail-multi-exec", "/v:test.example.invalid", nullptr };
	if ((parse(&xfc, 3, enable) != 0) || !xfc.railMultiExec)
	{
		fprintf(stderr, "enable case failed\n");
		goto fail;
	}

	char* disable[] = { "xfreerdp", "-rail-multi-exec", "/v:test.example.invalid", nullptr };
	if ((parse(&xfc, 3, disable) != 0) || xfc.railMultiExec)
	{
		fprintf(stderr, "disable case failed\n");
		goto fail;
	}

	char* malformed[] = { "xfreerdp", "+rail-multi-exec:yes", "/v:test.example.invalid", nullptr };
	if (parse(&xfc, 3, malformed) >= 0)
	{
		fprintf(stderr, "malformed-value case failed\n");
		goto fail;
	}

	freerdp_settings_free(settings);
	if (!test_help_contains_option())
	{
		fprintf(stderr, "help-presence case failed\n");
		return 1;
	}
	printf("PASS rail-multi-exec is default-off, preserves global options, accepts +/-, rejects "
	       "malformed syntax, and appears in /help\n");
	return 0;

fail:
	freerdp_settings_free(settings);
	return 1;
}
