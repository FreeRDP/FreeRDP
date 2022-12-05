/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Activation Sequence
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

#include <freerdp/config.h>

#include <winpr/assert.h>

#include "activation.h"
#include "display.h"

#define TAG FREERDP_TAG("core.activation")

static BOOL rdp_recv_client_font_list_pdu(wStream* s);
static BOOL rdp_recv_client_persistent_key_list_pdu(wStream* s);
static BOOL rdp_send_server_font_map_pdu(rdpRdp* rdp);

static BOOL rdp_write_synchronize_pdu(wStream* s, const rdpSettings* settings)
{
	const UINT32 PduSource = freerdp_settings_get_uint32(settings, FreeRDP_PduSource);

	if (Stream_GetRemainingCapacity(s) < 4)
		return FALSE;
	Stream_Write_UINT16(s, SYNCMSGTYPE_SYNC); /* messageType (2 bytes) */
	Stream_Write_UINT16(s, PduSource);        /* targetUser (2 bytes) */
	return TRUE;
}

static BOOL rdp_recv_sync_pdu(rdpRdp* rdp, wStream* s, const char* what)
{
	UINT16 msgType, targetUser;

	WINPR_UNUSED(rdp);
	if (!Stream_CheckAndLogRequiredLengthEx(TAG, WLOG_WARN, s, 4, "%s(%s:%" PRIuz ") %s",
	                                        __FUNCTION__, __FILE__, __LINE__, what))
		return FALSE;
	Stream_Read_UINT16(s, msgType);
	if (msgType != SYNCMSGTYPE_SYNC)
	{
		WLog_WARN(TAG, "%s: Invalid messageType=0x%04" PRIx16 ", expected 0x%04" PRIx16, what,
		          msgType, SYNCMSGTYPE_SYNC);
		return FALSE;
	}
	Stream_Read_UINT16(s, targetUser);
	WLog_VRB(TAG, "%s: targetUser=0x%04" PRIx16, what, targetUser);
	return TRUE;
}

BOOL rdp_recv_server_synchronize_pdu(rdpRdp* rdp, wStream* s)
{
	if (!rdp_recv_sync_pdu(rdp, s, "[MS-RDPBCGR] 2.2.1.19 Server Synchronize PDU"))
		return FALSE;
	return rdp_finalize_set_flag(rdp, FINALIZE_SC_SYNCHRONIZE_PDU);
}

BOOL rdp_send_server_synchronize_pdu(rdpRdp* rdp)
{
	wStream* s = rdp_data_pdu_init(rdp);
	if (!s)
		return FALSE;

	WINPR_ASSERT(rdp);
	if (!rdp_write_synchronize_pdu(s, rdp->settings))
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	WINPR_ASSERT(rdp->mcs);
	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SYNCHRONIZE, rdp->mcs->userId);
}

BOOL rdp_recv_client_synchronize_pdu(rdpRdp* rdp, wStream* s)
{
	if (!rdp_recv_sync_pdu(rdp, s, "[MS-RDPBCGR] 2.2.1.14 Client Synchronize PDU"))
		return FALSE;
	return rdp_finalize_set_flag(rdp, FINALIZE_CS_SYNCHRONIZE_PDU);
}

BOOL rdp_send_client_synchronize_pdu(rdpRdp* rdp)
{
	wStream* s = rdp_data_pdu_init(rdp);
	if (!s)
		return FALSE;

	WINPR_ASSERT(rdp);
	if (!rdp_write_synchronize_pdu(s, rdp->settings))
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	WINPR_ASSERT(rdp->mcs);
	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_SYNCHRONIZE, rdp->mcs->userId);
}

static BOOL rdp_recv_control_pdu(wStream* s, UINT16* action, UINT16* grantId, UINT32* controlId)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(action);
	WINPR_ASSERT(grantId);
	WINPR_ASSERT(controlId);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	Stream_Read_UINT16(s, *action);    /* action (2 bytes) */
	Stream_Read_UINT16(s, *grantId);   /* grantId (2 bytes) */
	Stream_Read_UINT32(s, *controlId); /* controlId (4 bytes) */
	return TRUE;
}

