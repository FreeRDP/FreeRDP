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

#include <winpr/assert.h>

#include "xf_utils.h"

static const DWORD level = WLOG_TRACE;

int LogTagAndXChangeProperty(const char* tag, Display* display, Window w, Atom property, Atom type,
                             int format, int mode, const unsigned char* data, int nelements)
{
	wLog* log = WLog_Get(tag);
	return LogDynAndXChangeProperty(log, display, w, property, type, format, mode, data, nelements);
}

int LogDynAndXChangeProperty(wLog* log, Display* display, Window w, Atom property, Atom type,
                             int format, int mode, const unsigned char* data, int nelements)
{
	if (WLog_IsLevelActive(log, level))
	{
		char* propstr = XGetAtomName(display, property);
		char* typestr = XGetAtomName(display, type);
		WLog_Print(log, WLOG_DEBUG, "XChangeProperty(%p, %d, %s [%d], %s [%d], %d, %d, %p, %d)",
		           display, w, propstr, property, typestr, type, format, mode, data, nelements);
		XFree(propstr);
		XFree(typestr);
	}
	return XChangeProperty(display, w, property, type, format, mode, data, nelements);
}

int LogTagAndXDeleteProperty(const char* tag, Display* display, Window w, Atom property)
{
	wLog* log = WLog_Get(tag);
	return LogDynAndXDeleteProperty(log, display, w, property);
}

int LogDynAndXDeleteProperty(wLog* log, Display* display, Window w, Atom property)
{
	if (WLog_IsLevelActive(log, level))
	{
		char* propstr = XGetAtomName(display, property);
		WLog_Print(log, WLOG_DEBUG, "XDeleteProperty(%p, %d, %s [%d])", display, w, propstr,
		           property);
		XFree(propstr);
	}
	return XDeleteProperty(display, w, property);
}

int LogTagAndXGetWindowProperty(const char* tag, Display* display, Window w, Atom property,
                                long long_offset, long long_length, int delete, Atom req_type,
                                Atom* actual_type_return, int* actual_format_return,
                                unsigned long* nitems_return, unsigned long* bytes_after_return,
                                unsigned char** prop_return)
{
	wLog* log = WLog_Get(tag);
	return LogDynAndXGetWindowProperty(log, display, w, property, long_offset, long_length, delete,
	                                   req_type, actual_type_return, actual_format_return,
	                                   nitems_return, bytes_after_return, prop_return);
}

int LogDynAndXGetWindowProperty(wLog* log, Display* display, Window w, Atom property,
                                long long_offset, long long_length, int delete, Atom req_type,
                                Atom* actual_type_return, int* actual_format_return,
                                unsigned long* nitems_return, unsigned long* bytes_after_return,
                                unsigned char** prop_return)
{
	if (WLog_IsLevelActive(log, level))
	{
		char* propstr = XGetAtomName(display, property);
		char* req_type_str = XGetAtomName(display, req_type);
		WLog_Print(log, WLOG_DEBUG,
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
