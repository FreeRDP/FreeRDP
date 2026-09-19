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

#include <winpr/i18n.h>

int TestI18n(int argc, char* argv[])
{
	static const char msgid[] = "WinPR i18n test message";
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	if (!winpr_i18n_enable_translation("C"))
		return -1;
	if (winpr_i18n_bind_domain(nullptr, nullptr))
		return -1;
	if (!winpr_i18n_bind_domain("winpr-test", nullptr))
		return -1;
	const char* const result = winpr_i18n_dgettext("winpr-test", msgid);
	return result && (strcmp(result, msgid) == 0) ? 0 : -1;
}
