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

#include <winpr/config.h>

#include <winpr/synch.h>

/**
 * WakeByAddressAll
 * WakeByAddressSingle
 * WaitOnAddress
 */

#ifndef _WIN32

VOID WakeByAddressAll(WINPR_ATTR_UNUSED PVOID Address)
{
}

VOID WakeByAddressSingle(WINPR_ATTR_UNUSED PVOID Address)
{
}

BOOL WaitOnAddress(WINPR_ATTR_UNUSED VOID volatile* Address, WINPR_ATTR_UNUSED PVOID CompareAddress,
                   WINPR_ATTR_UNUSED size_t AddressSize, WINPR_ATTR_UNUSED DWORD dwMilliseconds)
{
	return TRUE;
}

#endif
