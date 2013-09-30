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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/error.h>

#ifndef _WIN32

#include <stdio.h>

#include <winpr/nt.h>

UINT GetErrorMode(void)
{
	return 0;
}

UINT SetErrorMode(UINT uMode)
{
	return 0;
}

DWORD GetLastError(VOID)
{
	return NtCurrentTeb()->LastErrorValue;
}

VOID SetLastError(DWORD dwErrCode)
{
	NtCurrentTeb()->LastErrorValue = dwErrCode;
}

VOID RestoreLastError(DWORD dwErrCode)
{

}

VOID RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD nNumberOfArguments, CONST ULONG_PTR* lpArguments)
{

}

LONG UnhandledExceptionFilter(PEXCEPTION_POINTERS ExceptionInfo)
{
	return 0;
}

LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter)
{
	return NULL;
}

PVOID AddVectoredExceptionHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler)
{
	return NULL;
}

ULONG RemoveVectoredExceptionHandler(PVOID Handle)
{
	return 0;
}

PVOID AddVectoredContinueHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler)
{
	return NULL;
}

ULONG RemoveVectoredContinueHandler(PVOID Handle)
{
	return 0;
}

#endif
