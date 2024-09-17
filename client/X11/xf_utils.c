/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * X11 helper utilities
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
 * Copyringht 2023 Thincast Technologies GmbH
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

#include <string.h>
#include <winpr/assert.h>
#include <winpr/wtypes.h>
#include <winpr/path.h>

#include "xf_utils.h"
#include "xfreerdp.h"

#include <freerdp/log.h>

#define TAG CLIENT_TAG("xfreerdp.utils")

static const DWORD log_level = WLOG_TRACE;

static void write_log(wLog* log, DWORD level, const char* fname, const char* fkt, size_t line, ...)
{
	va_list ap = { 0 };
	va_start(ap, line);
	WLog_PrintMessageVA(log, WLOG_MESSAGE_TEXT, level, line, fname, fkt, ap);
	va_end(ap);
}

char* Safe_XGetAtomNameEx(wLog* log, Display* display, Atom atom, const char* varname)
{
	WLog_Print(log, log_level, "XGetAtomName(%s, 0x%08" PRIx32 ")", varname, atom);
	if (atom == None)
		return strdup("Atom_None");
	return XGetAtomName(display, atom);
}

Atom Logging_XInternAtom(wLog* log, Display* display, _Xconst char* atom_name, Bool only_if_exists)
{
	Atom atom = XInternAtom(display, atom_name, only_if_exists);
	if (WLog_IsLevelActive(log, log_level))
	{
		WLog_Print(log, log_level, "XInternAtom(0x%08" PRIx32 ", %s, %s) -> 0x%08" PRIx32, display,
		           atom_name, only_if_exists ? "True" : "False", atom);
	}
	return atom;
}

int LogTagAndXChangeProperty_ex(const char* tag, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property, Atom type, int format,
                                int mode, const unsigned char* data, int nelements)
{
	wLog* log = WLog_Get(tag);
	return LogDynAndXChangeProperty_ex(log, file, fkt, line, display, w, property, type, format,
	                                   mode, data, nelements);
}

int LogDynAndXChangeProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property, Atom type, int format,
                                int mode, const unsigned char* data, int nelements)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		char* propstr = Safe_XGetAtomName(log, display, property);
		char* typestr = Safe_XGetAtomName(log, display, type);
		write_log(log, log_level, file, fkt, line,
		          "XChangeProperty(%p, %d, %s [%d], %s [%d], %d, %d, %p, %d)", display, w, propstr,
		          property, typestr, type, format, mode, data, nelements);
		XFree(propstr);
		XFree(typestr);
	}
	return XChangeProperty(display, w, property, type, format, mode, data, nelements);
}

int LogTagAndXDeleteProperty_ex(const char* tag, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property)
{
	wLog* log = WLog_Get(tag);
	return LogDynAndXDeleteProperty_ex(log, file, fkt, line, display, w, property);
}

int LogDynAndXDeleteProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                Display* display, Window w, Atom property)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		char* propstr = Safe_XGetAtomName(log, display, property);
		write_log(log, log_level, file, fkt, line, "XDeleteProperty(%p, %d, %s [%d])", display, w,
		          propstr, property);
		XFree(propstr);
	}
	return XDeleteProperty(display, w, property);
}

int LogTagAndXConvertSelection_ex(const char* tag, const char* file, const char* fkt, size_t line,
                                  Display* display, Atom selection, Atom target, Atom property,
                                  Window requestor, Time time)
{
	wLog* log = WLog_Get(tag);
	return LogDynAndXConvertSelection_ex(log, file, fkt, line, display, selection, target, property,
	                                     requestor, time);
}

int LogDynAndXConvertSelection_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                  Display* display, Atom selection, Atom target, Atom property,
                                  Window requestor, Time time)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		char* selectstr = Safe_XGetAtomName(log, display, selection);
		char* targetstr = Safe_XGetAtomName(log, display, target);
		char* propstr = Safe_XGetAtomName(log, display, property);
		write_log(log, log_level, file, fkt, line,
		          "XConvertSelection(%p, %s [%d], %s [%d], %s [%d], %d, %lu)", display, selectstr,
		          selection, targetstr, target, propstr, property, requestor, time);
		XFree(propstr);
		XFree(targetstr);
		XFree(selectstr);
	}
	return XConvertSelection(display, selection, target, property, requestor, time);
}

