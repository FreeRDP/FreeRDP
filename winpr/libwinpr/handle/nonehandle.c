/**
 * WinPR: Windows Portable Runtime
 * NoneHandle a.k.a. brathandle should be used where a handle is needed, but
 * functionality is not implemented yet or not implementable.
 *
 * Copyright 2014 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#include "nonehandle.h"

#ifndef _WIN32

#include <pthread.h>

static BOOL NoneHandleCloseHandle(HANDLE handle)
{
	WINPR_NONE_HANDLE* none = (WINPR_NONE_HANDLE*)handle;
	free(none);
	return TRUE;
}

static BOOL NoneHandleIsHandle(HANDLE handle)
{
	WINPR_NONE_HANDLE* none = (WINPR_NONE_HANDLE*)handle;

	if (!none || none->Type != HANDLE_TYPE_NONE)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}

	return TRUE;
}

static int NoneHandleGetFd(HANDLE handle)
{
	if (!NoneHandleIsHandle(handle))
		return -1;

	return -1;
}

static HANDLE_OPS ops = { NoneHandleIsHandle,
	                      NoneHandleCloseHandle,
	                      NoneHandleGetFd,
	                      NULL, /* CleanupHandle */
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL,
	                      NULL };

HANDLE CreateNoneHandle()
{
	WINPR_NONE_HANDLE* none;
	none = (WINPR_NONE_HANDLE*)calloc(1, sizeof(WINPR_NONE_HANDLE));

	if (!none)
		return NULL;

	none->ops = &ops;
	return (HANDLE)none;
}

#endif
