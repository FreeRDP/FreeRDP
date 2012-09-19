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
 * InitializeSynchronizationBarrier
 * EnterSynchronizationBarrier
 * DeleteSynchronizationBarrier
 */

#ifndef _WIN32

BOOL InitializeSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier, LONG lTotalThreads, LONG lSpinCount)
{
	return TRUE;
}

BOOL EnterSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier, DWORD dwFlags)
{
	return TRUE;
}

BOOL DeleteSynchronizationBarrier(LPSYNCHRONIZATION_BARRIER lpBarrier)
{
	return TRUE;
}

#endif