static BOOL rdp_write_client_control_pdu(wStream* s, UINT16 action, UINT16 grantId,
                                         UINT32 controlId)
{
	WINPR_ASSERT(s);
	if (Stream_GetRemainingCapacity(s) < 8)
		return FALSE;
	Stream_Write_UINT16(s, action);    /* action (2 bytes) */
	Stream_Write_UINT16(s, grantId);   /* grantId (2 bytes) */
	Stream_Write_UINT32(s, controlId); /* controlId (4 bytes) */
	return TRUE;
}

BOOL rdp_recv_server_control_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 action, grantId;
	UINT32 controlId;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	if (!rdp_recv_control_pdu(s, &action, &grantId, &controlId))
		return FALSE;

	switch (action)
	{
		case CTRLACTION_COOPERATE:
			return rdp_finalize_set_flag(rdp, FINALIZE_SC_CONTROL_COOPERATE_PDU);

		case CTRLACTION_GRANTED_CONTROL:
			rdp->resendFocus = TRUE;
			return rdp_finalize_set_flag(rdp, FINALIZE_SC_CONTROL_GRANTED_PDU);
		default:
		{
			char buffer[128] = { 0 };
			WLog_WARN(TAG, "Unexpected control PDU %s",
			          rdp_ctrlaction_string(action, buffer, sizeof(buffer)));

			return FALSE;
		}
	}
}

BOOL rdp_send_server_control_cooperate_pdu(rdpRdp* rdp)
{
	wStream* s = rdp_data_pdu_init(rdp);
	if (!s)
		return FALSE;
	if (Stream_GetRemainingCapacity(s) < 8)
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}
	Stream_Write_UINT16(s, CTRLACTION_COOPERATE); /* action (2 bytes) */
	Stream_Write_UINT16(s, 0);                    /* grantId (2 bytes) */
	Stream_Write_UINT32(s, 0);                    /* controlId (4 bytes) */

	WINPR_ASSERT(rdp->mcs);
	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_CONTROL, rdp->mcs->userId);
}

static BOOL rdp_send_server_control_granted_pdu(rdpRdp* rdp)
{
	wStream* s = rdp_data_pdu_init(rdp);
	if (!s)
		return FALSE;
	if (Stream_GetRemainingCapacity(s) < 8)
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	WINPR_ASSERT(rdp->mcs);
	Stream_Write_UINT16(s, CTRLACTION_GRANTED_CONTROL); /* action (2 bytes) */
	Stream_Write_UINT16(s, rdp->mcs->userId);           /* grantId (2 bytes) */
	Stream_Write_UINT32(s, 0x03EA);                     /* controlId (4 bytes) */
	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_CONTROL, rdp->mcs->userId);
}

BOOL rdp_send_client_control_pdu(rdpRdp* rdp, UINT16 action)
{
	wStream* s = rdp_data_pdu_init(rdp);
	UINT16 GrantId = 0;
	UINT16 ControlId = 0;

	switch (action)
	{
		case CTRLACTION_COOPERATE:
		case CTRLACTION_REQUEST_CONTROL:
			break;
		default:
			WLog_WARN(TAG,
			          "Invalid client control PDU::action 0x%04" PRIx16 ", not allowed by client",
			          action);
			return FALSE;
	}

	if (!s)
		return FALSE;
	if (!rdp_write_client_control_pdu(s, action, GrantId, ControlId))
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	WINPR_ASSERT(rdp->mcs);
	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_CONTROL, rdp->mcs->userId);
}

