/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Channel Manager Unit Tests
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <freerdp/freerdp.h>
#include <freerdp/chanman.h>

#include "test_chanman.h"

int init_chanman_suite(void)
{
	freerdp_chanman_global_init();
	return 0;
}

int clean_chanman_suite(void)
{
	freerdp_chanman_global_uninit();
	return 0;
}

int add_chanman_suite(void)
{
	add_test_suite(chanman);

	add_test_function(chanman);

	return 0;
}

void test_chanman(void)
{
	rdpChanMan* chan_man;
	rdpSettings settings = { 0 };

	chan_man = freerdp_chanman_new();

	freerdp_chanman_close(chan_man, NULL);
	freerdp_chanman_free(chan_man);
}
