/**
 * FreeRDP: A Remote Desktop Protocol Client
 * T.124 Generic Conference Control (GCC) Unit Tests
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

int init_gcc_suite(void);
int clean_gcc_suite(void);
int add_gcc_suite(void);

void test_gcc_write_conference_create_request(void);
void test_gcc_write_client_core_data(void);
void test_gcc_write_client_security_data(void);
void test_gcc_write_client_cluster_data(void);
void test_gcc_write_client_network_data(void);
