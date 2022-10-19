/**
 * WinPR: Windows Portable Runtime
 * OpenSSL Library Initialization
 *
 * Copyright 2014 Thincast Technologies GmbH
 * Copyright 2014 Norbert Federa <norbert.federa@thincast.com>
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

#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/synch.h>
#include <winpr/ssl.h>
#include <winpr/thread.h>
#include <winpr/crypto.h>

#ifdef WITH_OPENSSL

#include <openssl/ssl.h>
#include <openssl/err.h>

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
#include <openssl/provider.h>
#endif

#include "../log.h"
#define TAG WINPR_TAG("utils.ssl")

static BOOL g_winpr_openssl_initialized_by_winpr = FALSE;

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
static OSSL_PROVIDER* s_winpr_openssl_provider_fips = NULL;
static OSSL_PROVIDER* s_winpr_openssl_provider_legacy = NULL;
static OSSL_PROVIDER* s_winpr_openssl_provider_default = NULL;
#endif

/**
 * Note from OpenSSL 1.1.0 "CHANGES":
 * OpenSSL now uses a new threading API. It is no longer necessary to
 * set locking callbacks to use OpenSSL in a multi-threaded environment.
 */

#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)

#define WINPR_OPENSSL_LOCKING_REQUIRED 1

static int g_winpr_openssl_num_locks = 0;
static HANDLE* g_winpr_openssl_locks = NULL;

struct CRYPTO_dynlock_value
{
	HANDLE mutex;
};

#if (OPENSSL_VERSION_NUMBER < 0x10000000L) || defined(LIBRESSL_VERSION_NUMBER)
static unsigned long _winpr_openssl_id(void)
{
	return (unsigned long)GetCurrentThreadId();
}
#endif

static void _winpr_openssl_locking(int mode, int type, const char* file, int line)
{
	if (mode & CRYPTO_LOCK)
	{
		WaitForSingleObject(g_winpr_openssl_locks[type], INFINITE);
	}
	else
	{
		ReleaseMutex(g_winpr_openssl_locks[type]);
	}
}

static struct CRYPTO_dynlock_value* _winpr_openssl_dynlock_create(const char* file, int line)
{
	struct CRYPTO_dynlock_value* dynlock;

	if (!(dynlock = (struct CRYPTO_dynlock_value*)malloc(sizeof(struct CRYPTO_dynlock_value))))
		return NULL;

	if (!(dynlock->mutex = CreateMutex(NULL, FALSE, NULL)))
	{
		free(dynlock);
		return NULL;
	}

	return dynlock;
}

static void _winpr_openssl_dynlock_lock(int mode, struct CRYPTO_dynlock_value* dynlock,
                                        const char* file, int line)
{
	if (mode & CRYPTO_LOCK)
	{
		WaitForSingleObject(dynlock->mutex, INFINITE);
	}
	else
	{
		ReleaseMutex(dynlock->mutex);
	}
}

static void _winpr_openssl_dynlock_destroy(struct CRYPTO_dynlock_value* dynlock, const char* file,
                                           int line)
{
	CloseHandle(dynlock->mutex);
	free(dynlock);
}

