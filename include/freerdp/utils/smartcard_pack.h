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

#ifdef __cplusplus
extern "C"
{
#endif

	/** @brief Write adding bytes to align to \p alignment.
	 *  @param s         Stream to write padding into.
	 *  @param size      Number of data bytes written so far.
	 *  @param alignment Required alignment boundary.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_write_size_align(wStream* s, size_t size, UINT32 alignment);

	/** @brief Read and skip padding bytes to align to \p alignment.
	 *  @param s         Stream to read padding from.
	 *  @param size      Number of data bytes read so far.
	 *  @param alignment Required alignment boundary.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_read_size_align(wStream* s, size_t size, UINT32 alignment);

	/** @brief Convert a REDIR_SCARDCONTEXT to a native SCARDCONTEXT.
	 *  @param context The redirected context to convert.
	 *  @return The native SCARDCONTEXT value.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API SCARDCONTEXT
	smartcard_scard_context_native_from_redir(const REDIR_SCARDCONTEXT* context);

	/** @brief Convert a native SCARDCONTEXT to a REDIR_SCARDCONTEXT.
	 *  @param context  [out] The redirected context to populate.
	 *  @param hContext The native SCARDCONTEXT value.
	 */
	FREERDP_API void smartcard_scard_context_native_to_redir(REDIR_SCARDCONTEXT* context,
	                                                         SCARDCONTEXT hContext);

	/** @brief Convert a REDIR_SCARDHANDLE to a native SCARDHANDLE.
	 *  @param handle The redirected handle to convert.
	 *  @return The native SCARDHANDLE value.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API SCARDHANDLE
	smartcard_scard_handle_native_from_redir(const REDIR_SCARDHANDLE* handle);

	/** @brief Convert a native SCARDHANDLE to a REDIR_SCARDHANDLE.
	 *  @param handle [out] The redirected handle to populate.
	 *  @param hCard  The native SCARDHANDLE value.
	 */
	FREERDP_API void smartcard_scard_handle_native_to_redir(REDIR_SCARDHANDLE* handle,
	                                                        SCARDHANDLE hCard);

	/** @brief Unpack the Common Type Header [MS-RPCE] from a stream.
	 *  @param s Stream to read from.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_common_type_header(wStream* s);

	/** @brief Pack the Common Type Header [MS-RPCE] into a stream.
	 *  @param s Stream to write into.
	 */
	FREERDP_API void smartcard_pack_common_type_header(wStream* s);

	/** @brief Unpack the Private Type Header [MS-RPCE] from a stream.
	 *  @param s Stream to read from.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_private_type_header(wStream* s);

	/** @brief Pack the Private Type Header [MS-RPCE] into a stream.
	 *  @param s                  Stream to write into.
	 *  @param objectBufferLength Length of the payload following this header.
	 */
	FREERDP_API void smartcard_pack_private_type_header(wStream* s, UINT32 objectBufferLength);

	/** @brief Unpack an EstablishContext_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded EstablishContext_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_establish_context_call(wStream* s,
	                                                         EstablishContext_Call* call);

	/** @brief Pack an EstablishContext_Call into a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call The EstablishContext_Call to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_establish_context_call(wStream* s,
	                                                       const EstablishContext_Call* call);

	/** @brief Pack an EstablishContext_Return into a stream.
	 *  @param s   Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The EstablishContext_Return to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_establish_context_return(wStream* s,
	                                                         const EstablishContext_Return* ret);

	/** @brief Unpack an EstablishContext_Return from a stream.
	 *  @param s   Stream positioned after the the Common/Private Type Headers and the ReturnCode.
	 *  @param ret [out] The decoded EstablishContext_Return.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_establish_context_return(wStream* s,
	                                                           EstablishContext_Return* ret);

	/** @brief Unpack a Context_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded Context_Call.
	 *  @param name Operation name used for trace logging.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_context_call(wStream* s, Context_Call* call,
	                                               const char* name);

	/** @brief Log a Long_Return for tracing purposes.
	 *  @param ret  The Long_Return to trace.
	 *  @param name Operation name used for the log message.
	 */
	FREERDP_API void smartcard_trace_long_return(const Long_Return* ret, const char* name);

	/** @brief Unpack a ListReaderGroups_Call from a stream.
	 *  @param s       Stream positioned after the Common/Private Type Headers.
	 *  @param call    [out] The decoded ListReaderGroups_Call.
	 *  @param unicode TRUE for wide-char (W) variant, FALSE for ANSI (A).
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_list_reader_groups_call(wStream* s,
	                                                          ListReaderGroups_Call* call,
	                                                          BOOL unicode);

	/** @brief Pack a ListReaderGroups_Call into a stream.
	 *  @param s       Stream positioned after the Common/Private Type Headers.
	 *  @param call    The ListReaderGroups_Call to encode.
	 *  @param unicode TRUE for wide-char (W) variant, FALSE for ANSI (A).
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_list_reader_groups_call(wStream* s,
	                                                        const ListReaderGroups_Call* call,
	                                                        BOOL unicode);

	/** @brief Pack a ListReaderGroups_Return into a stream.
	 *  @param s       Stream positioned after the the Common/Private Type Headers and the
	 * ReturnCode.
	 *  @param ret     The ListReaderGroups_Return to encode.
	 *  @param unicode TRUE for wide-char (W) variant, FALSE for ANSI (A).
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_list_reader_groups_return(wStream* s,
	                                                          const ListReaderGroups_Return* ret,
	                                                          BOOL unicode);

	/** @brief Unpack a ListReaderGroups_Return from a stream.
	 *  @param s       Stream positioned after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret     [out] The decoded ListReaderGroups_Return.
	 *  @param unicode TRUE for the W variant, FALSE for the A variant.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_list_reader_groups_return(wStream* s,
	                                                            ListReaderGroups_Return* ret,
	                                                            BOOL unicode);

	/** @brief Unpack a ListReaders_Call from a stream.
	 *  @param s       Stream positioned after the Common/Private Type Headers.
	 *  @param call    [out] The decoded ListReaders_Call.
	 *  @param unicode TRUE for wide-char (W) variant, FALSE for ANSI (A).
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_list_readers_call(wStream* s, ListReaders_Call* call,
	                                                    BOOL unicode);

	/** @brief Pack a ListReaders_Return into a stream.
	 *  @param s       Stream to write into after the Common/Private Type Headers and the
	 * ReturnCode.
	 *  @param ret     The ListReaders_Return to encode.
	 *  @param unicode TRUE for wide-char (W) variant, FALSE for ANSI (A).
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_list_readers_return(wStream* s, const ListReaders_Return* ret,
	                                                    BOOL unicode);

	/** @brief Unpack a ContextAndTwoStringA_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded ContextAndTwoStringA_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG
	smartcard_unpack_context_and_two_strings_a_call(wStream* s, ContextAndTwoStringA_Call* call);

	/** @brief Unpack a ContextAndTwoStringW_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded ContextAndTwoStringW_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG
	smartcard_unpack_context_and_two_strings_w_call(wStream* s, ContextAndTwoStringW_Call* call);

	/** @brief Unpack a ContextAndStringA_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded ContextAndStringA_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_context_and_string_a_call(wStream* s,
	                                                            ContextAndStringA_Call* call);

	/** @brief Unpack a ContextAndStringW_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded ContextAndStringW_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_context_and_string_w_call(wStream* s,
	                                                            ContextAndStringW_Call* call);

	/** @brief Unpack a LocateCardsA_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded LocateCardsA_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_locate_cards_a_call(wStream* s, LocateCardsA_Call* call);

	/** @brief Pack a LocateCards_Return into a stream.
	 *  @param s   Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The LocateCards_Return to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_locate_cards_return(wStream* s, const LocateCards_Return* ret);

	/** @brief Unpack a LocateCardsW_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded LocateCardsW_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_locate_cards_w_call(wStream* s, LocateCardsW_Call* call);

	/** @brief Pack a LocateCardsW return into a stream.
	 *  @param s   Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The LocateCards_Return containing the return data to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_locate_cards_w_return(wStream* s,
	                                                      const LocateCards_Return* ret);

	/** @brief Unpack a ConnectA_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded ConnectA_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_connect_a_call(wStream* s, ConnectA_Call* call);

	/** @brief Unpack a ConnectW_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded ConnectW_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_connect_w_call(wStream* s, ConnectW_Call* call);

	/** @brief Pack a Connect_Return into a stream.
	 *  @param s   Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The Connect_Return to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_connect_return(wStream* s, const Connect_Return* ret);

	/** @brief Unpack a Connect_Return from a stream.
	 *  @param s   Stream positioned after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret [out] The decoded Connect_Return.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_connect_return(wStream* s, Connect_Return* ret);

	/** @brief Pack a Control_Call into a stream.
	 *  @param s    Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param call The Control_Call to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_control_call(wStream* s, const Control_Call* call);

	/** @brief Unpack a Control_return from a stream.
	 *  @param s   Stream positioned after the Common/Private Type Headers.
	 *  @param ret [out] the decoded Control_Return.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_control_return(wStream* s, Control_Return* ret);

	/** @brief Unpack a Reconnect_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded Reconnect_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_reconnect_call(wStream* s, Reconnect_Call* call);

	/** @brief Pack a Reconnect_Call into a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call The Reconnect_Call to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_reconnect_call(wStream* s, const Reconnect_Call* call);

	/** @brief Pack a Reconnect_Return into a stream.
	 *  @param s   Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The Reconnect_Return to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_reconnect_return(wStream* s, const Reconnect_Return* ret);

	/** @brief Unpack a Reconnect_Return from a stream.
	 *  @param s   Stream positioned after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret [out] The decoded Reconnect_Return.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_reconnect_return(wStream* s, Reconnect_Return* ret);

	/** @brief Unpack a HCardAndDisposition_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded HCardAndDisposition_Call.
	 *  @param name Operation name used for trace logging.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_hcard_and_disposition_call(wStream* s,
	                                                             HCardAndDisposition_Call* call,
	                                                             const char* name);

	/** @brief Unpack a GetStatusChangeA_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded GetStatusChangeA_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_get_status_change_a_call(wStream* s,
	                                                           GetStatusChangeA_Call* call);

	/** @brief Unpack a GetStatusChangeW_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded GetStatusChangeW_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_get_status_change_w_call(wStream* s,
	                                                           GetStatusChangeW_Call* call);

	/** @brief Pack a GetStatusChange_Return into a stream.
	 *  @param s       Stream to write into after the Common/Private Type Headers and the
	 * ReturnCode.
	 *  @param ret     The GetStatusChange_Return to encode.
	 *  @param unicode TRUE for wide-char (W) variant, FALSE for ANSI (A).
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_get_status_change_return(wStream* s,
	                                                         const GetStatusChange_Return* ret,
	                                                         BOOL unicode);

	/** @brief Unpack a State_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded State_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_state_call(wStream* s, State_Call* call);

	/** @brief Pack a State_Return into a stream.
	 *  @param s   Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The State_Return to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_state_return(wStream* s, const State_Return* ret);

	/** @brief Unpack a Status_Call from a stream.
	 *  @param s       Stream positioned after the Common/Private Type Headers.
	 *  @param call    [out] The decoded Status_Call.
	 *  @param unicode TRUE for wide-char (W) variant, FALSE for ANSI (A).
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_status_call(wStream* s, Status_Call* call, BOOL unicode);

	/** @brief Pack a Status_Call into a stream.
	 *  @param s       Stream positioned after the Common/Private Type Headers.
	 *  @param call    The Status_Call to encode.
	 *  @param unicode TRUE for wide-char (W) variant, FALSE for ANSI (A).
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_status_call(wStream* s, const Status_Call* call, BOOL unicode);

	/** @brief Pack a Status_Return into a stream.
	 *  @param s       Stream to write into after the Common/Private Type Headers and the
	 * ReturnCode.
	 *  @param ret     The Status_Return to encode.
	 *  @param unicode TRUE for wide-char (W) variant, FALSE for ANSI (A).
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_status_return(wStream* s, const Status_Return* ret,
	                                              BOOL unicode);

	/** @brief Unpack a Status_Return from a stream.
	 *  @param s       Stream positioned after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret     [out] The decoded Status_Return.
	 *  @param unicode TRUE for the W variant, FALSE for the A variant.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_status_return(wStream* s, Status_Return* ret, BOOL unicode);

	/** @brief Unpack a GetAttrib_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded GetAttrib_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_get_attrib_call(wStream* s, GetAttrib_Call* call);

	/** @brief Pack a GetAttrib_Call to a stream.
	 *  @param s    wStream to write to.
	 *  @param call The GetAttrib_Call to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_get_attrib_call(wStream* s, const GetAttrib_Call* call);

	/** @brief Pack a GetAttrib_Return into a stream.
	 *  @param s            Stream to write into after the Common/Private Type Headers and the
	 * ReturnCode.
	 *  @param ret          The GetAttrib_Return to encode.
	 *  @param dwAttrId     The attribute ID from the original request.
	 *  @param cbAttrCallLen The cbAttrLen from the original request.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_get_attrib_return(wStream* s, const GetAttrib_Return* ret,
	                                                  DWORD dwAttrId, DWORD cbAttrCallLen);

	/** @brief Unpack a GetAttrib_Return from a stream.
	 *  @param s   Stream positioned after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret [out] The decoded GetAttrib_Return.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_get_attrib_return(wStream* s, GetAttrib_Return* ret);

	/** @brief Unpack a SetAttrib_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded SetAttrib_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_set_attrib_call(wStream* s, SetAttrib_Call* call);

	/** @brief Pack a SetAttrib_Call into a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call The SetAttrib_Call to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_set_attrib_call(wStream* s, const SetAttrib_Call* call);

	/** @brief Unpack a Control_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded Control_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_control_call(wStream* s, Control_Call* call);

	/** @brief Pack a Control_Return into a stream.
	 *  @param s   Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The Control_Return to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_control_return(wStream* s, const Control_Return* ret);

	/** @brief Unpack a Transmit_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded Transmit_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_transmit_call(wStream* s, Transmit_Call* call);

	/** @brief Pack a Transmit_Return into a stream.
	 *  @param s   Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The Transmit_Return to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_transmit_return(wStream* s, const Transmit_Return* ret);

	/** @brief Unpack a Transmit_Return from a stream.
	 *  @param s   Stream positioned after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret [out] The decoded Transmit_Return.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_transmit_return(wStream* s, Transmit_Return* ret);

	/** @brief Unpack a LocateCardsByATRA_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded LocateCardsByATRA_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_locate_cards_by_atr_a_call(wStream* s,
	                                                             LocateCardsByATRA_Call* call);

	/** @brief Unpack a LocateCardsByATRW_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded LocateCardsByATRW_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_locate_cards_by_atr_w_call(wStream* s,
	                                                             LocateCardsByATRW_Call* call);

	/** @brief Unpack a ReadCacheA_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded ReadCacheA_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_read_cache_a_call(wStream* s, ReadCacheA_Call* call);

	/** @brief Unpack a ReadCacheW_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded ReadCacheW_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_read_cache_w_call(wStream* s, ReadCacheW_Call* call);

	/** @brief Pack a ReadCache_Return into a stream.
	 *  @param s   Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The ReadCache_Return to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_read_cache_return(wStream* s, const ReadCache_Return* ret);

	/** @brief Unpack a WriteCacheA_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded WriteCacheA_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_write_cache_a_call(wStream* s, WriteCacheA_Call* call);

	/** @brief Unpack a WriteCacheW_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded WriteCacheW_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_write_cache_w_call(wStream* s, WriteCacheW_Call* call);

	/** @brief Unpack a GetTransmitCount_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded GetTransmitCount_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_get_transmit_count_call(wStream* s,
	                                                          GetTransmitCount_Call* call);

	/** @brief Pack a GetTransmitCount_Return into a stream.
	 *  @param s    Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The GetTransmitCount_Return to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_get_transmit_count_return(wStream* s,
	                                                          const GetTransmitCount_Return* ret);

	/** @brief Unpack a GetReaderIcon_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded GetReaderIcon_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_get_reader_icon_call(wStream* s, GetReaderIcon_Call* call);

	/** @brief Pack a GetReaderIcon_Return into a stream.
	 *  @param s   Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The GetReaderIcon_Return to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_get_reader_icon_return(wStream* s,
	                                                       const GetReaderIcon_Return* ret);

	/** @brief Unpack a GetDeviceTypeId_Call from a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call [out] The decoded GetDeviceTypeId_Call.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_get_device_type_id_call(wStream* s,
	                                                          GetDeviceTypeId_Call* call);

	/** @brief Pack a GetDeviceTypeId_Return into a stream.
	 *  @param s   Stream to write into after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret The GetDeviceTypeId_Return to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_device_type_id_return(wStream* s,
	                                                      const GetDeviceTypeId_Return* ret);

	/** @brief Pack a Context_Call into a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call The Context_Call to encode.
	 *  @param name Operation name used for trace logging.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_context_call(wStream* s, const Context_Call* call,
	                                             const char* name);

	/** @brief Pack a ListReaders_Call into a stream.
	 *  @param s       Stream positioned after the Common/Private Type Headers.
	 *  @param call    The ListReaders_Call to encode.
	 *  @param unicode TRUE for wide-char (W) variant, FALSE for ANSI (A).
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_list_readers_call(wStream* s, const ListReaders_Call* call,
	                                                  BOOL unicode);

	/** @brief Pack a GetStatusChangeA_Call into a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call The GetStatusChangeA_Call to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_get_status_change_a_call(wStream* s,
	                                                         const GetStatusChangeA_Call* call);

	/** @brief Pack a GetStatusChangeW_Call into a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call The GetStatusChangeW_Call to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_get_status_change_w_call(wStream* s,
	                                                         const GetStatusChangeW_Call* call);

	/** @brief Pack a ConnectA_Call into a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call The ConnectA_Call to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_connect_a_call(wStream* s, const ConnectA_Call* call);

	/** @brief Pack a ConnectW_Call into a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call The ConnectW_Call to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_connect_w_call(wStream* s, const ConnectW_Call* call);

	/** @brief Pack a HCardAndDisposition_Call into a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call The HCardAndDisposition_Call to encode.
	 *  @param name Operation name used for trace logging.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_hcard_and_disposition_call(wStream* s,
	                                                           const HCardAndDisposition_Call* call,
	                                                           const char* name);

	/** @brief Pack a Transmit_Call into a stream.
	 *  @param s    Stream positioned after the Common/Private Type Headers.
	 *  @param call The Transmit_Call to encode.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_pack_transmit_call(wStream* s, const Transmit_Call* call);

	/** @brief Unpack a ListReaders_Return from a stream.
	 *  @param s       Stream positioned after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret     [out] The decoded ListReaders_Return.
	 *  @param unicode TRUE for the W variant, FALSE for the A variant.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_list_readers_return(wStream* s, ListReaders_Return* ret,
	                                                      BOOL unicode);

	/** @brief Unpack a GetStatusChange_Return from a stream.
	 *  @param s       Stream positioned after the Common/Private Type Headers and the ReturnCode.
	 *  @param ret     [out] The decoded GetStatusChange_Return.
	 *  @param unicode TRUE for the W variant, FALSE for the A variant.
	 *  @return \b SCARD_S_SUCCESS on success, an error code on failure.
	 *  @since version 3.28.0
	 */
	WINPR_ATTR_NODISCARD
	FREERDP_API LONG smartcard_unpack_get_status_change_return(wStream* s,
	                                                           GetStatusChange_Return* ret,
	                                                           BOOL unicode);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_SMARTCARD_CLIENT_PACK_H */
