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

#include "FileAppender.h"
#include "Message.h"

#include <winpr/crt.h>
#include <winpr/environment.h>
#include <winpr/file.h>
#include <winpr/path.h>

struct _wLogFileAppender
{
       WLOG_APPENDER_COMMON();

       char* FileName;
       char* FilePath;
       char* FullFileName;
       FILE* FileDescriptor;
};
typedef struct _wLogFileAppender wLogFileAppender;


static BOOL WLog_FileAppender_SetOutputFileName(wLogFileAppender* appender, const char* filename)
{
	appender->FileName = _strdup(filename);
	if (!appender->FileName)
		return FALSE;

	return TRUE;
}

static BOOL WLog_FileAppender_SetOutputFilePath(wLogFileAppender* appender, const char* filepath)
{
	appender->FilePath = _strdup(filepath);
	if (!appender->FilePath)
		return FALSE;

	return TRUE;
}

static BOOL WLog_FileAppender_Open(wLog* log, wLogAppender* appender)
{
	wLogFileAppender *fileAppender;

	if (!log || !appender)
		return FALSE;

	fileAppender = (wLogFileAppender *)appender;

	if (!fileAppender->FilePath)
	{
		fileAppender->FilePath = GetKnownSubPath(KNOWN_PATH_TEMP, "wlog");
		if (!fileAppender->FilePath)
			return FALSE;
	}

	if (!fileAppender->FileName)
	{
		fileAppender->FileName = (char*) malloc(MAX_PATH);
		if (!fileAppender->FileName)
			return FALSE;

		sprintf_s(fileAppender->FileName, MAX_PATH, "%u.log", (unsigned int) GetCurrentProcessId());
	}

	if (!fileAppender->FullFileName)
	{
		fileAppender->FullFileName = GetCombinedPath(fileAppender->FilePath, fileAppender->FileName);
		if (!fileAppender->FullFileName)
			return FALSE;
	}

	if (!PathFileExistsA(fileAppender->FilePath))
	{
		if (!PathMakePathA(fileAppender->FilePath, 0))
			return FALSE;
		UnixChangeFileMode(fileAppender->FilePath, 0xFFFF);
	}

	fileAppender->FileDescriptor = fopen(fileAppender->FullFileName, "a+");

	if (!fileAppender->FileDescriptor)
		return FALSE;

	return TRUE;
}

static BOOL WLog_FileAppender_Close(wLog* log, wLogAppender* appender)
{
	wLogFileAppender *fileAppender;
	if (!log || !appender)
		return FALSE;

	fileAppender = (wLogFileAppender *)appender;

	if (!fileAppender->FileDescriptor)
		return TRUE;

	fclose(fileAppender->FileDescriptor);

	fileAppender->FileDescriptor = NULL;

	return TRUE;
}

static BOOL WLog_FileAppender_WriteMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	FILE* fp;
	char prefix[WLOG_MAX_PREFIX_SIZE];
	wLogFileAppender *fileAppender;

	if (!log || !appender || !message)
		return FALSE;

	fileAppender = (wLogFileAppender *)appender;

	fp = fileAppender->FileDescriptor;

	if (!fp)
		return FALSE;

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	fprintf(fp, "%s%s\n", message->PrefixString, message->TextString);

	fflush(fp); /* slow! */
	
	return TRUE;
}

static int g_DataId = 0;

static BOOL WLog_FileAppender_WriteDataMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	int DataId;
	char* FullFileName;

	if (!log || !appender || !message)
		return FALSE;

	DataId = g_DataId++;
	FullFileName = WLog_Message_GetOutputFileName(DataId, "dat");

	WLog_DataMessage_Write(FullFileName, message->Data, message->Length);

	free(FullFileName);

	return TRUE;
}

static int g_ImageId = 0;

static BOOL WLog_FileAppender_WriteImageMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	int ImageId;
	char* FullFileName;

	if (!log || !appender || !message)
		return FALSE;

	ImageId = g_ImageId++;
	FullFileName = WLog_Message_GetOutputFileName(ImageId, "bmp");

	WLog_ImageMessage_Write(FullFileName, message->ImageData,
			message->ImageWidth, message->ImageHeight, message->ImageBpp);

	free(FullFileName);

	return TRUE;
}

static BOOL WLog_FileAppender_Set(wLogAppender* appender, const char *setting, void *value)
{
	wLogFileAppender *fileAppender = (wLogFileAppender *) appender;

	if (!value || !strlen(value))
		return FALSE;

	if (!strcmp("outputfilename", setting))
		return WLog_FileAppender_SetOutputFileName(fileAppender, (const char *)value);
	else if (!strcmp("outputfilepath", setting))
		return WLog_FileAppender_SetOutputFilePath(fileAppender, (const char *)value);
	else
		return FALSE;

	return TRUE;
}

static void WLog_FileAppender_Free(wLogAppender* appender)
{
	wLogFileAppender* fileAppender = NULL;

	if (appender)
	{
		fileAppender = (wLogFileAppender *)appender;
		free(fileAppender->FileName);
		free(fileAppender->FilePath);
		free(fileAppender->FullFileName);
		free(fileAppender);
	}
}

wLogAppender* WLog_FileAppender_New(wLog* log)
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

	FileAppender->Open = WLog_FileAppender_Open;
	FileAppender->Close = WLog_FileAppender_Close;
	FileAppender->WriteMessage = WLog_FileAppender_WriteMessage;
	FileAppender->WriteDataMessage = WLog_FileAppender_WriteDataMessage;
	FileAppender->WriteImageMessage = WLog_FileAppender_WriteImageMessage;
	FileAppender->Free = WLog_FileAppender_Free;
	FileAppender->Set = WLog_FileAppender_Set;

	name = "WLOG_FILEAPPENDER_OUTPUT_FILE_PATH";
	nSize = GetEnvironmentVariableA(name, NULL, 0);
	if (nSize)
	{
		env = (LPSTR) malloc(nSize);
		if (!env)
			goto error_free;

		nSize = GetEnvironmentVariableA(name, env, nSize);
		status = WLog_FileAppender_SetOutputFilePath(FileAppender, env);
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

		GetEnvironmentVariableA(name, env, nSize);
		status = WLog_FileAppender_SetOutputFileName(FileAppender, env);
		free(env);

		if (!status)
			goto error_output_file_name;
	}

	return (wLogAppender *)FileAppender;

error_output_file_name:
	free(FileAppender->FilePath);
error_free:
	free(FileAppender);
	return NULL;
}
