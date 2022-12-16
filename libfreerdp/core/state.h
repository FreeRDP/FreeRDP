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

#ifndef FREERDP_LIB_CORE_STATE_H
#define FREERDP_LIB_CORE_STATE_H

#include <winpr/wtypes.h>
#include <freerdp/api.h>

typedef enum
{
	STATE_RUN_ACTIVE = 2,
	STATE_RUN_REDIRECT = 1,
	STATE_RUN_SUCCESS = 0,
	STATE_RUN_FAILED = -1,
	STATE_RUN_TRY_AGAIN = -23,
	STATE_RUN_CONTINUE = -24
} state_run_t;

FREERDP_LOCAL BOOL state_run_failed(state_run_t status);
FREERDP_LOCAL BOOL state_run_success(state_run_t status);
FREERDP_LOCAL BOOL state_run_continue(state_run_t status);
FREERDP_LOCAL const char* state_run_result_string(state_run_t status, char* buffer,
                                                  size_t buffersize);

#endif /* FREERDP_LIB_CORE_STATE_H */
