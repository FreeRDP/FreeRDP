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

#include <unistd.h>
#include <syslog.h>
#include <systemd/sd-journal.h>

#include <winpr/crt.h>
#include <winpr/environment.h>
#include <winpr/thread.h>

#include <winpr/wlog.h>

#include "wlog/Message.h"
#include "wlog/JournaldAppender.h"

BOOL Wlog_JournaldAppender_SetIdentifier(wLogJournaldAppender* appender, const char *id)
{
	if (appender->identifier)
		free(appender->identifier);

	if (appender->stream)
	{
		fclose(appender->stream);
		appender->stream = NULL;
	}

	return ((appender->identifier = _strdup(id)) != NULL);
}

static BOOL WLog_JournaldAppender_Open(wLog* log, wLogJournaldAppender* appender)
{
	int fd;

	if (!log || !appender)
		return FALSE;

	if (appender->stream)
		return TRUE;

	fd = sd_journal_stream_fd(appender->identifier, LOG_INFO, 1);
	if (fd < 0)
		return FALSE;

	appender->stream = fdopen(fd, "w");
	if (!appender->stream)
	{
		close(fd);
		return FALSE;
	}

	setbuffer(appender->stream, NULL, 0);
	return TRUE;
}

static BOOL WLog_JournaldAppender_Close(wLog* log, wLogJournaldAppender* appender)
{
	if (!log || !appender)
		return FALSE;

	return TRUE;
}

static BOOL WLog_JournaldAppender_WriteMessage(wLog* log, wLogJournaldAppender* appender, wLogMessage* message)
{
	char *formatStr;

	if (!log || !appender || !message)
		return FALSE;

	switch (message->Level)
	{
	case WLOG_TRACE:
	case WLOG_DEBUG:
		formatStr = "<7>%s\n";
		break;
	case WLOG_INFO:
		formatStr = "<6>%s\n";
		break;
	case WLOG_WARN:
		formatStr = "<4>%s\n";
		break;
	case WLOG_ERROR:
		formatStr = "<3>%s\n";
		break;
	case WLOG_FATAL:
		formatStr = "<2>%s\n";
		break;
	case WLOG_OFF:
		return TRUE;
	default:
		fprintf(stderr, "%s: unknown level %d\n", __FUNCTION__, message->Level);
		return FALSE;
	}

	fprintf(appender->stream, formatStr, message->TextString);
	return TRUE;
}

static BOOL WLog_JournaldAppender_WriteDataMessage(wLog* log, wLogJournaldAppender* appender, wLogMessage* message)
{
	if (!log || !appender || !message)
		return FALSE;

	return TRUE;
}

static BOOL WLog_JournaldAppender_WriteImageMessage(wLog* log, wLogJournaldAppender* appender, wLogMessage* message)
{
	if (!log || !appender || !message)
		return FALSE;


	return TRUE;
}

wLogJournaldAppender* WLog_JournaldAppender_New(wLog* log)
{
	wLogJournaldAppender* appender;
	DWORD nSize;
	LPCSTR name;

	appender = (wLogJournaldAppender*) calloc(1, sizeof(wLogJournaldAppender));
	if (!appender)
		return NULL;

	appender->Type = WLOG_APPENDER_JOURNALD;

	appender->Open = (WLOG_APPENDER_OPEN_FN) WLog_JournaldAppender_Open;
	appender->Close = (WLOG_APPENDER_OPEN_FN) WLog_JournaldAppender_Close;
	appender->WriteMessage =
			(WLOG_APPENDER_WRITE_MESSAGE_FN) WLog_JournaldAppender_WriteMessage;
	appender->WriteDataMessage =
			(WLOG_APPENDER_WRITE_DATA_MESSAGE_FN) WLog_JournaldAppender_WriteDataMessage;
	appender->WriteImageMessage =
			(WLOG_APPENDER_WRITE_IMAGE_MESSAGE_FN) WLog_JournaldAppender_WriteImageMessage;

	name = "WLOG_JOURNALD_ID";
	nSize = GetEnvironmentVariableA(name, NULL, 0);
	if (nSize)
	{
		appender->identifier = (LPSTR) malloc(nSize);
		if (!appender->identifier)
			goto error_env_malloc;

		GetEnvironmentVariableA(name, appender->identifier, nSize);

		if (!WLog_JournaldAppender_Open(log, appender))
			goto error_open;
	}
	else
	{
		appender->identifier = _strdup("winpr");
		if (!appender->identifier)
			goto error_env_malloc;
	}

	return appender;

error_open:
	free(appender->identifier);
error_env_malloc:
	free(appender);
	return NULL;
}

void WLog_JournaldAppender_Free(wLog* log, wLogJournaldAppender* appender)
{
	if (appender)
	{
		if (appender->stream)
			fclose(appender->stream);
		free(appender->identifier);
		free(appender);
	}
}
