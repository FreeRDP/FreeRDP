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

#include "wlog/wlog.h"

#include "wlog/FileAppender.h"
#include "wlog/ConsoleAppender.h"

/**
 * References for general logging concepts:
 *
 * Short introduction to log4j:
 * http://logging.apache.org/log4j/1.2/manual.html
 *
 * logging - Logging facility for Python:
 * http://docs.python.org/2/library/logging.html
 */

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
	wLogAppender* appender;

	appender = WLog_GetLogAppender(log);

	if (!appender)
		return -1;

	if (!appender->WriteMessage)
		return -1;

	return appender->WriteMessage(log, appender, message);
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
		char formattedLogMessage[WLOG_MAX_STRING_SIZE];
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
	wLogAppender* appender;

	appender = WLog_GetLogAppender(log);

	return appender->Layout;
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

int WLog_ParseName(wLog* log, LPCSTR name)
{
	char* p;
	int count;
	LPSTR names;

	count = 1;
	p = (char*) name;

	while ((p = strchr(p, '.')) != NULL)
	{
		count++;
		p++;
	}

	names = _strdup(name);
	log->NameCount = count;
	log->Names = (LPSTR*) malloc(sizeof(LPSTR) * (count + 1));
	log->Names[count] = NULL;

	count = 0;
	p = (char*) names;
	log->Names[count++] = p;

	while ((p = strchr(p, '.')) != NULL)
	{
		log->Names[count++] = p + 1;
		*p = '\0';
		p++;
	}

	return 0;
}

wLog* WLog_New(LPCSTR name)
{
	wLog* log;

	log = (wLog*) malloc(sizeof(wLog));

	if (log)
	{
		ZeroMemory(log, sizeof(wLog));

		log->Name = _strdup(name);
		WLog_ParseName(log, name);

		log->Level = WLOG_TRACE;

		log->Parent = NULL;
		log->ChildrenCount = 0;

		log->ChildrenSize = 16;
		log->Children = (wLog**) malloc(sizeof(wLog*) * log->ChildrenSize);

		log->Appender = NULL;
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
		free(log->Names[0]);
		free(log->Names);

		free(log);
	}
}

static wLog* g_RootLog = NULL;

wLog* WLog_GetRoot()
{
	if (!g_RootLog)
	{
		g_RootLog = WLog_New("");
		g_RootLog->IsRoot = TRUE;
		WLog_SetLogAppenderType(g_RootLog, WLOG_APPENDER_CONSOLE);
	}

	return g_RootLog;
}

int WLog_AddChild(wLog* parent, wLog* child)
{
	if (parent->ChildrenCount >= parent->ChildrenSize)
	{
		parent->ChildrenSize *= 2;
		parent->Children = (wLog**) realloc(parent->Children, sizeof(wLog*) * parent->ChildrenSize);
	}

	parent->Children[parent->ChildrenCount++] = child;
	child->Parent = parent;

	return 0;
}

wLog* WLog_FindChild(LPCSTR name)
{
	int index;
	wLog* root;
	wLog* child = NULL;
	BOOL found = FALSE;

	root = WLog_GetRoot();

	for (index = 0; index < root->ChildrenCount; index++)
	{
		child = root->Children[index];

		if (strcmp(child->Name, name) == 0)
		{
			found = TRUE;
			break;
		}
	}

	return (found) ? child : NULL;
}

wLog* WLog_Get(LPCSTR name)
{
	wLog* log;
	wLog* root;

	root = WLog_GetRoot();

	log = WLog_FindChild(name);

	if (!log)
	{
		log = WLog_New(name);
		WLog_AddChild(root, log);
	}

	return log;
}

void WLog_Init()
{
	WLog_GetRoot();
}

void WLog_Uninit()
{
	wLog* root = WLog_GetRoot();

	WLog_Free(root);
	g_RootLog = NULL;
}
