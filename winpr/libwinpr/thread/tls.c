/**
 * WinPR: Windows Portable Runtime
 * Process Thread Functions
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include <winpr/handle.h>

#include <winpr/thread.h>

/**
 * TlsAlloc
 * TlsFree
 * TlsGetValue
 * TlsSetValue
 */

#ifndef _WIN32

#include <pthread.h>

struct tls_map_t
{
	pthread_key_t pkey;
};

static struct tls_map_t tls_map[PTHREAD_KEYS_MAX] = WINPR_C_ARRAY_INIT;
static pthread_mutex_t tls_lock = PTHREAD_MUTEX_INITIALIZER;

static DWORD map_pthread(pthread_key_t pkey)
{
	pthread_mutex_lock(&tls_lock);
	for (size_t x = 0; x < ARRAYSIZE(tls_map); x++)
	{
		struct tls_map_t* cur = &tls_map[x];
		if (cur->pkey == 0)
		{
			cur->pkey = pkey;
			pthread_mutex_unlock(&tls_lock);
			return WINPR_ASSERTING_INT_CAST(DWORD, x);
		}
	}
	pthread_mutex_unlock(&tls_lock);
	return TLS_OUT_OF_INDEXES;
}

static BOOL is_valid(DWORD index)
{
	if (index >= ARRAYSIZE(tls_map))
		return FALSE;
	return tls_map[index].pkey != 0;
}

static pthread_key_t map_dword(DWORD index)
{
	if (index >= ARRAYSIZE(tls_map))
		return TLS_OUT_OF_INDEXES;
	return tls_map[index].pkey;
}

DWORD TlsAlloc(void)
{
	pthread_key_t key = 0;

	if (pthread_key_create(&key, nullptr) != 0)
		return TLS_OUT_OF_INDEXES;
	return map_pthread(key);
}

LPVOID TlsGetValue(DWORD dwTlsIndex)
{
	pthread_mutex_lock(&tls_lock);
	if (!is_valid(dwTlsIndex))
	{
		pthread_mutex_unlock(&tls_lock);
		return nullptr;
	}
	pthread_key_t key = map_dword(dwTlsIndex);
	pthread_mutex_unlock(&tls_lock);
	return (LPVOID)pthread_getspecific(key);
}

BOOL TlsSetValue(DWORD dwTlsIndex, LPVOID lpTlsValue)
{
	pthread_mutex_lock(&tls_lock);
	if (!is_valid(dwTlsIndex))
	{
		pthread_mutex_unlock(&tls_lock);
		return FALSE;
	}
	pthread_key_t key = map_dword(dwTlsIndex);
	pthread_mutex_unlock(&tls_lock);
	pthread_setspecific(key, lpTlsValue);
	return TRUE;
}

BOOL TlsFree(DWORD dwTlsIndex)
{
	pthread_mutex_lock(&tls_lock);
	if (!is_valid(dwTlsIndex))
	{
		pthread_mutex_unlock(&tls_lock);
		return FALSE;
	}

	pthread_key_t key = map_dword(dwTlsIndex);

	const struct tls_map_t empty = WINPR_C_ARRAY_INIT;
	tls_map[dwTlsIndex] = empty;
	pthread_key_delete(key);
	pthread_mutex_unlock(&tls_lock);
	return TRUE;
}

#endif
