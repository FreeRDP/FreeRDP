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
#pragma once

#include <freerdp/settings_types.h>

bool operator==(const MONITOR_ATTRIBUTES& l, const MONITOR_ATTRIBUTES& r);
bool operator==(const rdpMonitor& l, const rdpMonitor& r);

class WinPREvent
{
  public:
	explicit WinPREvent(bool initial = false);
	WinPREvent(const WinPREvent& other) = delete;
	WinPREvent(WinPREvent&& other) = delete;

	WinPREvent& operator=(const WinPREvent& other) = delete;
	WinPREvent& operator=(WinPREvent&& other) = delete;

	~WinPREvent();

	void set();
	void clear();
	[[nodiscard]] bool isSet() const;

	[[nodiscard]] HANDLE handle() const;

  private:
	HANDLE _handle;
};

class CriticalSection
{
  public:
	CriticalSection();
	CriticalSection(const CriticalSection& other) = delete;
	CriticalSection(CriticalSection&& other) = delete;
	~CriticalSection();

	CriticalSection& operator=(const CriticalSection& other) = delete;
	CriticalSection& operator=(const CriticalSection&& other) = delete;

	void lock();
	void unlock();

  private:
	CRITICAL_SECTION _section{};
};
