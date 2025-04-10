/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Error Base
 *
 * Copyright 2015 Armin Novak <armin.novak@thincast.com>
 * Copyright 2015 Thincast Technologies GmbH
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

#include <stdio.h>

#include <freerdp/log.h>

#include "errinfo.h"

#define ERRBASE_DEFINE(_code)                                            \
	{                                                                    \
		ERRBASE_##_code, "ERRBASE_" #_code, ERRBASE_##_code##_STRING, "" \
	}

/* Protocol-independent codes */

/* Special codes */
#define ERRBASE_SUCCESS_STRING "Success."
#define ERRBASE_NONE_STRING ""

static const ERRINFO ERRBASE_CODES[] = { ERRBASE_DEFINE(SUCCESS),

	                                     ERRBASE_DEFINE(NONE) };

const char* freerdp_get_error_base_string(UINT32 code)
{
	for (size_t x = 0; x < ARRAYSIZE(ERRBASE_CODES); x++)
	{
		const ERRINFO* errInfo = &ERRBASE_CODES[x];

		if (code == errInfo->code)
		{
			return errInfo->info;
		}
	}

	return "ERRBASE_UNKNOWN";
}

const char* freerdp_get_error_base_category(UINT32 code)
{
	for (size_t x = 0; x < ARRAYSIZE(ERRBASE_CODES); x++)
	{
		const ERRINFO* errInfo = &ERRBASE_CODES[x];
		if (code == errInfo->code)
		{
			return errInfo->category;
		}
	}

	return "ERRBASE_UNKNOWN";
}

const char* freerdp_get_error_base_name(UINT32 code)
{
	for (size_t x = 0; x < ARRAYSIZE(ERRBASE_CODES); x++)
	{
		const ERRINFO* errInfo = &ERRBASE_CODES[x];
		if (code == errInfo->code)
		{
			return errInfo->name;
		}
	}

	return "ERRBASE_UNKNOWN";
}
