/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Warning log functions
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

#include <winpr/wlog.h>
#include <freerdp/api.h>

/** Print a command line warning about the component being unmaintained.
 *
 * \param log A pointer to a logger to use.
 * \param what A format string to write to the log
 *  \since version 3.28.0
 */
WINPR_ATTR_FORMAT_ARG(2, 3)
FREERDP_API void freerdp_warn_unmaintained(wLog* log, WINPR_FORMAT_ARG const char* what, ...);

/** Print a command line warning about the component being experimental.
 *
 * \param log A pointer to a logger to use.
 * \param what A format string to write to the log
 *  \since version 3.28.0
 */
WINPR_ATTR_FORMAT_ARG(2, 3)
FREERDP_API void freerdp_warn_experimental(wLog* log, WINPR_FORMAT_ARG const char* what, ...);

/** Print a command line warning about the component being deprecated.
 *
 * \param log A pointer to a logger to use.
 * \param what A format string to write to the log
 * \param replacement A optional string pointing to something replacing the feature
 *  \since version 3.28.0
 */
WINPR_ATTR_FORMAT_ARG(2, 4)
FREERDP_API void freerdp_warn_deprecated(wLog* log, WINPR_FORMAT_ARG const char* what,
                                         const char* replacement, ...);