static BOOL rdp_write_client_persistent_key_list_pdu(wStream* s, RDP_BITMAP_PERSISTENT_INFO* info)
{
	WINPR_ASSERT(s);
	WINPR_ASSERT(info);

	if (!Stream_EnsureRemainingCapacity(s, 24))
		return FALSE;

	Stream_Write_UINT16(s, info->numEntriesCache0);              /* numEntriesCache0 (2 bytes) */
	Stream_Write_UINT16(s, info->numEntriesCache1);              /* numEntriesCache1 (2 bytes) */
	Stream_Write_UINT16(s, info->numEntriesCache2);              /* numEntriesCache2 (2 bytes) */
	Stream_Write_UINT16(s, info->numEntriesCache3);              /* numEntriesCache3 (2 bytes) */
	Stream_Write_UINT16(s, info->numEntriesCache4);              /* numEntriesCache4 (2 bytes) */
	Stream_Write_UINT16(s, info->totalEntriesCache0);            /* totalEntriesCache0 (2 bytes) */
	Stream_Write_UINT16(s, info->totalEntriesCache1);            /* totalEntriesCache1 (2 bytes) */
	Stream_Write_UINT16(s, info->totalEntriesCache2);            /* totalEntriesCache2 (2 bytes) */
	Stream_Write_UINT16(s, info->totalEntriesCache3);            /* totalEntriesCache3 (2 bytes) */
	Stream_Write_UINT16(s, info->totalEntriesCache4);            /* totalEntriesCache4 (2 bytes) */
	Stream_Write_UINT8(s, PERSIST_FIRST_PDU | PERSIST_LAST_PDU); /* bBitMask (1 byte) */
	Stream_Write_UINT8(s, 0);                                    /* pad1 (1 byte) */
	Stream_Write_UINT16(s, 0);                                   /* pad3 (2 bytes) */
	                                                             /* entries */

	if (!Stream_EnsureRemainingCapacity(s, info->keyCount * 8ull))
		return FALSE;

	for (UINT32 index = 0; index < info->keyCount; index++)
	{
		const UINT32 key1 = (UINT32)info->keyList[index];
		const UINT32 key2 = (UINT32)(info->keyList[index] >> 32);
		Stream_Write_UINT32(s, key1);
		Stream_Write_UINT32(s, key2);
	}

	return TRUE;
}

static UINT32 rdp_load_persistent_key_list(rdpRdp* rdp, UINT64** pKeyList)
{
	int index;
	int count;
	int status;
	UINT32 keyCount;
	UINT64* keyList = NULL;
	rdpPersistentCache* persistent;
	rdpSettings* settings = rdp->settings;

	*pKeyList = NULL;

	if (!settings->BitmapCachePersistEnabled)
		return 0;

	if (!settings->BitmapCachePersistFile)
		return 0;

	persistent = persistent_cache_new();

	if (!persistent)
		return 0;

	status = persistent_cache_open(persistent, settings->BitmapCachePersistFile, FALSE, 0);

	if (status < 1)
		goto error;

	count = persistent_cache_get_count(persistent);

	keyCount = (UINT32)count;
	keyList = (UINT64*)malloc(keyCount * sizeof(UINT64));

	if (!keyList)
		goto error;

	for (index = 0; index < count; index++)
	{
		PERSISTENT_CACHE_ENTRY cacheEntry = { 0 };

		if (persistent_cache_read_entry(persistent, &cacheEntry) < 1)
			continue;

		keyList[index] = cacheEntry.key64;
	}

	*pKeyList = keyList;

	persistent_cache_free(persistent);
	return keyCount;
error:
	persistent_cache_free(persistent);
	free(keyList);
	return 0;
}

