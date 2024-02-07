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
#ifndef WINPR_RC4_H
#define WINPR_RC4_H

#include <winpr/wtypes.h>
#include <winpr/winpr.h>

typedef struct winpr_int_rc4_ctx winpr_int_RC4_CTX;

void winpr_int_rc4_free(winpr_int_RC4_CTX* ctx);

WINPR_ATTR_MALLOC(winpr_int_rc4_free, 1)
winpr_int_RC4_CTX* winpr_int_rc4_new(const BYTE* key, size_t keylength);
BOOL winpr_int_rc4_update(winpr_int_RC4_CTX* ctx, size_t length, const BYTE* input, BYTE* output);

#endif
