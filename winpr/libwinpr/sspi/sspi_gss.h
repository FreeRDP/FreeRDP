/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Generic Security Service Application Program Interface (GSSAPI)
 *
 * Copyright 2015 ANSSI, Author Thomas Calderon
 * Copyright 2015 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2017 Dorian Ducournau <dorian.ducournau@gmail.com>
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

#ifndef FREERDP_SSPI_GSS_PRIVATE_H
#define FREERDP_SSPI_GSS_PRIVATE_H

#include <winpr/crt.h>
#include <winpr/sspi.h>

/**
 * The following are ABI-compatible, non-conflicting GSSAPI definitions
 *
 * http://tools.ietf.org/html/rfc2743
 * http://tools.ietf.org/html/rfc2744
 */

#define SSPI_GSSAPI
#define SSPI_GSSOID

struct sspi_gss_name_struct;
typedef struct sspi_gss_name_struct* sspi_gss_name_t;

struct sspi_gss_cred_id_struct;
typedef struct sspi_gss_cred_id_struct* sspi_gss_cred_id_t;

struct sspi_gss_ctx_id_struct;
typedef struct sspi_gss_ctx_id_struct* sspi_gss_ctx_id_t;

typedef struct sspi_gss_OID_desc_struct
{
	UINT32 length;
	void* elements;
} sspi_gss_OID_desc, *sspi_gss_OID;

typedef struct sspi_gss_OID_set_desc_struct
{
	size_t count;
	sspi_gss_OID elements;
} sspi_gss_OID_set_desc, *sspi_gss_OID_set;

typedef struct sspi_gss_buffer_desc_struct
{
	size_t length;
	void* value;
} sspi_gss_buffer_desc, *sspi_gss_buffer_t;

typedef struct sspi_gss_channel_bindings_struct
{
	UINT32 initiator_addrtype;
	sspi_gss_buffer_desc initiator_address;
	UINT32 acceptor_addrtype;
	sspi_gss_buffer_desc acceptor_address;
	sspi_gss_buffer_desc application_data;
}* sspi_gss_channel_bindings_t;

typedef UINT32 sspi_gss_qop_t;
typedef int sspi_gss_cred_usage_t;

#define SSPI_GSS_C_DELEG_FLAG			1
#define SSPI_GSS_C_MUTUAL_FLAG			2
#define SSPI_GSS_C_REPLAY_FLAG			4
#define SSPI_GSS_C_SEQUENCE_FLAG		8
#define SSPI_GSS_C_CONF_FLAG			16
#define SSPI_GSS_C_INTEG_FLAG			32
#define SSPI_GSS_C_ANON_FLAG			64
#define SSPI_GSS_C_PROT_READY_FLAG		128
#define SSPI_GSS_C_TRANS_FLAG			256
#define SSPI_GSS_C_DELEG_POLICY_FLAG		32768

#define SSPI_GSS_C_BOTH				0
#define SSPI_GSS_C_INITIATE			1
#define SSPI_GSS_C_ACCEPT			2

#define SSPI_GSS_C_GSS_CODE			1
#define SSPI_GSS_C_MECH_CODE			2

#define SSPI_GSS_C_AF_UNSPEC			0
#define SSPI_GSS_C_AF_LOCAL			1
#define SSPI_GSS_C_AF_INET			2
#define SSPI_GSS_C_AF_IMPLINK			3
#define SSPI_GSS_C_AF_PUP			4
#define SSPI_GSS_C_AF_CHAOS			5
#define SSPI_GSS_C_AF_NS			6
#define SSPI_GSS_C_AF_NBS			7
#define SSPI_GSS_C_AF_ECMA			8
#define SSPI_GSS_C_AF_DATAKIT			9
#define SSPI_GSS_C_AF_CCITT			10
#define SSPI_GSS_C_AF_SNA			11
#define SSPI_GSS_C_AF_DECnet			12
#define SSPI_GSS_C_AF_DLI			13
#define SSPI_GSS_C_AF_LAT			14
#define SSPI_GSS_C_AF_HYLINK			15
#define SSPI_GSS_C_AF_APPLETALK			16
#define SSPI_GSS_C_AF_BSC			17
#define SSPI_GSS_C_AF_DSS			18
#define SSPI_GSS_C_AF_OSI			19
#define SSPI_GSS_C_AF_NETBIOS			20
#define SSPI_GSS_C_AF_X25			21
#define SSPI_GSS_C_AF_NULLADDR			255

