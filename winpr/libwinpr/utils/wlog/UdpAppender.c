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


#include <winpr/crt.h>
#include <winpr/environment.h>
#include <winpr/thread.h>

#include <winpr/wlog.h>

#include "wlog/Message.h"
#include "wlog/UdpAppender.h"

#ifndef _WIN32
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#endif

static BOOL WLog_UdpAppender_Open(wLog* log, wLogUdpAppender* appender)
{
	char addressString[256];
	struct addrinfo hints;
	struct addrinfo* result;
	int status, addrLen;
	char *colonPos;


	if (!log || !appender)
		return FALSE;

	if (appender->targetAddrLen) /* already opened */
		return TRUE;

	colonPos = strchr(appender->host, ':');
	if (!colonPos)
		return FALSE;
	addrLen = colonPos - appender->host;
	memcpy(addressString, appender->host, addrLen);
	addressString[addrLen] = '\0';

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	status = getaddrinfo(addressString, colonPos+1, &hints, &result);
	if (status != 0)
		return FALSE;

	if (result->ai_addrlen > sizeof(appender->targetAddr))
	{
		freeaddrinfo(result);
		return FALSE;
	}

	memcpy(&appender->targetAddr, result->ai_addr, result->ai_addrlen);
	appender->targetAddrLen = result->ai_addrlen;


	return TRUE;
}

BOOL Wlog_UdpAppender_SetTarget(wLogUdpAppender* appender, const char *host)
{

	appender->targetAddrLen = 0;
	if (appender->host)
		free(appender->host);

	appender->host = _strdup(host);
	return (appender->host != NULL) && WLog_UdpAppender_Open(NULL, appender);
}

static BOOL WLog_UdpAppender_Close(wLog* log, wLogUdpAppender* appender)
{
	if (!log || !appender)
		return FALSE;

	return TRUE;
}

static BOOL WLog_UdpAppender_WriteMessage(wLog* log, wLogUdpAppender* appender, wLogMessage* message)
{
	char prefix[WLOG_MAX_PREFIX_SIZE];

	if (!log || !appender || !message)
		return FALSE;

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	_sendto(appender->sock, message->PrefixString, strlen(message->PrefixString),
				0, &appender->targetAddr, appender->targetAddrLen);

	_sendto(appender->sock, message->TextString, strlen(message->TextString),
						0, &appender->targetAddr, appender->targetAddrLen);

	_sendto(appender->sock, "\n", 1, 0, &appender->targetAddr, appender->targetAddrLen);

	return TRUE;
}

static BOOL WLog_UdpAppender_WriteDataMessage(wLog* log, wLogUdpAppender* appender, wLogMessage* message)
{
	if (!log || !appender || !message)
		return FALSE;

	return TRUE;
}

static BOOL WLog_UdpAppender_WriteImageMessage(wLog* log, wLogUdpAppender* appender, wLogMessage* message)
{
	if (!log || !appender || !message)
		return FALSE;


	return TRUE;
}

wLogUdpAppender* WLog_UdpAppender_New(wLog* log)
{
	wLogUdpAppender* appender;
	DWORD nSize;
	LPCSTR name;

	appender = (wLogUdpAppender*) calloc(1, sizeof(wLogUdpAppender));
	if (!appender)
		return NULL;

	appender->Type = WLOG_APPENDER_UDP;

	appender->Open = (WLOG_APPENDER_OPEN_FN) WLog_UdpAppender_Open;
	appender->Close = (WLOG_APPENDER_OPEN_FN) WLog_UdpAppender_Close;
	appender->WriteMessage =
			(WLOG_APPENDER_WRITE_MESSAGE_FN) WLog_UdpAppender_WriteMessage;
	appender->WriteDataMessage =
			(WLOG_APPENDER_WRITE_DATA_MESSAGE_FN) WLog_UdpAppender_WriteDataMessage;
	appender->WriteImageMessage =
			(WLOG_APPENDER_WRITE_IMAGE_MESSAGE_FN) WLog_UdpAppender_WriteImageMessage;

	appender->sock = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (appender->sock == INVALID_SOCKET)
		goto error_sock;

	name = "WLOG_UDP_TARGET";
	nSize = GetEnvironmentVariableA(name, NULL, 0);
	if (nSize)
	{
		appender->host = (LPSTR) malloc(nSize);
		if (!appender->host)
			goto error_env_malloc;

		GetEnvironmentVariableA(name, appender->host, nSize);

		if (!WLog_UdpAppender_Open(log, appender))
			goto error_open;
	}
	else
	{
		appender->host = _strdup("127.0.0.1:20000");
		if (!appender->host)
			goto error_env_malloc;
	}

	return appender;

error_open:
	free(appender->host);
error_env_malloc:
	closesocket(appender->sock);
error_sock:
	free(appender);
	return NULL;
}

void WLog_UdpAppender_Free(wLog* log, wLogUdpAppender* appender)
{
	if (appender)
	{
		if (appender->sock != INVALID_SOCKET)
		{
			closesocket(appender->sock);
			appender->sock = INVALID_SOCKET;
		}
		free(appender->host);
		free(appender);
	}
}
