/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_WLOG_PRIVATE_H
#define WINPR_WLOG_PRIVATE_H

#include <winpr/wlog.h>

#define WLOG_MAX_PREFIX_SIZE	512
#define WLOG_MAX_STRING_SIZE	8192

void WLog_Layout_GetMessagePrefix(wLog* log, wLogLayout* layout, wLogMessage* message);

#include "wlog/Layout.h"
#include "wlog/Appender.h"

#endif /* WINPR_WLOG_PRIVATE_H */