#define SSPI_GSS_C_NO_NAME			((sspi_gss_name_t) 0)
#define SSPI_GSS_C_NO_BUFFER			((sspi_gss_buffer_t) 0)
#define SSPI_GSS_C_NO_OID			((sspi_gss_OID) 0)
#define SSPI_GSS_C_NO_OID_SET			((sspi_gss_OID_set) 0)
#define SSPI_GSS_C_NO_CONTEXT			((sspi_gss_ctx_id_t) 0)
#define SSPI_GSS_C_NO_CREDENTIAL		((sspi_gss_cred_id_t) 0)
#define SSPI_GSS_C_NO_CHANNEL_BINDINGS		((sspi_gss_channel_bindings_t) 0)
#define SSPI_GSS_C_EMPTY_BUFFER			{0, NULL}

#define SSPI_GSS_C_NULL_OID			SSPI_GSS_C_NO_OID
#define SSPI_GSS_C_NULL_OID_SET			SSPI_GSS_C_NO_OID_SET

#define SSPI_GSS_C_QOP_DEFAULT			0

#define SSPI_GSS_C_INDEFINITE			((UINT32) 0xFFFFFFFF)

#define SSPI_GSS_S_COMPLETE			0

#define SSPI_GSS_C_CALLING_ERROR_OFFSET		24
#define SSPI_GSS_C_ROUTINE_ERROR_OFFSET		16
#define SSPI_GSS_C_SUPPLEMENTARY_OFFSET		0
#define SSPI_GSS_C_CALLING_ERROR_MASK		((UINT32) 0377)
#define SSPI_GSS_C_ROUTINE_ERROR_MASK		((UINT32) 0377)
#define SSPI_GSS_C_SUPPLEMENTARY_MASK		((UINT32) 0177777)

#define SSPI_GSS_CALLING_ERROR(_x) \
	((_x) & (SSPI_GSS_C_CALLING_ERROR_MASK << SSPI_GSS_C_CALLING_ERROR_OFFSET))
#define SSPI_GSS_ROUTINE_ERROR(_x) \
	((_x) & (SSPI_GSS_C_ROUTINE_ERROR_MASK << SSPI_GSS_C_ROUTINE_ERROR_OFFSET))
#define SSPI_GSS_SUPPLEMENTARY_INFO(_x) \
	((_x) & (SSPI_GSS_C_SUPPLEMENTARY_MASK << SSPI_GSS_C_SUPPLEMENTARY_OFFSET))
#define SSPI_GSS_ERROR(_x) \
	((_x) & ((SSPI_GSS_C_CALLING_ERROR_MASK << SSPI_GSS_C_CALLING_ERROR_OFFSET) | \
	         (SSPI_GSS_C_ROUTINE_ERROR_MASK << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)))

#define SSPI_GSS_S_CALL_INACCESSIBLE_READ	(((UINT32) 1) << SSPI_GSS_C_CALLING_ERROR_OFFSET)
#define SSPI_GSS_S_CALL_INACCESSIBLE_WRITE	(((UINT32) 2) << SSPI_GSS_C_CALLING_ERROR_OFFSET)
#define SSPI_GSS_S_CALL_BAD_STRUCTURE		(((UINT32) 3) << SSPI_GSS_C_CALLING_ERROR_OFFSET)

