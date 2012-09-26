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

#ifndef WINPR_ERROR_H
#define WINPR_ERROR_H

#include <winpr/winpr.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

#define EXCEPTION_MAXIMUM_PARAMETERS		15

typedef struct _EXCEPTION_RECORD EXCEPTION_RECORD;
typedef struct _EXCEPTION_RECORD *PEXCEPTION_RECORD;

struct _EXCEPTION_RECORD
{
	DWORD ExceptionCode;
	DWORD ExceptionFlags;
	PEXCEPTION_RECORD ExceptionRecord;
	PVOID ExceptionAddress;
	DWORD NumberParameters;
	ULONG_PTR ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
};

typedef void* PCONTEXT;

typedef struct _EXCEPTION_POINTERS
{
	PEXCEPTION_RECORD ExceptionRecord;
	PCONTEXT ContextRecord;
} EXCEPTION_POINTERS, *PEXCEPTION_POINTERS;

typedef LONG (*PTOP_LEVEL_EXCEPTION_FILTER)(PEXCEPTION_POINTERS ExceptionInfo);
typedef PTOP_LEVEL_EXCEPTION_FILTER LPTOP_LEVEL_EXCEPTION_FILTER;

typedef LONG (*PVECTORED_EXCEPTION_HANDLER)(PEXCEPTION_POINTERS ExceptionInfo);

WINPR_API UINT GetErrorMode(void);

WINPR_API UINT SetErrorMode(UINT uMode);

WINPR_API DWORD GetLastError(VOID);

WINPR_API VOID SetLastError(DWORD dwErrCode);

WINPR_API VOID RestoreLastError(DWORD dwErrCode);

WINPR_API VOID RaiseException(DWORD dwExceptionCode, DWORD dwExceptionFlags, DWORD nNumberOfArguments, CONST ULONG_PTR* lpArguments);

WINPR_API LONG UnhandledExceptionFilter(PEXCEPTION_POINTERS ExceptionInfo);

WINPR_API LPTOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER lpTopLevelExceptionFilter);

WINPR_API PVOID AddVectoredExceptionHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler);

WINPR_API ULONG RemoveVectoredExceptionHandler(PVOID Handle);

WINPR_API PVOID AddVectoredContinueHandler(ULONG First, PVECTORED_EXCEPTION_HANDLER Handler);

WINPR_API ULONG RemoveVectoredContinueHandler(PVOID Handle);

#endif

#endif /* WINPR_ERROR_H */

