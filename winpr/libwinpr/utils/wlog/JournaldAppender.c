/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright © 2015 Thincast Technologies GmbH
 * Copyright © 2015 David FORT <contact@hardening-consulting.com>
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

#include "JournaldAppender.h"

#include <unistd.h>
#include <syslog.h>
#include <systemd/sd-journal.h>

#include <winpr/crt.h>
#include <winpr/environment.h>


struct _wLogJournaldAppender
{
	WLOG_APPENDER_COMMON();
	char *identifier;
	FILE *stream;
};
typedef struct _wLogJournaldAppender wLogJournaldAppender;

static BOOL WLog_JournaldAppender_Open(wLog* log, wLogAppender* appender)
{
	int fd;
	wLogJournaldAppender *journaldAppender;

	if (!log || !appender)
		return FALSE;

	journaldAppender = (wLogJournaldAppender*)appender;
	if (journaldAppender->stream)
		return TRUE;

	fd = sd_journal_stream_fd(journaldAppender->identifier, LOG_INFO, 1);
	if (fd < 0)
		return FALSE;

	journaldAppender->stream = fdopen(fd, "w");
	if (!journaldAppender->stream)
	{
		close(fd);
		return FALSE;
	}

	setbuffer(journaldAppender->stream, NULL, 0);
	return TRUE;
}

static BOOL WLog_JournaldAppender_Close(wLog* log, wLogAppender* appender)
{
	if (!log || !appender)
		return FALSE;

	return TRUE;
}

static BOOL WLog_JournaldAppender_WriteMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	char *formatStr;
	wLogJournaldAppender* journaldAppender;
	char prefix[WLOG_MAX_PREFIX_SIZE];

	if (!log || !appender || !message)
		return FALSE;

	journaldAppender = (wLogJournaldAppender *)appender;

	switch (message->Level)
	{
	case WLOG_TRACE:
	case WLOG_DEBUG:
		formatStr = "<7>%s%s\n";
		break;
	case WLOG_INFO:
		formatStr = "<6>%s%s\n";
		break;
	case WLOG_WARN:
		formatStr = "<4>%s%s\n";
		break;
	case WLOG_ERROR:
		formatStr = "<3>%s%s\n";
		break;
	case WLOG_FATAL:
		formatStr = "<2>%s%s\n";
		break;
	case WLOG_OFF:
		return TRUE;
	default:
		fprintf(stderr, "%s: unknown level %d\n", __FUNCTION__, message->Level);
		return FALSE;
	}

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	if (message->Level != WLOG_OFF)
		fprintf(journaldAppender->stream, formatStr, message->PrefixString, message->TextString);
	return TRUE;
}

static BOOL WLog_JournaldAppender_WriteDataMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	if (!log || !appender || !message)
		return FALSE;

	return TRUE;
}

static BOOL WLog_JournaldAppender_WriteImageMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	if (!log || !appender || !message)
		return FALSE;


	return TRUE;
}

static BOOL WLog_JournaldAppender_Set(wLogAppender* appender, const char *setting, void *value)
{
	wLogJournaldAppender* journaldAppender = (wLogJournaldAppender *)appender;

	if (!value || !strlen(value))
		return FALSE;

	if (strcmp("identifier", setting))
		return FALSE;

	/* If the stream is already open the identifier can't be changed */
	if (journaldAppender->stream)
		return FALSE;

	if (journaldAppender->identifier)
		free(journaldAppender->identifier);

	return ((journaldAppender->identifier = _strdup((const char *)value)) != NULL);
}

static void WLog_JournaldAppender_Free(wLogAppender* appender)
{
	wLogJournaldAppender *journaldAppender;
	if (appender)
	{
		journaldAppender = (wLogJournaldAppender*)appender;
		if (journaldAppender->stream)
			fclose(journaldAppender->stream);
		free(journaldAppender->identifier);
		free(journaldAppender);
	}
}

wLogAppender* WLog_JournaldAppender_New(wLog* log)
{
	wLogJournaldAppender* appender;
	DWORD nSize;
	LPCSTR name;

	appender = (wLogJournaldAppender*) calloc(1, sizeof(wLogJournaldAppender));
	if (!appender)
		return NULL;

	appender->Type = WLOG_APPENDER_JOURNALD;
	appender->Open = WLog_JournaldAppender_Open;
	appender->Close = WLog_JournaldAppender_Close;
	appender->WriteMessage = WLog_JournaldAppender_WriteMessage;
	appender->WriteDataMessage = WLog_JournaldAppender_WriteDataMessage;
	appender->WriteImageMessage = WLog_JournaldAppender_WriteImageMessage;
	appender->Set = WLog_JournaldAppender_Set;
	appender->Free = WLog_JournaldAppender_Free;

	name = "WLOG_JOURNALD_ID";
	nSize = GetEnvironmentVariableA(name, NULL, 0);
	if (nSize)
	{
		appender->identifier = (LPSTR) malloc(nSize);
		if (!appender->identifier)
			goto error_env_malloc;

		GetEnvironmentVariableA(name, appender->identifier, nSize);

		if (!WLog_JournaldAppender_Open(log, (wLogAppender *)appender))
			goto error_open;
	}

	return (wLogAppender *)appender;

error_open:
	free(appender->identifier);
error_env_malloc:
	free(appender);
	return NULL;
}