#define SSPI_GSS_S_BAD_MECH			(((UINT32) 1) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_BAD_NAME			(((UINT32) 2) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_BAD_NAMETYPE			(((UINT32) 3) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_BAD_BINDINGS			(((UINT32) 4) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_BAD_STATUS			(((UINT32) 5) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_BAD_SIG			(((UINT32) 6) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_NO_CRED			(((UINT32) 7) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_NO_CONTEXT			(((UINT32) 8) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_DEFECTIVE_TOKEN		(((UINT32) 9) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_DEFECTIVE_CREDENTIAL		(((UINT32) 10) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_CREDENTIALS_EXPIRED		(((UINT32) 11) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_CONTEXT_EXPIRED		(((UINT32) 12) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_FAILURE			(((UINT32) 13) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_BAD_QOP			(((UINT32) 14) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_UNAUTHORIZED			(((UINT32) 15) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_UNAVAILABLE			(((UINT32) 16) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_DUPLICATE_ELEMENT		(((UINT32) 17) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_NAME_NOT_MN			(((UINT32) 18) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)
#define SSPI_GSS_S_BAD_MECH_ATTR		(((UINT32) 19) << SSPI_GSS_C_ROUTINE_ERROR_OFFSET)

#define SSPI_GSS_S_CONTINUE_NEEDED		(1 << (SSPI_GSS_C_SUPPLEMENTARY_OFFSET + 0))
#define SSPI_GSS_S_DUPLICATE_TOKEN		(1 << (SSPI_GSS_C_SUPPLEMENTARY_OFFSET + 1))
#define SSPI_GSS_S_OLD_TOKEN			(1 << (SSPI_GSS_C_SUPPLEMENTARY_OFFSET + 2))
#define SSPI_GSS_S_UNSEQ_TOKEN			(1 << (SSPI_GSS_C_SUPPLEMENTARY_OFFSET + 3))
#define SSPI_GSS_S_GAP_TOKEN			(1 << (SSPI_GSS_C_SUPPLEMENTARY_OFFSET + 4))

#define SSPI_GSS_C_PRF_KEY_FULL			0
#define SSPI_GSS_C_PRF_KEY_PARTIAL		1

#ifdef __cplusplus
extern "C" {
#endif

SSPI_GSSOID extern sspi_gss_OID SSPI_GSS_C_NT_USER_NAME;
SSPI_GSSOID extern sspi_gss_OID SSPI_GSS_C_NT_MACHINE_UID_NAME;
SSPI_GSSOID extern sspi_gss_OID SSPI_GSS_C_NT_STRING_UID_NAME;
SSPI_GSSOID extern sspi_gss_OID SSPI_GSS_C_NT_HOSTBASED_SERVICE_X;
SSPI_GSSOID extern sspi_gss_OID SSPI_GSS_C_NT_HOSTBASED_SERVICE;
SSPI_GSSOID extern sspi_gss_OID SSPI_GSS_C_NT_ANONYMOUS;
SSPI_GSSOID extern sspi_gss_OID SSPI_GSS_C_NT_EXPORT_NAME;

UINT32 SSPI_GSSAPI sspi_gss_acquire_cred(
    UINT32* minor_status,
    sspi_gss_name_t desired_name,
    UINT32 time_req,
    sspi_gss_OID_set desired_mechs,
    sspi_gss_cred_usage_t cred_usage,
    sspi_gss_cred_id_t* output_cred_handle,
    sspi_gss_OID_set* actual_mechs,
    UINT32* time_rec);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_acquire_cred)(
    UINT32* minor_status,
    sspi_gss_name_t desired_name,
    UINT32 time_req,
    sspi_gss_OID_set desired_mechs,
    sspi_gss_cred_usage_t cred_usage,
    sspi_gss_cred_id_t* output_cred_handle,
    sspi_gss_OID_set* actual_mechs,
    UINT32* time_rec);

UINT32 SSPI_GSSAPI sspi_gss_release_cred(
    UINT32* minor_status,
    sspi_gss_cred_id_t* cred_handle);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_release_cred)(
    UINT32* minor_status,
    sspi_gss_cred_id_t* cred_handle);

