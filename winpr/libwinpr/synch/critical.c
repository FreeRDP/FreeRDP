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

/**
 * InitializeCriticalSection
 * InitializeCriticalSectionAndSpinCount
 * InitializeCriticalSectionEx
 * SetCriticalSectionSpinCount
 * EnterCriticalSection
 * TryEnterCriticalSection
 * LeaveCriticalSection
 * DeleteCriticalSection
 */

VOID InitializeCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{

}

BOOL InitializeCriticalSectionEx(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount, DWORD Flags)
{
	return TRUE;
}

BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
{
	return TRUE;
}

DWORD SetCriticalSectionSpinCount(LPCRITICAL_SECTION lpCriticalSection, DWORD dwSpinCount)
{
	return 0;
}

VOID EnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{

}

BOOL TryEnterCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{
	return TRUE;
}

VOID LeaveCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{

}

VOID DeleteCriticalSection(LPCRITICAL_SECTION lpCriticalSection)
{

}
