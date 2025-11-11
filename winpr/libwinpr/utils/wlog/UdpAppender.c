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

#include <winpr/config.h>

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

static BOOL WLog_UdpAppender_Open(WINPR_ATTR_UNUSED wLog* log, wLogAppender* appender)
{
	wLogUdpAppender* udpAppender = NULL;
	char addressString[256] = { 0 };
	struct addrinfo hints = { 0 };
	struct addrinfo* result = { 0 };
	int status = 0;
	char* colonPos = NULL;

	if (!appender)
		return FALSE;

	udpAppender = (wLogUdpAppender*)appender;

	if (udpAppender->targetAddrLen) /* already opened */
		return TRUE;

	colonPos = strchr(udpAppender->host, ':');

	if (!colonPos)
		return FALSE;

	const size_t addrLen = WINPR_ASSERTING_INT_CAST(size_t, (colonPos - udpAppender->host));
	memcpy(addressString, udpAppender->host, addrLen);
	addressString[addrLen] = '\0';
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

static BOOL WLog_UdpAppender_WriteMessage(wLog* log, wLogAppender* appender,
                                          const wLogMessage* cmessage)
{
	if (!log || !appender || !cmessage)
		return FALSE;

	wLogUdpAppender* udpAppender = (wLogUdpAppender*)appender;

	char prefix[WLOG_MAX_PREFIX_SIZE] = { 0 };
	WLog_Layout_GetMessagePrefix(log, appender->Layout, cmessage, prefix, sizeof(prefix));

	(void)_sendto(udpAppender->sock, prefix, (int)strnlen(prefix, ARRAYSIZE(prefix)), 0,
	              &udpAppender->targetAddr, udpAppender->targetAddrLen);
	(void)_sendto(udpAppender->sock, cmessage->TextString,
	              (int)strnlen(cmessage->TextString, INT_MAX), 0, &udpAppender->targetAddr,
	              udpAppender->targetAddrLen);
	(void)_sendto(udpAppender->sock, "\n", 1, 0, &udpAppender->targetAddr,
	              udpAppender->targetAddrLen);
	return TRUE;
}

static BOOL WLog_UdpAppender_WriteDataMessage(wLog* log, wLogAppender* appender,
                                              const wLogMessage* message)
{
	if (!log || !appender || !message)
		return FALSE;

	return TRUE;
}

static BOOL WLog_UdpAppender_WriteImageMessage(wLog* log, wLogAppender* appender,
                                               const wLogMessage* message)
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

	if (strncmp(target, setting, sizeof(target)) != 0)
		return FALSE;

	udpAppender->targetAddrLen = 0;

	if (udpAppender->host)
		free(udpAppender->host);

	udpAppender->host = _strdup((const char*)value);
	return (udpAppender->host != NULL) && WLog_UdpAppender_Open(NULL, appender);
}

static void WLog_UdpAppender_Free(wLogAppender* appender)
{
	wLogUdpAppender* udpAppender = NULL;

	if (appender)
	{
		udpAppender = (wLogUdpAppender*)appender;

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
	DWORD nSize = 0;
	LPCSTR name = NULL;
	wLogUdpAppender* appender = (wLogUdpAppender*)calloc(1, sizeof(wLogUdpAppender));

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
		goto error_sock;

	name = "WLOG_UDP_TARGET";
	nSize = GetEnvironmentVariableA(name, NULL, 0);

	if (nSize)
	{
		appender->host = (LPSTR)malloc(nSize);

		if (!appender->host)
			goto error_open;

		if (GetEnvironmentVariableA(name, appender->host, nSize) != nSize - 1)
			goto error_open;

		if (!WLog_UdpAppender_Open(log, (wLogAppender*)appender))
			goto error_open;
	}
	else
	{
		appender->host = _strdup("127.0.0.1:20000");

		if (!appender->host)
			goto error_open;
	}

	return &appender->common;
error_open:
	free(appender->host);
	closesocket(appender->sock);
error_sock:
	free(appender);
	return NULL;
}
