/**
 * WinPR: Windows Portable Runtime
 * RC4 implementation for RDP
 *
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#include "rc4.h"

#define CTX_SIZE 256

struct winpr_int_rc4_ctx
{
	size_t i;
	size_t j;
	BYTE s[CTX_SIZE];
	BYTE t[CTX_SIZE];
};

static void swap(BYTE* p1, BYTE* p2)
{
	BYTE t = *p1;
	*p1 = *p2;
	*p2 = t;
}

winpr_int_RC4_CTX* winpr_int_rc4_new(const BYTE* key, size_t keylength)
{
	winpr_int_RC4_CTX* ctx = calloc(1, sizeof(winpr_int_RC4_CTX));
	if (!ctx)
		return NULL;

	for (size_t i = 0; i < CTX_SIZE; i++)
	{
		ctx->s[i] = i;
		ctx->t[i] = key[i % keylength];
	}

	size_t j = 0;
	for (size_t i = 0; i < CTX_SIZE; i++)
	{
		j = (j + ctx->s[i] + ctx->t[i]) % CTX_SIZE;
		swap(&ctx->s[i], &ctx->s[j]);
	}
	return ctx;
}

void winpr_int_rc4_free(winpr_int_RC4_CTX* ctx)
{
	free(ctx);
}

BOOL winpr_int_rc4_update(winpr_int_RC4_CTX* ctx, size_t length, const BYTE* input, BYTE* output)
{
	WINPR_ASSERT(ctx);

	UINT32 t1 = ctx->i;
	UINT32 t2 = ctx->j;
	for (size_t i = 0; i < length; i++)
	{
		t1 = (t1 + 1) % CTX_SIZE;
		t2 = (t2 + ctx->s[t1]) % CTX_SIZE;
		swap(&ctx->s[t1], &ctx->s[t2]);

		const size_t idx = ((size_t)ctx->s[t1] + ctx->s[t2]) % CTX_SIZE;
		const BYTE val = ctx->s[idx];
		const BYTE out = *input++ ^ val;
		*output++ = out;
	}

	ctx->i = t1;
	ctx->j = t2;
	return TRUE;
}
