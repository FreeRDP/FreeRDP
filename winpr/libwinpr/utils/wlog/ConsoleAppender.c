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

#include "ConsoleAppender.h"
#include "Message.h"

#ifdef ANDROID
#include <android/log.h>
#endif

#define WLOG_CONSOLE_DEFAULT  0
#define WLOG_CONSOLE_STDOUT   1
#define WLOG_CONSOLE_STDERR   2
#define WLOG_CONSOLE_DEBUG    4

struct _wLogConsoleAppender
{
       WLOG_APPENDER_COMMON();

       int outputStream;
};
typedef struct _wLogConsoleAppender wLogConsoleAppender;

static BOOL WLog_ConsoleAppender_Open(wLog* log, wLogAppender* appender)
{
	return TRUE;
}

static BOOL WLog_ConsoleAppender_Close(wLog* log, wLogAppender* appender)
{
	return TRUE;
}

static BOOL WLog_ConsoleAppender_WriteMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	FILE* fp;
	char prefix[WLOG_MAX_PREFIX_SIZE];
	wLogConsoleAppender *consoleAppender;
	if (!appender)
		return FALSE;

	consoleAppender = (wLogConsoleAppender *)appender;


	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

#ifdef _WIN32
	if (consoleAppender->outputStream == WLOG_CONSOLE_DEBUG)
	{
		char MessageString[4096];

		sprintf_s(MessageString, sizeof(MessageString), "%s%s\n",
			  message->PrefixString, message->TextString);

		OutputDebugStringA(MessageString);

		return TRUE;
	}
#endif
#ifdef ANDROID
	(void)fp;
	android_LogPriority level;
	switch(message->Level)
	{
		case WLOG_TRACE:
			level = ANDROID_LOG_VERBOSE;
			break;
		case WLOG_DEBUG:
			level = ANDROID_LOG_DEBUG;
			break;
		case WLOG_INFO:
			level = ANDROID_LOG_INFO;
			break;
		case WLOG_WARN:
			level = ANDROID_LOG_WARN;
			break;
		case WLOG_ERROR:
			level = ANDROID_LOG_ERROR;
			break;
		case WLOG_FATAL:
			level = ANDROID_LOG_FATAL;
			break;
		case WLOG_OFF:
			level = ANDROID_LOG_SILENT;
			break;
		default:
			level = ANDROID_LOG_FATAL;
			break;
	}

	if (level != ANDROID_LOG_SILENT)
		__android_log_print(level, log->Name, "%s%s", message->PrefixString, message->TextString);

#else
	switch(consoleAppender->outputStream)
	{
		case WLOG_CONSOLE_STDOUT:
			fp = stdout;
			break;
		case WLOG_CONSOLE_STDERR:
			fp = stderr;
			break;
		default:
			switch(message->Level)
			{
				case WLOG_TRACE:
				case WLOG_DEBUG:
				case WLOG_INFO:
					fp = stdout;
					break;
				default:
					fp = stderr;
					break;
			}
			break;
	}

	if (message->Level != WLOG_OFF)
		fprintf(fp, "%s%s\n", message->PrefixString, message->TextString);
#endif
	return TRUE;
}

static int g_DataId = 0;

static BOOL WLog_ConsoleAppender_WriteDataMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
#if defined(ANDROID)
	return FALSE;
#else
	int DataId;
	char* FullFileName;

	DataId = g_DataId++;
	FullFileName = WLog_Message_GetOutputFileName(DataId, "dat");

	WLog_DataMessage_Write(FullFileName, message->Data, message->Length);

	free(FullFileName);

	return TRUE;
#endif
}

static int g_ImageId = 0;

static BOOL WLog_ConsoleAppender_WriteImageMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
#if defined(ANDROID)
	return FALSE;
#else
	int ImageId;
	char* FullFileName;

	ImageId = g_ImageId++;
	FullFileName = WLog_Message_GetOutputFileName(ImageId, "bmp");

	WLog_ImageMessage_Write(FullFileName, message->ImageData,
			message->ImageWidth, message->ImageHeight, message->ImageBpp);

	free(FullFileName);

	return TRUE;
#endif
}

static int g_PacketId = 0;

static BOOL WLog_ConsoleAppender_WritePacketMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
#if defined(ANDROID)
	return FALSE;
#else
	int PacketId;
	char* FullFileName;

	PacketId = g_PacketId++;

	if (!appender->PacketMessageContext)
	{
		FullFileName = WLog_Message_GetOutputFileName(-1, "pcap");
		appender->PacketMessageContext = (void*) Pcap_Open(FullFileName, TRUE);
		free(FullFileName);
	}

	if (appender->PacketMessageContext)
		return WLog_PacketMessage_Write((wPcap*) appender->PacketMessageContext,
				message->PacketData, message->PacketLength, message->PacketFlags);

	return TRUE;
#endif
}
static BOOL WLog_ConsoleAppender_Set(wLogAppender* appender, const char *setting, void *value)
{
	wLogConsoleAppender *consoleAppender = (wLogConsoleAppender *)appender;

	if (!value || !strlen(value))
		return FALSE;

	if (strcmp("outputstream", setting))
		return FALSE;

	if (!strcmp("stdout", value))
		consoleAppender->outputStream = WLOG_CONSOLE_STDOUT;
	else if (!strcmp("stderr", value))
		consoleAppender->outputStream = WLOG_CONSOLE_STDERR;
	else if (!strcmp("default", value))
		consoleAppender->outputStream = WLOG_CONSOLE_DEFAULT;
	else if (!strcmp("debug", value))
		consoleAppender->outputStream = WLOG_CONSOLE_DEBUG;
	else
		return FALSE;

	return TRUE;
}

static void WLog_ConsoleAppender_Free(wLogAppender* appender)
{
	if (appender)
	{
		if (appender->PacketMessageContext)
		{
			Pcap_Close((wPcap*) appender->PacketMessageContext);
		}

		free(appender);
	}
}

wLogAppender* WLog_ConsoleAppender_New(wLog* log)
{
	wLogConsoleAppender* ConsoleAppender;

	ConsoleAppender = (wLogConsoleAppender*) calloc(1, sizeof(wLogConsoleAppender));

	if (!ConsoleAppender)
		return NULL;

	ConsoleAppender->Type = WLOG_APPENDER_CONSOLE;

	ConsoleAppender->Open =  WLog_ConsoleAppender_Open;
	ConsoleAppender->Close = WLog_ConsoleAppender_Close;
	ConsoleAppender->WriteMessage = WLog_ConsoleAppender_WriteMessage;
	ConsoleAppender->WriteDataMessage = WLog_ConsoleAppender_WriteDataMessage;
	ConsoleAppender->WriteImageMessage = WLog_ConsoleAppender_WriteImageMessage;
	ConsoleAppender->WritePacketMessage = WLog_ConsoleAppender_WritePacketMessage;
	ConsoleAppender->Set = WLog_ConsoleAppender_Set;
	ConsoleAppender->Free = WLog_ConsoleAppender_Free;

	ConsoleAppender->outputStream = WLOG_CONSOLE_DEFAULT;

#ifdef _WIN32
	if (IsDebuggerPresent())
		ConsoleAppender->outputStream = WLOG_CONSOLE_DEBUG;
#endif

	return (wLogAppender *)ConsoleAppender;
}

