/**
 * WinPR: Windows Portable Runtime
 * Windows Native System Services
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2013 Thincast Technologies GmbH
 * Copyright 2013 Norbert Federa <norbert.federa@thincast.com>
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
#include <winpr/config.h>

#include <winpr/crt.h>
#include <winpr/library.h>
#include <winpr/wlog.h>
#include <winpr/nt.h>
#include <winpr/endian.h>

#include "../log.h"
#define TAG WINPR_TAG("nt")

#ifndef _WIN32

#include <pthread.h>
#include <winpr/crt.h>

#include "../handle/handle.h"

static pthread_once_t sTebOnceControl = PTHREAD_ONCE_INIT;
static pthread_key_t sTebKey;

static void sTebDestruct(void* teb)
{
	free(teb);
}

static void sTebInitOnce(void)
{
	pthread_key_create(&sTebKey, sTebDestruct);
}

PTEB NtCurrentTeb(void)
{
	PTEB teb = NULL;

	if (pthread_once(&sTebOnceControl, sTebInitOnce) == 0)
	{
		if ((teb = pthread_getspecific(sTebKey)) == NULL)
		{
			teb = calloc(1, sizeof(TEB));
			if (teb)
				pthread_setspecific(sTebKey, teb);
		}
	}
	return teb;
}
#endif
