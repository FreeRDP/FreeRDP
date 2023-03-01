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

#include "xf_utils.h"

int LogTagAndXChangeProperty(const char* tag, Display* display, Window w, Atom property, Atom type,
                             int format, int mode, const unsigned char* data, int nelements)
{
	wLog* log = WLog_Get(tag);
	return LogDynAndXChangeProperty(log, display, w, property, type, format, mode, data, nelements);
}

int LogDynAndXChangeProperty(wLog* log, Display* display, Window w, Atom property, Atom type,
                             int format, int mode, const unsigned char* data, int nelements)
{
	const DWORD level = WLOG_TRACE;

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
