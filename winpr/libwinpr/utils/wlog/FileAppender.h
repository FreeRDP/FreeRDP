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

#ifndef WINPR_WLOG_FILE_APPENDER_PRIVATE_H
#define WINPR_WLOG_FILE_APPENDER_PRIVATE_H

#include "wlog.h"

WINPR_ATTR_MALLOC(WLog_Appender_Free, 2)
wLogAppender* WLog_FileAppender_New(wLog* log);

#endif /* WINPR_WLOG_FILE_APPENDER_PRIVATE_H */
