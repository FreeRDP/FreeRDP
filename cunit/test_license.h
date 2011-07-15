/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Licensing Unit Tests
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

int init_license_suite(void);
int clean_license_suite(void);
int add_license_suite(void);

void test_license(void);
void test_license_generate_keys(void);
void test_license_encrypt_premaster_secret(void);
void test_license_decrypt_platform_challenge(void);
