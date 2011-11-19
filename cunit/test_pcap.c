/**
 * FreeRDP: A Remote Desktop Protocol Client
 * pcap File Format Unit Tests
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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
#include <freerdp/utils/pcap.h>

#include "test_pcap.h"

int init_pcap_suite(void)
{
	return 0;
}

int clean_pcap_suite(void)
{
	return 0;
}

int add_pcap_suite(void)
{
	add_test_suite(pcap);

	add_test_function(pcap);

	return 0;
}

uint8 test_packet_1[16] =
	"\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA\xAA";

uint8 test_packet_2[32] =
	"\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB"
	"\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB\xBB";

uint8 test_packet_3[64] =
	"\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC"
	"\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC"
	"\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC"
	"\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC";

typedef struct
{
	void* data;
	uint32 length;
} test_packet;

void test_pcap(void)
{
	rdpPcap* pcap;
	pcap_record record;
	test_packet packets[3];

	packets[0].data = test_packet_1;
	packets[0].length = sizeof(test_packet_1);
	packets[1].data = test_packet_2;
	packets[1].length = sizeof(test_packet_2);
	packets[2].data = test_packet_3;
	packets[2].length = sizeof(test_packet_3);

	pcap = pcap_open("/tmp/test.pcap", true);
	pcap_add_record(pcap, test_packet_1, sizeof(test_packet_1));
	pcap_flush(pcap);
	pcap_add_record(pcap, test_packet_2, sizeof(test_packet_2));
	pcap_flush(pcap);
	pcap_add_record(pcap, test_packet_3, sizeof(test_packet_3));
	pcap_close(pcap);

	pcap = pcap_open("/tmp/test.pcap", false);

	int i = 0;
	while (pcap_has_next_record(pcap))
	{
		pcap_get_next_record(pcap, &record);
		CU_ASSERT(record.length == packets[i].length)
		i++;
	}

	CU_ASSERT(i == 3);

	pcap_close(pcap);
}

