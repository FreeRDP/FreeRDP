/**
 * WinPR: Windows Portable Runtime
 * atexit routines
 *
 * Copyright 2026 Armin Novak <anovak@thincast.com>
 * Copyright 2026 Thincast Technologies GmbH
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

#include <stdlib.h>
#include <errno.h>

#include <winpr/debug.h>
#include <winpr/atexit.h>
#include "../log.h"

#define TAG WINPR_TAG("atexit")

static size_t atexit_count = 0;

BOOL winpr_atexit(void (*fkt)(void))
{
	atexit_count++;
	const int rc = atexit(fkt);
	if (rc != 0)
	{
		char buffer[128] = WINPR_C_ARRAY_INIT;
		WLog_ERR(TAG, "atexit[%" PRIuz "] failed: %s [%d]", atexit_count,
		         winpr_strerror(errno, buffer, sizeof(buffer)), errno);
		return FALSE;
	}
	return TRUE;
}