BOOL rdp_send_client_persistent_key_list_pdu(rdpRdp* rdp)
{
	UINT32 keyCount;
	UINT32 keyMaxFrag = 2042;
	UINT64* keyList = NULL;
	RDP_BITMAP_PERSISTENT_INFO info;
	rdpSettings* settings = rdp->settings;

	keyCount = rdp_load_persistent_key_list(rdp, &keyList);

	WLog_DBG(TAG, "Persistent Key List: TotalKeyCount: %" PRIu32 " MaxKeyFrag: %" PRIu32, keyCount,
	         keyMaxFrag);

	// MS-RDPBCGR recommends sending no more than 169 entries at once.
	// In practice, sending more than 2042 entries at once triggers an error.
	// It should be possible to advertise the entire client bitmap cache
	// by sending multiple persistent key list PDUs, but the current code
	// only bothers sending a single, smaller list of entries instead.

	if (keyCount > keyMaxFrag)
		keyCount = keyMaxFrag;

	info.totalEntriesCache0 = settings->BitmapCacheV2CellInfo[0].numEntries;
	info.totalEntriesCache1 = settings->BitmapCacheV2CellInfo[1].numEntries;
	info.totalEntriesCache2 = settings->BitmapCacheV2CellInfo[2].numEntries;
	info.totalEntriesCache3 = settings->BitmapCacheV2CellInfo[3].numEntries;
	info.totalEntriesCache4 = settings->BitmapCacheV2CellInfo[4].numEntries;

	info.numEntriesCache0 = MIN(keyCount, info.totalEntriesCache0);
	keyCount -= info.numEntriesCache0;
	info.numEntriesCache1 = MIN(keyCount, info.totalEntriesCache1);
	keyCount -= info.numEntriesCache1;
	info.numEntriesCache2 = MIN(keyCount, info.totalEntriesCache2);
	keyCount -= info.numEntriesCache2;
	info.numEntriesCache3 = MIN(keyCount, info.totalEntriesCache3);
	keyCount -= info.numEntriesCache3;
	info.numEntriesCache4 = MIN(keyCount, info.totalEntriesCache4);
	keyCount -= info.numEntriesCache4;

	info.totalEntriesCache0 = info.numEntriesCache0;
	info.totalEntriesCache1 = info.numEntriesCache1;
	info.totalEntriesCache2 = info.numEntriesCache2;
	info.totalEntriesCache3 = info.numEntriesCache3;
	info.totalEntriesCache4 = info.numEntriesCache4;

	keyCount = info.totalEntriesCache0 + info.totalEntriesCache1 + info.totalEntriesCache2 +
	           info.totalEntriesCache3 + info.totalEntriesCache4;

	info.keyCount = keyCount;
	info.keyList = keyList;

	WLog_DBG(TAG, "persistentKeyList count: %" PRIu32, info.keyCount);

	WLog_DBG(TAG,
	         "numEntriesCache: [0]: %" PRIu16 " [1]: %" PRIu16 " [2]: %" PRIu16 " [3]: %" PRIu16
	         " [4]: %" PRIu16,
	         info.numEntriesCache0, info.numEntriesCache1, info.numEntriesCache2,
	         info.numEntriesCache3, info.numEntriesCache4);

	WLog_DBG(TAG,
	         "totalEntriesCache: [0]: %" PRIu16 " [1]: %" PRIu16 " [2]: %" PRIu16 " [3]: %" PRIu16
	         " [4]: %" PRIu16,
	         info.totalEntriesCache0, info.totalEntriesCache1, info.totalEntriesCache2,
	         info.totalEntriesCache3, info.totalEntriesCache4);

	wStream* s = rdp_data_pdu_init(rdp);

	if (!s)
		return FALSE;

	if (!rdp_write_client_persistent_key_list_pdu(s, &info))
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	WINPR_ASSERT(rdp->mcs);
	free(keyList);

	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_BITMAP_CACHE_PERSISTENT_LIST, rdp->mcs->userId);
}

BOOL rdp_recv_client_font_list_pdu(wStream* s)
{
	WINPR_ASSERT(s);
	/* 2.2.1.18 Client Font List PDU */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return FALSE;

	return Stream_SafeSeek(s, 8);
}

