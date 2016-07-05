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

#include "CallbackAppender.h"


struct _wLogCallbackAppender
{
	WLOG_APPENDER_COMMON();

	wLogCallbacks *callbacks;
};
typedef struct _wLogCallbackAppender wLogCallbackAppender;

static BOOL WLog_CallbackAppender_Open(wLog* log, wLogAppender* appender)
{
	return TRUE;
}

static BOOL WLog_CallbackAppender_Close(wLog* log, wLogAppender* appender)
{
	return TRUE;
}

static BOOL WLog_CallbackAppender_WriteMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	char prefix[WLOG_MAX_PREFIX_SIZE];
	wLogCallbackAppender* callbackAppender;

	if (!appender)
		return FALSE;

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	callbackAppender = (wLogCallbackAppender *)appender;

	if (callbackAppender->callbacks && callbackAppender->callbacks->message)
		return callbackAppender->callbacks->message(message);
	else
		return FALSE;
}

static BOOL WLog_CallbackAppender_WriteDataMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	char prefix[WLOG_MAX_PREFIX_SIZE];
	wLogCallbackAppender* callbackAppender;

	if (!appender)
		return FALSE;

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	callbackAppender = (wLogCallbackAppender *)appender;
	if (callbackAppender->callbacks && callbackAppender->callbacks->data)
		return callbackAppender->callbacks->data(message);
	else
		return FALSE;
}

static BOOL WLog_CallbackAppender_WriteImageMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	char prefix[WLOG_MAX_PREFIX_SIZE];
	wLogCallbackAppender* callbackAppender;

	if (!appender)
		return FALSE;

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	callbackAppender = (wLogCallbackAppender *)appender;
	if (callbackAppender->callbacks && callbackAppender->callbacks->image)
		return callbackAppender->callbacks->image(message);
	else
		return FALSE;
}

static BOOL WLog_CallbackAppender_WritePacketMessage(wLog* log, wLogAppender* appender, wLogMessage* message)
{
	char prefix[WLOG_MAX_PREFIX_SIZE];
	wLogCallbackAppender* callbackAppender;

	if (!appender)
		return FALSE;

	message->PrefixString = prefix;
	WLog_Layout_GetMessagePrefix(log, appender->Layout, message);

	callbackAppender = (wLogCallbackAppender *)appender;
	if (callbackAppender->callbacks && callbackAppender->callbacks->package)
		return callbackAppender->callbacks->package(message);
	else
		return FALSE;
}

static BOOL WLog_CallbackAppender_Set(wLogAppender* appender, const char *setting, void *value)
{
	wLogCallbackAppender *callbackAppender = (wLogCallbackAppender *)appender;

	if (!value || strcmp(setting, "callbacks"))
		return FALSE;

	if (!(callbackAppender->callbacks = calloc(1, sizeof(wLogCallbacks)))) {
		return FALSE;
	}

	callbackAppender->callbacks = memcpy(callbackAppender->callbacks, value, sizeof(wLogCallbacks));
	return TRUE;
}

static void WLog_CallbackAppender_Free(wLogAppender* appender)
{
	wLogCallbackAppender *callbackAppender;
	if (!appender) {
		return;
	}

	callbackAppender = (wLogCallbackAppender *)appender;

	free(callbackAppender->callbacks);
	free(appender);
}

wLogAppender* WLog_CallbackAppender_New(wLog* log)
{
	wLogCallbackAppender* CallbackAppender;

	CallbackAppender = (wLogCallbackAppender*) calloc(1, sizeof(wLogCallbackAppender));
	if (!CallbackAppender)
		return NULL;

	CallbackAppender->Type = WLOG_APPENDER_CALLBACK;

	CallbackAppender->Open = WLog_CallbackAppender_Open;
	CallbackAppender->Close = WLog_CallbackAppender_Close;
	CallbackAppender->WriteMessage = WLog_CallbackAppender_WriteMessage;
	CallbackAppender->WriteDataMessage = WLog_CallbackAppender_WriteDataMessage;
	CallbackAppender->WriteImageMessage = WLog_CallbackAppender_WriteImageMessage;
	CallbackAppender->WritePacketMessage = WLog_CallbackAppender_WritePacketMessage;
	CallbackAppender->Free = WLog_CallbackAppender_Free;
	CallbackAppender->Set = WLog_CallbackAppender_Set;

	return (wLogAppender *)CallbackAppender;
}
