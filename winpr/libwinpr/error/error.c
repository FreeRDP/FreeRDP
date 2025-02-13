/**
 * WinPR: Windows Portable Runtime
 * Error Handling Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/config.h>

#include <winpr/error.h>
#include <winpr/wlog.h>

#ifndef _WIN32

#include <stdio.h>

#include <winpr/nt.h>

UINT GetErrorMode(void)
{
	WLog_ERR("TODO", "TOdO: implement");
	return 0;
}

UINT SetErrorMode(WINPR_ATTR_UNUSED UINT uMode)
{
	WLog_ERR("TODO", "TOdO: implement");
	return 0;
}

DWORD GetLastError(VOID)
{
	PTEB pt = NtCurrentTeb();
	if (pt)
	{
		return pt->LastErrorValue;
	}
	return ERROR_OUTOFMEMORY;
}

VOID SetLastError(DWORD dwErrCode)
{
	PTEB pt = NtCurrentTeb();
	if (pt)
	{
		pt->LastErrorValue = dwErrCode;
	}
}

VOID RestoreLastError(WINPR_ATTR_UNUSED DWORD dwErrCode)
{
	WLog_ERR("TODO", "TOdO: implement");
}

VOID RaiseException(WINPR_ATTR_UNUSED DWORD dwExceptionCode,
                    WINPR_ATTR_UNUSED DWORD dwExceptionFlags,
                    WINPR_ATTR_UNUSED DWORD nNumberOfArguments,
                    WINPR_ATTR_UNUSED CONST ULONG_PTR* lpArguments)
{
	WLog_ERR("TODO", "TOdO: implement");
}

LONG UnhandledExceptionFilter(WINPR_ATTR_UNUSED PEXCEPTION_POINTERS ExceptionInfo)
{
	WLog_ERR("TODO", "TOdO: implement");
	return 0;
}

LPTOP_LEVEL_EXCEPTION_FILTER
SetUnhandledExceptionFilter(
    WINPR_ATTR_UNUSED LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
{
	WLog_ERR("TODO", "TOdO: implement");
	return NULL;
}

PVOID AddVectoredExceptionHandler(WINPR_ATTR_UNUSED ULONG First,
                                  WINPR_ATTR_UNUSED PVECTORED_EXCEPTION_HANDLER Handler)
{
	WLog_ERR("TODO", "TOdO: implement");
	return NULL;
}

ULONG RemoveVectoredExceptionHandler(WINPR_ATTR_UNUSED PVOID Handle)
{
	WLog_ERR("TODO", "TOdO: implement");
	return 0;
}

PVOID AddVectoredContinueHandler(WINPR_ATTR_UNUSED ULONG First,
                                 WINPR_ATTR_UNUSED PVECTORED_EXCEPTION_HANDLER Handler)
{
	WLog_ERR("TODO", "TOdO: implement");
	return NULL;
}

ULONG RemoveVectoredContinueHandler(WINPR_ATTR_UNUSED PVOID Handle)
{
	WLog_ERR("TODO", "TOdO: implement");
	return 0;
}

#endif
