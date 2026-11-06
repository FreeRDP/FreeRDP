/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Test for the [MS-RDPEAR] redirected-call servicing
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

#include <krb5.h>

#include <winpr/wtypes.h>
#include <winpr/stream.h>
#include <winpr/print.h>

#include <rdpear-common/rdpear_kerb.h>

/* A decrypted KerbNegotiateVersion request captured from a live Remote Credential Guard session:
 * the [MS-RDPEAR] inner packet, an ASN.1 SEQUENCE { [1] "Kerberos" (UTF-16), [2] NDR payload }. */
static const BYTE kerbNegotiateVersionRequest[] = {
	0x30, 0x48, 0xa1, 0x12, 0x04, 0x10, 0x4b, 0x00, 0x65, 0x00, 0x72, 0x00, 0x62, 0x00, 0x65,
	0x00, 0x72, 0x00, 0x6f, 0x00, 0x73, 0x00, 0xa2, 0x32, 0x04, 0x30, 0x01, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x10, 0x08,
	0x00, 0xcc, 0xcc, 0xcc, 0xcc, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00
};

int TestRdpearKerb(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	krb5_context krb = NULL;
	if (krb5_init_context(&krb) != 0)
	{
		(void)fprintf(stderr, "krb5_init_context failed\n");
		return -1;
	}

	/* The handler runs krb5 crypto only on keys carried in the request (NegotiateVersion carries
	 * none), so this exercises the full decode -> dispatch -> encode path without a ticket cache.
	 */
	wStream* resp =
	    rdpear_kerb_service(krb, kerbNegotiateVersionRequest, sizeof(kerbNegotiateVersionRequest));
	krb5_free_context(krb);

	if (!resp)
	{
		(void)fprintf(stderr, "rdpear_kerb_service returned NULL\n");
		return -1;
	}

	int rc = 0;
	const size_t len = Stream_GetPosition(resp);
	const BYTE* buffer = Stream_Buffer(resp);

	/* The response is the reply payload envelope: an ASN.1 SEQUENCE carrying the NDR response. */
	if ((len < 4) || (buffer[0] != 0x30))
	{
		(void)fprintf(stderr, "unexpected response (len=%" PRIuz ", first byte 0x%02x)\n", len,
		              (len > 0) ? buffer[0] : 0);
		winpr_HexDump("TestRdpearKerb", WLOG_ERROR, buffer, len);
		rc = -1;
	}

	Stream_Free(resp, TRUE);
	return rc;
}
