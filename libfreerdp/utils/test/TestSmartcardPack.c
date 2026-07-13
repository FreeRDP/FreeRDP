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
	wStream* s = Stream_New(nullptr, 24 + 8 + sizeof(LocateCards_ATRMask));
	if (!s)
		return nullptr;

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
		return nullptr;
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

	const BOOL accepted = status == SCARD_S_SUCCESS;
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

/* Build a SetAttrib call (cbContext=0, cbHandle=0) whose pbAttr NDR conformant array
 * carries cbAttr payload bytes, with the requested cbAttrLen field. */
static wStream* build_set_attrib(UINT32 cbAttr, UINT32 cbAttrLen)
{
	const UINT32 pad = (4 - (cbAttr % 4)) % 4;
	wStream* s = Stream_New(nullptr, 28 + 4 + cbAttr + pad);
	if (!s)
		return nullptr;

	Stream_Write_UINT32(s, 0);          /* cbContext = 0 */
	Stream_Write_UINT32(s, 0);          /* pbContextNdrPtr = 0 */
	Stream_Write_UINT32(s, 0);          /* cbHandle = 0 */
	Stream_Write_UINT32(s, 0x00020000); /* pbHandleNdrPtr */
	Stream_Write_UINT32(s, 0);          /* dwAttrId */
	Stream_Write_UINT32(s, cbAttrLen);  /* cbAttrLen */
	Stream_Write_UINT32(s, 0x00020004); /* pbAttrNdrPtr */
	Stream_Write_UINT32(s, cbAttr);     /* conformant count */
	Stream_Zero(s, cbAttr);             /* pbAttr payload */
	if (pad)
		Stream_Zero(s, pad); /* NDR 4-byte alignment padding */

	Stream_SealLength(s);
	if (!Stream_SetPosition(s, 0))
	{
		Stream_Free(s, TRUE);
		return nullptr;
	}
	return s;
}

/* cbAttrLen must be clamped to the pbAttr payload length, never to the padded
 * NDR size, otherwise SCardSetAttrib reads past the pbAttr allocation. */
static BOOL run_set_attrib_case(UINT32 cbAttr, UINT32 cbAttrLen)
{
	wStream* s = build_set_attrib(cbAttr, cbAttrLen);
	if (!s)
		return FALSE;

	SetAttrib_Call call = { 0 };
	const LONG status = smartcard_unpack_set_attrib_call(s, &call);
	Stream_Free(s, TRUE);

	BOOL rc = TRUE;
	if (status != SCARD_S_SUCCESS)
	{
		printf("set_attrib cbAttr=%" PRIu32 ": unpack returned 0x%08" PRIX32 "\n", cbAttr,
		       (UINT32)status);
		rc = FALSE;
	}
	else if (call.cbAttrLen > cbAttr)
	{
		printf("set_attrib cbAttr=%" PRIu32 ": cbAttrLen=%" PRIu32 " exceeds payload\n", cbAttr,
		       call.cbAttrLen);
		rc = FALSE;
	}

	free(call.pbAttr);
	return rc;
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

	/* Unaligned pbAttr lengths must not let cbAttrLen reach into the NDR padding. */
	if (!run_set_attrib_case(1, 0xFFFFFFFF))
		return -1;
	if (!run_set_attrib_case(5, 0xFFFFFFFF))
		return -1;
	/* Aligned length and a small cbAttrLen must be preserved. */
	if (!run_set_attrib_case(4, 0xFFFFFFFF))
		return -1;
	if (!run_set_attrib_case(8, 2))
		return -1;

	return 0;
}
