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
#include <winpr/stream.h>

#include <winpr/wlog.h>

#include "wlog/Message.h"

#include "wlog/BinaryAppender.h"

/**
 * Binary Appender
 */

void WLog_BinaryAppender_SetOutputFileName(wLog* log, wLogBinaryAppender* appender, const char* filename)
{
	if (!appender)
		return;

	if (appender->Type != WLOG_APPENDER_BINARY)
		return;

	if (!filename)
		return;

	appender->FileName = _strdup(filename);
}

void WLog_BinaryAppender_SetOutputFilePath(wLog* log, wLogBinaryAppender* appender, const char* filepath)
{
	if (!appender)
		return;

	if (appender->Type != WLOG_APPENDER_BINARY)
		return;

	if (!filepath)
		return;

	appender->FilePath = _strdup(filepath);
}

int WLog_BinaryAppender_Open(wLog* log, wLogBinaryAppender* appender)
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
		sprintf_s(appender->FileName, 256, "%u.wlog", (unsigned int) ProcessId);
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

int WLog_BinaryAppender_Close(wLog* log, wLogBinaryAppender* appender)
{
	if (!appender->FileDescriptor)
		return 0;

	fclose(appender->FileDescriptor);

	appender->FileDescriptor = NULL;

	return 0;
}

int WLog_BinaryAppender_WriteMessage(wLog* log, wLogBinaryAppender* appender, wLogMessage* message)
{
	FILE* fp;
	wStream* s;
	int MessageLength;
	int FileNameLength;
	int FunctionNameLength;
	int TextStringLength;

	if (message->Level > log->Level)
		return 0;

	fp = appender->FileDescriptor;

	if (!fp)
		return -1;

	FileNameLength = strlen(message->FileName);
	FunctionNameLength = strlen(message->FunctionName);
	TextStringLength = strlen(message->TextString);

	MessageLength = 16 +
			(4 + FileNameLength + 1) +
			(4 + FunctionNameLength + 1) +
			(4 + TextStringLength + 1);

	s = Stream_New(NULL, MessageLength);

	Stream_Write_UINT32(s, MessageLength);

	Stream_Write_UINT32(s, message->Type);
	Stream_Write_UINT32(s, message->Level);

	Stream_Write_UINT32(s, message->LineNumber);

	Stream_Write_UINT32(s, FileNameLength);
	Stream_Write(s, message->FileName, FileNameLength + 1);

	Stream_Write_UINT32(s, FunctionNameLength);
	Stream_Write(s, message->FunctionName, FunctionNameLength + 1);

	Stream_Write_UINT32(s, TextStringLength);
	Stream_Write(s, message->TextString, TextStringLength + 1);

	Stream_SealLength(s);

	fwrite(Stream_Buffer(s), MessageLength, 1, fp);

	Stream_Free(s, TRUE);

	return 1;
}

int WLog_BinaryAppender_WriteDataMessage(wLog* log, wLogBinaryAppender* appender, wLogMessage* message)
{
	return 1;
}

int WLog_BinaryAppender_WriteImageMessage(wLog* log, wLogBinaryAppender* appender, wLogMessage* message)
{
	return 1;
}

wLogBinaryAppender* WLog_BinaryAppender_New(wLog* log)
{
	wLogBinaryAppender* BinaryAppender;

	BinaryAppender = (wLogBinaryAppender*) malloc(sizeof(wLogBinaryAppender));

	if (BinaryAppender)
	{
		ZeroMemory(BinaryAppender, sizeof(wLogBinaryAppender));

		BinaryAppender->Type = WLOG_APPENDER_BINARY;

		BinaryAppender->Open = (WLOG_APPENDER_OPEN_FN) WLog_BinaryAppender_Open;
		BinaryAppender->Close = (WLOG_APPENDER_OPEN_FN) WLog_BinaryAppender_Close;

		BinaryAppender->WriteMessage =
				(WLOG_APPENDER_WRITE_MESSAGE_FN) WLog_BinaryAppender_WriteMessage;
		BinaryAppender->WriteDataMessage =
				(WLOG_APPENDER_WRITE_DATA_MESSAGE_FN) WLog_BinaryAppender_WriteDataMessage;
		BinaryAppender->WriteImageMessage =
				(WLOG_APPENDER_WRITE_IMAGE_MESSAGE_FN) WLog_BinaryAppender_WriteImageMessage;

		BinaryAppender->FileName = NULL;
		BinaryAppender->FilePath = NULL;
		BinaryAppender->FullFileName = NULL;
	}

	return BinaryAppender;
}

void WLog_BinaryAppender_Free(wLog* log, wLogBinaryAppender* appender)
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
