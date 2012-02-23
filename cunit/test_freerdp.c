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
#include "test_gdi.h"
#include "test_list.h"
#include "test_sspi.h"
#include "test_stream.h"
#include "test_utils.h"
#include "test_orders.h"
#include "test_ntlmssp.h"
#include "test_license.h"
#include "test_channels.h"
#include "test_cliprdr.h"
#include "test_drdynvc.h"
#include "test_rfx.h"
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

typedef boolean (*pInitTestSuite)(void);

struct _test_suite
{
	char name[32];
	pInitTestSuite Init;
};
typedef struct _test_suite test_suite;

static test_suite suites[] =
{
	{ "ber", add_ber_suite },
	{ "bitmap", add_bitmap_suite },
	{ "channels", add_channels_suite },
	{ "cliprdr", add_cliprdr_suite },
	{ "color", add_color_suite },
	{ "drdynvc", add_drdynvc_suite },
	{ "gcc", add_gcc_suite },
	{ "gdi", add_gdi_suite },
	{ "license", add_license_suite },
	{ "list", add_list_suite },
	{ "mcs", add_mcs_suite },
	{ "mppc", add_mppc_suite },
	{ "ntlmssp", add_ntlmssp_suite },
	{ "orders", add_orders_suite },
	{ "pcap", add_pcap_suite },
	{ "per", add_per_suite },
	{ "rail", add_rail_suite },
	{ "rfx", add_rfx_suite },
	{ "sspi", add_sspi_suite },
	{ "stream", add_stream_suite },
	{ "utils", add_utils_suite },
	{ "", NULL }
};

int main(int argc, char* argv[])
{
	int k;
	int index = 1;
	int *pindex = &index;
	int status = 0;

	if (CU_initialize_registry() != CUE_SUCCESS)
		return CU_get_error();

	if (argc < *pindex + 1)
	{
		k = 0;

		printf("\ntest suites:\n\n");
		while (suites[k].Init != NULL)
		{
			printf("\t%s\n", suites[k].name);
			k++;
		}

		printf("\nusage: ./test_freerdp <suite-1> <suite-2> ... <suite-n>\n");

		return 0;
	}
	else
	{
		while (*pindex < argc)
		{
			k = 0;

			while (suites[k].Init != NULL)
			{
				if (strcmp(suites[k].name, argv[*pindex]) == 0)
				{
					suites[k].Init();
					break;
				}

				k++;
			}

			*pindex = *pindex + 1;
		}
	}

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();

	status = CU_get_number_of_failure_records();
	CU_cleanup_registry();

	return status;
}

