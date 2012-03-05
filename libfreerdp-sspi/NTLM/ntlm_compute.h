/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * NTLM Security Package (Compute)
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

#ifndef FREERDP_SSPI_NTLM_COMPUTE_H
#define FREERDP_SSPI_NTLM_COMPUTE_H

#include "ntlm.h"

void ntlm_output_restriction_encoding(NTLM_CONTEXT* context);
void ntlm_output_target_name(NTLM_CONTEXT* context);
void ntlm_output_channel_bindings(NTLM_CONTEXT* context);

void ntlm_input_av_pairs(NTLM_CONTEXT* context, STREAM* s);
void ntlm_output_av_pairs(NTLM_CONTEXT* context, SEC_BUFFER* buffer);
void ntlm_populate_av_pairs(NTLM_CONTEXT* context);
void ntlm_print_av_pairs(NTLM_CONTEXT* context);
void ntlm_free_av_pairs(NTLM_CONTEXT* context);

void ntlm_current_time(uint8* timestamp);
void ntlm_generate_timestamp(NTLM_CONTEXT* context);

void ntlm_compute_ntlm_hash(uint16* password, uint32 length, char* hash);
void ntlm_compute_ntlm_v2_hash(NTLM_CONTEXT* context, char* hash);
void ntlm_compute_lm_v2_response(NTLM_CONTEXT* context);
void ntlm_compute_ntlm_v2_response(NTLM_CONTEXT* context);

void ntlm_rc4k(uint8* key, int length, uint8* plaintext, uint8* ciphertext);
void ntlm_generate_client_challenge(NTLM_CONTEXT* context);
void ntlm_generate_server_challenge(NTLM_CONTEXT* context);
void ntlm_generate_key_exchange_key(NTLM_CONTEXT* context);
void ntlm_generate_random_session_key(NTLM_CONTEXT* context);
void ntlm_generate_exported_session_key(NTLM_CONTEXT* context);
void ntlm_encrypt_random_session_key(NTLM_CONTEXT* context);

void ntlm_generate_client_signing_key(NTLM_CONTEXT* context);
void ntlm_generate_server_signing_key(NTLM_CONTEXT* context);
void ntlm_generate_client_sealing_key(NTLM_CONTEXT* context);
void ntlm_generate_server_sealing_key(NTLM_CONTEXT* context);
void ntlm_init_rc4_seal_states(NTLM_CONTEXT* context);

void ntlm_compute_message_integrity_check(NTLM_CONTEXT* context);

#endif /*  FREERDP_AUTH_NTLM_COMPUTE_H */