BOOL rdp_recv_client_persistent_key_list_pdu(wStream* s)
{
	BYTE flags;
	size_t count = 0;
	size_t total = 0;
	UINT16 cache, x;

	WINPR_ASSERT(s);

	/* 2.2.1.17.1 Persistent Key List PDU Data (TS_BITMAPCACHE_PERSISTENT_LIST_PDU) */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 21))
	{
		WLog_ERR(TAG, "short TS_BITMAPCACHE_PERSISTENT_LIST_PDU, need 21 bytes, got %" PRIuz,
		         Stream_GetRemainingLength(s));
		return FALSE;
	}
	/* Read numEntriesCacheX for variable length data in PDU */
	for (x = 0; x < 5; x++)
	{
		Stream_Read_UINT16(s, cache);
		count += cache;
	}

	/* Skip totalEntriesCacheX */
	for (x = 0; x < 5; x++)
	{
		UINT16 tmp;
		Stream_Read_UINT16(s, tmp);
		total += tmp;
	}

	if (total > 262144)
	{
		WLog_ERR(TAG,
		         "TS_BITMAPCACHE_PERSISTENT_LIST_PDU::totalEntriesCacheX exceeds 262144 entries");
		return FALSE;
	}

	Stream_Read_UINT8(s, flags);
	if ((flags & ~(PERSIST_LAST_PDU | PERSIST_FIRST_PDU)) != 0)
	{
		WLog_ERR(TAG,
		         "TS_BITMAPCACHE_PERSISTENT_LIST_PDU::bBitMask has an invalid value of 0x%02" PRIx8,
		         flags);
		return FALSE;
	}

	/* Skip padding */
	if (!Stream_SafeSeek(s, 3))
	{
		WLog_ERR(TAG, "short TS_BITMAPCACHE_PERSISTENT_LIST_PDU, need 3 bytes, got %" PRIuz,
		         Stream_GetRemainingLength(s));
		return FALSE;
	}
	/* Skip actual entries sent by client */
	if (!Stream_SafeSeek(s, count * sizeof(UINT64)))
	{
		WLog_ERR(TAG,
		         "short TS_BITMAPCACHE_PERSISTENT_LIST_PDU, need %" PRIuz " bytes, got %" PRIuz,
		         count * sizeof(UINT64), Stream_GetRemainingLength(s));
		return FALSE;
	}
	return TRUE;
}

static BOOL rdp_write_client_font_list_pdu(wStream* s, UINT16 flags)
{
	WINPR_ASSERT(s);

	if (Stream_GetRemainingCapacity(s) < 8)
		return FALSE;
	Stream_Write_UINT16(s, 0);     /* numberFonts (2 bytes) */
	Stream_Write_UINT16(s, 0);     /* totalNumFonts (2 bytes) */
	Stream_Write_UINT16(s, flags); /* listFlags (2 bytes) */
	Stream_Write_UINT16(s, 50);    /* entrySize (2 bytes) */
	return TRUE;
}

BOOL rdp_send_client_font_list_pdu(rdpRdp* rdp, UINT16 flags)
{
	wStream* s = rdp_data_pdu_init(rdp);
	if (!s)
		return FALSE;
	if (!rdp_write_client_font_list_pdu(s, flags))
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}

	WINPR_ASSERT(rdp->mcs);
	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_FONT_LIST, rdp->mcs->userId);
}