UINT32 SSPI_GSSAPI sspi_gss_init_sec_context(
    UINT32* minor_status,
    sspi_gss_cred_id_t claimant_cred_handle,
    sspi_gss_ctx_id_t* context_handle,
    sspi_gss_name_t target_name,
    sspi_gss_OID mech_type,
    UINT32 req_flags,
    UINT32 time_req,
    sspi_gss_channel_bindings_t input_chan_bindings,
    sspi_gss_buffer_t input_token,
    sspi_gss_OID* actual_mech_type,
    sspi_gss_buffer_t output_token,
    UINT32* ret_flags,
    UINT32* time_rec);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_init_sec_context)(
    UINT32* minor_status,
    sspi_gss_cred_id_t claimant_cred_handle,
    sspi_gss_ctx_id_t* context_handle,
    sspi_gss_name_t target_name,
    sspi_gss_OID mech_type,
    UINT32 req_flags,
    UINT32 time_req,
    sspi_gss_channel_bindings_t input_chan_bindings,
    sspi_gss_buffer_t input_token,
    sspi_gss_OID* actual_mech_type,
    sspi_gss_buffer_t output_token,
    UINT32* ret_flags,
    UINT32* time_rec);

UINT32 SSPI_GSSAPI sspi_gss_accept_sec_context(
    UINT32* minor_status,
    sspi_gss_ctx_id_t* context_handle,
    sspi_gss_cred_id_t acceptor_cred_handle,
    sspi_gss_buffer_t input_token_buffer,
    sspi_gss_channel_bindings_t input_chan_bindings,
    sspi_gss_name_t* src_name,
    sspi_gss_OID* mech_type,
    sspi_gss_buffer_t output_token,
    UINT32* ret_flags,
    UINT32* time_rec,
    sspi_gss_cred_id_t* delegated_cred_handle);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_accept_sec_context)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t* context_handle,
    sspi_gss_cred_id_t acceptor_cred_handle,
    sspi_gss_buffer_t input_token_buffer,
    sspi_gss_channel_bindings_t input_chan_bindings,
    sspi_gss_name_t* src_name,
    sspi_gss_OID* mech_type,
    sspi_gss_buffer_t output_token,
    UINT32* ret_flags,
    UINT32* time_rec,
    sspi_gss_cred_id_t* delegated_cred_handle);

UINT32 SSPI_GSSAPI sspi_gss_process_context_token(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t token_buffer);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_process_context_token)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t token_buffer);

UINT32 SSPI_GSSAPI sspi_gss_delete_sec_context(
    UINT32* minor_status,
    sspi_gss_ctx_id_t* context_handle,
    sspi_gss_buffer_t output_token);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_delete_sec_context)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t* context_handle,
    sspi_gss_buffer_t output_token);

UINT32 SSPI_GSSAPI sspi_gss_context_time(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    UINT32* time_rec);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_context_time)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    UINT32* time_rec);

UINT32 SSPI_GSSAPI sspi_gss_get_mic(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_qop_t qop_req,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t message_token);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_get_mic)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_qop_t qop_req,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t message_token);

UINT32 SSPI_GSSAPI sspi_gss_verify_mic(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t message_token,
    sspi_gss_qop_t* qop_state);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_verify_mic)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t message_token,
    sspi_gss_qop_t* qop_state);

UINT32 SSPI_GSSAPI sspi_gss_wrap(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int conf_req_flag,
    sspi_gss_qop_t qop_req,
    sspi_gss_buffer_t input_message_buffer,
    int* conf_state,
    sspi_gss_buffer_t output_message_buffer);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_wrap)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int conf_req_flag,
    sspi_gss_qop_t qop_req,
    sspi_gss_buffer_t input_message_buffer,
    int* conf_state,
    sspi_gss_buffer_t output_message_buffer);

UINT32 SSPI_GSSAPI sspi_gss_unwrap(
    UINT32* minor_status,
    const sspi_gss_ctx_id_t context_handle,
    const sspi_gss_buffer_t input_message_buffer,
    sspi_gss_buffer_t output_message_buffer,
    int* conf_state,
    sspi_gss_qop_t* qop_state);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_unwrap)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t input_message_buffer,
    sspi_gss_buffer_t output_message_buffer,
    int* conf_state,
    sspi_gss_qop_t* qop_state);

UINT32 SSPI_GSSAPI sspi_gss_display_status(
    UINT32* minor_status,
    UINT32 status_value,
    int status_type,
    sspi_gss_OID mech_type,
    UINT32* message_context,
    sspi_gss_buffer_t status_string);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_display_status)(
    UINT32* minor_status,
    UINT32 status_value,
    int status_type,
    sspi_gss_OID mech_type,
    UINT32* message_context,
    sspi_gss_buffer_t status_string);

