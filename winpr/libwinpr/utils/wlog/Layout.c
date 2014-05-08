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
#include <stdarg.h>

#include <winpr/crt.h>
#include <winpr/print.h>

#include <winpr/wlog.h>

#include "wlog/Layout.h"

extern const char* WLOG_LEVELS[7];

/**
 * Log Layout
 */

void WLog_PrintMessagePrefixVA(wLog* log, wLogMessage* message, const char* format, va_list args)
{
	if (!strchr(format, '%'))
	{
		message->PrefixString = (LPSTR) format;
	}
	else
	{
		wvsnprintfx(message->PrefixString, WLOG_MAX_PREFIX_SIZE - 1, format, args);
	}
}

void WLog_PrintMessagePrefix(wLog* log, wLogMessage* message, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	WLog_PrintMessagePrefixVA(log, message, format, args);
	va_end(args);
}

void WLog_Layout_GetMessagePrefix(wLog* log, wLogLayout* layout, wLogMessage* message)
{
	char* p;
	int index;
	int argc = 0;
	void* args[32];
	char format[128];

	index = 0;
	p = (char*) layout->FormatString;

	while (*p)
	{
		if (*p == '%')
		{
			p++;

			if (*p)
			{
				if ((*p == 'l') && (*(p + 1) == 'v')) /* log level */
				{
					args[argc++] = (void*) WLOG_LEVELS[message->Level];
					format[index++] = '%';
					format[index++] = 's';
					p++;
				}
				else if ((*p == 'm') && (*(p + 1) == 'n')) /* module name */
				{
					args[argc++] = (void*) log->Name;
					format[index++] = '%';
					format[index++] = 's';
					p++;
				}
				else if ((*p == 'f') && (*(p + 1) == 'l')) /* file */
				{
					char* file;

					file = strrchr(message->FileName, '/');

					if (!file)
						file = strrchr(message->FileName, '\\');

					if (file)
						file++;
					else
						file = (char*) message->FileName;

					args[argc++] = (void*) file;
					format[index++] = '%';
					format[index++] = 's';
					p++;
				}
				else if ((*p == 'f') && (*(p + 1) == 'n')) /* function */
				{
					args[argc++] = (void*) message->FunctionName;
					format[index++] = '%';
					format[index++] = 's';
					p++;
				}
				else if ((*p == 'l') && (*(p + 1) == 'n')) /* line number */
				{
					args[argc++] = (void*) (size_t) message->LineNumber;
					format[index++] = '%';
					format[index++] = 'd';
					p++;
				}
			}
		}
		else
		{
			format[index++] = *p;
		}

		p++;
	}

	format[index++] = '\0';

	switch (argc)
	{
		case 0:
			WLog_PrintMessagePrefix(log, message, format);
			break;

		case 1:
			WLog_PrintMessagePrefix(log, message, format, args[0]);
			break;

		case 2:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1]);
			break;

		case 3:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2]);
			break;

		case 4:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3]);
			break;

		case 5:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4]);
			break;

		case 6:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5]);
			break;

		case 7:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6]);
			break;

		case 8:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7]);
			break;

		case 9:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8]);
			break;

		case 10:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9]);
			break;

		case 11:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10]);
			break;

		case 12:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10],
					args[11]);
			break;

		case 13:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10],
					args[11], args[12]);
			break;

		case 14:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10],
					args[11], args[12], args[13]);
			break;

		case 15:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10],
					args[11], args[12], args[13], args[14]);
			break;

		case 16:
			WLog_PrintMessagePrefix(log, message, format, args[0], args[1], args[2], args[3],
					args[4], args[5], args[6], args[7], args[8], args[9], args[10],
					args[11], args[12], args[13], args[14], args[15]);
			break;
	}
}

wLogLayout* WLog_GetLogLayout(wLog* log)
{
	wLogAppender* appender;

	appender = WLog_GetLogAppender(log);

	return appender->Layout;
}

void WLog_Layout_SetPrefixFormat(wLog* log, wLogLayout* layout, const char* format)
{
	if (layout->FormatString)
		free(layout->FormatString);

	layout->FormatString = _strdup(format);
}

wLogLayout* WLog_Layout_New(wLog* log)
{
	wLogLayout* layout;

	layout = (wLogLayout*) malloc(sizeof(wLogLayout));

	if (layout)
	{
		ZeroMemory(layout, sizeof(wLogLayout));

		layout->FormatString = _strdup("[%lv][%mn] - ");
	}

	return layout;
}

void WLog_Layout_Free(wLog* log, wLogLayout* layout)
{
	if (layout)
	{
		if (layout->FormatString)
			free(layout->FormatString);

		free(layout);
	}
}
