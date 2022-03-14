/**
 * WinPR: Windows Portable Runtime
 * NTLM Security Package (AV_PAIRs)
 *
 * Copyright 2011-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_SSPI_NTLM_AV_PAIRS_H
#define WINPR_SSPI_NTLM_AV_PAIRS_H

#include <winpr/config.h>

#include "ntlm.h"

#include <winpr/stream.h>

ULONG ntlm_av_pair_list_length(NTLM_AV_PAIR* pAvPairList, size_t cbAvPairList);

#ifdef WITH_DEBUG_NTLM
void ntlm_print_av_pair_list(NTLM_AV_PAIR* pAvPairList, size_t cbAvPairList);
#endif

PBYTE ntlm_av_pair_get_value_pointer(NTLM_AV_PAIR* pAvPair);
NTLM_AV_PAIR* ntlm_av_pair_get(NTLM_AV_PAIR* pAvPairList, size_t cbAvPairList, NTLM_AV_ID AvId,
                               size_t* pcbAvPairListRemaining);

BOOL ntlm_construct_challenge_target_info(NTLM_CONTEXT* context);
BOOL ntlm_construct_authenticate_target_info(NTLM_CONTEXT* context);

#endif /* WINPR_SSPI_NTLM_AV_PAIRS_H */
