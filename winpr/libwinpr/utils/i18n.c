/**
 * WinPR: Windows Portable Runtime
 * Internationalization helpers
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include <winpr/i18n.h>

#ifdef WITH_WINPR_I18N
#include <libintl.h>
#include <locale.h>
#endif

BOOL winpr_i18n_enable_translation(const char* locale)
{
#ifdef WITH_WINPR_I18N
	const char* const requested = (locale && (locale[0] != '\0')) ? locale : "";
	return setlocale(LC_ALL, requested) ? TRUE : FALSE;
#else
	WINPR_UNUSED(locale);
	return TRUE;
#endif
}

BOOL winpr_i18n_bind_domain(const char* domain, const char* searchPath)
{
	if (!domain || (domain[0] == '\0'))
		return FALSE;

#ifdef WITH_WINPR_I18N
	if (searchPath && (searchPath[0] != '\0') && !bindtextdomain(domain, searchPath))
		return FALSE;
	return bind_textdomain_codeset(domain, "UTF-8") ? TRUE : FALSE;
#else
	WINPR_UNUSED(searchPath);
	return TRUE;
#endif
}

const char* winpr_i18n_dgettext(const char* domain, const char* msgid)
{
	if (!msgid)
		return nullptr;

#ifdef WITH_WINPR_I18N
	if (domain && (domain[0] != '\0'))
		return dgettext(domain, msgid);
#else
	WINPR_UNUSED(domain);
#endif
	return msgid;
}
