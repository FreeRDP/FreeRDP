/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2025 Thincast Technologies GmbH
 * Copyright 2025 Armin Novak <anovak@thincast.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <stdlib.h>
#include <stdbool.h>

void* test_codec_helper_read_data(const char* codec, const char* type, const char* name,
                                  size_t* plength);
void test_codec_helper_write_data(const char* codec, const char* type, const char* name,
                                  const void* data, size_t length);
bool test_codec_helper_compare(const char* codec, const char* type, const char* name,
                               const void* data, size_t length);