BOOL rdp_recv_font_map_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 numberEntries, totalNumEntries, mapFlags, entrySize;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(rdp->settings);
	WINPR_ASSERT(s);
	WINPR_ASSERT(!freerdp_settings_get_bool(rdp->settings, FreeRDP_ServerMode));

	/* Do not fail here, see https://github.com/FreeRDP/FreeRDP/issues/925 */
	if (Stream_CheckAndLogRequiredLength(TAG, s, 8))
	{
		Stream_Read_UINT16(s, numberEntries); /* numberEntries (2 bytes) */
		if (numberEntries != 0)
			WLog_WARN(
			    TAG,
			    "[MS-RDPBCGR] 2.2.1.22.1 Font Map PDU Data (TS_FONT_MAP_PDU)::numberEntries != 0 "
			    "[%" PRIu16 "]",
			    numberEntries);
		Stream_Read_UINT16(s, totalNumEntries); /* totalNumEntries (2 bytes) */
		if (totalNumEntries != 0)
			WLog_WARN(
			    TAG,
			    "[MS-RDPBCGR] 2.2.1.22.1 Font Map PDU Data (TS_FONT_MAP_PDU)::totalNumEntries != "
			    "0 [%" PRIu16 "]",
			    totalNumEntries);
		Stream_Read_UINT16(s, mapFlags); /* mapFlags (2 bytes) */
		if (mapFlags != (FONTLIST_FIRST | FONTLIST_LAST))
			WLog_WARN(
			    TAG,
			    "[MS-RDPBCGR] 2.2.1.22.1 Font Map PDU Data (TS_FONT_MAP_PDU)::mapFlags != 0x0003 "
			    "(FONTLIST_FIRST | FONTLIST_LAST) "
			    "[0x%04" PRIx16 "]",
			    mapFlags);
		Stream_Read_UINT16(s, entrySize); /* entrySize (2 bytes) */
		if (entrySize != 4)
			WLog_WARN(TAG,
			          "[MS-RDPBCGR] 2.2.1.22.1 Font Map PDU Data (TS_FONT_MAP_PDU)::entrySize != 4 "
			          "[%" PRIu16 "]",
			          entrySize);
	}
	else
		WLog_WARN(TAG,
		          "[MS-RDPBCGR] 2.2.1.22.1 Font Map PDU Data (TS_FONT_MAP_PDU) paylaod size is "
		          "0 instead of 8");

	return rdp_finalize_set_flag(rdp, FINALIZE_SC_FONT_MAP_PDU);
}

BOOL rdp_send_server_font_map_pdu(rdpRdp* rdp)
{
	wStream* s = rdp_data_pdu_init(rdp);
	if (!s)
		return FALSE;
	if (Stream_GetRemainingCapacity(s) < 8)
	{
		Stream_Free(s, TRUE);
		return FALSE;
	}
	Stream_Write_UINT16(s, 0);                              /* numberEntries (2 bytes) */
	Stream_Write_UINT16(s, 0);                              /* totalNumEntries (2 bytes) */
	Stream_Write_UINT16(s, FONTLIST_FIRST | FONTLIST_LAST); /* mapFlags (2 bytes) */
	Stream_Write_UINT16(s, 4);                              /* entrySize (2 bytes) */

	WINPR_ASSERT(rdp->mcs);
	return rdp_send_data_pdu(rdp, s, DATA_PDU_TYPE_FONT_MAP, rdp->mcs->userId);
}

BOOL rdp_recv_deactivate_all(rdpRdp* rdp, wStream* s)
{
	UINT16 lengthSourceDescriptor;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	if (rdp_get_state(rdp) == CONNECTION_STATE_ACTIVE)
	{
		if (!rdp_finalize_set_flag(rdp, FINALIZE_DEACTIVATE_REACTIVATE))
			return FALSE;

		rdp->was_deactivated = TRUE;
		rdp->deactivated_height = freerdp_settings_get_uint32(rdp->settings, FreeRDP_DesktopHeight);
		rdp->deactivated_width = freerdp_settings_get_uint32(rdp->settings, FreeRDP_DesktopWidth);
	}

	/*
	 * Windows XP can send short DEACTIVATE_ALL PDU that doesn't contain
	 * the following fields.
	 */

	WINPR_ASSERT(rdp->settings);
	if (Stream_GetRemainingLength(s) > 0)
	{
		do
		{
			UINT32 ShareId;
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
				break;

			Stream_Read_UINT32(s, ShareId); /* shareId (4 bytes) */
			if (!freerdp_settings_set_uint32(rdp->settings, FreeRDP_ShareId, ShareId))
				return FALSE;

			if (!Stream_CheckAndLogRequiredLength(TAG, s, 2))
				break;

			Stream_Read_UINT16(s, lengthSourceDescriptor); /* lengthSourceDescriptor (2 bytes) */

			if (!Stream_CheckAndLogRequiredLength(TAG, s, lengthSourceDescriptor))
				break;

			Stream_Seek(s, lengthSourceDescriptor); /* sourceDescriptor (should be 0x00) */
		} while (0);
	}

	return rdp_client_transition_to_state(rdp,
	                                      CONNECTION_STATE_CAPABILITIES_EXCHANGE_DEMAND_ACTIVE);
}

