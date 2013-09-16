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

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#include <winpr/wlog.h>

/**
 * References for general logging concepts:
 *
 * Short introduction to log4j:
 * http://logging.apache.org/log4j/1.2/manual.html
 *
 * logging - Logging facility for Python:
 * http://docs.python.org/2/library/logging.html
 */

#define WLOG_BUFFER_SIZE	8192

const char* WLOG_LEVELS[7] =
{
	"Trace",
	"Debug",
	"Info",
	"Warn",
	"Error",
	"Fatal",
	"Off"
};

int WLog_Write(wLog* log, DWORD logLevel, wLogMessage* logMessage)
{
	if (!log->Appender)
		return -1;

	if (!log->Appender->WriteMessage)
		return -1;

	return log->Appender->WriteMessage(log, log->Appender, logLevel, logMessage);
}

void WLog_LogVA(wLog* log, DWORD logLevel, wLogMessage* logMessage, va_list args)
{
	if (!strchr(logMessage->FormatString, '%'))
	{
		logMessage->TextString = (LPSTR) logMessage->FormatString;
		WLog_Write(log, logLevel, logMessage);
	}
	else
	{
		char formattedLogMessage[8192];
		wvsnprintfx(formattedLogMessage, WLOG_BUFFER_SIZE - 1, logMessage->FormatString, args);

		logMessage->TextString = formattedLogMessage;
		WLog_Write(log, logLevel, logMessage);
	}
}

void WLog_PrintMessage(wLog* log, DWORD logLevel, wLogMessage* logMessage, ...)
{
	va_list args;
	va_start(args, logMessage);
	WLog_LogVA(log, logLevel, logMessage, args);
	va_end(args);
}

DWORD WLog_GetLogLevel(wLog* log)
{
	return log->Level;
}

void WLog_SetLogLevel(wLog* log, DWORD logLevel)
{
	if (logLevel > WLOG_OFF)
		logLevel = WLOG_OFF;

	log->Level = logLevel;
}

/**
 * Console Appender
 */

void WLog_ConsoleAppender_SetOutputStream(wLog* log, wLogConsoleAppender* appender, int outputStream)
{
	if (!appender)
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

int WLog_ConsoleAppender_WriteMessage(wLog* log, wLogConsoleAppender* appender, DWORD logLevel, wLogMessage* logMessage)
{
	FILE* fp;
	char* logLevelStr;

	if (logLevel > log->Level)
		return 0;

	logLevelStr = (char*) WLOG_LEVELS[logLevel];

	fp = (appender->outputStream == WLOG_CONSOLE_STDERR) ? stderr : stdout;

	fprintf(fp, "[%s] [%s] (%s,%s@%d): %s\n", logLevelStr, log->Name,
			logMessage->FunctionName, logMessage->FileName,
			logMessage->LineNumber, logMessage->TextString);

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

void WLog_ConsoleAppender_Free(wLog* log, wLogConsoleAppender* consoleAppender)
{
	if (consoleAppender)
	{
		free(consoleAppender);
	}
}

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

int WLog_FileAppender_WriteMessage(wLog* log, wLogFileAppender* appender, DWORD logLevel, wLogMessage* logMessage)
{
	FILE* fp;
	char* logLevelStr;

	if (logLevel > log->Level)
		return 0;

	logLevelStr = (char*) WLOG_LEVELS[logLevel];

	fp = appender->FileDescriptor;

	if (!fp || (fp < 0))
		return -1;

	fprintf(fp, "[%s] [%s] (%s,%s@%d): %s\n", logLevelStr, log->Name,
			logMessage->FunctionName, logMessage->FileName,
			logMessage->LineNumber, logMessage->TextString);

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

wLogAppender* WLog_Appender_New(wLog* log, DWORD logAppenderType)
{
	if (logAppenderType == WLOG_APPENDER_CONSOLE)
	{
		return (wLogAppender*) WLog_ConsoleAppender_New(log);
	}
	else if (logAppenderType == WLOG_APPENDER_FILE)
	{
		return (wLogAppender*) WLog_FileAppender_New(log);
	}

	return (wLogAppender*) WLog_ConsoleAppender_New(log);
}

void WLog_Appender_Free(wLog* log, wLogAppender* appender)
{
	if (appender)
	{
		if (appender->Type == WLOG_APPENDER_CONSOLE)
		{
			WLog_ConsoleAppender_Free(log, (wLogConsoleAppender*) appender);
		}
		else if (appender->Type == WLOG_APPENDER_FILE)
		{
			WLog_FileAppender_Free(log, (wLogFileAppender*) appender);
		}
	}
}

wLogAppender* WLog_GetLogAppender(wLog* log)
{
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
	if (!log->Appender)
		return -1;

	if (!log->Appender->Open)
		return 0;

	return log->Appender->Open(log, log->Appender);
}

int WLog_CloseAppender(wLog* log)
{
	if (!log->Appender)
		return -1;

	if (!log->Appender->Close)
		return 0;

	return log->Appender->Close(log, log->Appender);
}

wLog* WLog_New(LPCSTR name)
{
	wLog* log;

	log = (wLog*) malloc(sizeof(wLog));

	if (log)
	{
		ZeroMemory(log, sizeof(wLog));

		log->Name = _strdup(name);
		log->Level = WLOG_TRACE;

		WLog_SetLogAppenderType(log, WLOG_APPENDER_CONSOLE);
	}

	return log;
}

void WLog_Free(wLog* log)
{
	if (log)
	{
		if (log->Appender)
		{
			WLog_Appender_Free(log, log->Appender);
			log->Appender = NULL;
		}

		free(log->Name);
		free(log);
	}
}
