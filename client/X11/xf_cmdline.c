/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 client-specific command-line options
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

#include <winpr/assert.h>
#include <winpr/crt.h>

#include "xf_cmdline.h"
#include "xfreerdp.h"

static const char xfreerdp_rail_multi_exec[] = "rail-multi-exec";

static COMMAND_LINE_ARGUMENT_A xfreerdp_cmd_args[] = {
	{ xfreerdp_rail_multi_exec, COMMAND_LINE_VALUE_BOOL, nullptr, BoolValueFalse, nullptr, -1,
	  nullptr, "a primary-side FIFO for launching additional RemoteApps on this connection" },
	{ nullptr, 0, nullptr, nullptr, nullptr, -1, nullptr, nullptr }
};

COMMAND_LINE_ARGUMENT_A* xf_command_line_arguments(size_t* count)
{
	WINPR_ASSERT(count);
	*count = ARRAYSIZE(xfreerdp_cmd_args) - 1U;
	return xfreerdp_cmd_args;
}

int xf_command_line_handle_option(const COMMAND_LINE_ARGUMENT_A* arg, void* userData)
{
	xfContext* xfc = userData;

	WINPR_ASSERT(arg);
	WINPR_ASSERT(xfc);

	if (arg->Name && (strcmp(arg->Name, xfreerdp_rail_multi_exec) == 0))
	{
		xfc->railMultiExec = arg->Value != nullptr;
		return 0;
	}

	return 0;
}
