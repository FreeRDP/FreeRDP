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

#define SMARTCARD_COMMON_TYPE_HEADER_LENGTH 8
#define SMARTCARD_PRIVATE_TYPE_HEADER_LENGTH 8

#include "smartcard_main.h"

LONG smartcard_pack_write_size_align(SMARTCARD_DEVICE* smartcard, wStream* s, size_t size,
                                     UINT32 alignment);
LONG smartcard_unpack_read_size_align(SMARTCARD_DEVICE* smartcard, wStream* s, size_t size,
                                      UINT32 alignment);

SCARDCONTEXT smartcard_scard_context_native_from_redir(SMARTCARD_DEVICE* smartcard,
                                                       REDIR_SCARDCONTEXT* context);
void smartcard_scard_context_native_to_redir(SMARTCARD_DEVICE* smartcard,
                                             REDIR_SCARDCONTEXT* context, SCARDCONTEXT hContext);

SCARDHANDLE smartcard_scard_handle_native_from_redir(SMARTCARD_DEVICE* smartcard,
                                                     REDIR_SCARDHANDLE* handle);
void smartcard_scard_handle_native_to_redir(SMARTCARD_DEVICE* smartcard, REDIR_SCARDHANDLE* handle,
                                            SCARDHANDLE hCard);

LONG smartcard_unpack_common_type_header(SMARTCARD_DEVICE* smartcard, wStream* s);
void smartcard_pack_common_type_header(SMARTCARD_DEVICE* smartcard, wStream* s);

LONG smartcard_unpack_private_type_header(SMARTCARD_DEVICE* smartcard, wStream* s);
void smartcard_pack_private_type_header(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        UINT32 objectBufferLength);

LONG smartcard_unpack_establish_context_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                             EstablishContext_Call* call);

LONG smartcard_pack_establish_context_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                             const EstablishContext_Return* ret);

LONG smartcard_unpack_context_call(SMARTCARD_DEVICE* smartcard, wStream* s, Context_Call* call,
                                   const char* name);

void smartcard_trace_long_return(SMARTCARD_DEVICE* smartcard, const Long_Return* ret,
                                 const char* name);

LONG smartcard_unpack_list_reader_groups_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              ListReaderGroups_Call* call, BOOL unicode);

LONG smartcard_pack_list_reader_groups_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              const ListReaderGroups_Return* ret, BOOL unicode);

LONG smartcard_unpack_list_readers_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        ListReaders_Call* call, BOOL unicode);

LONG smartcard_pack_list_readers_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        const ListReaders_Return* ret, BOOL unicode);

LONG smartcard_unpack_context_and_two_strings_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                     ContextAndTwoStringA_Call* call);

LONG smartcard_unpack_context_and_two_strings_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                     ContextAndTwoStringW_Call* call);

LONG smartcard_unpack_context_and_string_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                ContextAndStringA_Call* call);

LONG smartcard_unpack_context_and_string_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                ContextAndStringW_Call* call);

LONG smartcard_unpack_locate_cards_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                          LocateCardsA_Call* call);

LONG smartcard_pack_locate_cards_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        const LocateCards_Return* ret);

LONG smartcard_unpack_locate_cards_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                          LocateCardsW_Call* call);

LONG smartcard_pack_locate_cards_w_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                          const LocateCardsW_Call* ret);

LONG smartcard_unpack_connect_a_call(SMARTCARD_DEVICE* smartcard, wStream* s, ConnectA_Call* call);

LONG smartcard_unpack_connect_w_call(SMARTCARD_DEVICE* smartcard, wStream* s, ConnectW_Call* call);

LONG smartcard_pack_connect_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                   const Connect_Return* ret);

LONG smartcard_unpack_reconnect_call(SMARTCARD_DEVICE* smartcard, wStream* s, Reconnect_Call* call);

LONG smartcard_pack_reconnect_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                     const Reconnect_Return* ret);

LONG smartcard_unpack_hcard_and_disposition_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                 HCardAndDisposition_Call* call, const char* name);

LONG smartcard_unpack_get_status_change_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                               GetStatusChangeA_Call* call);

LONG smartcard_unpack_get_status_change_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                               GetStatusChangeW_Call* call);

LONG smartcard_pack_get_status_change_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                             const GetStatusChange_Return* ret, BOOL unicode);

LONG smartcard_unpack_state_call(SMARTCARD_DEVICE* smartcard, wStream* s, State_Call* call);
LONG smartcard_pack_state_return(SMARTCARD_DEVICE* smartcard, wStream* s, const State_Return* ret);

LONG smartcard_unpack_status_call(SMARTCARD_DEVICE* smartcard, wStream* s, Status_Call* call,
                                  BOOL unicode);

LONG smartcard_pack_status_return(SMARTCARD_DEVICE* smartcard, wStream* s, const Status_Return* ret,
                                  BOOL unicode);

LONG smartcard_unpack_get_attrib_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                      GetAttrib_Call* call);

LONG smartcard_pack_get_attrib_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                      const GetAttrib_Return* ret, DWORD dwAttrId,
                                      DWORD cbAttrCallLen);

LONG smartcard_unpack_set_attrib_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                      SetAttrib_Call* call);

LONG smartcard_unpack_control_call(SMARTCARD_DEVICE* smartcard, wStream* s, Control_Call* call);

LONG smartcard_pack_control_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                   const Control_Return* ret);

LONG smartcard_unpack_transmit_call(SMARTCARD_DEVICE* smartcard, wStream* s, Transmit_Call* call);

LONG smartcard_pack_transmit_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                    const Transmit_Return* ret);

LONG smartcard_unpack_locate_cards_by_atr_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                 LocateCardsByATRA_Call* call);

LONG smartcard_unpack_locate_cards_by_atr_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                                 LocateCardsByATRW_Call* call);

LONG smartcard_unpack_read_cache_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        ReadCacheA_Call* call);

LONG smartcard_unpack_read_cache_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                        ReadCacheW_Call* call);

LONG smartcard_pack_read_cache_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                      const ReadCache_Return* ret);

LONG smartcard_unpack_write_cache_a_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                         WriteCacheA_Call* call);

LONG smartcard_unpack_write_cache_w_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                         WriteCacheW_Call* call);

LONG smartcard_unpack_get_transmit_count_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              GetTransmitCount_Call* call);
LONG smartcard_pack_get_transmit_count_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              const GetTransmitCount_Return* call);

LONG smartcard_unpack_get_reader_icon_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                           GetReaderIcon_Call* call);
LONG smartcard_pack_get_reader_icon_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                           const GetReaderIcon_Return* ret);

LONG smartcard_unpack_get_device_type_id_call(SMARTCARD_DEVICE* smartcard, wStream* s,
                                              GetDeviceTypeId_Call* call);

LONG smartcard_pack_device_type_id_return(SMARTCARD_DEVICE* smartcard, wStream* s,
                                          const GetDeviceTypeId_Return* ret);

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_PACK_H */
