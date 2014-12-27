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

#include <winpr/crt.h>
#include <winpr/print.h>
#include <winpr/debug.h>
#include <winpr/environment.h>

#if defined(ANDROID)
#include <android/log.h>
#endif

#include <winpr/wlog.h>

#include "wlog/wlog.h"

#include "../../log.h"

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

static DWORD g_FilterCount = 0;
static wLogFilter* g_Filters = NULL;

static void log_recursion(const char* file, const char* fkt, int line)
{
	size_t used, i;
	void* bt = winpr_backtrace(20);
	char** msg = winpr_backtrace_symbols(bt, &used);
#if defined(ANDROID)
	const char* tag = WINPR_TAG("utils.wlog");
	__android_log_print(ANDROID_LOG_FATAL, tag, "Recursion detected!!!");
	__android_log_print(ANDROID_LOG_FATAL, tag, "Check %s [%s:%d]", fkt, file, line);

	for (i=0; i<used; i++)
		__android_log_print(ANDROID_LOG_FATAL, tag, "%d: %s", i, msg[i]);

#else
	fprintf(stderr, "[%s]: Recursion detected!\n", fkt);
	fprintf(stderr, "[%s]: Check %s:%d\n", fkt, file, line);

	for (i=0; i<used; i++)
		fprintf(stderr, "%s: %zd: %s\n", fkt, i, msg[i]);

#endif

	if (msg)
		free(msg);

	winpr_backtrace_free(bt);
}

int WLog_Write(wLog* log, wLogMessage* message)
{
	int status = -1;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return -1;

	if (!appender->State)
		WLog_OpenAppender(log);

	if (!appender->WriteMessage)
		return -1;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WriteMessage(log, appender, message);
		appender->recursive = FALSE;
	}

	LeaveCriticalSection(&appender->lock);
	return status;
}

int WLog_WriteData(wLog* log, wLogMessage* message)
{
	int status = -1;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return -1;

	if (!appender->State)
		WLog_OpenAppender(log);

	if (!appender->WriteDataMessage)
		return -1;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WriteDataMessage(log, appender, message);
		appender->recursive = FALSE;
	}

	LeaveCriticalSection(&appender->lock);
	return status;
}

int WLog_WriteImage(wLog* log, wLogMessage* message)
{
	int status = -1;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return -1;

	if (!appender->State)
		WLog_OpenAppender(log);

	if (!appender->WriteImageMessage)
		return -1;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WriteImageMessage(log, appender, message);
		appender->recursive = FALSE;
	}

	LeaveCriticalSection(&appender->lock);
	return status;
}

