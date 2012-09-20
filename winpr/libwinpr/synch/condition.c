/**
 * WinPR: Windows Portable Runtime
 * Synchronization Functions
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

#include <winpr/synch.h>

#include "synch.h"

/**
 * InitializeConditionVariable
 * SleepConditionVariableCS
 * SleepConditionVariableSRW
 * WakeAllConditionVariable
 * WakeConditionVariable
 */

#ifndef _WIN32

VOID InitializeConditionVariable(PCONDITION_VARIABLE ConditionVariable)
{

}

BOOL SleepConditionVariableCS(PCONDITION_VARIABLE ConditionVariable, PCRITICAL_SECTION CriticalSection, DWORD dwMilliseconds)
{
	return TRUE;
}

BOOL SleepConditionVariableSRW(PCONDITION_VARIABLE ConditionVariable, PSRWLOCK SRWLock, DWORD dwMilliseconds, ULONG Flags)
{
	return TRUE;
}

VOID WakeAllConditionVariable(PCONDITION_VARIABLE ConditionVariable)
{

}

VOID WakeConditionVariable(PCONDITION_VARIABLE ConditionVariable)
{

}

#endif
