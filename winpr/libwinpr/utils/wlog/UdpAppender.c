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


struct _wLogUdpAppender
{
	WLOG_APPENDER_COMMON();
	char *host;
	struct sockaddr targetAddr;
	int	targetAddrLen;
	SOCKET sock;
};
typedef struct _wLogUdpAppender wLogUdpAppender;

static BOOL WLog_UdpAppender_Open(wLog* log, wLogAppender* appender)
{
	wLogUdpAppender *udpAppender;
	char addressString[256];
	struct addrinfo hints;
	struct addrinfo* result;
	int status, addrLen;
	char *colonPos;

	if (!appender)
		return FALSE;

	udpAppender = (wLogUdpAppender*)appender;

	if (udpAppender->targetAddrLen) /* already opened */
		return TRUE;

	colonPos = strchr(udpAppender->host, ':');
	if (!colonPos)
		return FALSE;
	addrLen = (int) (colonPos - udpAppender->host);
	memcpy(addressString, udpAppender->host, addrLen);
	addressString[addrLen] = '\0';

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	status = getaddrinfo(addressString, colonPos+1, &hints, &result);
	if (status != 0)
		return FALSE;

	if (result->ai_addrlen > sizeof(udpAppender->targetAddr))
	{
		freeaddrinfo(result);
		return FALSE;
	}

	memcpy(&udpAppender->targetAddr, result->ai_addr, result->ai_addrlen);
	udpAppender->targetAddrLen = (int) result->ai_addrlen;

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
	wLogUdpAppender *udpAppender;


	if (!log || !appender || !message)
		return FALSE;

	udpAppender = (wLogUdpAppender *)appender;

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	_sendto(udpAppender->sock, message->PrefixString, strlen(message->PrefixString),
				0, &udpAppender->targetAddr, udpAppender->targetAddrLen);

	_sendto(udpAppender->sock, message->TextString, strlen(message->TextString),
						0, &udpAppender->targetAddr, udpAppender->targetAddrLen);

	_sendto(udpAppender->sock, "\n", 1, 0, &udpAppender->targetAddr, udpAppender->targetAddrLen);

	return TRUE;
}

static BOOL WLog_UdpAppender_WriteDataMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	if (!log || !appender || !message)
		return FALSE;

	return TRUE;
}

static BOOL WLog_UdpAppender_WriteImageMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	if (!log || !appender || !message)
		return FALSE;


	return TRUE;
}

static BOOL WLog_UdpAppender_Set(wLogAppender* appender, const char *setting, void *value)
{
	wLogUdpAppender *udpAppender = (wLogUdpAppender *)appender;

	if (!value || !strlen(value))
		return FALSE;

	if (strcmp("target", setting))
		return FALSE;

	udpAppender->targetAddrLen = 0;
	if (udpAppender->host)
		free(udpAppender->host);

	udpAppender->host = _strdup((const char *)value);
	return (udpAppender->host != NULL) && WLog_UdpAppender_Open(NULL, appender);
}

static void WLog_UdpAppender_Free(wLogAppender* appender)
{
	wLogUdpAppender* udpAppender;
	if (appender)
	{
		udpAppender = (wLogUdpAppender *)appender;
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

	appender = (wLogUdpAppender*) calloc(1, sizeof(wLogUdpAppender));
	if (!appender)
		return NULL;

	appender->Type = WLOG_APPENDER_UDP;

	appender->Open = WLog_UdpAppender_Open;
	appender->Close = WLog_UdpAppender_Close;
	appender->WriteMessage = WLog_UdpAppender_WriteMessage;
	appender->WriteDataMessage = WLog_UdpAppender_WriteDataMessage;
	appender->WriteImageMessage = WLog_UdpAppender_WriteImageMessage;
	appender->Free = WLog_UdpAppender_Free;
	appender->Set = WLog_UdpAppender_Set;

	appender->sock = _socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (appender->sock == INVALID_SOCKET)
		goto error_sock;

	name = "WLOG_UDP_TARGET";
	nSize = GetEnvironmentVariableA(name, NULL, 0);
	if (nSize)
	{
		appender->host = (LPSTR) malloc(nSize);
		if (!appender->host)
			goto error_host_alloc;

		GetEnvironmentVariableA(name, appender->host, nSize);

		if (!WLog_UdpAppender_Open(log, (wLogAppender *)appender))
			goto error_open;
	}
	else
	{
		appender->host = _strdup("127.0.0.1:20000");
		if (!appender->host)
			goto error_host_alloc;
	}

	return (wLogAppender*)appender;

error_open:
	free(appender->host);
error_host_alloc:
	closesocket(appender->sock);
error_sock:
	free(appender);
	return NULL;
}
