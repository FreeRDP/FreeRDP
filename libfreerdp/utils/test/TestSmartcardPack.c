/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>

#include <winpr/stream.h>
#include <freerdp/channels/scard.h>
#include <freerdp/utils/smartcard_pack.h>

/* Build a LocateCardsByATRA call (cbContext=0, cAtrs=1, cReaders=0) holding a single
 * LocateCards_ATRMask whose cbAtr is set to the requested value. */
static wStream* build_locate_cards_by_atr_a(UINT32 cbAtr)
{
	wStream* s = Stream_New(NULL, 24 + 8 + sizeof(LocateCards_ATRMask));
	if (!s)
		return NULL;

	Stream_Write_UINT32(s, 0);          /* cbContext = 0 */
	Stream_Write_UINT32(s, 0);          /* pbContextNdrPtr = 0 */
	Stream_Write_UINT32(s, 1);          /* cAtrs = 1 */
	Stream_Write_UINT32(s, 0x00020000); /* rgAtrMasksNdrPtr */
	Stream_Write_UINT32(s, 0);          /* cReaders = 0 */
	Stream_Write_UINT32(s, 0);          /* rgReaderStatesNdrPtr = 0 */
	Stream_Write_UINT32(s, 1);          /* conformant count = cAtrs */
	Stream_Write_UINT32(s, cbAtr);      /* LocateCards_ATRMask::cbAtr */
	Stream_Zero(s, 36);                 /* rgbAtr[36] */
	Stream_Zero(s, 36);                 /* rgbMask[36] */

	Stream_SealLength(s);
	if (!Stream_SetPosition(s, 0))
	{
		Stream_Free(s, TRUE);
		return NULL;
	}
	return s;
}

static BOOL run_case(UINT32 cbAtr, BOOL expectAccept)
{
	wStream* s = build_locate_cards_by_atr_a(cbAtr);
	if (!s)
		return FALSE;

	LocateCardsByATRA_Call call = { 0 };
	const LONG status = smartcard_unpack_locate_cards_by_atr_a_call(s, &call);
	Stream_Free(s, TRUE);

	const BOOL accepted = (status == SCARD_S_SUCCESS) ? TRUE : FALSE;
	if (accepted != expectAccept)
	{
		printf("cbAtr=%" PRIu32 ": expected %s, unpack returned 0x%08" PRIX32 "\n", cbAtr,
		       expectAccept ? "accept" : "reject", (UINT32)status);
		free(call.rgAtrMasks);
		return FALSE;
	}

	free(call.rgAtrMasks);
	return TRUE;
}

int TestSmartcardPack(int argc, char* argv[])
{
	WINPR_UNUSED(argc);
	WINPR_UNUSED(argv);

	/* Valid: cbAtr within the fixed 36-byte rgbAtr/rgbMask arrays. */
	if (!run_case(0, TRUE))
		return -1;
	if (!run_case(36, TRUE))
		return -1;

	/* Out of range: cbAtr larger than the buffers would later overrun them. */
	if (!run_case(37, FALSE))
		return -1;
	if (!run_case(0xFFFFFFFF, FALSE))
		return -1;

	return 0;
}