BOOL rdp_send_deactivate_all(rdpRdp* rdp)
{
	wStream* s = rdp_send_stream_pdu_init(rdp);
	BOOL status = FALSE;

	if (!s)
		return FALSE;

	if (Stream_GetRemainingCapacity(s) < 7)
		goto fail;

	WINPR_ASSERT(rdp->settings);
	const UINT32 ShareId = freerdp_settings_get_uint32(rdp->settings, FreeRDP_ShareId);
	Stream_Write_UINT32(s, ShareId); /* shareId (4 bytes) */
	Stream_Write_UINT16(s, 1);       /* lengthSourceDescriptor (2 bytes) */
	Stream_Write_UINT8(s, 0);        /* sourceDescriptor (should be 0x00) */

	WINPR_ASSERT(rdp->mcs);
	status = rdp_send_pdu(rdp, s, PDU_TYPE_DEACTIVATE_ALL, rdp->mcs->userId);
fail:
	Stream_Release(s);
	return status;
}

BOOL rdp_server_accept_client_control_pdu(rdpRdp* rdp, wStream* s)
{
	UINT16 action = 0;
	UINT16 GrantId = 0;
	UINT32 ControlId = 0;
	const CONNECTION_STATE state = rdp_get_state(rdp);

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	if (!rdp_recv_control_pdu(s, &action, &GrantId, &ControlId))
		return FALSE;

	switch (action)
	{

		case CTRLACTION_REQUEST_CONTROL:
			if (!rdp_finalize_is_flag_set(rdp, FINALIZE_CS_CONTROL_COOPERATE_PDU))
			{
				char abuffer[128] = { 0 };
				char buffer[1024] = { 0 };
				WLog_WARN(TAG,
				          "Received action=%s with GrantId=0x%04" PRIx16 ", ControlId=0x%08" PRIx32
				          " in unexpected state %s [missing %s]",
				          rdp_ctrlaction_string(action, abuffer, sizeof(abuffer)), GrantId,
				          ControlId, rdp_state_string(state),
				          rdp_finalize_flags_to_str(FINALIZE_CS_CONTROL_COOPERATE_PDU, buffer,
				                                    sizeof(buffer)));
				return FALSE;
			}
			if ((GrantId != 0) || (ControlId != 0))
			{
				WLog_WARN(TAG,
				          "Received CTRLACTION_COOPERATE with GrantId=0x%04" PRIx16
				          " != 0x00, ControlId=0x%08" PRIx32 " != 0x00",
				          GrantId, ControlId);
				return FALSE;
			}
			return rdp_finalize_set_flag(rdp, FINALIZE_CS_CONTROL_REQUEST_PDU);
		case CTRLACTION_COOPERATE:
			if (!rdp_finalize_is_flag_set(rdp, FINALIZE_CS_SYNCHRONIZE_PDU))
			{
				char abuffer[128] = { 0 };
				char buffer[1024] = { 0 };
				WLog_WARN(
				    TAG,
				    "Received action=%s with GrantId=0x%04" PRIx16 ", ControlId=0x%08" PRIx32
				    " in unexpected state %s [missing %s]",
				    rdp_ctrlaction_string(action, abuffer, sizeof(abuffer)), GrantId, ControlId,
				    rdp_state_string(state),
				    rdp_finalize_flags_to_str(FINALIZE_CS_SYNCHRONIZE_PDU, buffer, sizeof(buffer)));
				return FALSE;
			}
			if ((GrantId != 0) || (ControlId != 0))
			{
				WLog_WARN(TAG,
				          "Received CTRLACTION_COOPERATE with GrantId=0x%04" PRIx16
				          " != 0x00, ControlId=0x%08" PRIx32 " != 0x00",
				          GrantId, ControlId);
				return FALSE;
			}
			return rdp_finalize_set_flag(rdp, FINALIZE_CS_CONTROL_COOPERATE_PDU);
		default:
		{
			char abuffer[128] = { 0 };
			WLog_WARN(TAG,
			          "Received unexpected action=%s with GrantId=0x%04" PRIx16
			          ", ControlId=0x%08" PRIx32,
			          rdp_ctrlaction_string(action, abuffer, sizeof(abuffer)), GrantId, ControlId);
			return FALSE;
		}
	}

	return TRUE;
}

