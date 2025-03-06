/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL common utilitis
 *
 * Copyright 2025 Armin Novak <armin.novak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#include "sdl_common_utils.hpp"

CriticalSection::CriticalSection()
{
	InitializeCriticalSection(&_section);
}

CriticalSection::~CriticalSection()
{
	DeleteCriticalSection(&_section);
}

void CriticalSection::lock()
{
	EnterCriticalSection(&_section);
}

void CriticalSection::unlock()
{
	LeaveCriticalSection(&_section);
}

WinPREvent::WinPREvent(bool initial) : _handle(CreateEventA(nullptr, TRUE, initial, nullptr))
{
}

WinPREvent::~WinPREvent()
{
	(void)CloseHandle(_handle);
}

void WinPREvent::set()
{
	(void)SetEvent(_handle);
}

void WinPREvent::clear()
{
	(void)ResetEvent(_handle);
}

bool WinPREvent::isSet() const
{
	return WaitForSingleObject(_handle, 0) == WAIT_OBJECT_0;
}

HANDLE WinPREvent::handle() const
{
	return _handle;
}

bool operator==(const rdpMonitor& l, const rdpMonitor& r)
{
	if (l.x != r.x)
		return false;
	if (l.y != r.y)
		return false;
	if (l.width != r.width)
		return false;
	if (l.height != r.height)
		return false;
	if (l.is_primary != r.is_primary)
		return false;
	if (l.orig_screen != r.orig_screen)
		return false;

	return l.attributes == r.attributes;
}

bool operator==(const MONITOR_ATTRIBUTES& l, const MONITOR_ATTRIBUTES& r)
{
	if (l.physicalWidth != r.physicalWidth)
		return false;
	if (l.physicalHeight != r.physicalHeight)
		return false;
	if (l.orientation != r.orientation)
		return false;
	if (l.desktopScaleFactor != r.desktopScaleFactor)
		return false;
	if (l.deviceScaleFactor != r.deviceScaleFactor)
		return false;
	return true;
}