UINT32 SSPI_GSSAPI sspi_gss_indicate_mechs(
    UINT32* minor_status,
    sspi_gss_OID_set* mech_set);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_indicate_mechs)(
    UINT32* minor_status,
    sspi_gss_OID_set* mech_set);

UINT32 SSPI_GSSAPI sspi_gss_compare_name(
    UINT32* minor_status,
    sspi_gss_name_t name1,
    sspi_gss_name_t name2,
    int* name_equal);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_compare_name)(
    UINT32* minor_status,
    sspi_gss_name_t name1,
    sspi_gss_name_t name2,
    int* name_equal);

UINT32 SSPI_GSSAPI sspi_gss_display_name(
    UINT32* minor_status,
    sspi_gss_name_t input_name,
    sspi_gss_buffer_t output_name_buffer,
    sspi_gss_OID* output_name_type);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_display_name)(
    UINT32* minor_status,
    sspi_gss_name_t input_name,
    sspi_gss_buffer_t output_name_buffer,
    sspi_gss_OID* output_name_type);

UINT32 SSPI_GSSAPI sspi_gss_import_name(
    UINT32* minor_status,
    sspi_gss_buffer_t input_name_buffer,
    sspi_gss_OID input_name_type,
    sspi_gss_name_t* output_name);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_import_name)(
    UINT32* minor_status,
    sspi_gss_buffer_t input_name_buffer,
    sspi_gss_OID input_name_type,
    sspi_gss_name_t* output_name);

UINT32 SSPI_GSSAPI sspi_gss_release_name(
    UINT32* minor_status,
    sspi_gss_name_t* input_name);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_release_name)(
    UINT32* minor_status,
    sspi_gss_name_t* input_name);

UINT32 SSPI_GSSAPI sspi_gss_release_buffer(
    UINT32* minor_status,
    sspi_gss_buffer_t buffer);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_release_buffer)(
    UINT32* minor_status,
    sspi_gss_buffer_t buffer);

UINT32 SSPI_GSSAPI sspi_gss_release_oid_set(
    UINT32* minor_status,
    sspi_gss_OID_set* set);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_release_oid_set)(
    UINT32* minor_status,
    sspi_gss_OID_set* set);

UINT32 SSPI_GSSAPI sspi_gss_inquire_cred(
    UINT32* minor_status,
    sspi_gss_cred_id_t cred_handle,
    sspi_gss_name_t* name,
    UINT32* lifetime,
    sspi_gss_cred_usage_t* cred_usage,
    sspi_gss_OID_set* mechanisms);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_inquire_cred)(
    UINT32* minor_status,
    sspi_gss_cred_id_t cred_handle,
    sspi_gss_name_t* name,
    UINT32* lifetime,
    sspi_gss_cred_usage_t* cred_usage,
    sspi_gss_OID_set* mechanisms);

UINT32 SSPI_GSSAPI sspi_gss_inquire_context(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_name_t* src_name,
    sspi_gss_name_t* targ_name,
    UINT32* lifetime_rec,
    sspi_gss_OID* mech_type,
    UINT32* ctx_flags,
    int* locally_initiated,
    int* open);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_inquire_context)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_name_t* src_name,
    sspi_gss_name_t* targ_name,
    UINT32* lifetime_rec,
    sspi_gss_OID* mech_type,
    UINT32* ctx_flags,
    int* locally_initiated,
    int* open);

UINT32 SSPI_GSSAPI sspi_gss_wrap_size_limit(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int conf_req_flag,
    sspi_gss_qop_t qop_req,
    UINT32 req_output_size,
    UINT32* max_input_size);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_wrap_size_limit)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int conf_req_flag,
    sspi_gss_qop_t qop_req,
    UINT32 req_output_size,
    UINT32* max_input_size);

UINT32 SSPI_GSSAPI sspi_gss_import_name_object(
    UINT32* minor_status,
    void* input_name,
    sspi_gss_OID input_name_type,
    sspi_gss_name_t* output_name);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_import_name_object)(
    UINT32* minor_status,
    void* input_name,
    sspi_gss_OID input_name_type,
    sspi_gss_name_t* output_name);

