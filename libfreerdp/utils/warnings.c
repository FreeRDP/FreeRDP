/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Warning log functions
 *
 * Copyright 2026 Armin Novak <anovak@thincast.com>
 * Copyright 2026 Thincast Technologies GmbH
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

#include <winpr/string.h>
#include <freerdp/utils/warnings.h>

static const DWORD level = WLOG_WARN;

static void warn_contact_details(wLog* log)
{
	WLog_Print_unchecked(
	    log, level,
	    " If problems occur please check https://github.com/FreeRDP/FreeRDP/issues for "
	    "known issues!");
	WLog_Print_unchecked(
	    log, level,
	    "Be prepared to fix issues yourself though as nobody is actively working on this.");
	WLog_Print_unchecked(
	    log, level,
	    " Developers hang out in https://matrix.to/#/#FreeRDP:matrix.org?via=matrix.org "
	    "- don't hesitate to ask some questions. (replies might take some time depending "
	    "on your timezone) - if you intend using this component write us a message");
}

void freerdp_warn_unmaintained(wLog* log, const char* what, ...)
{
	if (!WLog_IsLevelActive(log, level))
		return;

	char* str = nullptr;
	size_t slen = 0;
	va_list ap;
	va_start(ap, what);
	winpr_vasprintf(&str, &slen, what, ap);
	va_end(ap);

	WLog_Print_unchecked(log, level, "[unmaintained] %s is currently unmaintained!", str);
	free(str);
	warn_contact_details(log);
}

void freerdp_warn_experimental(wLog* log, const char* what, ...)
{
	if (!WLog_IsLevelActive(log, level))
		return;

	char* str = nullptr;
	size_t slen = 0;
	va_list ap;
	va_start(ap, what);
	winpr_vasprintf(&str, &slen, what, ap);
	va_end(ap);

	WLog_Print_unchecked(log, level, "[experimental] %s is currently experimental!", str);
	free(str);
	warn_contact_details(log);
}

void freerdp_warn_deprecated(wLog* log, const char* what, const char* replacement, ...)
{
	if (!WLog_IsLevelActive(log, level))
		return;

	char* str = nullptr;
	size_t slen = 0;
	va_list ap;
	va_start(ap, replacement);
	winpr_vasprintf(&str, &slen, what, ap);
	va_end(ap);

	WLog_Print_unchecked(log, level, "[deprecated] %s has been deprecated", str);
	if (replacement)
		WLog_Print_unchecked(log, level, "%s", replacement);
	WLog_Print_unchecked(
	    log, level, "If you are interested in keeping %s alive get in touch with the developers",
	    str);

	free(str);
	warn_contact_details(log);
}
