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
#include <winpr/environment.h>

#include <winpr/wlog.h>

#include "wlog/wlog.h"

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
	int status;
	wLogAppender* appender;

	appender = WLog_GetLogAppender(log);

	if (!appender)
		return -1;

	if (!appender->State)
		WLog_OpenAppender(log);

	if (!appender->WriteMessage)
		return -1;

	EnterCriticalSection(&appender->lock);

	status = appender->WriteMessage(log, appender, message);

	LeaveCriticalSection(&appender->lock);

	return status;
}

int WLog_WriteData(wLog* log, wLogMessage* message)
{
	int status;
	wLogAppender* appender;

	appender = WLog_GetLogAppender(log);

	if (!appender)
		return -1;

	if (!appender->State)
		WLog_OpenAppender(log);

	if (!appender->WriteDataMessage)
		return -1;

	EnterCriticalSection(&appender->lock);

	status = appender->WriteDataMessage(log, appender, message);

	LeaveCriticalSection(&appender->lock);

	return status;
}

int WLog_WriteImage(wLog* log, wLogMessage* message)
{
	int status;
	wLogAppender* appender;

	appender = WLog_GetLogAppender(log);

	if (!appender)
		return -1;

	if (!appender->State)
		WLog_OpenAppender(log);

	if (!appender->WriteImageMessage)
		return -1;

	EnterCriticalSection(&appender->lock);

	status = appender->WriteImageMessage(log, appender, message);

	LeaveCriticalSection(&appender->lock);

	return status;
}

int WLog_WritePacket(wLog* log, wLogMessage* message)
{
	int status;
	wLogAppender* appender;

	appender = WLog_GetLogAppender(log);

	if (!appender)
		return -1;

	if (!appender->State)
		WLog_OpenAppender(log);

	if (!appender->WritePacketMessage)
		return -1;

	EnterCriticalSection(&appender->lock);

	status = appender->WritePacketMessage(log, appender, message);

	LeaveCriticalSection(&appender->lock);

	return status;
}

int WLog_PrintMessageVA(wLog* log, wLogMessage* message, va_list args)
{
	int status = -1;

	if (message->Type == WLOG_MESSAGE_TEXT)
	{
		if (!strchr(message->FormatString, '%'))
		{
			message->TextString = (LPSTR) message->FormatString;
			status = WLog_Write(log, message);
		}
		else
		{
			char formattedLogMessage[WLOG_MAX_STRING_SIZE];
			wvsnprintfx(formattedLogMessage, WLOG_MAX_STRING_SIZE - 1, message->FormatString, args);

			message->TextString = formattedLogMessage;
			status = WLog_Write(log, message);
		}
	}
	else if (message->Type == WLOG_MESSAGE_DATA)
	{
		message->Data = va_arg(args, void*);
		message->Length = va_arg(args, int);

		status = WLog_WriteData(log, message);
	}
	else if (message->Type == WLOG_MESSAGE_IMAGE)
	{
		message->ImageData = va_arg(args, void*);
		message->ImageWidth = va_arg(args, int);
		message->ImageHeight = va_arg(args, int);
		message->ImageBpp = va_arg(args, int);

		status = WLog_WriteImage(log, message);
	}
	else if (message->Type == WLOG_MESSAGE_PACKET)
	{
		message->PacketData = va_arg(args, void*);
		message->PacketLength = va_arg(args, int);
		message->PacketFlags = va_arg(args, int);

		status = WLog_WritePacket(log, message);
	}

	return status;
}

void WLog_PrintMessage(wLog* log, wLogMessage* message, ...)
{
	int status;
	va_list args;

	va_start(args, message);

	status = WLog_PrintMessageVA(log, message, args);

	va_end(args);
}

DWORD WLog_GetLogLevel(wLog* log)
{
	if (log->Level == WLOG_LEVEL_INHERIT) {
		return WLog_GetLogLevel(log->Parent);
	} else {
		return log->Level;
	}
}

void WLog_SetLogLevel(wLog* log, DWORD logLevel)
{

	if ((logLevel > WLOG_OFF) && (logLevel != WLOG_LEVEL_INHERIT))
	{
		logLevel = WLOG_OFF;
	}
	log->Level = logLevel;
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

wLog* WLog_New(LPCSTR name , wLog* rootLogger)
{
	wLog* log;
	char* env;
	DWORD nSize;

	log = (wLog*) malloc(sizeof(wLog));

	if (log)
	{
		ZeroMemory(log, sizeof(wLog));

		log->Name = _strdup(name);
		WLog_ParseName(log, name);

		log->Parent = rootLogger;
		log->ChildrenCount = 0;

		log->ChildrenSize = 16;
		log->Children = (wLog**) malloc(sizeof(wLog*) * log->ChildrenSize);

		log->Appender = NULL;

		if (rootLogger) {
			log->Level = WLOG_LEVEL_INHERIT;
		} else {
			log->Level = WLOG_WARN;

			nSize = GetEnvironmentVariableA("WLOG_LEVEL", NULL, 0);

			if (nSize)
			{
				env = (LPSTR) malloc(nSize);
				nSize = GetEnvironmentVariableA("WLOG_LEVEL", env, nSize);

				if (env)
				{
					if (_stricmp(env, "TRACE") == 0)
						log->Level = WLOG_TRACE;
					else if (_stricmp(env, "DEBUG") == 0)
						log->Level = WLOG_DEBUG;
					else if (_stricmp(env, "INFO") == 0)
						log->Level = WLOG_INFO;
					else if (_stricmp(env, "WARN") == 0)
						log->Level = WLOG_WARN;
					else if (_stricmp(env, "ERROR") == 0)
						log->Level = WLOG_ERROR;
					else if (_stricmp(env, "FATAL") == 0)
						log->Level = WLOG_FATAL;
					else if (_stricmp(env, "OFF") == 0)
						log->Level = WLOG_OFF;
					else if (_strnicmp(env, "0x", 2) == 0)
					{
						/* TODO: read custom hex value */
					}

					free(env);
				}
			}
		}
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
		free(log->Children);

		free(log);
	}
}

static wLog* g_RootLog = NULL;

wLog* WLog_GetRoot()
{
	char* env;
	DWORD nSize;
	DWORD logAppenderType;

	if (!g_RootLog)
	{
		g_RootLog = WLog_New("",NULL);
		g_RootLog->IsRoot = TRUE;

		logAppenderType = WLOG_APPENDER_CONSOLE;

		nSize = GetEnvironmentVariableA("WLOG_APPENDER", NULL, 0);

		if (nSize)
		{
			env = (LPSTR) malloc(nSize);
			nSize = GetEnvironmentVariableA("WLOG_APPENDER", env, nSize);

			if (env)
			{
				if (_stricmp(env, "CONSOLE") == 0)
					logAppenderType = WLOG_APPENDER_CONSOLE;
				else if (_stricmp(env, "FILE") == 0)
					logAppenderType = WLOG_APPENDER_FILE;
				else if (_stricmp(env, "BINARY") == 0)
					logAppenderType = WLOG_APPENDER_BINARY;

				free(env);
			}
		}

		WLog_SetLogAppenderType(g_RootLog, logAppenderType);
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
	DWORD index;
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
		log = WLog_New(name,root);
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

	DWORD index;
	wLog* child = NULL;

	for (index = 0; index < root->ChildrenCount; index++)
	{
		child = root->Children[index];
		WLog_Free(child);
	}

	WLog_Free(root);
	g_RootLog = NULL;
}
