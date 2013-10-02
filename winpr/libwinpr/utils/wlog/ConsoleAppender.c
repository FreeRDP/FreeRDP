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

#include <winpr/crt.h>

#include <winpr/wlog.h>

#include "wlog/ConsoleAppender.h"

/**
 * Console Appender
 */

void WLog_ConsoleAppender_SetOutputStream(wLog* log, wLogConsoleAppender* appender, int outputStream)
{
	if (!appender)
		return;

	if (appender->Type != WLOG_APPENDER_CONSOLE)
		return;

	if (outputStream < 0)
		outputStream = WLOG_CONSOLE_STDOUT;

	if (outputStream == WLOG_CONSOLE_STDOUT)
		appender->outputStream = WLOG_CONSOLE_STDOUT;
	else if (outputStream == WLOG_CONSOLE_STDERR)
		appender->outputStream = WLOG_CONSOLE_STDERR;
	else
		appender->outputStream = WLOG_CONSOLE_STDOUT;
}

int WLog_ConsoleAppender_Open(wLog* log, wLogConsoleAppender* appender)
{
	return 0;
}

int WLog_ConsoleAppender_Close(wLog* log, wLogConsoleAppender* appender)
{
	return 0;
}

int WLog_ConsoleAppender_WriteMessage(wLog* log, wLogConsoleAppender* appender, wLogMessage* message)
{
	FILE* fp;
	char prefix[WLOG_MAX_PREFIX_SIZE];

	if (message->Level > log->Level)
		return 0;

	fp = (appender->outputStream == WLOG_CONSOLE_STDERR) ? stderr : stdout;

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	fprintf(fp, "%s%s\n", message->PrefixString, message->TextString);

	return 1;
}

wLogConsoleAppender* WLog_ConsoleAppender_New(wLog* log)
{
	wLogConsoleAppender* ConsoleAppender;

	ConsoleAppender = (wLogConsoleAppender*) malloc(sizeof(wLogConsoleAppender));

	if (ConsoleAppender)
	{
		ZeroMemory(ConsoleAppender, sizeof(wLogConsoleAppender));

		ConsoleAppender->Open = (WLOG_APPENDER_OPEN_FN) WLog_ConsoleAppender_Open;
		ConsoleAppender->Close = (WLOG_APPENDER_OPEN_FN) WLog_ConsoleAppender_Close;
		ConsoleAppender->WriteMessage = (WLOG_APPENDER_WRITE_MESSAGE_FN) WLog_ConsoleAppender_WriteMessage;

		ConsoleAppender->outputStream = WLOG_CONSOLE_STDOUT;
	}

	return ConsoleAppender;
}

void WLog_ConsoleAppender_Free(wLog* log, wLogConsoleAppender* appender)
{
	if (appender)
	{
		free(appender);
	}
}