UINT32 SSPI_GSSAPI sspi_gss_export_name_object(
    UINT32* minor_status,
    sspi_gss_name_t input_name,
    sspi_gss_OID desired_name_type,
    void** output_name);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_export_name_object)(
    UINT32* minor_status,
    sspi_gss_name_t input_name,
    sspi_gss_OID desired_name_type,
    void** output_name);

UINT32 SSPI_GSSAPI sspi_gss_add_cred(
    UINT32* minor_status,
    sspi_gss_cred_id_t input_cred_handle,
    sspi_gss_name_t desired_name,
    sspi_gss_OID desired_mech,
    sspi_gss_cred_usage_t cred_usage,
    UINT32 initiator_time_req,
    UINT32 acceptor_time_req,
    sspi_gss_cred_id_t* output_cred_handle,
    sspi_gss_OID_set* actual_mechs,
    UINT32* initiator_time_rec,
    UINT32* acceptor_time_rec);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_add_cred)(
    UINT32* minor_status,
    sspi_gss_cred_id_t input_cred_handle,
    sspi_gss_name_t desired_name,
    sspi_gss_OID desired_mech,
    sspi_gss_cred_usage_t cred_usage,
    UINT32 initiator_time_req,
    UINT32 acceptor_time_req,
    sspi_gss_cred_id_t* output_cred_handle,
    sspi_gss_OID_set* actual_mechs,
    UINT32* initiator_time_rec,
    UINT32* acceptor_time_rec);

UINT32 SSPI_GSSAPI sspi_gss_inquire_cred_by_mech(
    UINT32* minor_status,
    sspi_gss_cred_id_t cred_handle,
    sspi_gss_OID mech_type,
    sspi_gss_name_t* name,
    UINT32* initiator_lifetime,
    UINT32* acceptor_lifetime,
    sspi_gss_cred_usage_t* cred_usage);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_inquire_cred_by_mech)(
    UINT32* minor_status,
    sspi_gss_cred_id_t cred_handle,
    sspi_gss_OID mech_type,
    sspi_gss_name_t* name,
    UINT32* initiator_lifetime,
    UINT32* acceptor_lifetime,
    sspi_gss_cred_usage_t* cred_usage);

UINT32 SSPI_GSSAPI sspi_gss_export_sec_context(
    UINT32* minor_status,
    sspi_gss_ctx_id_t* context_handle,
    sspi_gss_buffer_t interprocess_token);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_export_sec_context)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t* context_handle,
    sspi_gss_buffer_t interprocess_token);

UINT32 SSPI_GSSAPI sspi_gss_import_sec_context(
    UINT32* minor_status,
    sspi_gss_buffer_t interprocess_token,
    sspi_gss_ctx_id_t* context_handle);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_import_sec_context)(
    UINT32* minor_status,
    sspi_gss_buffer_t interprocess_token,
    sspi_gss_ctx_id_t* context_handle);

UINT32 SSPI_GSSAPI sspi_gss_release_oid(
    UINT32* minor_status,
    sspi_gss_OID* oid);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_release_oid)(
    UINT32* minor_status,
    sspi_gss_OID* oid);

UINT32 SSPI_GSSAPI sspi_gss_create_empty_oid_set(
    UINT32* minor_status,
    sspi_gss_OID_set* oid_set);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_create_empty_oid_set)(
    UINT32* minor_status,
    sspi_gss_OID_set* oid_set);

UINT32 SSPI_GSSAPI sspi_gss_add_oid_set_member(
    UINT32* minor_status,
    sspi_gss_OID member_oid,
    sspi_gss_OID_set* oid_set);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_add_oid_set_member)(
    UINT32* minor_status,
    sspi_gss_OID member_oid,
    sspi_gss_OID_set* oid_set);

UINT32 SSPI_GSSAPI sspi_gss_test_oid_set_member(
    UINT32* minor_status,
    sspi_gss_OID member,
    sspi_gss_OID_set set,
    int* present);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_test_oid_set_member)(
    UINT32* minor_status,
    sspi_gss_OID member,
    sspi_gss_OID_set set,
    int* present);

