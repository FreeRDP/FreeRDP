/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#include "test_gcc.h"
#include "test_mcs.h"
#include "test_color.h"
#include "test_bitmap.h"
#include "test_gdi.h"
#include "test_orders.h"
#include "test_ntlm.h"
#include "test_license.h"
#include "test_cliprdr.h"
#include "test_drdynvc.h"
#include "test_rfx.h"
#include "test_nsc.h"
#include "test_freerdp.h"
#include "test_rail.h"
#include "test_pcap.h"
#include "test_mppc.h"
#include "test_mppc_enc.h"

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

void assert_stream(wStream* s, BYTE* data, int length, const char* func, int line)
{
	int i;
	int actual_length;
	BYTE* actual_data;

	actual_data = s->buffer;
	actual_length = Stream_GetPosition(s);

	if (actual_length != length)
	{
		printf("\n %s (%d): length mismatch, actual:%d, expected:%d\n", func, line, actual_length, length);

		printf("\nActual:\n");
		winpr_HexDump(actual_data, actual_length);

		printf("Expected:\n");
		winpr_HexDump(data, length);

		CU_FAIL("assert_stream, length mismatch");
		return;
	}

	for (i = 0; i < length; i++)
	{
		if (actual_data[i] != data[i])
		{
			printf("\n %s (%d): buffer mismatch:\n", func, line);

			printf("\nActual:\n");
			winpr_HexDump(actual_data, length);

			printf("Expected:\n");
			winpr_HexDump(data, length);

			CU_FAIL("assert_stream, buffer mismatch");
			return;
		}
	}
}

typedef BOOL (*pInitTestSuite)(void);

struct _test_suite
{
	char name[32];
	pInitTestSuite Init;
};
typedef struct _test_suite test_suite;

const static test_suite suites[] =
{
	{ "bitmap", add_bitmap_suite },
	//{ "cliprdr", add_cliprdr_suite },
	{ "color", add_color_suite },
	//{ "drdynvc", add_drdynvc_suite },
	//{ "gcc", add_gcc_suite },
	{ "gdi", add_gdi_suite },
	{ "license", add_license_suite },
	//{ "mcs", add_mcs_suite },
	{ "mppc", add_mppc_suite },
	{ "mppc_enc", add_mppc_enc_suite },
	{ "ntlm", add_ntlm_suite },
	//{ "orders", add_orders_suite },
	{ "pcap", add_pcap_suite },
	//{ "rail", add_rail_suite },
	{ "rfx", add_rfx_suite },
	{ "nsc", add_nsc_suite }
};
#define N_SUITES (sizeof suites / sizeof suites[0])

int main(int argc, char* argv[])
{
	int i, k;
	int status = 0;

	if (CU_initialize_registry() != CUE_SUCCESS)
		return CU_get_error();

	if (argc < 2)
	{
		for (k = 0; k < N_SUITES; k++)
			suites[k].Init();
	}
	else
	{
		if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help"))
		{
			puts("Test suites:");
			for (k = 0; k < N_SUITES; k++)
				printf("\t%s\n", suites[k].name);
			printf("\nUsage: %s [suite-1] [suite-2] ...\n", argv[0]);
			return 0;
		}

		for (i = 1; i < argc; i++)
		{
			for (k = 0; k < N_SUITES; k++)
			{
				if (!strcmp(suites[k].name, argv[i]))
				{
					suites[k].Init();
					break;
				}
			}
		}
	}

	CU_basic_set_mode(CU_BRM_VERBOSE);
	CU_basic_run_tests();

	status = CU_get_number_of_failure_records();
	CU_cleanup_registry();

	return status;
}

