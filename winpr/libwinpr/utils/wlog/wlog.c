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
#include "../log.h"
#endif

#include "wlog.h"


struct _wLogFilter
{
	DWORD Level;
	LPSTR* Names;
	DWORD NameCount;
};
typedef struct _wLogFilter wLogFilter;

/**
 * References for general logging concepts:
 *
 * Short introduction to log4j:
 * http://logging.apache.org/log4j/1.2/manual.html
 *
 * logging - Logging facility for Python:
 * http://docs.python.org/2/library/logging.html
 */

LPCSTR WLOG_LEVELS[7] =
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

static int WLog_ParseLogLevel(LPCSTR level);
static BOOL WLog_ParseFilter(wLogFilter* filter, LPCSTR name);

static BOOL log_recursion(LPCSTR file, LPCSTR fkt, int line)
{
	char** msg;
	size_t used, i;
	void* bt = winpr_backtrace(20);
#if defined(ANDROID)
	LPCSTR tag = WINPR_TAG("utils.wlog");
#endif

	if (!bt)
		return FALSE;
	msg = winpr_backtrace_symbols(bt, &used);
	if (!msg)
		return FALSE;
#if defined(ANDROID)
	if (__android_log_print(ANDROID_LOG_FATAL, tag, "Recursion detected!!!") < 0)
		return FALSE;
	if (__android_log_print(ANDROID_LOG_FATAL, tag, "Check %s [%s:%d]", fkt, file, line) < 0)
		return FALSE;

	for (i=0; i<used; i++)
		if (__android_log_print(ANDROID_LOG_FATAL, tag, "%d: %s", i, msg[i]) < 0)
			return FALSE;

#else
	if (fprintf(stderr, "[%s]: Recursion detected!\n", fkt) < 0)
		return FALSE;
	if (fprintf(stderr, "[%s]: Check %s:%d\n", fkt, file, line) < 0)
		return FALSE;

	for (i=0; i<used; i++)
		if (fprintf(stderr, "%s: %zd: %s\n", fkt, i, msg[i]) < 0)
			return FALSE;

#endif

	free(msg);

	winpr_backtrace_free(bt);
	return TRUE;
}

BOOL WLog_Write(wLog* log, wLogMessage* message)
{
	BOOL status;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return FALSE;

	if (!appender->active)
		if (!WLog_OpenAppender(log))
			return FALSE;

	if (!appender->WriteMessage)
		return FALSE;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		status = log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WriteMessage(log, appender, message);
		appender->recursive = FALSE;
	}

	LeaveCriticalSection(&appender->lock);
	return status;
}

BOOL WLog_WriteData(wLog* log, wLogMessage* message)
{
	BOOL status;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return FALSE;

	if (!appender->active)
		if (!WLog_OpenAppender(log))
			return FALSE;

	if (!appender->WriteDataMessage)
		return FALSE;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		status = log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WriteDataMessage(log, appender, message);
		appender->recursive = FALSE;
	}

	LeaveCriticalSection(&appender->lock);
	return status;
}

BOOL WLog_WriteImage(wLog* log, wLogMessage* message)
{
	BOOL status;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return FALSE;

	if (!appender->active)
		if (!WLog_OpenAppender(log))
			return FALSE;

	if (!appender->WriteImageMessage)
		return FALSE;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		status = log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WriteImageMessage(log, appender, message);
		appender->recursive = FALSE;
	}

	LeaveCriticalSection(&appender->lock);
	return status;
}

BOOL WLog_WritePacket(wLog* log, wLogMessage* message)
{
	BOOL status;
	wLogAppender* appender;
	appender = WLog_GetLogAppender(log);

	if (!appender)
		return FALSE;

	if (!appender->active)
		if (!WLog_OpenAppender(log))
			return FALSE;

	if (!appender->WritePacketMessage)
		return FALSE;

	EnterCriticalSection(&appender->lock);

	if (appender->recursive)
		status = log_recursion(message->FileName, message->FunctionName, message->LineNumber);
	else
	{
		appender->recursive = TRUE;
		status = appender->WritePacketMessage(log, appender, message);
		appender->recursive = FALSE;
	}

	LeaveCriticalSection(&appender->lock);
	return status;
}

