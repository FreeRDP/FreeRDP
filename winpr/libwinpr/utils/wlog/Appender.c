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
	wLogAppender* appender = NULL;
	if (!log)
		return NULL;

	if (logAppenderType == WLOG_APPENDER_CONSOLE)
	{
		appender = (wLogAppender*) WLog_ConsoleAppender_New(log);
	}
	else if (logAppenderType == WLOG_APPENDER_FILE)
	{
		appender = (wLogAppender*) WLog_FileAppender_New(log);
	}
	else if (logAppenderType == WLOG_APPENDER_BINARY)
	{
		appender = (wLogAppender*) WLog_BinaryAppender_New(log);
	}
	else if (logAppenderType == WLOG_APPENDER_CALLBACK)
	{
		appender = (wLogAppender*) WLog_CallbackAppender_New(log);
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
	if (appender)
	{
		if (appender->Layout)
		{
			WLog_Layout_Free(log, appender->Layout);
			appender->Layout = NULL;
		}

		DeleteCriticalSection(&appender->lock);

		if (appender->Type == WLOG_APPENDER_CONSOLE)
		{
			WLog_ConsoleAppender_Free(log, (wLogConsoleAppender*) appender);
		}
		else if (appender->Type == WLOG_APPENDER_FILE)
		{
			WLog_FileAppender_Free(log, (wLogFileAppender*) appender);
		}
		else if (appender->Type == WLOG_APPENDER_BINARY)
		{
			WLog_BinaryAppender_Free(log, (wLogBinaryAppender*) appender);
		}
		else if (appender->Type == WLOG_APPENDER_CALLBACK)
		{
			WLog_CallbackAppender_Free(log, (wLogCallbackAppender*) appender);
		}
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

void WLog_SetLogAppenderType(wLog* log, DWORD logAppenderType)
{
	if (log->Appender)
	{
		WLog_Appender_Free(log, log->Appender);
		log->Appender = NULL;
	}

	log->Appender = WLog_Appender_New(log, logAppenderType);
}

int WLog_OpenAppender(wLog* log)
{
	int status = 0;
	wLogAppender* appender;

	appender = WLog_GetLogAppender(log);

	if (!appender)
		return -1;

	if (!appender->Open)
		return 0;

	if (!appender->State)
	{
		status = appender->Open(log, appender);
		appender->State = 1;
	}

	return status;
}

int WLog_CloseAppender(wLog* log)
{
	int status = 0;
	wLogAppender* appender;

	appender = WLog_GetLogAppender(log);

	if (!appender)
		return -1;

	if (!appender->Close)
		return 0;

	if (appender->State)
	{
		status = appender->Close(log, appender);
		appender->State = 0;
	}

	return status;
}
