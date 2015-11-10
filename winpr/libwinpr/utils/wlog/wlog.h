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


typedef BOOL (*WLOG_APPENDER_OPEN_FN)(wLog* log, wLogAppender* appender);
typedef BOOL (*WLOG_APPENDER_CLOSE_FN)(wLog* log, wLogAppender* appender);
typedef BOOL (*WLOG_APPENDER_WRITE_MESSAGE_FN)(wLog* log, wLogAppender* appender, wLogMessage* message);
typedef BOOL (*WLOG_APPENDER_WRITE_DATA_MESSAGE_FN)(wLog* log, wLogAppender* appender, wLogMessage* message);
typedef BOOL (*WLOG_APPENDER_WRITE_IMAGE_MESSAGE_FN)(wLog* log, wLogAppender* appender, wLogMessage* message);
typedef BOOL (*WLOG_APPENDER_WRITE_PACKET_MESSAGE_FN)(wLog* log, wLogAppender* appender, wLogMessage* message);
typedef BOOL (*WLOG_APPENDER_SET)(wLogAppender* appender, const char *setting, void *value);
typedef void (*WLOG_APPENDER_FREE)(wLogAppender* appender);

#define WLOG_APPENDER_COMMON() \
	DWORD Type; \
	BOOL active; \
	wLogLayout* Layout; \
	CRITICAL_SECTION lock; \
	BOOL recursive; \
	void* TextMessageContext; \
	void* DataMessageContext; \
	void* ImageMessageContext; \
	void* PacketMessageContext; \
	WLOG_APPENDER_OPEN_FN Open; \
	WLOG_APPENDER_CLOSE_FN Close; \
	WLOG_APPENDER_WRITE_MESSAGE_FN WriteMessage; \
	WLOG_APPENDER_WRITE_DATA_MESSAGE_FN WriteDataMessage; \
	WLOG_APPENDER_WRITE_IMAGE_MESSAGE_FN WriteImageMessage; \
	WLOG_APPENDER_WRITE_PACKET_MESSAGE_FN WritePacketMessage; \
	WLOG_APPENDER_FREE Free; \
	WLOG_APPENDER_SET Set


struct _wLogAppender
{
	WLOG_APPENDER_COMMON();
};

struct _wLog
{
	LPSTR Name;
	DWORD Level;

	BOOL IsRoot;
	LPSTR* Names;
	DWORD NameCount;
	wLogAppender* Appender;

	wLog* Parent;
	wLog** Children;
	DWORD ChildrenCount;
	DWORD ChildrenSize;
};


BOOL WLog_Layout_GetMessagePrefix(wLog* log, wLogLayout* layout, wLogMessage* message);

#include "wlog/Layout.h"
#include "wlog/Appender.h"


#endif /* WINPR_WLOG_PRIVATE_H */
