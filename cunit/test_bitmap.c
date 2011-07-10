/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Bitmap Unit Tests
 *
 * Copyright 2011 Jay Sorg <jay.sorg@gmail.com>
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

#include <freerdp/freerdp.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/stream.h>

#include "test_bitmap.h"

int init_bitmap_suite(void)
{
	return 0;
}

int clean_bitmap_suite(void)
{
	return 0;
}

int add_bitmap_suite(void)
{
	add_test_suite(bitmap);

	add_test_function(bitmap);

	return 0;
}

void test_bitmap(void)
{

}

