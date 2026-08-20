/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Windows console helpers
 *
 * Copyright 2026 The FreeRDP Contributors
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

#include "sdl_win32_console.hpp"

#include <cstdio>

#if defined(_WIN32)
#include <winpr/windows.h>
#endif

#if defined(_WIN32) && defined(WITH_WIN_CONSOLE)
#include <io.h>
#endif

namespace sdl
{
	namespace win32
	{
		bool has_inherited_console()
		{
#if defined(_WIN32) && defined(WITH_WIN_CONSOLE)
			const int file = _fileno(stdin);
			const int tty = _isatty(file);
			DWORD processes[2] = {};
			const DWORD count = GetConsoleProcessList(processes, ARRAYSIZE(processes));

			return (tty != 0) && (count > 1);
#else
			return false;
#endif
		}

		void release_transient_console()
		{
#if defined(_WIN32) && defined(WITH_WIN_CONSOLE)
			if (has_inherited_console())
				return;

			if (const HWND hwndConsole = GetConsoleWindow())
				ShowWindow(hwndConsole, SW_HIDE);
			(void)FreeConsole();
#endif
		}
	} // namespace win32
} // namespace sdl