int WLog_WritePacket(wLog* log, wLogMessage* message)
{
	int status = -1;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return -1;

	if (!appender->State)
		WLog_OpenAppender(log);

	if (!appender->WritePacketMessage)
		return -1;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WritePacketMessage(log, appender, message);
		appender->recursive = FALSE;
	}

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
	if (log->Level == WLOG_LEVEL_INHERIT)
	{
		return WLog_GetLogLevel(log->Parent);
	}
	else
	{
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

int WLog_ParseLogLevel(const char* level)
{
	int iLevel = -1;

	if (!level)
		return -1;

	if (_stricmp(level, "TRACE") == 0)
		iLevel = WLOG_TRACE;
	else if (_stricmp(level, "DEBUG") == 0)
		iLevel = WLOG_DEBUG;
	else if (_stricmp(level, "INFO") == 0)
		iLevel = WLOG_INFO;
	else if (_stricmp(level, "WARN") == 0)
		iLevel = WLOG_WARN;
	else if (_stricmp(level, "ERROR") == 0)
		iLevel = WLOG_ERROR;
	else if (_stricmp(level, "FATAL") == 0)
		iLevel = WLOG_FATAL;
	else if (_stricmp(level, "OFF") == 0)
		iLevel = WLOG_OFF;

	return iLevel;
}

int WLog_ParseFilter(wLogFilter* filter, LPCSTR name)
{
	char* p;
	char* q;
	int count;
	LPSTR names;
	int iLevel;
	count = 1;
	p = (char*) name;

	if (p)
	{
		while ((p = strchr(p, '.')) != NULL)
		{
			count++;
			p++;
		}
	}

	names = _strdup(name);
	filter->NameCount = count;
	filter->Names = (LPSTR*) malloc(sizeof(LPSTR) * (count + 1));
	filter->Names[count] = NULL;
	count = 0;
	p = (char*) names;
	filter->Names[count++] = p;
	q = strrchr(p, ':');

	if (!q)
		return -1;

	*q = '\0';
	q++;
	iLevel = WLog_ParseLogLevel(q);

	if (iLevel < 0)
		return -1;

	filter->Level = (DWORD) iLevel;

	while ((p = strchr(p, '.')) != NULL)
	{
		filter->Names[count++] = p + 1;
		*p = '\0';
		p++;
	}

	return 0;
}

int WLog_ParseFilters()
{
	char* p;
	char* env;
	DWORD count;
	DWORD nSize;
	int status;
	char** strs;

	nSize = GetEnvironmentVariableA("WLOG_FILTER", NULL, 0);

	if (nSize < 1)
		return 0;

	env = (LPSTR) malloc(nSize);

	if (!env)
		return -1;

	nSize = GetEnvironmentVariableA("WLOG_FILTER", env, nSize);
	count = 1;
	p = env;

	while ((p = strchr(p, ',')) != NULL)
	{
		count++;
		p++;
	}

	g_FilterCount = count;
	p = env;
	count = 0;
	strs = (char**) calloc(g_FilterCount, sizeof(char*));

	if (!strs)
		return -1;

	strs[count++] = p;

	while ((p = strchr(p, ',')) != NULL)
	{
		strs[count++] = p + 1;
		*p = '\0';
		p++;
	}

	g_Filters = calloc(g_FilterCount, sizeof(wLogFilter));

	if (!g_Filters)
	{
		free(strs);
		return -1;
	}

	for (count = 0; count < g_FilterCount; count++)
	{
		status = WLog_ParseFilter(&g_Filters[count], strs[count]);

		if (status < 0)
		{
			free(strs);
			return -1;
		}
	}

	free(strs);
	return 0;
}

int WLog_GetFilterLogLevel(wLog* log)
{
	DWORD i, j;
	int iLevel = -1;
	BOOL match = FALSE;

	for (i = 0; i < g_FilterCount; i++)
	{
		for (j = 0; j < g_Filters[i].NameCount; j++)
		{
			if (j >= log->NameCount)
				break;

			if (_stricmp(g_Filters[i].Names[j], "*") == 0)
			{
				match = TRUE;
				break;
			}

			if (_stricmp(g_Filters[i].Names[j], log->Names[j]) != 0)
				break;

			if (j == (log->NameCount - 1))
			{
				match = TRUE;
				break;
			}
		}

		if (match)
			break;
	}

	if (match)
	{
		iLevel = (int) g_Filters[i].Level;
	}

	return iLevel;
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

wLog* WLog_New(LPCSTR name, wLog* rootLogger)
{
	wLog* log;
	char* env;
	DWORD nSize;
	int iLevel;
	log = (wLog*) calloc(1, sizeof(wLog));

	if (log)
	{
		log->Name = _strdup(name);

		if (!log->Name)
		{
			free (log);
			return NULL;
		}

		WLog_ParseName(log, name);
		log->Parent = rootLogger;
		log->ChildrenCount = 0;
		log->ChildrenSize = 16;
		log->Children = (wLog**) calloc(log->ChildrenSize, sizeof(wLog*));

		if (!log->Children)
		{
			free (log->Name);
			free (log);
			return NULL;
		}

		log->Appender = NULL;

		if (rootLogger)
		{
			log->Level = WLOG_LEVEL_INHERIT;
		}
		else
		{
			log->Level = WLOG_INFO;
			nSize = GetEnvironmentVariableA("WLOG_LEVEL", NULL, 0);

			if (nSize)
			{
				env = (LPSTR) malloc(nSize);
				nSize = GetEnvironmentVariableA("WLOG_LEVEL", env, nSize);
				iLevel = WLog_ParseLogLevel(env);

				if (iLevel >= 0)
					log->Level = (DWORD) iLevel;

				free(env);
			}
		}

		iLevel = WLog_GetFilterLogLevel(log);

		if (iLevel >= 0)
			log->Level = (DWORD) iLevel;
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
		g_RootLog = WLog_New("", NULL);
		g_RootLog->IsRoot = TRUE;
		WLog_ParseFilters();
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
		wLog **tmp;
		parent->ChildrenSize *= 2;
		if (!parent->ChildrenSize)
		{
			if (parent->Children)
				free (parent->Children);
			parent->Children = NULL;
			
		}
		else
		{
			tmp = (wLog**) realloc(parent->Children, sizeof(wLog*) * parent->ChildrenSize);
			if (!tmp)
			{
				if (parent->Children)
					free (parent->Children);
				parent->Children = NULL;
				return -1;
			}
			parent->Children = tmp;
		}
	}

	if (!parent->Children)
		return -1;

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
	DWORD index;
	wLog* child = NULL;
	wLog* root = WLog_GetRoot();

	for (index = 0; index < root->ChildrenCount; index++)
	{
		child = root->Children[index];
		WLog_Free(child);
	}

	WLog_Free(root);
	g_RootLog = NULL;
}
