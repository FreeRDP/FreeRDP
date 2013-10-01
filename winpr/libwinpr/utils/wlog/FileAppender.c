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

#include "wlog/FileAppender.h"

/**
 * File Appender
 */

void WLog_FileAppender_SetOutputFileName(wLog* log, wLogFileAppender* appender, const char* filename)
{
	if (!appender)
		return;

	if (!filename)
		return;

	appender->FileName = _strdup(filename);
}

int WLog_FileAppender_Open(wLog* log, wLogFileAppender* appender)
{
	if (!appender->FileName)
		return -1;

	appender->FileDescriptor = fopen(appender->FileName, "a+");

	if (!appender->FileDescriptor)
		return -1;

	return 0;
}

int WLog_FileAppender_Close(wLog* log, wLogFileAppender* appender)
{
	if (!appender->FileDescriptor)
		return 0;

	fclose(appender->FileDescriptor);

	appender->FileDescriptor = NULL;

	return 0;
}

int WLog_FileAppender_WriteMessage(wLog* log, wLogFileAppender* appender, wLogMessage* message)
{
	FILE* fp;
	char prefix[WLOG_MAX_PREFIX_SIZE];

	if (message->Level > log->Level)
		return 0;

	fp = appender->FileDescriptor;

	if (!fp)
		return -1;

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	fprintf(fp, "%s%s\n", message->PrefixString, message->TextString);

	return 1;
}

wLogFileAppender* WLog_FileAppender_New(wLog* log)
{
	wLogFileAppender* FileAppender;

	FileAppender = (wLogFileAppender*) malloc(sizeof(wLogFileAppender));

	if (FileAppender)
	{
		ZeroMemory(FileAppender, sizeof(wLogFileAppender));

		FileAppender->Open = (WLOG_APPENDER_OPEN_FN) WLog_FileAppender_Open;
		FileAppender->Close = (WLOG_APPENDER_OPEN_FN) WLog_FileAppender_Close;
		FileAppender->WriteMessage = (WLOG_APPENDER_WRITE_MESSAGE_FN) WLog_FileAppender_WriteMessage;
	}

	return FileAppender;
}

void WLog_FileAppender_Free(wLog* log, wLogFileAppender* appender)
{
	if (appender)
	{
		if (appender->FileName)
			free(appender->FileName);

		free(appender);
	}
}
