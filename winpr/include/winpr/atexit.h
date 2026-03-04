/**
 * WinPR: Windows Portable Runtime
 * atexit routines
 *
 * Copyright 2026 Armin Novak <anovak@thincast.com>
 * Copyright 2026 Thincast Technologies GmbH
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

#include <winpr/wtypes.h>

/** @brief registers a function called on program termination.
 *  This is a wrapper around \ref atexit posix function. The main purpose is having a central point
 * to check if the registrations work and print error logs if not.
 *
 *  @param fkt The exit handler to call
 *  @return \b TRUE if registration was successful, \b FALSE otherwise.
 *  @since version 3.24.0
 */
WINPR_API BOOL winpr_atexit(void (*fkt)(void));
