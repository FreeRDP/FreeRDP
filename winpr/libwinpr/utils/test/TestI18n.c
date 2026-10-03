/**
 * WinPR: Windows Portable Runtime
 * Internationalization tests
 *
 * Copyright 2026 Daniel Nylander <1206564+yeager@users.noreply.github.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include <string.h>

#ifdef WITH_WINPR_I18N
#include <locale.h>
#endif

#include <winpr/i18n.h>

int TestI18n(int argc, char* argv[])
{
	static const char msgid[] = "WinPR i18n test message";
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

#ifdef WITH_WINPR_I18N
	/* A translation locale must not change number formatting in the application. */
	char numericLocale[128] = { 0 };
	const char* numeric = setlocale(LC_NUMERIC, "C.UTF-8");
	if (numeric && (strlen(numeric) < sizeof(numericLocale)))
		memcpy(numericLocale, numeric, strlen(numeric) + 1);
#endif
	if (!winpr_i18n_enable_translation("C"))
		return -1;
#ifdef WITH_WINPR_I18N
	if (numericLocale[0] && (strcmp(setlocale(LC_NUMERIC, nullptr), numericLocale) != 0))
		return -1;
#endif
	if (winpr_i18n_bind_domain(nullptr, nullptr))
		return -1;
	if (!winpr_i18n_bind_domain("winpr-test", nullptr))
		return -1;
	const char* const result = winpr_i18n_dgettext("winpr-test", msgid);
	return result && (strcmp(result, msgid) == 0) ? 0 : -1;
}
