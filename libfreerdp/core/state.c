/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * RDP state machine types and helper functions
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#include "state.h"

#include <winpr/string.h>

BOOL state_run_failed(state_run_t status)
{
	return status == STATE_RUN_FAILED;
}

BOOL state_run_success(state_run_t status)
{
	if (status == STATE_RUN_CONTINUE)
		return TRUE;
	return status >= STATE_RUN_SUCCESS;
}

const char* state_run_result_string(state_run_t status, char* buffer, size_t buffersize)
{
	const char* name;

	switch (status)
	{
		case STATE_RUN_ACTIVE:
			name = "STATE_RUN_ACTIVE";
			break;
		case STATE_RUN_REDIRECT:
			name = "STATE_RUN_REDIRECT";
			break;
		case STATE_RUN_SUCCESS:
			name = "STATE_RUN_SUCCESS";
			break;
		case STATE_RUN_FAILED:
			name = "STATE_RUN_FAILED";
			break;
		case STATE_RUN_TRY_AGAIN:
			name = "STATE_RUN_TRY_AGAIN";
			break;
		case STATE_RUN_CONTINUE:
			name = "STATE_RUN_CONTINUE";
			break;
		default:
			name = "STATE_RUN_UNKNOWN";
			break;
	}

	_snprintf(buffer, buffersize, "%s [%d]", name, status);
	return buffer;
}

BOOL state_run_continue(state_run_t status)
{
	return (status == STATE_RUN_TRY_AGAIN) || (status == STATE_RUN_CONTINUE) ||
	       (status == STATE_RUN_ACTIVE);
}