UINT32 SSPI_GSSAPI sspi_gss_str_to_oid(
    UINT32* minor_status,
    sspi_gss_buffer_t oid_str,
    sspi_gss_OID* oid);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_str_to_oid)(
    UINT32* minor_status,
    sspi_gss_buffer_t oid_str,
    sspi_gss_OID* oid);

UINT32 SSPI_GSSAPI sspi_gss_oid_to_str(
    UINT32* minor_status,
    sspi_gss_OID oid,
    sspi_gss_buffer_t oid_str);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_oid_to_str)(
    UINT32* minor_status,
    sspi_gss_OID oid,
    sspi_gss_buffer_t oid_str);

UINT32 SSPI_GSSAPI sspi_gss_inquire_names_for_mech(
    UINT32* minor_status,
    sspi_gss_OID mechanism,
    sspi_gss_OID_set* name_types);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_inquire_names_for_mech)(
    UINT32* minor_status,
    sspi_gss_OID mechanism,
    sspi_gss_OID_set* name_types);

UINT32 SSPI_GSSAPI sspi_gss_inquire_mechs_for_name(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    sspi_gss_OID_set* mech_types);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_inquire_mechs_for_name)(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    sspi_gss_OID_set* mech_types);

UINT32 SSPI_GSSAPI sspi_gss_sign(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int qop_req,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t message_token);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_sign)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int qop_req,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t message_token);

UINT32 SSPI_GSSAPI sspi_gss_verify(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t token_buffer,
    int* qop_state);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_verify)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t token_buffer,
    int* qop_state);

UINT32 SSPI_GSSAPI sspi_gss_seal(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int conf_req_flag,
    int qop_req,
    sspi_gss_buffer_t input_message_buffer,
    int* conf_state,
    sspi_gss_buffer_t output_message_buffer);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_seal)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int conf_req_flag,
    int qop_req,
    sspi_gss_buffer_t input_message_buffer,
    int* conf_state,
    sspi_gss_buffer_t output_message_buffer);

UINT32 SSPI_GSSAPI sspi_gss_unseal(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t input_message_buffer,
    sspi_gss_buffer_t output_message_buffer,
    int* conf_state,
    int* qop_state);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_unseal)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t input_message_buffer,
    sspi_gss_buffer_t output_message_buffer,
    int* conf_state,
    int* qop_state);

UINT32 SSPI_GSSAPI sspi_gss_export_name(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    sspi_gss_buffer_t exported_name);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_export_name)(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    sspi_gss_buffer_t exported_name);

UINT32 SSPI_GSSAPI sspi_gss_duplicate_name(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    sspi_gss_name_t* dest_name);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_duplicate_name)(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    sspi_gss_name_t* dest_name);

UINT32 SSPI_GSSAPI sspi_gss_canonicalize_name(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    const sspi_gss_OID mech_type,
    sspi_gss_name_t* output_name);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_canonicalize_name)(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    const sspi_gss_OID mech_type,
    sspi_gss_name_t* output_name);

UINT32 SSPI_GSSAPI sspi_gss_pseudo_random(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context,
    int prf_key,
    const sspi_gss_buffer_t prf_in,
    SSIZE_T desired_output_len,
    sspi_gss_buffer_t prf_out);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_pseudo_random)(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context,
    int prf_key,
    const sspi_gss_buffer_t prf_in,
    SSIZE_T desired_output_len,
    sspi_gss_buffer_t prf_out);

UINT32 SSPI_GSSAPI sspi_gss_store_cred(
    UINT32* minor_status,
    const sspi_gss_cred_id_t input_cred_handle,
    sspi_gss_cred_usage_t input_usage,
    const sspi_gss_OID desired_mech,
    UINT32 overwrite_cred,
    UINT32 default_cred,
    sspi_gss_OID_set* elements_stored,
    sspi_gss_cred_usage_t* cred_usage_stored);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_store_cred)(
    UINT32* minor_status,
    const sspi_gss_cred_id_t input_cred_handle,
    sspi_gss_cred_usage_t input_usage,
    const sspi_gss_OID desired_mech,
    UINT32 overwrite_cred,
    UINT32 default_cred,
    sspi_gss_OID_set* elements_stored,
    sspi_gss_cred_usage_t* cred_usage_stored);