BOOL WLog_PrintMessageVA(wLog* log, wLogMessage* message, va_list args)
{
	BOOL status = FALSE;

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
			if (wvsnprintfx(formattedLogMessage, WLOG_MAX_STRING_SIZE - 1, message->FormatString, args) < 0)
				return FALSE;
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

BOOL WLog_PrintMessage(wLog* log, wLogMessage* message, ...)
{
	BOOL status;
	va_list args;
	va_start(args, message);
	status = WLog_PrintMessageVA(log, message, args);
	va_end(args);
	return status;
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

BOOL WLog_SetStringLogLevel(wLog* log, LPCSTR level)
{
	int lvl;
	if (!log || !level)
		return FALSE;

	lvl = WLog_ParseLogLevel(level);
	if (lvl < 0)
		return FALSE;

	return WLog_SetLogLevel(log, lvl);
}

BOOL WLog_AddStringLogFilters(LPCSTR filter)
{
	DWORD pos;
	DWORD size;
	DWORD count;
	LPSTR p;
	LPSTR filterStr;
	LPSTR cp;
	wLogFilter* tmp;

	if (!filter)
		return FALSE;

	count = 1;
	p = (LPSTR)filter;

	while ((p = strchr(p, ',')) != NULL)
	{
		count++;
		p++;
	}

	pos = g_FilterCount;
	size = g_FilterCount + count;
	tmp = (wLogFilter*) realloc(g_Filters, size * sizeof(wLogFilter));

	if (!tmp)
		return FALSE;

	g_Filters = tmp;

	cp = (LPSTR)_strdup(filter);
	if (!cp)
		return FALSE;

	p = cp;
	filterStr = cp;

	do
	{
		p = strchr(p, ',');
		if (p)
			*p = '\0';

		if (pos < size)
		{
			if (!WLog_ParseFilter(&g_Filters[pos++], filterStr))
			{
				free (cp);
				return FALSE;
			}
		}
		else
			break;

		if (p)
		{
			filterStr = p + 1;
			p++;
		}
	}
	while (p != NULL);

	g_FilterCount = size;
	free (cp);

	return TRUE;
}

BOOL WLog_SetLogLevel(wLog* log, DWORD logLevel)
{
	if (!log)
		return FALSE;

	if ((logLevel > WLOG_OFF) && (logLevel != WLOG_LEVEL_INHERIT))
	{
		logLevel = WLOG_OFF;
	}

	log->Level = logLevel;
	return TRUE;
}

int WLog_ParseLogLevel(LPCSTR level)
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

BOOL WLog_ParseFilter(wLogFilter* filter, LPCSTR name)
{
	char* p;
	char* q;
	int count;
	LPSTR names;
	int iLevel;
	count = 1;

	if(!name)
		return FALSE;

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
	if (!names)
		return FALSE;
	filter->NameCount = count;
	filter->Names = (LPSTR*) calloc((count + 1UL), sizeof(LPSTR));
	if(!filter->Names)
	{
		free(names);
		filter->NameCount = 0;
		return FALSE;
	}
	filter->Names[count] = NULL;
	count = 0;
	p = (char*) names;
	filter->Names[count++] = p;
	q = strrchr(p, ':');

	if (!q)
	{
		free(names);
		free(filter->Names);
		filter->Names = NULL;
		filter->NameCount = 0;
		return FALSE;
	}

	*q = '\0';
	q++;
	iLevel = WLog_ParseLogLevel(q);

	if (iLevel < 0)
	{
		free(names);
		free(filter->Names);
		filter->Names = NULL;
		filter->NameCount = 0;
		return FALSE;
	}

	filter->Level = (DWORD) iLevel;

	while ((p = strchr(p, '.')) != NULL)
	{
		if (count < (int) filter->NameCount)
			filter->Names[count++] = p + 1;
		*p = '\0';
		p++;
	}

	return TRUE;
}

BOOL WLog_ParseFilters()
{
	BOOL res;
	char* env;
	DWORD nSize;

	g_Filters = NULL;
	g_FilterCount = 0;

	nSize = GetEnvironmentVariableA("WLOG_FILTER", NULL, 0);

	if (nSize < 1)
		return TRUE;

	env = (LPSTR) malloc(nSize);

	if (!env)
		return FALSE;

	if (!GetEnvironmentVariableA("WLOG_FILTER", env, nSize))
		return FALSE;

	res = WLog_AddStringLogFilters(env);

	free(env);

	return res;
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

BOOL WLog_ParseName(wLog* log, LPCSTR name)
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
	if (!names)
		return FALSE;
	log->NameCount = count;
	log->Names = (LPSTR*) calloc((count + 1UL), sizeof(LPSTR));
	if(!log->Names)
	{
		free(names);
		return FALSE;
	}
	log->Names[count] = NULL;
	count = 0;
	p = (char*) names;
	log->Names[count++] = p;

	while ((p = strchr(p, '.')) != NULL)
	{
		if (count < (int) log->NameCount)
			log->Names[count++] = p + 1;
		*p = '\0';
		p++;
	}

	return TRUE;
}

wLog* WLog_New(LPCSTR name, wLog* rootLogger)
{
	wLog* log = NULL;
	char* env = NULL;
	DWORD nSize;
	int iLevel;

	log = (wLog*) calloc(1, sizeof(wLog));
	if (!log)
		return NULL;

    log->Name = _strdup(name);

    if (!log->Name)
		goto out_fail;

    if (!WLog_ParseName(log, name))
		goto out_fail;

    log->Parent = rootLogger;
    log->ChildrenCount = 0;
    log->ChildrenSize = 16;

    if (!(log->Children = (wLog**) calloc(log->ChildrenSize, sizeof(wLog*))))
		goto out_fail;

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
			if (!env)
				goto out_fail;

			if (!GetEnvironmentVariableA("WLOG_LEVEL", env, nSize))
			{
				fprintf(stderr, "WLOG_LEVEL environment variable changed in my back !\n");
				free(env);
				goto out_fail;
			}

			iLevel = WLog_ParseLogLevel(env);
			free(env);

			if (iLevel >= 0)
				log->Level = (DWORD) iLevel;
        }
    }

    iLevel = WLog_GetFilterLogLevel(log);

    if (iLevel >= 0)
        log->Level = (DWORD) iLevel;

	return log;

out_fail:
	free (log->Children);
	free (log->Name);
	free (log);
	return NULL;
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
		if (!(g_RootLog = WLog_New("", NULL)))
			return NULL;

		g_RootLog->IsRoot = TRUE;
		WLog_ParseFilters();
		logAppenderType = WLOG_APPENDER_CONSOLE;
		nSize = GetEnvironmentVariableA("WLOG_APPENDER", NULL, 0);

		if (nSize)
		{
			env = (LPSTR) malloc(nSize);
			if (!env)
				goto fail;

			if (!GetEnvironmentVariableA("WLOG_APPENDER", env, nSize))
			{
				fprintf(stderr, "WLOG_APPENDER environment variable modified in my back");
				free(env);
				goto fail;
			}

			if (_stricmp(env, "CONSOLE") == 0)
				logAppenderType = WLOG_APPENDER_CONSOLE;
			else if (_stricmp(env, "FILE") == 0)
				logAppenderType = WLOG_APPENDER_FILE;
			else if (_stricmp(env, "BINARY") == 0)
				logAppenderType = WLOG_APPENDER_BINARY;
#ifdef HAVE_SYSLOG_H
			else if (_stricmp(env, "SYSLOG") == 0)
				logAppenderType = WLOG_APPENDER_SYSLOG;
#endif /* HAVE_SYSLOG_H */
#ifdef HAVE_JOURNALD_H
			else if (_stricmp(env, "JOURNALD") == 0)
				logAppenderType = WLOG_APPENDER_JOURNALD;
#endif
			else if (_stricmp(env, "UDP") == 0)
				logAppenderType = WLOG_APPENDER_UDP;

			free(env);
		}

		if (!WLog_SetLogAppenderType(g_RootLog, logAppenderType))
			goto fail;
	}

	return g_RootLog;

