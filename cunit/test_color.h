/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Color Conversion Unit Tests
 *
 * Copyright 2010 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

int init_color_suite(void);
int clean_color_suite(void);
int add_color_suite(void);

void test_color_GetRGB32(void);
void test_color_GetBGR32(void);
void test_color_GetRGB_565(void);
void test_color_GetRGB16(void);
void test_color_GetBGR_565(void);
void test_color_GetBGR16(void);
