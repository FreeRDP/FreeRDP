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

#include "ntlm.h"

#include <winpr/stream.h>

void ntlm_av_pair_list_init(NTLM_AV_PAIR* pAvPairList);
ULONG ntlm_av_pair_list_length(NTLM_AV_PAIR* pAvPairList);
void ntlm_print_av_pair_list(NTLM_AV_PAIR* pAvPairList);
ULONG ntlm_av_pair_list_size(ULONG AvPairsCount, ULONG AvPairsValueLength);
PBYTE ntlm_av_pair_get_value_pointer(NTLM_AV_PAIR* pAvPair);
int ntlm_av_pair_get_next_offset(NTLM_AV_PAIR* pAvPair);
NTLM_AV_PAIR* ntlm_av_pair_get_next_pointer(NTLM_AV_PAIR* pAvPair);
NTLM_AV_PAIR* ntlm_av_pair_get(NTLM_AV_PAIR* pAvPairList, NTLM_AV_ID AvId);
NTLM_AV_PAIR* ntlm_av_pair_add(NTLM_AV_PAIR* pAvPairList, NTLM_AV_ID AvId, PBYTE Value, UINT16 AvLen);
NTLM_AV_PAIR* ntlm_av_pair_add_copy(NTLM_AV_PAIR* pAvPairList, NTLM_AV_PAIR* pAvPair);

void ntlm_construct_challenge_target_info(NTLM_CONTEXT* context);
void ntlm_construct_authenticate_target_info(NTLM_CONTEXT* context);

#endif /* WINPR_SSPI_NTLM_AV_PAIRS_H */
