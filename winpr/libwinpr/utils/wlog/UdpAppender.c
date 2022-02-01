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
#include <winpr/winsock.h>

#include "wlog.h"

typedef struct
{
	wLogAppender common;
	char* host;
	struct sockaddr targetAddr;
	int targetAddrLen;
	SOCKET sock;
} wLogUdpAppender;

static BOOL WLog_UdpAppender_Open(wLog* log, wLogAppender* appender)
{
	wLogUdpAppender* udpAppender;
	char addressString[256];
	struct addrinfo hints;
	struct addrinfo* result;
	int status;
	size_t addrLen;
	char* colonPos;

	if (!appender)
		return FALSE;

	udpAppender = (wLogUdpAppender*)appender;

	if (udpAppender->targetAddrLen) /* already opened */
		return TRUE;

	colonPos = strchr(udpAppender->host, ':');

	if (!colonPos)
		return FALSE;

	addrLen = (colonPos - udpAppender->host);
	memcpy(addressString, udpAppender->host, addrLen);
	addressString[addrLen] = '\0';
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;
	status = getaddrinfo(addressString, colonPos + 1, &hints, &result);

	if (status != 0)
		return FALSE;

	if (result->ai_addrlen > sizeof(udpAppender->targetAddr))
	{
		freeaddrinfo(result);
		return FALSE;
	}

	memcpy(&udpAppender->targetAddr, result->ai_addr, result->ai_addrlen);
	udpAppender->targetAddrLen = (int)result->ai_addrlen;
	freeaddrinfo(result);
	return TRUE;
}

static BOOL WLog_UdpAppender_Close(wLog* log, wLogAppender* appender)
{
	if (!log || !appender)
		return FALSE;

	return TRUE;
}

static BOOL WLog_UdpAppender_WriteMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	char prefix[WLOG_MAX_PREFIX_SIZE];
	wLogUdpAppender* udpAppender;

	if (!log || !appender || !message)
		return FALSE;

	udpAppender = (wLogUdpAppender*)appender;
	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);
	_sendto(udpAppender->sock, message->PrefixString, (int)strnlen(message->PrefixString, INT_MAX),
	        0, &udpAppender->targetAddr, udpAppender->targetAddrLen);
	_sendto(udpAppender->sock, message->TextString, (int)strnlen(message->TextString, INT_MAX), 0,
	        &udpAppender->targetAddr, udpAppender->targetAddrLen);
	_sendto(udpAppender->sock, "\n", 1, 0, &udpAppender->targetAddr, udpAppender->targetAddrLen);
	return TRUE;
}

static BOOL WLog_UdpAppender_WriteDataMessage(wLog* log, wLogAppender* appender,
                                              wLogMessage* message)
{
	if (!log || !appender || !message)
		return FALSE;

	return TRUE;
}

static BOOL WLog_UdpAppender_WriteImageMessage(wLog* log, wLogAppender* appender,
                                               wLogMessage* message)
{
	if (!log || !appender || !message)
		return FALSE;

	return TRUE;
}

static BOOL WLog_UdpAppender_Set(wLogAppender* appender, const char* setting, void* value)
{
	const char target[] = "target";
	wLogUdpAppender* udpAppender = (wLogUdpAppender*)appender;

	/* Just check the value string is not empty */
	if (!value || (strnlen(value, 2) == 0))
		return FALSE;

	if (strncmp(target, setting, sizeof(target)))
		return FALSE;

	udpAppender->targetAddrLen = 0;

	if (udpAppender->host)
		free(udpAppender->host);

	udpAppender->host = _strdup((const char*)value);
	return (udpAppender->host != NULL) && WLog_UdpAppender_Open(NULL, appender);
}

static void WLog_UdpAppender_Free(wLogAppender* appender)
{
	if (appender)
	{
		wLogUdpAppender* udpAppender = (wLogUdpAppender*)appender;

		if (udpAppender->sock != INVALID_SOCKET)
		{
			closesocket(udpAppender->sock);
			udpAppender->sock = INVALID_SOCKET;
		}

		free(udpAppender->host);
		free(udpAppender);
	}
}

wLogAppender* WLog_UdpAppender_New(wLog* log)
{
	wLogUdpAppender* appender;
	DWORD nSize;
	LPCSTR name;
	appender = (wLogUdpAppender*)calloc(1, sizeof(wLogUdpAppender));

	if (!appender)
		return NULL;

	appender->common.Type = WLOG_APPENDER_UDP;
	appender->common.Open = WLog_UdpAppender_Open;
	appender->common.Close = WLog_UdpAppender_Close;
	appender->common.WriteMessage = WLog_UdpAppender_WriteMessage;
	appender->common.WriteDataMessage = WLog_UdpAppender_WriteDataMessage;
	appender->common.WriteImageMessage = WLog_UdpAppender_WriteImageMessage;
	appender->common.Free = WLog_UdpAppender_Free;
	appender->common.Set = WLog_UdpAppender_Set;
	appender->sock = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (appender->sock == INVALID_SOCKET)
		goto fail;

	name = "WLOG_UDP_TARGET";
	nSize = GetEnvironmentVariableA(name, NULL, 0);

	if (nSize)
	{
		appender->host = (LPSTR)malloc(nSize);

		if (!appender->host)
			goto fail;

		if (GetEnvironmentVariableA(name, appender->host, nSize) != nSize - 1)
			goto fail;

		if (!WLog_UdpAppender_Open(log, &appender->common))
			goto fail;
	}
	else
	{
		appender->host = _strdup("127.0.0.1:20000");

		if (!appender->host)
			goto fail;
	}

	return &appender->common;
fail:
	WLog_UdpAppender_Free(&appender->common);
	return NULL;
}
