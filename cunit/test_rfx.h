/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RemoteFX Codec Library Unit Tests
 *
 * Copyright 2011 Vic Lee
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

int init_rfx_suite(void);
int clean_rfx_suite(void);
int add_rfx_suite(void);

void test_bitstream(void);
void test_bitstream_enc(void);
void test_rlgr(void);
void test_differential(void);
void test_quantization(void);
void test_dwt(void);
void test_decode(void);
void test_encode(void);
void test_message(void);
