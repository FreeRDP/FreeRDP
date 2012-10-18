/**
 * WinPR: Windows Portable Runtime
 * Pipe Functions
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

#ifndef WINPR_PIPE_H
#define WINPR_PIPE_H

#include <winpr/file.h>
#include <winpr/winpr.h>
#include <winpr/error.h>
#include <winpr/handle.h>
#include <winpr/wtypes.h>

#ifndef _WIN32

WINPR_API BOOL CreatePipe(PHANDLE hReadPipe, PHANDLE hWritePipe, LPSECURITY_ATTRIBUTES lpPipeAttributes, DWORD nSize);

#endif

#endif /* WINPR_PIPE_H */
