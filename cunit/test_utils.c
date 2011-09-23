/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Utils Unit Tests
 *
 * Copyright 2011 Vic Lee, 2011 Shea Levy
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
#include <freerdp/utils/mutex.h>
#include <freerdp/utils/semaphore.h>
#include <freerdp/utils/load_plugin.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/utils/args.h>
#include <freerdp/utils/passphrase.h>

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

	add_test_function(mutex);
	add_test_function(semaphore);
	add_test_function(load_plugin);
	add_test_function(wait_obj);
	add_test_function(args);
	add_test_function(passphrase_read);

	return 0;
}

void test_mutex(void)
{
	freerdp_mutex mutex;

	mutex = freerdp_mutex_new();
	freerdp_mutex_lock(mutex);
	freerdp_mutex_unlock(mutex);
	freerdp_mutex_free(mutex);
}

void test_semaphore(void)
{
	freerdp_sem sem;

	sem = freerdp_sem_new(1);
	freerdp_sem_wait(sem);
	freerdp_sem_signal(sem);
	freerdp_sem_free(sem);
}

void test_load_plugin(void)
{
	void* entry;

#ifdef _WIN32
	/* untested */
	entry = freerdp_load_plugin("..\\channels\\cliprdr\\cliprdr", "VirtualChannelEntry");
#else
	entry = freerdp_load_plugin("../channels/cliprdr/cliprdr.so", "VirtualChannelEntry");
#endif
	CU_ASSERT(entry != NULL);
}

void test_wait_obj(void)
{
	struct wait_obj* wo;
	int set;

	wo = wait_obj_new();

	set = wait_obj_is_set(wo);
	CU_ASSERT(set == 0);

	wait_obj_set(wo);
	set = wait_obj_is_set(wo);
	CU_ASSERT(set == 1);

	wait_obj_clear(wo);
	set = wait_obj_is_set(wo);
	CU_ASSERT(set == 0);

	wait_obj_select(&wo, 1, 1000);

	wait_obj_free(wo);
}

static int process_plugin_args(rdpSettings* settings, const char* name,
	RDP_PLUGIN_DATA* plugin_data, void* user_data)
{
	/*printf("load plugin: %s\n", name);*/
	return 1;
}

static int process_ui_args(rdpSettings* settings, const char* opt,
	const char* val, void* user_data)
{
	/*printf("ui arg: %s %s\n", opt, val);*/
	return 1;
}

void test_args(void)
{
	char* argv_c[] =
	{
		"freerdp", "-a", "8", "-u", "testuser", "-d", "testdomain", "-g", "640x480", "address1:3389",
		"freerdp", "-a", "16", "-u", "testuser", "-d", "testdomain", "-g", "1280x960", "address2:3390"
	};
	char** argv = argv_c;
	int argc = sizeof(argv_c) / sizeof(char*);
	int i;
	int c;
	rdpSettings* settings;

	i = 0;
	while (argc > 0)
	{
		settings = settings_new();

		i++;
		c = freerdp_parse_args(settings, argc, argv, process_plugin_args, NULL, process_ui_args, NULL);
		CU_ASSERT(c > 0);
		if (c == 0)
		{
			settings_free(settings);
			break;
		}
		CU_ASSERT(settings->color_depth == i * 8);
		CU_ASSERT(settings->width == i * 640);
		CU_ASSERT(settings->height == i * 480);
		CU_ASSERT(settings->port == i + 3388);

		settings_free(settings);
		argc -= c;
		argv += c;
	}
	CU_ASSERT(i == 2);
}

void test_passphrase_read(void)
{
	freerdp_passphrase_read(NULL, NULL, 0, 0);
	return;
}
