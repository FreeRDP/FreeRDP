/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Utils Unit Tests
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
#include <freerdp/utils/semaphore.h>

#include "test_utils.h"

int init_utils_suite(void)
{
	return 0;
}

int clean_utils_suite(void)
{
	return 0;
}

int add_utils_suite(void)
{
	add_test_suite(utils);

	add_test_function(semaphore);

	return 0;
}

void test_semaphore(void)
{
	freerdp_sem sem;

	sem = freerdp_sem_new(1);
	freerdp_sem_wait(sem);
	freerdp_sem_signal(sem);
	freerdp_sem_free(sem);
}