BOOL rdp_server_accept_client_font_list_pdu(rdpRdp* rdp, wStream* s)
{
	rdpSettings* settings;
	freerdp_peer* peer;

	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	settings = rdp->settings;
	WINPR_ASSERT(settings);

	WINPR_ASSERT(rdp->context);
	peer = rdp->context->peer;
	WINPR_ASSERT(peer);

	if (!rdp_recv_client_font_list_pdu(s))
		return FALSE;
	rdp_finalize_set_flag(rdp, FINALIZE_CS_FONT_LIST_PDU);

	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_CLIENT_SYNC))
		return FALSE;

	if (!rdp_send_server_synchronize_pdu(rdp))
		return FALSE;

	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_CLIENT_COOPERATE))
		return FALSE;

	if (!rdp_send_server_control_cooperate_pdu(rdp))
		return FALSE;

	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_CLIENT_GRANTED_CONTROL))
		return FALSE;

	if (!rdp_send_server_control_granted_pdu(rdp))
		return FALSE;

	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_FINALIZATION_CLIENT_FONT_MAP))
		return FALSE;

	if (!rdp_send_server_font_map_pdu(rdp))
		return FALSE;

	if (!rdp_server_transition_to_state(rdp, CONNECTION_STATE_ACTIVE))
		return FALSE;

	return TRUE;
}

BOOL rdp_server_accept_client_persistent_key_list_pdu(rdpRdp* rdp, wStream* s)
{
	WINPR_ASSERT(rdp);
	WINPR_ASSERT(s);

	if (!rdp_recv_client_persistent_key_list_pdu(s))
		return FALSE;

	rdp_finalize_set_flag(rdp, FINALIZE_CS_PERSISTENT_KEY_LIST_PDU);
	// TODO: Actually do something with this
	return TRUE;
}

const char* rdp_ctrlaction_string(UINT16 action, char* buffer, size_t size)
{
	const char* actstr;
	switch (action)
	{
		case CTRLACTION_COOPERATE:
			actstr = "CTRLACTION_COOPERATE";
			break;
		case CTRLACTION_DETACH:
			actstr = "CTRLACTION_DETACH";
			break;
		case CTRLACTION_GRANTED_CONTROL:
			actstr = "CTRLACTION_GRANTED_CONTROL";
			break;
		case CTRLACTION_REQUEST_CONTROL:
			actstr = "CTRLACTION_REQUEST_CONTROL";
			break;
		default:
			actstr = "CTRLACTION_UNKNOWN";
			break;
	}

	_snprintf(buffer, size, "%s [0x%04" PRIx16 "]", actstr, action);
	return buffer;
}
