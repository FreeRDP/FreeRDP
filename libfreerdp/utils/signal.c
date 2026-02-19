/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Signal handling
 *
 * Copyright 2011 Shea Levy <shea@shealevy.com>
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

#include <freerdp/config.h>

#include <string.h>

#include <winpr/crt.h>
#include <winpr/debug.h>

#include <freerdp/utils/signal.h>
#include <freerdp/log.h>

#include "signal.h"

#define TAG FREERDP_TAG("utils.signal")

BOOL fsig_handlers_registered = FALSE;

typedef struct
{
	void* context;
	freerdp_signal_handler_t handler;
} cleanup_handler_t;

static size_t cleanup_handler_count = 0;
static cleanup_handler_t cleanup_handlers[20] = { 0 };

void fsig_term_handler(int signum)
{
	static BOOL recursive = FALSE;

	if (!recursive)
	{
		recursive = TRUE;
		// NOLINTNEXTLINE(concurrency-mt-unsafe)
		WLog_ERR(TAG, "Caught signal '%s' [%d]", strsignal(signum), signum);
	}

	fsig_lock();
	for (size_t x = 0; x < cleanup_handler_count; x++)
	{
		const cleanup_handler_t empty = { 0 };
		cleanup_handler_t* cur = &cleanup_handlers[x];
		if (cur->handler)
		{
			// NOLINTNEXTLINE(concurrency-mt-unsafe)
			cur->handler(signum, strsignal(signum), cur->context);
		}
		*cur = empty;
	}
	cleanup_handler_count = 0;
	fsig_unlock();
}

BOOL freerdp_add_signal_cleanup_handler(void* context, freerdp_signal_handler_t handler)
{
	BOOL rc = FALSE;
	fsig_lock();
	if (fsig_handlers_registered)
	{
		if (cleanup_handler_count < ARRAYSIZE(cleanup_handlers))
		{
			cleanup_handler_t* cur = &cleanup_handlers[cleanup_handler_count++];
			cur->context = context;
			cur->handler = handler;
		}
		else
			WLog_WARN(TAG, "Failed to register cleanup handler, only %" PRIuz " handlers supported",
			          ARRAYSIZE(cleanup_handlers));
	}
	rc = TRUE;
	fsig_unlock();
	return rc;
}

BOOL freerdp_del_signal_cleanup_handler(void* context, freerdp_signal_handler_t handler)
{
	BOOL rc = FALSE;
	fsig_lock();
	if (fsig_handlers_registered)
	{
		for (size_t x = 0; x < cleanup_handler_count; x++)
		{
			cleanup_handler_t* cur = &cleanup_handlers[x];
			if ((cur->context == context) && (cur->handler == handler))
			{
				const cleanup_handler_t empty = { 0 };
				for (size_t y = x + 1; y < cleanup_handler_count - 1; y++)
				{
					*cur++ = cleanup_handlers[y];
				}

				*cur = empty;
				cleanup_handler_count--;
				break;
			}
		}
	}
	rc = TRUE;
	fsig_unlock();
	return rc;
}
