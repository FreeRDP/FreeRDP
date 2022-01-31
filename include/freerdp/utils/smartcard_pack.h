/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smart Card Structure Packing
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
 * Copyright 2020 Armin Novak <armin.novak@thincast.com>
 * Copyright 2020 Thincast Technologies GmbH
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

#ifndef FREERDP_CHANNEL_SMARTCARD_CLIENT_PACK_H
#define FREERDP_CHANNEL_SMARTCARD_CLIENT_PACK_H

#include <winpr/crt.h>
#include <winpr/stream.h>
#include <winpr/smartcard.h>

#include <freerdp/api.h>
#include <freerdp/channels/scard.h>

#define SMARTCARD_COMMON_TYPE_HEADER_LENGTH 8
#define SMARTCARD_PRIVATE_TYPE_HEADER_LENGTH 8

FREERDP_API LONG smartcard_pack_write_size_align(wStream* s, size_t size, UINT32 alignment);
FREERDP_API LONG smartcard_unpack_read_size_align(wStream* s, size_t size, UINT32 alignment);

FREERDP_API SCARDCONTEXT smartcard_scard_context_native_from_redir(REDIR_SCARDCONTEXT* context);
FREERDP_API void smartcard_scard_context_native_to_redir(REDIR_SCARDCONTEXT* context,
                                                         SCARDCONTEXT hContext);

FREERDP_API SCARDHANDLE smartcard_scard_handle_native_from_redir(REDIR_SCARDHANDLE* handle);
FREERDP_API void smartcard_scard_handle_native_to_redir(REDIR_SCARDHANDLE* handle,
                                                        SCARDHANDLE hCard);

FREERDP_API LONG smartcard_unpack_common_type_header(wStream* s);
FREERDP_API void smartcard_pack_common_type_header(wStream* s);

FREERDP_API LONG smartcard_unpack_private_type_header(wStream* s);
FREERDP_API void smartcard_pack_private_type_header(wStream* s, UINT32 objectBufferLength);

FREERDP_API LONG smartcard_unpack_establish_context_call(wStream* s, EstablishContext_Call* call);

FREERDP_API LONG smartcard_pack_establish_context_return(wStream* s,
                                                         const EstablishContext_Return* ret);

FREERDP_API LONG smartcard_unpack_context_call(wStream* s, Context_Call* call, const char* name);

FREERDP_API void smartcard_trace_long_return(const Long_Return* ret, const char* name);

FREERDP_API LONG smartcard_unpack_list_reader_groups_call(wStream* s, ListReaderGroups_Call* call,
                                                          BOOL unicode);

FREERDP_API LONG smartcard_pack_list_reader_groups_return(wStream* s,
                                                          const ListReaderGroups_Return* ret,
                                                          BOOL unicode);

FREERDP_API LONG smartcard_unpack_list_readers_call(wStream* s, ListReaders_Call* call,
                                                    BOOL unicode);

FREERDP_API LONG smartcard_pack_list_readers_return(wStream* s, const ListReaders_Return* ret,
                                                    BOOL unicode);

FREERDP_API LONG smartcard_unpack_context_and_two_strings_a_call(wStream* s,
                                                                 ContextAndTwoStringA_Call* call);

FREERDP_API LONG smartcard_unpack_context_and_two_strings_w_call(wStream* s,
                                                                 ContextAndTwoStringW_Call* call);

FREERDP_API LONG smartcard_unpack_context_and_string_a_call(wStream* s,
                                                            ContextAndStringA_Call* call);

FREERDP_API LONG smartcard_unpack_context_and_string_w_call(wStream* s,
                                                            ContextAndStringW_Call* call);

FREERDP_API LONG smartcard_unpack_locate_cards_a_call(wStream* s, LocateCardsA_Call* call);

FREERDP_API LONG smartcard_pack_locate_cards_return(wStream* s, const LocateCards_Return* ret);

FREERDP_API LONG smartcard_unpack_locate_cards_w_call(wStream* s, LocateCardsW_Call* call);

FREERDP_API LONG smartcard_pack_locate_cards_w_return(wStream* s, const LocateCardsW_Call* ret);

