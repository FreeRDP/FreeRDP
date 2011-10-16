/**
 * FreeRDP: A Remote Desktop Protocol Client
 * FreeRDP Unit Tests
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

#include <CUnit/Basic.h>

#include "test_per.h"
#include "test_ber.h"
#include "test_gcc.h"
#include "test_mcs.h"
#include "test_color.h"
#include "test_bitmap.h"
#include "test_libgdi.h"
#include "test_list.h"
#include "test_stream.h"
#include "test_utils.h"
#include "test_orders.h"
#include "test_license.h"
#include "test_channels.h"
#include "test_cliprdr.h"
#include "test_drdynvc.h"
#include "test_librfx.h"
#include "test_freerdp.h"
#include "test_rail.h"
#include "test_pcap.h"
#include "test_mppc.h"

void dump_data(unsigned char * p, int len, int width, char* name)
{
	unsigned char *line = p;
	int i, thisline, offset = 0;

	printf("\n%s[%d][%d]:\n", name, len / width, width);
	while (offset < len)
	{
		printf("%04x ", offset);
		thisline = len - offset;
		if (thisline > width)
			thisline = width;

		for (i = 0; i < thisline; i++)
			printf("%02x ", line[i]);

		for (; i < width; i++)
			printf("   ");

		printf("\n");
		offset += thisline;
		line += thisline;
	}
	printf("\n");
}

void assert_stream(STREAM* s, uint8* data, int length, const char* func, int line)
{
	int i;
	int actual_length;
	uint8* actual_data;

	actual_data = s->data;
	actual_length = stream_get_length(s);

	if (actual_length != length)
	{
		printf("\n %s (%d): length mismatch, actual:%d, expected:%d\n", func, line, actual_length, length);

		printf("\nActual:\n");
		freerdp_hexdump(actual_data, actual_length);

		printf("Expected:\n");
		freerdp_hexdump(data, length);

		CU_FAIL("assert_stream, length mismatch");
		return;
	}

	for (i = 0; i < length; i++)
	{
		if (actual_data[i] != data[i])
		{
			printf("\n %s (%d): buffer mismatch:\n", func, line);

			printf("\nActual:\n");
			freerdp_hexdump(actual_data, length);

			printf("Expected:\n");
			freerdp_hexdump(data, length);

			CU_FAIL("assert_stream, buffer mismatch");
			return;
		}
	}
}

int main(int argc, char* argv[])
{
	int index = 1;
	int *pindex = &index;
	int ret = 0;

	if (CU_initialize_registry() != CUE_SUCCESS)
		return CU_get_error();

	if (argc < *pindex + 1)
	{
		add_per_suite();
		add_ber_suite();
		add_gcc_suite();
		add_mcs_suite();
		add_color_suite();
		add_bitmap_suite();
		add_libgdi_suite();
		add_list_suite();
		add_orders_suite();
		add_license_suite();
		add_stream_suite();
		add_mppc_suite();
	}
	else
	{
		while (*pindex < argc)
		{
			if (strcmp("rail", argv[*pindex]) == 0)
			{
				add_rail_suite();
			}
			if (strcmp("color", argv[*pindex]) == 0)
			{
				add_color_suite();
			}
			if (strcmp("bitmap", argv[*pindex]) == 0)
			{
				add_bitmap_suite();
			}
			else if (strcmp("libgdi", argv[*pindex]) == 0)
			{
				add_libgdi_suite();
			}
			else if (strcmp("list", argv[*pindex]) == 0)
			{
				add_list_suite();
			}
			else if (strcmp("orders", argv[*pindex]) == 0)
			{
				add_orders_suite();
			}
			else if (strcmp("license", argv[*pindex]) == 0)
			{
				add_license_suite();
			}
			else if (strcmp("stream", argv[*pindex]) == 0)
			{
				add_stream_suite();
			}
			else if (strcmp("utils", argv[*pindex]) == 0)
			{
				add_utils_suite();
			}
			else if (strcmp("chanman", argv[*pindex]) == 0)
			{
				add_chanman_suite();
			}
			else if (strcmp("cliprdr", argv[*pindex]) == 0)
			{
				add_cliprdr_suite();
			}
			else if (strcmp("drdynvc", argv[*pindex]) == 0)
			{
				add_drdynvc_suite();
			}
			else if (strcmp("librfx", argv[*pindex]) == 0)
			{
				add_librfx_suite();
			}
			else if (strcmp("per", argv[*pindex]) == 0)
			{
				add_per_suite();
			}
			else if (strcmp("pcap", argv[*pindex]) == 0)
			{
				add_pcap_suite();
			}
			else if (strcmp("ber", argv[*pindex]) == 0)
			{
				add_ber_suite();
			}
			else if (strcmp("gcc", argv[*pindex]) == 0)
			{
				add_gcc_suite();
			}
			else if (strcmp("mcs", argv[*pindex]) == 0)
			{
				add_mcs_suite();
			}
			else if (strcmp("mppc", argv[*pindex]) == 0)
			{
				add_mppc_suite();
			}

			*pindex = *pindex + 1;
		}
	}

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();
	ret = CU_get_number_of_failure_records();
	CU_cleanup_registry();

	return ret;
}

