/**
 * WinPR: Windows Portable Runtime
 * WinPR Debugging helpers
 *
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <stdlib.h>
#include <unwind.h>

#include <winpr/string.h>
#include "debug.h"

#include <dlfcn.h>

typedef struct
{
	uintptr_t pc;
	void* langSpecificData;
} unwind_info_t;

typedef struct
{
	size_t pos;
	size_t size;
	unwind_info_t* info;
} unwind_context_t;

static _Unwind_Reason_Code unwind_backtrace_callback(struct _Unwind_Context* context, void* arg)
{
	unwind_context_t* ctx = arg;

	assert(ctx);

	if (ctx->pos < ctx->size)
	{
		unwind_info_t* info = &ctx->info[ctx->pos++];
		info->pc = _Unwind_GetIP(context);
		info->langSpecificData = _Unwind_GetLanguageSpecificData(context);
	}

	return _URC_NO_REASON;
}

void* winpr_unwind_backtrace(DWORD size)
{
	_Unwind_Reason_Code rc;
	unwind_context_t* ctx = calloc(1, sizeof(unwind_context_t));
	if (!ctx)
		goto fail;
	ctx->size = size;
	ctx->info = calloc(size, sizeof(unwind_info_t));
	if (!ctx->info)
		goto fail;

	rc = _Unwind_Backtrace(unwind_backtrace_callback, ctx);
	if (rc != _URC_END_OF_STACK)
		goto fail;

	return ctx;
fail:
	winpr_unwind_backtrace_free(ctx);
	return NULL;
}

void winpr_unwind_backtrace_free(void* buffer)
{
	unwind_context_t* ctx = buffer;
	if (!ctx)
		return;
	free(ctx->info);
	free(ctx);
}

#define UNWIND_MAX_LINE_SIZE 1024

char** winpr_unwind_backtrace_symbols(void* buffer, size_t* used)
{
	size_t x;
	union
	{
		char* cp;
		char** cpp;
	} cnv;
	unwind_context_t* ctx = buffer;
	cnv.cpp = NULL;

	if (!ctx)
		return NULL;

	cnv.cpp = calloc(ctx->pos * (sizeof(char*) + UNWIND_MAX_LINE_SIZE), sizeof(char*));
	if (!cnv.cpp)
		return NULL;

	if (used)
		*used = ctx->pos;

	for (x = 0; x < ctx->pos; x++)
	{
		char* msg = cnv.cp + ctx->pos * sizeof(char*) + x * UNWIND_MAX_LINE_SIZE;
		const unwind_info_t* info = &ctx->info[x];
		Dl_info dlinfo = { 0 };
		int rc = dladdr((void*)info->pc, &dlinfo);

		cnv.cpp[x] = msg;

		if (rc == 0)
			_snprintf(msg, UNWIND_MAX_LINE_SIZE, "unresolvable, address=%p", (void*)info->pc);
		else
			_snprintf(msg, UNWIND_MAX_LINE_SIZE, "dli_fname=%s [%p], dli_sname=%s [%p]",
			          dlinfo.dli_fname, dlinfo.dli_fbase, dlinfo.dli_sname, dlinfo.dli_saddr);
	}

	return cnv.cpp;
}