static BOOL _winpr_openssl_initialize_locking(void)
{
	int i, count;

	/* OpenSSL static locking */

	if (CRYPTO_get_locking_callback())
	{
		WLog_WARN(TAG, "OpenSSL static locking callback is already set");
	}
	else
	{
		if ((count = CRYPTO_num_locks()) > 0)
		{
			HANDLE* locks;

			if (!(locks = calloc(count, sizeof(HANDLE))))
			{
				WLog_ERR(TAG, "error allocating lock table");
				return FALSE;
			}

			for (i = 0; i < count; i++)
			{
				if (!(locks[i] = CreateMutex(NULL, FALSE, NULL)))
				{
					WLog_ERR(TAG, "error creating lock #%d", i);

					while (i--)
					{
						if (locks[i])
							CloseHandle(locks[i]);
					}

					free(locks);
					return FALSE;
				}
			}

			g_winpr_openssl_locks = locks;
			g_winpr_openssl_num_locks = count;
			CRYPTO_set_locking_callback(_winpr_openssl_locking);
		}
	}

	/* OpenSSL dynamic locking */

	if (CRYPTO_get_dynlock_create_callback() || CRYPTO_get_dynlock_lock_callback() ||
	    CRYPTO_get_dynlock_destroy_callback())
	{
		WLog_WARN(TAG, "dynamic locking callbacks are already set");
	}
	else
	{
		CRYPTO_set_dynlock_create_callback(_winpr_openssl_dynlock_create);
		CRYPTO_set_dynlock_lock_callback(_winpr_openssl_dynlock_lock);
		CRYPTO_set_dynlock_destroy_callback(_winpr_openssl_dynlock_destroy);
	}

	/* Use the deprecated CRYPTO_get_id_callback() if building against OpenSSL < 1.0.0 */
#if (OPENSSL_VERSION_NUMBER < 0x10000000L) || defined(LIBRESSL_VERSION_NUMBER)

	if (CRYPTO_get_id_callback())
	{
		WLog_WARN(TAG, "OpenSSL id_callback is already set");
	}
	else
	{
		CRYPTO_set_id_callback(_winpr_openssl_id);
	}

#endif
	return TRUE;
}

static BOOL _winpr_openssl_cleanup_locking(void)
{
	/* undo our static locking modifications */
	if (CRYPTO_get_locking_callback() == _winpr_openssl_locking)
	{
		int i;
		CRYPTO_set_locking_callback(NULL);

		for (i = 0; i < g_winpr_openssl_num_locks; i++)
		{
			CloseHandle(g_winpr_openssl_locks[i]);
		}

		g_winpr_openssl_num_locks = 0;
		free(g_winpr_openssl_locks);
		g_winpr_openssl_locks = NULL;
	}

	/* unset our dynamic locking callbacks */

	if (CRYPTO_get_dynlock_create_callback() == _winpr_openssl_dynlock_create)
	{
		CRYPTO_set_dynlock_create_callback(NULL);
	}

	if (CRYPTO_get_dynlock_lock_callback() == _winpr_openssl_dynlock_lock)
	{
		CRYPTO_set_dynlock_lock_callback(NULL);
	}

	if (CRYPTO_get_dynlock_destroy_callback() == _winpr_openssl_dynlock_destroy)
	{
		CRYPTO_set_dynlock_destroy_callback(NULL);
	}

#if (OPENSSL_VERSION_NUMBER < 0x10000000L) || defined(LIBRESSL_VERSION_NUMBER)

	if (CRYPTO_get_id_callback() == _winpr_openssl_id)
	{
		CRYPTO_set_id_callback(NULL);
	}

#endif
	return TRUE;
}

#endif /* OpenSSL < 1.1.0 */

static BOOL winpr_enable_fips(DWORD flags)
{
	if (flags & WINPR_SSL_INIT_ENABLE_FIPS)
	{
#if (OPENSSL_VERSION_NUMBER < 0x10001000L) || defined(LIBRESSL_VERSION_NUMBER)
		WLog_ERR(TAG, "Openssl fips mode not available on openssl versions less than 1.0.1!");
		return FALSE;
#else
		WLog_DBG(TAG, "Ensuring openssl fips mode is enabled");

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
		s_winpr_openssl_provider_fips = OSSL_PROVIDER_load(NULL, "fips");
		if (s_winpr_openssl_provider_fips == NULL)
		{
			WLog_WARN(TAG, "OpenSSL FIPS provider failled to load");
		}
		if (!EVP_default_properties_is_fips_enabled(NULL))
#else
		if (FIPS_mode() != 1)
#endif
		{
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
			if (EVP_set_default_properties(NULL, "fips=yes"))
#else
			if (FIPS_mode_set(1))
#endif
				WLog_INFO(TAG, "Openssl fips mode enabled!");
			else
			{
				WLog_ERR(TAG, "Openssl fips mode enable failed!");
				return FALSE;
			}
		}

#endif
	}

	return TRUE;
}