UINT32 SSPI_GSSAPI sspi_gss_set_neg_mechs(
    UINT32* minor_status,
    sspi_gss_cred_id_t cred_handle,
    const sspi_gss_OID_set mech_set);

typedef UINT32(SSPI_GSSAPI* fn_sspi_gss_set_neg_mechs)(
    UINT32* minor_status,
    sspi_gss_cred_id_t cred_handle,
    const sspi_gss_OID_set mech_set);

#ifdef __cplusplus
}
#endif

struct _GSSAPI_FUNCTION_TABLE
{
	fn_sspi_gss_acquire_cred gss_acquire_cred;
	fn_sspi_gss_release_cred gss_release_cred;
	fn_sspi_gss_init_sec_context gss_init_sec_context;
	fn_sspi_gss_accept_sec_context gss_accept_sec_context;
	fn_sspi_gss_process_context_token gss_process_context_token;
	fn_sspi_gss_delete_sec_context gss_delete_sec_context;
	fn_sspi_gss_context_time gss_context_time;
	fn_sspi_gss_get_mic gss_get_mic;
	fn_sspi_gss_verify_mic gss_verify_mic;
	fn_sspi_gss_wrap gss_wrap;
	fn_sspi_gss_unwrap gss_unwrap;
	fn_sspi_gss_display_status gss_display_status;
	fn_sspi_gss_indicate_mechs gss_indicate_mechs;
	fn_sspi_gss_compare_name gss_compare_name;
	fn_sspi_gss_display_name gss_display_name;
	fn_sspi_gss_import_name gss_import_name;
	fn_sspi_gss_release_name gss_release_name;
	fn_sspi_gss_release_buffer gss_release_buffer;
	fn_sspi_gss_release_oid_set gss_release_oid_set;
	fn_sspi_gss_inquire_cred gss_inquire_cred;
	fn_sspi_gss_inquire_context gss_inquire_context;
	fn_sspi_gss_wrap_size_limit gss_wrap_size_limit;
	fn_sspi_gss_import_name_object gss_import_name_object;
	fn_sspi_gss_export_name_object gss_export_name_object;
	fn_sspi_gss_add_cred gss_add_cred;
	fn_sspi_gss_inquire_cred_by_mech gss_inquire_cred_by_mech;
	fn_sspi_gss_export_sec_context gss_export_sec_context;
	fn_sspi_gss_import_sec_context gss_import_sec_context;
	fn_sspi_gss_release_oid gss_release_oid;
	fn_sspi_gss_create_empty_oid_set gss_create_empty_oid_set;
	fn_sspi_gss_add_oid_set_member gss_add_oid_set_member;
	fn_sspi_gss_test_oid_set_member gss_test_oid_set_member;
	fn_sspi_gss_str_to_oid gss_str_to_oid;
	fn_sspi_gss_oid_to_str gss_oid_to_str;
	fn_sspi_gss_inquire_names_for_mech gss_inquire_names_for_mech;
	fn_sspi_gss_inquire_mechs_for_name gss_inquire_mechs_for_name;
	fn_sspi_gss_sign gss_sign;
	fn_sspi_gss_verify gss_verify;
	fn_sspi_gss_seal gss_seal;
	fn_sspi_gss_unseal gss_unseal;
	fn_sspi_gss_export_name gss_export_name;
	fn_sspi_gss_duplicate_name gss_duplicate_name;
	fn_sspi_gss_canonicalize_name gss_canonicalize_name;
	fn_sspi_gss_pseudo_random gss_pseudo_random;
	fn_sspi_gss_store_cred gss_store_cred;
	fn_sspi_gss_set_neg_mechs gss_set_neg_mechs;
};
typedef struct _GSSAPI_FUNCTION_TABLE GSSAPI_FUNCTION_TABLE;

GSSAPI_FUNCTION_TABLE* SEC_ENTRY gssApi_InitSecurityInterface(void);

#ifdef __cplusplus
extern "C" {
#endif



#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SSPI_GSS_PRIVATE_H */
