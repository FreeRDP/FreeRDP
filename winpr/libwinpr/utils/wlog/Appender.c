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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/wlog.h>

#include "wlog/Layout.h"

#include "wlog/Appender.h"

wLogAppender* WLog_Appender_New(wLog* log, DWORD logAppenderType)
{
	wLogAppender* appender;

	if (!log)
		return NULL;

	switch (logAppenderType)
	{
	case WLOG_APPENDER_CONSOLE:
		appender = (wLogAppender*) WLog_ConsoleAppender_New(log);
		break;
	case WLOG_APPENDER_FILE:
		appender = (wLogAppender*) WLog_FileAppender_New(log);
		break;
	case WLOG_APPENDER_BINARY:
		appender = (wLogAppender*) WLog_BinaryAppender_New(log);
		break;
	case WLOG_APPENDER_CALLBACK:
		appender = (wLogAppender*) WLog_CallbackAppender_New(log);
		break;
#ifdef HAVE_SYSLOG_H
	case WLOG_APPENDER_SYSLOG:
		appender = (wLogAppender*) WLog_SyslogAppender_New(log);
		break;
#endif
	default:
		fprintf(stderr, "%s: unknown handler type %d\n", __FUNCTION__, logAppenderType);
		appender = NULL;
		break;
	}

	if (!appender)
		appender = (wLogAppender*) WLog_ConsoleAppender_New(log);

	if (!appender)
		return NULL;

	if (!(appender->Layout = WLog_Layout_New(log)))
	{
		WLog_Appender_Free(log, appender);
		return NULL;
	}

	InitializeCriticalSectionAndSpinCount(&appender->lock, 4000);

	return appender;
}

void WLog_Appender_Free(wLog* log, wLogAppender* appender)
{
	if (!appender)
		return;

	if (appender->Layout)
	{
		WLog_Layout_Free(log, appender->Layout);
		appender->Layout = NULL;
	}

	DeleteCriticalSection(&appender->lock);

	switch (appender->Type)
	{
	case WLOG_APPENDER_CONSOLE:
		WLog_ConsoleAppender_Free(log, (wLogConsoleAppender*) appender);
		break;
	case WLOG_APPENDER_FILE:
		WLog_FileAppender_Free(log, (wLogFileAppender*) appender);
		break;
	case WLOG_APPENDER_BINARY:
		WLog_BinaryAppender_Free(log, (wLogBinaryAppender*) appender);
		break;
	case WLOG_APPENDER_CALLBACK:
		WLog_CallbackAppender_Free(log, (wLogCallbackAppender*) appender);
		break;
#ifdef HAVE_SYSLOG_H
	case WLOG_APPENDER_SYSLOG:
		WLog_SyslogAppender_Free(log, (wLogSyslogAppender *) appender);
		break;
#endif
	default:
		fprintf(stderr, "%s: don't know how to free appender type %d\n", __FUNCTION__, appender->Type);
		break;
	}
}

wLogAppender* WLog_GetLogAppender(wLog* log)
{
	if (!log)
		return NULL;

	if (!log->Appender)
		return WLog_GetLogAppender(log->Parent);

	return log->Appender;
}

BOOL WLog_SetLogAppenderType(wLog* log, DWORD logAppenderType)
{
	if (log->Appender)
	{
		WLog_Appender_Free(log, log->Appender);
		log->Appender = NULL;
	}

	log->Appender = WLog_Appender_New(log, logAppenderType);
	return log->Appender != NULL;
}

BOOL WLog_OpenAppender(wLog* log)
{
	int status = 0;
	wLogAppender* appender;

	appender = WLog_GetLogAppender(log);

	if (!appender)
		return FALSE;

	if (!appender->Open)
		return TRUE;

	if (!appender->State)
	{
		status = appender->Open(log, appender);
		appender->State = 1;
	}

	return status;
}

BOOL WLog_CloseAppender(wLog* log)
{
	int status = 0;
	wLogAppender* appender;

	appender = WLog_GetLogAppender(log);

	if (!appender)
		return FALSE;

	if (!appender->Close)
		return TRUE;

	if (appender->State)
	{
		status = appender->Close(log, appender);
		appender->State = 0;
	}

	return status;
}
