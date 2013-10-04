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

#ifndef WINPR_WLOG_APPENDER_PRIVATE_H
#define WINPR_WLOG_APPENDER_PRIVATE_H

#include <winpr/wlog.h>

#include "wlog/FileAppender.h"
#include "wlog/BinaryAppender.h"
#include "wlog/ConsoleAppender.h"

void WLog_Appender_Free(wLog* log, wLogAppender* appender);

#include "wlog/wlog.h"

#endif /* WINPR_WLOG_APPENDER_PRIVATE_H */
 
