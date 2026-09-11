/**
 * WinPR: Windows Portable Runtime
 * Internationalization helpers
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#ifndef WINPR_I18N_H
#define WINPR_I18N_H

#include <winpr/wtypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/**
	 * Initialize the process locale used for translations.
	 *
	 * @param locale Locale name, or NULL/empty to use the environment locale.
	 * @return TRUE. Builds without gettext support retain source strings.
	 */
	WINPR_API BOOL winpr_i18n_enable_translation(const char* locale);

	/**
	 * Register a translation domain and its catalog search directory.
	 *
	 * A module owns its domain; this avoids a shared FreeRDP catalog and lets
	 * applications choose an installation-specific locale directory.
	 */
	WINPR_API BOOL winpr_i18n_bind_domain(const char* domain, const char* searchPath);

	/** @return The translated message for @p domain, or @p msgid as a fallback. */
	WINPR_API const char* winpr_i18n_dgettext(const char* domain, const char* msgid);

#define WINPR_I18N_GETTEXT(domain, msgid) winpr_i18n_dgettext((domain), (msgid))

#ifdef __cplusplus
}
#endif

#endif /* WINPR_I18N_H */
