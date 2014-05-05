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
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/thread.h>

#include <winpr/wlog.h>

#include "wlog/Message.h"

#include "wlog/FileAppender.h"

/**
 * File Appender
 */

void WLog_FileAppender_SetOutputFileName(wLog* log, wLogFileAppender* appender, const char* filename)
{
	if (!appender)
		return;

	if (appender->Type != WLOG_APPENDER_FILE)
		return;

	if (!filename)
		return;

	appender->FileName = _strdup(filename);
}

void WLog_FileAppender_SetOutputFilePath(wLog* log, wLogFileAppender* appender, const char* filepath)
{
	if (!appender)
		return;

	if (appender->Type != WLOG_APPENDER_FILE)
		return;

	if (!filepath)
		return;

	appender->FilePath = _strdup(filepath);
}

int WLog_FileAppender_Open(wLog* log, wLogFileAppender* appender)
{
	DWORD ProcessId;

	ProcessId = GetCurrentProcessId();

	if (!appender->FilePath)
	{
		appender->FilePath = GetKnownSubPath(KNOWN_PATH_TEMP, "wlog");
	}

	if (!PathFileExistsA(appender->FilePath))
	{
		CreateDirectoryA(appender->FilePath, 0);
		UnixChangeFileMode(appender->FilePath, 0xFFFF);
	}

	if (!appender->FileName)
	{
		appender->FileName = (char*) malloc(256);
		sprintf_s(appender->FileName, 256, "%u.log", (unsigned int) ProcessId);
	}

	if (!appender->FullFileName)
	{
		appender->FullFileName = GetCombinedPath(appender->FilePath, appender->FileName);
	}

	appender->FileDescriptor = fopen(appender->FullFileName, "a+");

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

	fp = appender->FileDescriptor;

	if (!fp)
		return -1;

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	fprintf(fp, "%s%s\n", message->PrefixString, message->TextString);

	fflush(fp); /* slow! */
	
	return 1;
}

static int g_DataId = 0;

int WLog_FileAppender_WriteDataMessage(wLog* log, wLogFileAppender* appender, wLogMessage* message)
{
	int DataId;
	char* FullFileName;

	DataId = g_DataId++;
	FullFileName = WLog_Message_GetOutputFileName(DataId, "dat");

	WLog_DataMessage_Write(FullFileName, message->Data, message->Length);

	free(FullFileName);

	return DataId;
}

static int g_ImageId = 0;

int WLog_FileAppender_WriteImageMessage(wLog* log, wLogFileAppender* appender, wLogMessage* message)
{
	int ImageId;
	char* FullFileName;

	ImageId = g_ImageId++;
	FullFileName = WLog_Message_GetOutputFileName(ImageId, "bmp");

	WLog_ImageMessage_Write(FullFileName, message->ImageData,
			message->ImageWidth, message->ImageHeight, message->ImageBpp);

	free(FullFileName);

	return ImageId;
}

wLogFileAppender* WLog_FileAppender_New(wLog* log)
{
	wLogFileAppender* FileAppender;

	FileAppender = (wLogFileAppender*) malloc(sizeof(wLogFileAppender));

	if (FileAppender)
	{
		ZeroMemory(FileAppender, sizeof(wLogFileAppender));

		FileAppender->Type = WLOG_APPENDER_FILE;

		FileAppender->Open = (WLOG_APPENDER_OPEN_FN) WLog_FileAppender_Open;
		FileAppender->Close = (WLOG_APPENDER_OPEN_FN) WLog_FileAppender_Close;

		FileAppender->WriteMessage =
				(WLOG_APPENDER_WRITE_MESSAGE_FN) WLog_FileAppender_WriteMessage;
		FileAppender->WriteDataMessage =
				(WLOG_APPENDER_WRITE_DATA_MESSAGE_FN) WLog_FileAppender_WriteDataMessage;
		FileAppender->WriteImageMessage =
				(WLOG_APPENDER_WRITE_IMAGE_MESSAGE_FN) WLog_FileAppender_WriteImageMessage;

		FileAppender->FileName = NULL;
		FileAppender->FilePath = NULL;
		FileAppender->FullFileName = NULL;
	}

	return FileAppender;
}

void WLog_FileAppender_Free(wLog* log, wLogFileAppender* appender)
{
	if (appender)
	{
		if (appender->FileName)
			free(appender->FileName);

		if (appender->FilePath)
			free(appender->FilePath);

		if (appender->FullFileName)
			free(appender->FullFileName);

		free(appender);
	}
}