fail:
	free(g_RootLog);
	g_RootLog = NULL;
	return NULL;
}

BOOL WLog_AddChild(wLog* parent, wLog* child)
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
				return FALSE;
			}
			parent->Children = tmp;
		}
	}

	if (!parent->Children)
		return FALSE;

	parent->Children[parent->ChildrenCount++] = child;
	child->Parent = parent;
	return TRUE;
}

wLog* WLog_FindChild(LPCSTR name)
{
	DWORD index;
	wLog* root;
	wLog* child = NULL;
	BOOL found = FALSE;
	root = WLog_GetRoot();

	if (!root)
		return NULL;

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
	if (!(log = WLog_FindChild(name)))
	{
		wLog* root = WLog_GetRoot();
		if (!root)
			return NULL;
		if (!(log = WLog_New(name, root)))
			return NULL;
		if (!WLog_AddChild(root, log))
		{
			WLog_Free(log);
			return NULL;
		}
	}
	return log;
}

BOOL WLog_Init()
{
	return WLog_GetRoot() != NULL;
}

BOOL WLog_Uninit()
{
	DWORD index;
	wLog* child = NULL;
	wLog* root = g_RootLog;

	if (!root)
		return FALSE;

	for (index = 0; index < root->ChildrenCount; index++)
	{
		child = root->Children[index];
		WLog_Free(child);
	}

	WLog_Free(root);
	g_RootLog = NULL;
	return TRUE;
}