static BOOL CALLBACK _winpr_openssl_initialize(PINIT_ONCE once, PVOID param, PVOID* context)
{
	DWORD flags = param ? *(PDWORD)param : WINPR_SSL_INIT_DEFAULT;

	if (flags & WINPR_SSL_INIT_ALREADY_INITIALIZED)
	{
		return TRUE;
	}

#ifdef WINPR_OPENSSL_LOCKING_REQUIRED

	if (flags & WINPR_SSL_INIT_ENABLE_LOCKING)
	{
		if (!_winpr_openssl_initialize_locking())
		{
			return FALSE;
		}
	}

#endif
	/* SSL_load_error_strings() is void */
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
	SSL_load_error_strings();
	/* SSL_library_init() always returns "1" */
	SSL_library_init();
	OpenSSL_add_all_digests();
	OpenSSL_add_all_ciphers();
#else

	if (OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS |
	                         OPENSSL_INIT_ADD_ALL_CIPHERS | OPENSSL_INIT_ADD_ALL_DIGESTS |
	                         OPENSSL_INIT_ENGINE_ALL_BUILTIN,
	                     NULL) != 1)
		return FALSE;

#endif

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
	/* The legacy provider is needed for MD4. */
	s_winpr_openssl_provider_legacy = OSSL_PROVIDER_load(NULL, "legacy");
	if (s_winpr_openssl_provider_legacy == NULL)
	{
		WLog_WARN(TAG, "OpenSSL LEGACY provider failed to load, no md4 support available!");
	}
	s_winpr_openssl_provider_default = OSSL_PROVIDER_load(NULL, "default");
	if (s_winpr_openssl_provider_default == NULL)
	{
		WLog_WARN(TAG, "OpenSSL DEFAULT provider failed to load");
	}
#endif

	g_winpr_openssl_initialized_by_winpr = TRUE;
	return TRUE;
}

/* exported functions */

BOOL winpr_InitializeSSL(DWORD flags)
{
	static INIT_ONCE once = INIT_ONCE_STATIC_INIT;

	if (!InitOnceExecuteOnce(&once, _winpr_openssl_initialize, &flags, NULL))
		return FALSE;

	return winpr_enable_fips(flags);
}

BOOL winpr_CleanupSSL(DWORD flags)
{
	if (flags & WINPR_SSL_CLEANUP_GLOBAL)
	{
		if (!g_winpr_openssl_initialized_by_winpr)
		{
			WLog_WARN(TAG, "ssl was not initialized by winpr");
			return FALSE;
		}

		g_winpr_openssl_initialized_by_winpr = FALSE;
#ifdef WINPR_OPENSSL_LOCKING_REQUIRED
		_winpr_openssl_cleanup_locking();
#endif
#if (OPENSSL_VERSION_NUMBER < 0x10100000L) || defined(LIBRESSL_VERSION_NUMBER)
		CRYPTO_cleanup_all_ex_data();
		ERR_free_strings();
		EVP_cleanup();
#endif
#ifdef WINPR_OPENSSL_LOCKING_REQUIRED
		flags |= WINPR_SSL_CLEANUP_THREAD;
#endif
	}

#ifdef WINPR_OPENSSL_LOCKING_REQUIRED

	if (flags & WINPR_SSL_CLEANUP_THREAD)
	{
#if (OPENSSL_VERSION_NUMBER < 0x10000000L) || defined(LIBRESSL_VERSION_NUMBER)
		ERR_remove_state(0);
#else
		ERR_remove_thread_state(NULL);
#endif
	}

#endif
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
	OSSL_PROVIDER_unload(s_winpr_openssl_provider_fips);
	OSSL_PROVIDER_unload(s_winpr_openssl_provider_legacy);
	OSSL_PROVIDER_unload(s_winpr_openssl_provider_default);
#endif
	return TRUE;
}

BOOL winpr_FIPSMode(void)
{
#if (OPENSSL_VERSION_NUMBER < 0x10001000L) || defined(LIBRESSL_VERSION_NUMBER)
	return FALSE;
#elif defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
	return (EVP_default_properties_is_fips_enabled(NULL) == 1);
#else
	return (FIPS_mode() == 1);
#endif
}

#else

BOOL winpr_InitializeSSL(DWORD flags)
{
	return TRUE;
}

BOOL winpr_CleanupSSL(DWORD flags)
{
	return TRUE;
}

BOOL winpr_FIPSMode(void)
{
	return FALSE;
}

#endif
