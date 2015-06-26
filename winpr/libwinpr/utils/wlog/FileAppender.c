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
#include <winpr/environment.h>
#include <winpr/file.h>
#include <winpr/path.h>
#include <winpr/thread.h>

#include <winpr/wlog.h>

#include "wlog/Message.h"

#include "wlog/FileAppender.h"

/**
 * File Appender
 */

BOOL WLog_FileAppender_SetOutputFileName(wLog* log, wLogFileAppender* appender, const char* filename)
{
	if (!appender || !filename)
		return FALSE;

	if (appender->Type != WLOG_APPENDER_FILE)
		return FALSE;

	appender->FileName = _strdup(filename);
	if (!appender->FileName)
		return FALSE;

	return TRUE;
}

BOOL WLog_FileAppender_SetOutputFilePath(wLog* log, wLogFileAppender* appender, const char* filepath)
{
	if (!appender || !filepath)
		return FALSE;

	if (appender->Type != WLOG_APPENDER_FILE)
		return FALSE;

	appender->FilePath = _strdup(filepath);
	if (!appender->FilePath)
		return FALSE;

	return TRUE;
}

int WLog_FileAppender_Open(wLog* log, wLogFileAppender* appender)
{
	if (!log || !appender)
		return -1;

	if (!appender->FilePath)
	{
		appender->FilePath = GetKnownSubPath(KNOWN_PATH_TEMP, "wlog");
		if (!appender->FilePath)
			return -1;
	}

	if (!appender->FileName)
	{
		appender->FileName = (char*) malloc(MAX_PATH);
		if (!appender->FileName)
			return -1;

		sprintf_s(appender->FileName, MAX_PATH, "%u.log", (unsigned int) GetCurrentProcessId());
	}

	if (!appender->FullFileName)
	{
		appender->FullFileName = GetCombinedPath(appender->FilePath, appender->FileName);
		if (!appender->FullFileName)
			return -1;
	}

	if (!PathFileExistsA(appender->FilePath))
	{
		if (!PathMakePathA(appender->FilePath, 0))
			return -1;
		UnixChangeFileMode(appender->FilePath, 0xFFFF);
	}

	appender->FileDescriptor = fopen(appender->FullFileName, "a+");

	if (!appender->FileDescriptor)
		return -1;

	return 0;
}

int WLog_FileAppender_Close(wLog* log, wLogFileAppender* appender)
{
	if (!log || !appender)
		return -1;

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

	if (!log || !appender || !message)
		return -1;

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

	if (!log || !appender || !message)
		return -1;

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

	if (!log || !appender || !message)
		return -1;

	ImageId = g_ImageId++;
	FullFileName = WLog_Message_GetOutputFileName(ImageId, "bmp");

	WLog_ImageMessage_Write(FullFileName, message->ImageData,
			message->ImageWidth, message->ImageHeight, message->ImageBpp);

	free(FullFileName);

	return ImageId;
}

wLogFileAppender* WLog_FileAppender_New(wLog* log)
{
	LPSTR env;
	LPCSTR name;
	DWORD nSize;
	wLogFileAppender* FileAppender;
	BOOL status;

	FileAppender = (wLogFileAppender*) calloc(1, sizeof(wLogFileAppender));
	if (!FileAppender)
		return NULL;

	FileAppender->Type = WLOG_APPENDER_FILE;

	FileAppender->Open = (WLOG_APPENDER_OPEN_FN) WLog_FileAppender_Open;
	FileAppender->Close = (WLOG_APPENDER_OPEN_FN) WLog_FileAppender_Close;
	FileAppender->WriteMessage =
			(WLOG_APPENDER_WRITE_MESSAGE_FN) WLog_FileAppender_WriteMessage;
	FileAppender->WriteDataMessage =
			(WLOG_APPENDER_WRITE_DATA_MESSAGE_FN) WLog_FileAppender_WriteDataMessage;
	FileAppender->WriteImageMessage =
			(WLOG_APPENDER_WRITE_IMAGE_MESSAGE_FN) WLog_FileAppender_WriteImageMessage;

	name = "WLOG_FILEAPPENDER_OUTPUT_FILE_PATH";
	nSize = GetEnvironmentVariableA(name, NULL, 0);
	if (nSize)
	{
		env = (LPSTR) malloc(nSize);
		if (!env)
			goto error_free;

		nSize = GetEnvironmentVariableA(name, env, nSize);
		status = WLog_FileAppender_SetOutputFilePath(log, FileAppender, env);
		free(env);

		if (!status)
			goto error_free;
	}

	name = "WLOG_FILEAPPENDER_OUTPUT_FILE_NAME";
	nSize = GetEnvironmentVariableA(name, NULL, 0);
	if (nSize)
	{
		env = (LPSTR) malloc(nSize);
		if (!env)
			goto error_output_file_name;

		nSize = GetEnvironmentVariableA(name, env, nSize);
		status = WLog_FileAppender_SetOutputFileName(log, FileAppender, env);
		free(env);

		if (!status)
			goto error_output_file_name;
	}

	return FileAppender;

error_output_file_name:
	free(FileAppender->FilePath);
error_free:
	free(FileAppender);
	return NULL;
}

void WLog_FileAppender_Free(wLog* log, wLogFileAppender* appender)
{
	if (appender)
	{
		free(appender->FileName);
		free(appender->FilePath);
		free(appender->FullFileName);
		free(appender);
	}
}
