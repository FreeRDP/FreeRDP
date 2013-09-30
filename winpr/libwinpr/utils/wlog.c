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

#define WLOG_MAX_PREFIX_SIZE	512
#define WLOG_MAX_STRING_SIZE	8192

const char* WLOG_LEVELS[7] =
{
	"TRACE",
	"DEBUG",
	"INFO",
	"WARN",
	"ERROR",
	"FATAL",
	"OFF"
};

int WLog_Write(wLog* log, wLogMessage* message)
{
	if (!log->Appender)
		return -1;

	if (!log->Appender->WriteMessage)
		return -1;

	return log->Appender->WriteMessage(log, log->Appender, message);
}

void WLog_PrintMessageVA(wLog* log, wLogMessage* message, va_list args)
{
	if (!strchr(message->FormatString, '%'))
	{
		message->TextString = (LPSTR) message->FormatString;
		WLog_Write(log, message);
	}
	else
	{
		char formattedLogMessage[8192];
		wvsnprintfx(formattedLogMessage, WLOG_MAX_STRING_SIZE - 1, message->FormatString, args);

		message->TextString = formattedLogMessage;
		WLog_Write(log, message);
	}
}

void WLog_PrintMessage(wLog* log, wLogMessage* message, ...)
{
	va_list args;
	va_start(args, message);
	WLog_PrintMessageVA(log, message, args);
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
 * Log Layout
 */

void WLog_PrintMessagePrefixVA(wLog* log, wLogMessage* message, const char* format, va_list args)
{
	if (!strchr(format, '%'))
	{
		message->PrefixString = (LPSTR) format;
	}
	else
	{
		wvsnprintfx(message->PrefixString, WLOG_MAX_PREFIX_SIZE - 1, format, args);
	}
}

void WLog_PrintMessagePrefix(wLog* log, wLogMessage* message, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	WLog_PrintMessagePrefixVA(log, message, format, args);
	va_end(args);
}

void WLog_Layout_GetMessagePrefix(wLog* log, wLogLayout* layout, wLogMessage* message)
{
	char* p;
	int index;
	int argc = 0;
	void* args[32];
	char format[128];

	index = 0;
	p = (char*) layout->FormatString;

	while (*p)
	{
		if (*p == '%')
		{
			p++;

			if (*p)
			{
				if ((*p == 'l') && (*(p + 1) == 'v')) /* log level */
				{
					args[argc++] = (void*) WLOG_LEVELS[message->Level];
					format[index++] = '%';
					format[index++] = 's';
					p++;
				}
				else if ((*p == 'm') && (*(p + 1) == 'n')) /* module name */
				{
					args[argc++] = (void*) log->Name;
					format[index++] = '%';
					format[index++] = 's';
					p++;
				}
				else if ((*p == 'f') && (*(p + 1) == 'l')) /* file */
				{
					char* file;

					file = strrchr(message->FileName, '/');

					if (!file)
						file = strrchr(message->FileName, '\\');

					if (file)
						file++;
					else
						file = (char*) message->FileName;

					args[argc++] = (void*) file;
					format[index++] = '%';
					format[index++] = 's';
					p++;
				}
				else if ((*p == 'f') && (*(p + 1) == 'n')) /* function */
				{
					args[argc++] = (void*) message->FunctionName;
					format[index++] = '%';
					format[index++] = 's';
					p++;
				}
				else if ((*p == 'l') && (*(p + 1) == 'n')) /* line number */
				{
					args[argc++] = (void*) message->LineNumber;
					format[index++] = '%';
					format[index++] = 'd';
					p++;
				}
			}
		}
		else
		{
			format[index++] = *p;
		}

		p++;
	}

	format[index++] = '\0';

	switch (argc)
	{
		case 0:
			WLog_PrintMessagePrefix(log, message, format);
			break;

		case 1:
			WLog_PrintMessagePrefix(log, message, format, args[0]);
			break;

		case 2:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1]);
			break;

		case 3:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2]);
			break;

		case 4:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3]);
			break;

		case 5:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4]);
			break;

		case 6:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5]);
			break;

		case 7:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6]);
			break;

		case 8:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7]);
			break;

		case 9:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8]);
			break;

		case 10:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9]);
			break;

		case 11:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10]);
			break;

		case 12:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10],
					args[11]);
			break;

		case 13:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10],
					args[11], args[12]);
			break;

		case 14:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10],
					args[11], args[12], args[13]);
			break;

		case 15:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10],
					args[11], args[12], args[13], args[14]);
			break;

		case 16:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10],
					args[11], args[12], args[13], args[14], args[15]);
			break;
	}
}

wLogLayout* WLog_GetLogLayout(wLog* log)
{
	return log->Appender->Layout;
}

void WLog_Layout_SetPrefixFormat(wLog* log, wLogLayout* layout, const char* format)
{
	if (layout->FormatString)
		free(layout->FormatString);

	layout->FormatString = _strdup(format);
}

wLogLayout* WLog_Layout_New(wLog* log)
{
	wLogLayout* layout;

	layout = (wLogLayout*) malloc(sizeof(wLogLayout));

	if (layout)
	{
		ZeroMemory(layout, sizeof(wLogLayout));

		layout->FormatString = _strdup("[%lv][%mn] - ");
	}

	return layout;
}

void WLog_Layout_Free(wLog* log, wLogLayout* layout)
{
	if (layout)
	{
		if (layout->FormatString)
			free(layout->FormatString);

		free(layout);
	}
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

wLogAppender* WLog_Appender_New(wLog* log, DWORD logAppenderType)
{
	wLogAppender* appender = NULL;

	if (logAppenderType == WLOG_APPENDER_CONSOLE)
	{
		appender = (wLogAppender*) WLog_ConsoleAppender_New(log);
	}
	else if (logAppenderType == WLOG_APPENDER_FILE)
	{
		appender = (wLogAppender*) WLog_FileAppender_New(log);
	}

	if (!appender)
		appender = (wLogAppender*) WLog_ConsoleAppender_New(log);

	appender->Layout = WLog_Layout_New(log);

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
