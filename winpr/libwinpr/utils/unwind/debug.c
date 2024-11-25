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
#define _GNU_SOURCE // NOLINT(bugprone-reserved-identifier,cert-dcl37-c,cert-dcl51-cpp)
#endif

#include <assert.h>
#include <stdlib.h>
#include <unwind.h>

#include <winpr/string.h>
#include "debug.h"

#include <winpr/wlog.h>
#include "../log.h"

#include <dlfcn.h>

#define TAG WINPR_TAG("utils.unwind")

#define UNWIND_MAX_LINE_SIZE 1024ULL

typedef struct
{
	union
	{
		uintptr_t uw;
		void* pv;
	} pc;
	union
	{
		uintptr_t uptr;
		void* pv;
	} langSpecificData;
} unwind_info_t;

typedef struct
{
	size_t pos;
	size_t size;
	unwind_info_t* info;
} unwind_context_t;

static const char* unwind_reason_str(_Unwind_Reason_Code code)
{
	switch (code)
	{
#if defined(__arm__) && !defined(__USING_SJLJ_EXCEPTIONS__) && !defined(__ARM_DWARF_EH__) && \
    !defined(__SEH__)
		case _URC_OK:
			return "_URC_OK";
#else
		case _URC_NO_REASON:
			return "_URC_NO_REASON";
		case _URC_FATAL_PHASE2_ERROR:
			return "_URC_FATAL_PHASE2_ERROR";
		case _URC_FATAL_PHASE1_ERROR:
			return "_URC_FATAL_PHASE1_ERROR";
		case _URC_NORMAL_STOP:
			return "_URC_NORMAL_STOP";
#endif
		case _URC_FOREIGN_EXCEPTION_CAUGHT:
			return "_URC_FOREIGN_EXCEPTION_CAUGHT";
		case _URC_END_OF_STACK:
			return "_URC_END_OF_STACK";
		case _URC_HANDLER_FOUND:
			return "_URC_HANDLER_FOUND";
		case _URC_INSTALL_CONTEXT:
			return "_URC_INSTALL_CONTEXT";
		case _URC_CONTINUE_UNWIND:
			return "_URC_CONTINUE_UNWIND";
#if defined(__arm__) && !defined(__USING_SJLJ_EXCEPTIONS__) && !defined(__ARM_DWARF_EH__) && \
    !defined(__SEH__)
		case _URC_FAILURE:
			return "_URC_FAILURE";
#endif
		default:
			return "_URC_UNKNOWN";
	}
}

static const char* unwind_reason_str_buffer(_Unwind_Reason_Code code, char* buffer, size_t size)
{
	const char* str = unwind_reason_str(code);
	(void)_snprintf(buffer, size, "%s [0x%02x]", str, code);
	return buffer;
}

static _Unwind_Reason_Code unwind_backtrace_callback(struct _Unwind_Context* context, void* arg)
{
	unwind_context_t* ctx = arg;

	assert(ctx);

	if (ctx->pos < ctx->size)
	{
		unwind_info_t* info = &ctx->info[ctx->pos++];
		info->pc.uw = _Unwind_GetIP(context);

		/* _Unwind_GetLanguageSpecificData has various return value definitions,
		 * cast to the type we expect and disable linter warnings
		 */
		// NOLINTNEXTLINE(google-readability-casting,readability-redundant-casting)
		info->langSpecificData.pv = (void*)_Unwind_GetLanguageSpecificData(context);
	}

	return _URC_NO_REASON;
}

void* winpr_unwind_backtrace(DWORD size)
{
	_Unwind_Reason_Code rc = _URC_FOREIGN_EXCEPTION_CAUGHT;
	unwind_context_t* ctx = calloc(1, sizeof(unwind_context_t));
	if (!ctx)
		goto fail;
	ctx->size = size;
	ctx->info = calloc(size, sizeof(unwind_info_t));
	if (!ctx->info)
		goto fail;

	rc = _Unwind_Backtrace(unwind_backtrace_callback, ctx);
	if (rc != _URC_END_OF_STACK)
	{
		char buffer[64] = { 0 };
		WLog_ERR(TAG, "_Unwind_Backtrace failed with %s",
		         unwind_reason_str_buffer(rc, buffer, sizeof(buffer)));
		goto fail;
	}

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

char** winpr_unwind_backtrace_symbols(void* buffer, size_t* used)
{
	union
	{
		void* pv;
		char* cp;
		char** cpp;
	} cnv;
	unwind_context_t* ctx = buffer;
	cnv.cpp = NULL;

	if (!ctx)
		return NULL;

	cnv.pv = calloc(ctx->pos * (sizeof(char*) + UNWIND_MAX_LINE_SIZE), sizeof(char*));
	if (!cnv.pv)
		return NULL;

	if (used)
		*used = ctx->pos;

	for (size_t x = 0; x < ctx->pos; x++)
	{
		char* msg = cnv.cp + ctx->pos * sizeof(char*) + x * UNWIND_MAX_LINE_SIZE;
		const unwind_info_t* info = &ctx->info[x];
		Dl_info dlinfo = { 0 };
		int rc = dladdr(info->pc.pv, &dlinfo);

		cnv.cpp[x] = msg;

		if (rc == 0)
			(void)_snprintf(msg, UNWIND_MAX_LINE_SIZE, "unresolvable, address=%p", info->pc.pv);
		else
			(void)_snprintf(msg, UNWIND_MAX_LINE_SIZE, "dli_fname=%s [%p], dli_sname=%s [%p]",
			                dlinfo.dli_fname, dlinfo.dli_fbase, dlinfo.dli_sname, dlinfo.dli_saddr);
	}

	return cnv.cpp;
}
