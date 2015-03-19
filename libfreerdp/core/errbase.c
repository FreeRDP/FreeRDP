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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include <freerdp/log.h>

#include "errinfo.h"

#define TAG FREERDP_TAG("core")

#define ERRBASE_DEFINE(_code)	    { ERRBASE_##_code , "ERRBASE_" #_code , ERRBASE_##_code##_STRING }

/* Protocol-independent codes */

/* Special codes */
#define ERRBASE_SUCCESS_STRING "Success."
#define ERRBASE_NONE_STRING ""

static const ERRINFO ERRBASE_CODES[] =
{
		ERRBASE_DEFINE(SUCCESS),

		ERRBASE_DEFINE(NONE)
};

const char* freerdp_get_error_base_string(UINT32 code)
{
	const ERRINFO* errInfo;

	errInfo = &ERRBASE_CODES[0];

	while (errInfo->code != ERRBASE_NONE)
	{
		if (code == errInfo->code)
		{
			return errInfo->info;
		}

		errInfo++;
	}

	return "ERRBASE_UNKNOWN";
}

const char* freerdp_get_error_base_name(UINT32 code)
{
	const ERRINFO* errInfo;

	errInfo = &ERRBASE_CODES[0];

	while (errInfo->code != ERRBASE_NONE)
	{
		if (code == errInfo->code)
		{
			return errInfo->name;
		}

		errInfo++;
	}

	return "ERRBASE_UNKNOWN";
}

