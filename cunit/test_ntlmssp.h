/**
 * FreeRDP: A Remote Desktop Protocol Client
 * NT LAN Manager Security Support Provider (NTLMSSP) Unit Tests
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "test_freerdp.h"

int init_ntlmssp_suite(void);
int clean_ntlmssp_suite(void);
int add_ntlmssp_suite(void);

void test_ntlmssp_compute_lm_hash(void);
void test_ntlmssp_compute_ntlm_hash(void);
void test_ntlmssp_compute_ntlm_v2_hash(void);
void test_ntlmssp_compute_lm_response(void);
void test_ntlmssp_compute_lm_v2_response(void);
void test_ntlmssp_compute_ntlm_v2_response(void);
void test_ntlmssp_generate_client_signing_key(void);
void test_ntlmssp_generate_server_signing_key(void);
void test_ntlmssp_generate_client_sealing_key(void);
void test_ntlmssp_generate_server_sealing_key(void);
void test_ntlmssp_encrypt_random_session_key(void);
void test_ntlmssp_compute_message_integrity_check(void);
void test_ntlmssp_encrypt_message(void);
void test_ntlmssp_decrypt_message(void);