int LogTagAndXGetWindowProperty_ex(const char* tag, const char* file, const char* fkt, size_t line,
                                   Display* display, Window w, Atom property, long long_offset,
                                   long long_length, int delete, Atom req_type,
                                   Atom* actual_type_return, int* actual_format_return,
                                   unsigned long* nitems_return, unsigned long* bytes_after_return,
                                   unsigned char** prop_return)
{
	wLog* log = WLog_Get(tag);
	return LogDynAndXGetWindowProperty_ex(
	    log, file, fkt, line, display, w, property, long_offset, long_length, delete, req_type,
	    actual_type_return, actual_format_return, nitems_return, bytes_after_return, prop_return);
}

int LogDynAndXGetWindowProperty_ex(wLog* log, const char* file, const char* fkt, size_t line,
                                   Display* display, Window w, Atom property, long long_offset,
                                   long long_length, int delete, Atom req_type,
                                   Atom* actual_type_return, int* actual_format_return,
                                   unsigned long* nitems_return, unsigned long* bytes_after_return,
                                   unsigned char** prop_return)
{
	if (WLog_IsLevelActive(log, log_level))
	{
		char* propstr = Safe_XGetAtomName(log, display, property);
		char* req_type_str = Safe_XGetAtomName(log, display, req_type);
		write_log(log, log_level, file, fkt, line,
		          "XGetWindowProperty(%p, %d, %s [%d], %ld, %ld, %d, %s [%d], %p, %p, %p, %p, %p)",
		          display, w, propstr, property, long_offset, long_length, delete, req_type_str,
		          req_type, actual_type_return, actual_format_return, nitems_return,
		          bytes_after_return, prop_return);
		XFree(propstr);
		XFree(req_type_str);
	}
	return XGetWindowProperty(display, w, property, long_offset, long_length, delete, req_type,
	                          actual_type_return, actual_format_return, nitems_return,
	                          bytes_after_return, prop_return);
}

BOOL IsGnome(void)
{
	char* env = getenv("DESKTOP_SESSION");
	return (env != NULL && strcmp(env, "gnome") == 0);
}

BOOL run_action_script(xfContext* xfc, const char* what, const char* arg, fn_action_script_run fkt,
                       void* user)
{
	BOOL rc = FALSE;
	FILE* keyScript = NULL;
	WINPR_ASSERT(xfc);

	rdpSettings* settings = xfc->common.context.settings;
	WINPR_ASSERT(settings);

	const char* ActionScript = freerdp_settings_get_string(settings, FreeRDP_ActionScript);

	xfc->actionScriptExists = winpr_PathFileExists(ActionScript);

	if (!xfc->actionScriptExists)
		goto fail;

	char command[2048] = { 0 };
	(void)sprintf_s(command, sizeof(command), "%s %s", ActionScript, what);
	keyScript = popen(command, "r");

	if (!keyScript)
	{
		WLog_ERR(TAG, "Failed to execute '%s'", command);
		goto fail;
	}

	BOOL read_data = FALSE;
	char buffer[2048] = { 0 };
	while (fgets(buffer, sizeof(buffer), keyScript) != NULL)
	{
		char* context = NULL;
		(void)strtok_s(buffer, "\n", &context);

		if (fkt)
		{
			if (!fkt(xfc, buffer, strnlen(buffer, sizeof(buffer)), user, what, arg))
				goto fail;
		}
		read_data = TRUE;
	}

	rc = read_data;
fail:
	if (!rc)
		xfc->actionScriptExists = FALSE;
	if (keyScript)
		pclose(keyScript);
	return rc;
}

const char* x11_error_to_string(xfContext* xfc, int error, char* buffer, size_t size)
{
	WINPR_ASSERT(xfc);
	WINPR_ASSERT(size <= INT32_MAX);
	const int rc = XGetErrorText(xfc->display, error, buffer, (int)size);
	if (rc)
		WLog_WARN(TAG, "XGetErrorText returned %d", rc);
	return buffer;
}