FREERDP_API LONG smartcard_unpack_connect_a_call(wStream* s, ConnectA_Call* call);

FREERDP_API LONG smartcard_unpack_connect_w_call(wStream* s, ConnectW_Call* call);

FREERDP_API LONG smartcard_pack_connect_return(wStream* s, const Connect_Return* ret);

FREERDP_API LONG smartcard_unpack_reconnect_call(wStream* s, Reconnect_Call* call);

FREERDP_API LONG smartcard_pack_reconnect_return(wStream* s, const Reconnect_Return* ret);

FREERDP_API LONG smartcard_unpack_hcard_and_disposition_call(wStream* s,
                                                             HCardAndDisposition_Call* call,
                                                             const char* name);

FREERDP_API LONG smartcard_unpack_get_status_change_a_call(wStream* s, GetStatusChangeA_Call* call);

FREERDP_API LONG smartcard_unpack_get_status_change_w_call(wStream* s, GetStatusChangeW_Call* call);

FREERDP_API LONG smartcard_pack_get_status_change_return(wStream* s,
                                                         const GetStatusChange_Return* ret,
                                                         BOOL unicode);

FREERDP_API LONG smartcard_unpack_state_call(wStream* s, State_Call* call);
FREERDP_API LONG smartcard_pack_state_return(wStream* s, const State_Return* ret);

FREERDP_API LONG smartcard_unpack_status_call(wStream* s, Status_Call* call, BOOL unicode);

FREERDP_API LONG smartcard_pack_status_return(wStream* s, const Status_Return* ret, BOOL unicode);

FREERDP_API LONG smartcard_unpack_get_attrib_call(wStream* s, GetAttrib_Call* call);

FREERDP_API LONG smartcard_pack_get_attrib_return(wStream* s, const GetAttrib_Return* ret,
                                                  DWORD dwAttrId, DWORD cbAttrCallLen);

FREERDP_API LONG smartcard_unpack_set_attrib_call(wStream* s, SetAttrib_Call* call);

FREERDP_API LONG smartcard_unpack_control_call(wStream* s, Control_Call* call);

FREERDP_API LONG smartcard_pack_control_return(wStream* s, const Control_Return* ret);

FREERDP_API LONG smartcard_unpack_transmit_call(wStream* s, Transmit_Call* call);

FREERDP_API LONG smartcard_pack_transmit_return(wStream* s, const Transmit_Return* ret);

FREERDP_API LONG smartcard_unpack_locate_cards_by_atr_a_call(wStream* s,
                                                             LocateCardsByATRA_Call* call);

FREERDP_API LONG smartcard_unpack_locate_cards_by_atr_w_call(wStream* s,
                                                             LocateCardsByATRW_Call* call);

FREERDP_API LONG smartcard_unpack_read_cache_a_call(wStream* s, ReadCacheA_Call* call);

FREERDP_API LONG smartcard_unpack_read_cache_w_call(wStream* s, ReadCacheW_Call* call);

FREERDP_API LONG smartcard_pack_read_cache_return(wStream* s, const ReadCache_Return* ret);

FREERDP_API LONG smartcard_unpack_write_cache_a_call(wStream* s, WriteCacheA_Call* call);

FREERDP_API LONG smartcard_unpack_write_cache_w_call(wStream* s, WriteCacheW_Call* call);

FREERDP_API LONG smartcard_unpack_get_transmit_count_call(wStream* s, GetTransmitCount_Call* call);
FREERDP_API LONG smartcard_pack_get_transmit_count_return(wStream* s,
                                                          const GetTransmitCount_Return* call);

FREERDP_API LONG smartcard_unpack_get_reader_icon_call(wStream* s, GetReaderIcon_Call* call);
FREERDP_API LONG smartcard_pack_get_reader_icon_return(wStream* s, const GetReaderIcon_Return* ret);

FREERDP_API LONG smartcard_unpack_get_device_type_id_call(wStream* s, GetDeviceTypeId_Call* call);

FREERDP_API LONG smartcard_pack_device_type_id_return(wStream* s,
                                                      const GetDeviceTypeId_Return* ret);

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_PACK_H */
