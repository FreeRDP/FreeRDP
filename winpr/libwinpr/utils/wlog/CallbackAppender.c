/**
 * WinPR: Windows Portable Runtime
 * WinPR Logger
 *
 * Copyright 2014 Armin Novak <armin.novak@thincast.com>
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
#include <winpr/path.h>

#include <winpr/wlog.h>

#include "wlog/Message.h"

#include "wlog/CallbackAppender.h"

/**
 * Callback Appender
 */

WINPR_API void WLog_CallbackAppender_SetCallbacks(wLog* log, wLogCallbackAppender* appender,
	CallbackAppenderMessage_t msg, CallbackAppenderImage_t img, CallbackAppenderPackage_t pkg,
	CallbackAppenderData_t data)
{
	if (!appender)
		return;

	if (appender->Type != WLOG_APPENDER_CALLBACK)
		return;

	appender->message = msg;
	appender->image = img;
	appender->package = pkg;
	appender->data = data;
}

int WLog_CallbackAppender_Open(wLog* log, wLogCallbackAppender* appender)
{
	return 0;
}

int WLog_CallbackAppender_Close(wLog* log, wLogCallbackAppender* appender)
{
	return 0;
}

int WLog_CallbackAppender_WriteMessage(wLog* log, wLogCallbackAppender* appender, wLogMessage* message)
{
	char prefix[WLOG_MAX_PREFIX_SIZE];

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	if (appender->message)
	{
		appender->message(message);
	}
	else
	{
		return -1;
	}

	return 1;
}

int WLog_CallbackAppender_WriteDataMessage(wLog* log, wLogCallbackAppender* appender, wLogMessage* message)
{
	if (appender->data)
	{
		appender->data(message);
	}
	else
	{
		return -1;
	}

	return 1;
}

int WLog_CallbackAppender_WriteImageMessage(wLog* log, wLogCallbackAppender* appender, wLogMessage* message)
{
	if (appender->image)
	{
		appender->image(message);
	}
	else
	{
		return -1;
	}

	return 1;
}

int WLog_CallbackAppender_WritePacketMessage(wLog* log, wLogCallbackAppender* appender, wLogMessage* message)
{
	if (appender->package)
	{
		appender->package(message);
	}
	else
	{
		return -1;
	}

	return 1;
}

wLogCallbackAppender* WLog_CallbackAppender_New(wLog* log)
{
	wLogCallbackAppender* CallbackAppender;

	CallbackAppender = (wLogCallbackAppender*) calloc(1, sizeof(wLogCallbackAppender));
	if (!CallbackAppender)
		return NULL;

	CallbackAppender->Type = WLOG_APPENDER_CALLBACK;

	CallbackAppender->Open = (WLOG_APPENDER_OPEN_FN) WLog_CallbackAppender_Open;
	CallbackAppender->Close = (WLOG_APPENDER_OPEN_FN) WLog_CallbackAppender_Close;
	CallbackAppender->WriteMessage =
			(WLOG_APPENDER_WRITE_MESSAGE_FN) WLog_CallbackAppender_WriteMessage;
	CallbackAppender->WriteDataMessage =
			(WLOG_APPENDER_WRITE_DATA_MESSAGE_FN) WLog_CallbackAppender_WriteDataMessage;
	CallbackAppender->WriteImageMessage =
			(WLOG_APPENDER_WRITE_IMAGE_MESSAGE_FN) WLog_CallbackAppender_WriteImageMessage;
	CallbackAppender->WritePacketMessage =
			(WLOG_APPENDER_WRITE_PACKET_MESSAGE_FN) WLog_CallbackAppender_WritePacketMessage;

	return CallbackAppender;
}

void WLog_CallbackAppender_Free(wLog* log, wLogCallbackAppender* appender)
{
	free(appender);
}
