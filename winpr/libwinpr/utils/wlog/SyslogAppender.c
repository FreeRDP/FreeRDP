/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2013 David FORT <contact@hardening-consulting.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <syslog.h>

#include <winpr/crt.h>
#include <winpr/environment.h>
#include <winpr/thread.h>

#include <winpr/wlog.h>

#include "wlog/Message.h"
#include "wlog/SyslogAppender.h"

static int getSyslogLevel(DWORD level)
{
	switch (level)
	{
	case WLOG_TRACE:
	case WLOG_DEBUG:
		return LOG_DEBUG;
	case WLOG_INFO:
		return LOG_INFO;
	case WLOG_WARN:
		return LOG_WARNING;
	case WLOG_ERROR:
		return LOG_ERR;
	case WLOG_FATAL:
		return LOG_CRIT;
	case WLOG_OFF:
	default:
		return -1;
	}
}

static BOOL WLog_SyslogAppender_Open(wLog* log, wLogSyslogAppender* appender)
{
	if (!log || !appender)
		return FALSE;

	return TRUE;
}

static BOOL WLog_SyslogAppender_Close(wLog* log, wLogSyslogAppender* appender)
{
	if (!log || !appender)
		return FALSE;

	return TRUE;
}

static BOOL WLog_SyslogAppender_WriteMessage(wLog* log, wLogSyslogAppender* appender, wLogMessage* message)
{
	int syslogLevel;

	if (!log || !appender || !message)
		return FALSE;

	syslogLevel = getSyslogLevel(message->Level);
	if (syslogLevel >= 0)
		syslog(syslogLevel, "%s", message->TextString);
	
	return TRUE;
}

static BOOL WLog_SyslogAppender_WriteDataMessage(wLog* log, wLogSyslogAppender* appender, wLogMessage* message)
{
	int syslogLevel;

	if (!log || !appender || !message)
		return FALSE;

	syslogLevel = getSyslogLevel(message->Level);
	if (syslogLevel >= 0)
		syslog(syslogLevel, "skipped data message of %d bytes", message->Length);

	return TRUE;
}

static BOOL WLog_SyslogAppender_WriteImageMessage(wLog* log, wLogSyslogAppender* appender, wLogMessage* message)
{
	int syslogLevel;

	if (!log || !appender || !message)
		return FALSE;

	syslogLevel = getSyslogLevel(message->Level);
	if (syslogLevel >= 0)
		syslog(syslogLevel, "skipped image (%dx%dx%d)", message->ImageWidth, message->ImageHeight, message->ImageBpp);

	return TRUE;
}

wLogSyslogAppender* WLog_SyslogAppender_New(wLog* log)
{
	wLogSyslogAppender* appender;

	appender = (wLogSyslogAppender*) calloc(1, sizeof(wLogSyslogAppender));
	if (!appender)
		return NULL;

	appender->Type = WLOG_APPENDER_SYSLOG;

	appender->Open = (WLOG_APPENDER_OPEN_FN) WLog_SyslogAppender_Open;
	appender->Close = (WLOG_APPENDER_OPEN_FN) WLog_SyslogAppender_Close;
	appender->WriteMessage =
			(WLOG_APPENDER_WRITE_MESSAGE_FN) WLog_SyslogAppender_WriteMessage;
	appender->WriteDataMessage =
			(WLOG_APPENDER_WRITE_DATA_MESSAGE_FN) WLog_SyslogAppender_WriteDataMessage;
	appender->WriteImageMessage =
			(WLOG_APPENDER_WRITE_IMAGE_MESSAGE_FN) WLog_SyslogAppender_WriteImageMessage;

	return appender;
}

void WLog_SyslogAppender_Free(wLog* log, wLogSyslogAppender* appender)
{
	if (appender)
		free(appender);
}
