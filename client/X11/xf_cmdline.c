/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 Command Line
 *
 * Copyright 2026 William Poetra Yoga <william.poetra@gmail.com>
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

#include <freerdp/config.h>
#include <freerdp/client/cmdline.h>

#include "xf_cmdline.h"
#include "xfreerdp.h"

static COMMAND_LINE_ARGUMENT_A xf_args[] = {
	{ "defer-alt", COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1, nullptr,
	  "Defer Alt key down until another key is pressed. When enabled, Alt-Tab will not activate "
	  "the remote application menu. Useful when used together with -grab-keyboard (default: off)" },
	{ nullptr, 0, nullptr, nullptr, nullptr, -1, nullptr, nullptr }
};

static int xf_client_handle_option(const COMMAND_LINE_ARGUMENT_A* arg, void* custom)
{
	xfContext* xfc = custom;
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(arg);

	if (arg->Name && (strcmp(arg->Name, "defer-alt") == 0))
	{
		xfc->deferAlt = arg->Value != nullptr;
		return 0;
	}
	return COMMAND_LINE_ERROR_UNEXPECTED_VALUE;
}

int xf_client_settings_parse_command_line(xfContext* xfc, int argc, char** argv)
{
	WINPR_ASSERT(xfc);
	xfc->deferAlt = FALSE;
	return freerdp_client_settings_parse_command_line_ex(xfc->common.context.settings, argc, argv,
	                                                     FALSE, xf_args, ARRAYSIZE(xf_args) - 1,
	                                                     xf_client_handle_option, xfc);
}

int xf_client_settings_command_line_status_print(xfContext* xfc, int status, int argc, char** argv)
{
	WINPR_ASSERT(xfc);
	return freerdp_client_settings_command_line_status_print_ex(xfc->common.context.settings,
	                                                            status, argc, argv, xf_args);
}
