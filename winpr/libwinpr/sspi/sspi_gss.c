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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/crt.h>
#include <winpr/library.h>

#include "sspi_gss.h"

#include "../log.h"
#define TAG WINPR_TAG("sspi.gss")

static GSSAPI_FUNCTION_TABLE* g_GssApi = NULL;
static INIT_ONCE g_Initialized = INIT_ONCE_STATIC_INIT;

#ifdef WITH_GSSAPI

#include <gssapi/gssapi.h>

GSSAPI_FUNCTION_TABLE g_GssApiLink =
{
	(fn_sspi_gss_acquire_cred) gss_acquire_cred, /* gss_acquire_cred */
	(fn_sspi_gss_release_cred) gss_release_cred, /* gss_release_cred */
	(fn_sspi_gss_init_sec_context) gss_init_sec_context, /* gss_init_sec_context */
	(fn_sspi_gss_accept_sec_context) gss_accept_sec_context, /* gss_accept_sec_context */
	(fn_sspi_gss_process_context_token) gss_process_context_token, /* gss_process_context_token */
	(fn_sspi_gss_delete_sec_context) gss_delete_sec_context, /* gss_delete_sec_context */
	(fn_sspi_gss_context_time) gss_context_time, /* gss_context_time */
	(fn_sspi_gss_get_mic) gss_get_mic, /* gss_get_mic */
	(fn_sspi_gss_verify_mic) gss_verify_mic, /* gss_verify_mic */
	(fn_sspi_gss_wrap) gss_wrap, /* gss_wrap */
	(fn_sspi_gss_unwrap) gss_unwrap, /* gss_unwrap */
	(fn_sspi_gss_display_status) gss_display_status, /* gss_display_status */
	(fn_sspi_gss_indicate_mechs) gss_indicate_mechs, /* gss_indicate_mechs */
	(fn_sspi_gss_compare_name) gss_compare_name, /* gss_compare_name */
	(fn_sspi_gss_display_name) gss_display_name, /* gss_display_name */
	(fn_sspi_gss_import_name) gss_import_name, /* gss_import_name */
	(fn_sspi_gss_release_name) gss_release_name, /* gss_release_name */
	(fn_sspi_gss_release_buffer) gss_release_buffer, /* gss_release_buffer */
	(fn_sspi_gss_release_oid_set) gss_release_oid_set, /* gss_release_oid_set */
	(fn_sspi_gss_inquire_cred) gss_inquire_cred, /* gss_inquire_cred */
	(fn_sspi_gss_inquire_context) gss_inquire_context, /* gss_inquire_context */
	(fn_sspi_gss_wrap_size_limit) gss_wrap_size_limit, /* gss_wrap_size_limit */
#if 0
	(fn_sspi_gss_import_name_object) gss_import_name_object, /* gss_import_name_object */
	(fn_sspi_gss_export_name_object) gss_export_name_object, /* gss_export_name_object */
#else
	(fn_sspi_gss_import_name_object) NULL, /* gss_import_name_object */
	(fn_sspi_gss_export_name_object) NULL, /* gss_export_name_object */
#endif
	(fn_sspi_gss_add_cred) gss_add_cred, /* gss_add_cred */
	(fn_sspi_gss_inquire_cred_by_mech) gss_inquire_cred_by_mech, /* gss_inquire_cred_by_mech */
	(fn_sspi_gss_export_sec_context) gss_export_sec_context, /* gss_export_sec_context */
	(fn_sspi_gss_import_sec_context) gss_import_sec_context, /* gss_import_sec_context */
	(fn_sspi_gss_release_oid) gss_release_oid, /* gss_release_oid */
	(fn_sspi_gss_create_empty_oid_set) gss_create_empty_oid_set, /* gss_create_empty_oid_set */
	(fn_sspi_gss_add_oid_set_member) gss_add_oid_set_member, /* gss_add_oid_set_member */
	(fn_sspi_gss_test_oid_set_member) gss_test_oid_set_member, /* gss_test_oid_set_member */
#if 0
	(fn_sspi_gss_str_to_oid) gss_str_to_oid, /* gss_str_to_oid */
#else
	(fn_sspi_gss_str_to_oid) NULL, /* gss_str_to_oid */
#endif
	(fn_sspi_gss_oid_to_str) gss_oid_to_str, /* gss_oid_to_str */
	(fn_sspi_gss_inquire_names_for_mech) gss_inquire_names_for_mech, /* gss_inquire_names_for_mech */
	(fn_sspi_gss_inquire_mechs_for_name) gss_inquire_mechs_for_name, /* gss_inquire_mechs_for_name */
	(fn_sspi_gss_sign) gss_sign, /* gss_sign */
	(fn_sspi_gss_verify) gss_verify, /* gss_verify */
	(fn_sspi_gss_seal) gss_seal, /* gss_seal */
	(fn_sspi_gss_unseal) gss_unseal, /* gss_unseal */
	(fn_sspi_gss_export_name) gss_export_name, /* gss_export_name */
	(fn_sspi_gss_duplicate_name) gss_duplicate_name, /* gss_duplicate_name */
	(fn_sspi_gss_canonicalize_name) gss_canonicalize_name, /* gss_canonicalize_name */
	(fn_sspi_gss_pseudo_random) gss_pseudo_random, /* gss_pseudo_random */
	(fn_sspi_gss_store_cred) gss_store_cred, /* gss_store_cred */
#if 0
	(fn_sspi_gss_set_neg_mechs) gss_set_neg_mechs, /* gss_set_neg_mechs */
#else
	(fn_sspi_gss_set_neg_mechs) NULL, /* gss_set_neg_mechs */
#endif
};

#endif

GSSAPI_FUNCTION_TABLE* SEC_ENTRY gssApi_InitSecurityInterface(void)
{
#ifdef WITH_GSSAPI
	return &g_GssApiLink;
#else
	return NULL;
#endif
}

static BOOL CALLBACK sspi_GssApiInit(PINIT_ONCE once, PVOID param, PVOID* context)
{
	g_GssApi = gssApi_InitSecurityInterface();

	if (!g_GssApi)
		return FALSE;

	return TRUE;
}

/**
 * SSPI GSSAPI OIDs
 */

static sspi_gss_OID_desc g_SSPI_GSS_C_NT_USER_NAME =		{ 10, (void*) "\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x01" };
static sspi_gss_OID_desc g_SSPI_GSS_C_NT_MACHINE_UID_NAME =	{ 10, (void*) "\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x02" };
static sspi_gss_OID_desc g_SSPI_GSS_C_NT_STRING_UID_NAME =	{ 10, (void*) "\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x03" };
static sspi_gss_OID_desc g_SSPI_GSS_C_NT_HOSTBASED_SERVICE_X =	{ 6, (void*) "\x2b\x06\x01\x05\x06\x02" };
static sspi_gss_OID_desc g_SSPI_GSS_C_NT_HOSTBASED_SERVICE =	{ 10, (void*) "\x2a\x86\x48\x86\xf7\x12\x01\x02\x01\x04" };
static sspi_gss_OID_desc g_SSPI_GSS_C_NT_ANONYMOUS =		{ 6, (void*) "\x2b\x06\01\x05\x06\x03" };
static sspi_gss_OID_desc g_SSPI_GSS_C_NT_EXPORT_NAME =		{ 6, (void*) "\x2b\x06\x01\x05\x06\x04" };

sspi_gss_OID SSPI_GSS_C_NT_USER_NAME = &g_SSPI_GSS_C_NT_USER_NAME;
sspi_gss_OID SSPI_GSS_C_NT_MACHINE_UID_NAME = &g_SSPI_GSS_C_NT_MACHINE_UID_NAME;
sspi_gss_OID SSPI_GSS_C_NT_STRING_UID_NAME = &g_SSPI_GSS_C_NT_STRING_UID_NAME;
sspi_gss_OID SSPI_GSS_C_NT_HOSTBASED_SERVICE_X = &g_SSPI_GSS_C_NT_HOSTBASED_SERVICE_X;
sspi_gss_OID SSPI_GSS_C_NT_HOSTBASED_SERVICE = &g_SSPI_GSS_C_NT_HOSTBASED_SERVICE;
sspi_gss_OID SSPI_GSS_C_NT_ANONYMOUS = &g_SSPI_GSS_C_NT_ANONYMOUS;
sspi_gss_OID SSPI_GSS_C_NT_EXPORT_NAME = &g_SSPI_GSS_C_NT_EXPORT_NAME;


/**
 * SSPI GSSAPI
 */

UINT32 SSPI_GSSAPI sspi_gss_acquire_cred(
    UINT32* minor_status,
    sspi_gss_name_t desired_name,
    UINT32 time_req,
    sspi_gss_OID_set desired_mechs,
    sspi_gss_cred_usage_t cred_usage,
    sspi_gss_cred_id_t* output_cred_handle,
    sspi_gss_OID_set* actual_mechs,
    UINT32* time_rec)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_acquire_cred))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_acquire_cred(minor_status, desired_name, time_req,
	                                    desired_mechs, cred_usage, output_cred_handle, actual_mechs, time_rec);
	WLog_DBG(TAG, "gss_acquire_cred: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_release_cred(
    UINT32* minor_status,
    sspi_gss_cred_id_t* cred_handle)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_release_cred))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_release_cred(minor_status, cred_handle);
	WLog_DBG(TAG, "gss_release_cred: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

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
    UINT32* time_rec)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_init_sec_context))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_init_sec_context(minor_status, claimant_cred_handle, context_handle,
	                                        target_name, mech_type, req_flags, time_req, input_chan_bindings,
	                                        input_token, actual_mech_type, output_token, ret_flags, time_rec);
	WLog_DBG(TAG, "gss_init_sec_context: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

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
    sspi_gss_cred_id_t* delegated_cred_handle)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_accept_sec_context))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_accept_sec_context(minor_status, context_handle, acceptor_cred_handle,
	         input_token_buffer, input_chan_bindings, src_name, mech_type, output_token,
	         ret_flags, time_rec, delegated_cred_handle);
	WLog_DBG(TAG, "gss_accept_sec_context: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_process_context_token(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t token_buffer)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_process_context_token))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_process_context_token(minor_status, context_handle, token_buffer);
	WLog_DBG(TAG, "gss_process_context_token: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_delete_sec_context(
    UINT32* minor_status,
    sspi_gss_ctx_id_t* context_handle,
    sspi_gss_buffer_t output_token)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_delete_sec_context))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_delete_sec_context(minor_status, context_handle, output_token);
	WLog_DBG(TAG, "gss_delete_sec_context: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_context_time(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    UINT32* time_rec)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_context_time))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_context_time(minor_status, context_handle, time_rec);
	WLog_DBG(TAG, "gss_context_time: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_get_mic(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_qop_t qop_req,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t message_token)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_get_mic))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_get_mic(minor_status, context_handle, qop_req, message_buffer,
	                               message_token);
	WLog_DBG(TAG, "gss_get_mic: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_verify_mic(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t message_token,
    sspi_gss_qop_t* qop_state)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_verify_mic))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_verify_mic(minor_status, context_handle, message_buffer, message_token,
	                                  qop_state);
	WLog_DBG(TAG, "gss_verify_mic: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_wrap(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int conf_req_flag,
    sspi_gss_qop_t qop_req,
    sspi_gss_buffer_t input_message_buffer,
    int* conf_state,
    sspi_gss_buffer_t output_message_buffer)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_wrap))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_wrap(minor_status, context_handle, conf_req_flag,
	                            qop_req, input_message_buffer, conf_state, output_message_buffer);
	WLog_DBG(TAG, "gss_acquire_cred: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_unwrap(
    UINT32* minor_status,
    const sspi_gss_ctx_id_t context_handle,
    const sspi_gss_buffer_t input_message_buffer,
    sspi_gss_buffer_t output_message_buffer,
    int* conf_state,
    sspi_gss_qop_t* qop_state)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_unwrap))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_unwrap(minor_status, context_handle, input_message_buffer,
	                              output_message_buffer, conf_state, qop_state);
	WLog_DBG(TAG, "gss_unwrap: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_display_status(
    UINT32* minor_status,
    UINT32 status_value,
    int status_type,
    sspi_gss_OID mech_type,
    UINT32* message_context,
    sspi_gss_buffer_t status_string)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_display_status))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_display_status(minor_status, status_value, status_type,
	                                      mech_type, message_context, status_string);
	WLog_DBG(TAG, "gss_display_status: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_indicate_mechs(
    UINT32* minor_status,
    sspi_gss_OID_set* mech_set)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_indicate_mechs))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_indicate_mechs(minor_status, mech_set);
	WLog_DBG(TAG, "gss_indicate_mechs: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_compare_name(
    UINT32* minor_status,
    sspi_gss_name_t name1,
    sspi_gss_name_t name2,
    int* name_equal)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_compare_name))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_compare_name(minor_status, name1, name2, name_equal);
	WLog_DBG(TAG, "gss_compare_name: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_display_name(
    UINT32* minor_status,
    sspi_gss_name_t input_name,
    sspi_gss_buffer_t output_name_buffer,
    sspi_gss_OID* output_name_type)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_display_name))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_display_name(minor_status, input_name, output_name_buffer, output_name_type);
	WLog_DBG(TAG, "gss_display_name: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_import_name(
    UINT32* minor_status,
    sspi_gss_buffer_t input_name_buffer,
    sspi_gss_OID input_name_type,
    sspi_gss_name_t* output_name)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_import_name))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_import_name(minor_status, input_name_buffer, input_name_type, output_name);
	WLog_DBG(TAG, "gss_import_name: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_release_name(
    UINT32* minor_status,
    sspi_gss_name_t* input_name)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_release_name))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_release_name(minor_status, input_name);
	WLog_DBG(TAG, "gss_release_name: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_release_buffer(
    UINT32* minor_status,
    sspi_gss_buffer_t buffer)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_release_buffer))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_release_buffer(minor_status, buffer);
	WLog_DBG(TAG, "gss_release_buffer: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_release_oid_set(
    UINT32* minor_status,
    sspi_gss_OID_set* set)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_release_oid_set))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_release_oid_set(minor_status, set);
	WLog_DBG(TAG, "gss_release_oid_set: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_inquire_cred(
    UINT32* minor_status,
    sspi_gss_cred_id_t cred_handle,
    sspi_gss_name_t* name,
    UINT32* lifetime,
    sspi_gss_cred_usage_t* cred_usage,
    sspi_gss_OID_set* mechanisms)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_inquire_cred))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_inquire_cred(minor_status, cred_handle, name, lifetime, cred_usage,
	                                    mechanisms);
	WLog_DBG(TAG, "gss_inquire_cred: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_inquire_context(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_name_t* src_name,
    sspi_gss_name_t* targ_name,
    UINT32* lifetime_rec,
    sspi_gss_OID* mech_type,
    UINT32* ctx_flags,
    int* locally_initiated,
    int* open)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_inquire_context))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_inquire_context(minor_status, context_handle, src_name, targ_name,
	                                       lifetime_rec, mech_type, ctx_flags, locally_initiated, open);
	WLog_DBG(TAG, "gss_inquire_context: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_wrap_size_limit(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int conf_req_flag,
    sspi_gss_qop_t qop_req,
    UINT32 req_output_size,
    UINT32* max_input_size)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_wrap_size_limit))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_wrap_size_limit(minor_status, context_handle,
	                                       conf_req_flag, qop_req, req_output_size, max_input_size);
	WLog_DBG(TAG, "gss_wrap_size_limit: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_import_name_object(
    UINT32* minor_status,
    void* input_name,
    sspi_gss_OID input_name_type,
    sspi_gss_name_t* output_name)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_import_name_object))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_import_name_object(minor_status, input_name, input_name_type, output_name);
	WLog_DBG(TAG, "gss_import_name_object: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_export_name_object(
    UINT32* minor_status,
    sspi_gss_name_t input_name,
    sspi_gss_OID desired_name_type,
    void** output_name)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_export_name_object))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_export_name_object(minor_status, input_name, desired_name_type, output_name);
	WLog_DBG(TAG, "gss_export_name_object: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

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
    UINT32* acceptor_time_rec)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_add_cred))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_add_cred(minor_status, input_cred_handle, desired_name, desired_mech,
	                                cred_usage,
	                                initiator_time_req, acceptor_time_req, output_cred_handle, actual_mechs, initiator_time_rec,
	                                acceptor_time_rec);
	WLog_DBG(TAG, "gss_add_cred: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_inquire_cred_by_mech(
    UINT32* minor_status,
    sspi_gss_cred_id_t cred_handle,
    sspi_gss_OID mech_type,
    sspi_gss_name_t* name,
    UINT32* initiator_lifetime,
    UINT32* acceptor_lifetime,
    sspi_gss_cred_usage_t* cred_usage)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_inquire_cred_by_mech))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_inquire_cred_by_mech(minor_status, cred_handle, mech_type, name,
	         initiator_lifetime, acceptor_lifetime, cred_usage);
	WLog_DBG(TAG, "gss_inquire_cred_by_mech: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_export_sec_context(
    UINT32* minor_status,
    sspi_gss_ctx_id_t* context_handle,
    sspi_gss_buffer_t interprocess_token)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_export_sec_context))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_export_sec_context(minor_status, context_handle, interprocess_token);
	WLog_DBG(TAG, "gss_export_sec_context: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_import_sec_context(
    UINT32* minor_status,
    sspi_gss_buffer_t interprocess_token,
    sspi_gss_ctx_id_t* context_handle)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_import_sec_context))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_import_sec_context(minor_status, interprocess_token, context_handle);
	WLog_DBG(TAG, "gss_import_sec_context: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_release_oid(
    UINT32* minor_status,
    sspi_gss_OID* oid)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_release_oid))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_release_oid(minor_status, oid);
	WLog_DBG(TAG, "gss_release_oid: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_create_empty_oid_set(
    UINT32* minor_status,
    sspi_gss_OID_set* oid_set)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_create_empty_oid_set))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_create_empty_oid_set(minor_status, oid_set);
	WLog_DBG(TAG, "gss_create_empty_oid_set: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_add_oid_set_member(
    UINT32* minor_status,
    sspi_gss_OID member_oid,
    sspi_gss_OID_set* oid_set)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_add_oid_set_member))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_add_oid_set_member(minor_status, member_oid, oid_set);
	WLog_DBG(TAG, "gss_add_oid_set_member: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_test_oid_set_member(
    UINT32* minor_status,
    sspi_gss_OID member,
    sspi_gss_OID_set set,
    int* present)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_test_oid_set_member))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_test_oid_set_member(minor_status, member, set, present);
	WLog_DBG(TAG, "gss_test_oid_set_member: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_str_to_oid(
    UINT32* minor_status,
    sspi_gss_buffer_t oid_str,
    sspi_gss_OID* oid)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_str_to_oid))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_str_to_oid(minor_status, oid_str, oid);
	WLog_DBG(TAG, "gss_str_to_oid: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_oid_to_str(
    UINT32* minor_status,
    sspi_gss_OID oid,
    sspi_gss_buffer_t oid_str)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_oid_to_str))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_oid_to_str(minor_status, oid, oid_str);
	WLog_DBG(TAG, "gss_oid_to_str: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_inquire_names_for_mech(
    UINT32* minor_status,
    sspi_gss_OID mechanism,
    sspi_gss_OID_set* name_types)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_inquire_names_for_mech))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_inquire_names_for_mech(minor_status, mechanism, name_types);
	WLog_DBG(TAG, "gss_inquire_names_for_mech: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_inquire_mechs_for_name(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    sspi_gss_OID_set* mech_types)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_inquire_mechs_for_name))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_inquire_mechs_for_name(minor_status, input_name, mech_types);
	WLog_DBG(TAG, "gss_inquire_mechs_for_name: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_sign(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int qop_req,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t message_token)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_sign))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_sign(minor_status, context_handle, qop_req, message_buffer, message_token);
	WLog_DBG(TAG, "gss_sign: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_verify(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t message_buffer,
    sspi_gss_buffer_t token_buffer,
    int* qop_state)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_verify))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_verify(minor_status, context_handle, message_buffer, token_buffer,
	                              qop_state);
	WLog_DBG(TAG, "gss_verify: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_seal(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    int conf_req_flag,
    int qop_req,
    sspi_gss_buffer_t input_message_buffer,
    int* conf_state,
    sspi_gss_buffer_t output_message_buffer)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_seal))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_seal(minor_status, context_handle, conf_req_flag, qop_req,
	                            input_message_buffer, conf_state, output_message_buffer);
	WLog_DBG(TAG, "gss_seal: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_unseal(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context_handle,
    sspi_gss_buffer_t input_message_buffer,
    sspi_gss_buffer_t output_message_buffer,
    int* conf_state,
    int* qop_state)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_unseal))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_unseal(minor_status, context_handle, input_message_buffer,
	                              output_message_buffer,
	                              conf_state, qop_state);
	WLog_DBG(TAG, "gss_unseal: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_export_name(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    sspi_gss_buffer_t exported_name)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_export_name))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_export_name(minor_status, input_name, exported_name);
	WLog_DBG(TAG, "gss_export_name: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_duplicate_name(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    sspi_gss_name_t* dest_name)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_duplicate_name))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_duplicate_name(minor_status, input_name, dest_name);
	WLog_DBG(TAG, "gss_duplicate_name: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_canonicalize_name(
    UINT32* minor_status,
    const sspi_gss_name_t input_name,
    const sspi_gss_OID mech_type,
    sspi_gss_name_t* output_name)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_canonicalize_name))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_canonicalize_name(minor_status, input_name, mech_type, output_name);
	WLog_DBG(TAG, "gss_canonicalize_name: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_pseudo_random(
    UINT32* minor_status,
    sspi_gss_ctx_id_t context,
    int prf_key,
    const sspi_gss_buffer_t prf_in,
    SSIZE_T desired_output_len,
    sspi_gss_buffer_t prf_out)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_pseudo_random))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_pseudo_random(minor_status, context, prf_key, prf_in, desired_output_len,
	                                     prf_out);
	WLog_DBG(TAG, "gss_pseudo_random: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_store_cred(
    UINT32* minor_status,
    const sspi_gss_cred_id_t input_cred_handle,
    sspi_gss_cred_usage_t input_usage,
    const sspi_gss_OID desired_mech,
    UINT32 overwrite_cred,
    UINT32 default_cred,
    sspi_gss_OID_set* elements_stored,
    sspi_gss_cred_usage_t* cred_usage_stored)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_store_cred))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_store_cred(minor_status, input_cred_handle, input_usage, desired_mech,
	                                  overwrite_cred, default_cred, elements_stored, cred_usage_stored);
	WLog_DBG(TAG, "gss_store_cred: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}

UINT32 SSPI_GSSAPI sspi_gss_set_neg_mechs(
    UINT32* minor_status,
    sspi_gss_cred_id_t cred_handle,
    const sspi_gss_OID_set mech_set)
{
	SECURITY_STATUS status;
	InitOnceExecuteOnce(&g_Initialized, sspi_GssApiInit, NULL, NULL);

	if (!(g_GssApi && g_GssApi->gss_set_neg_mechs))
		return SEC_E_UNSUPPORTED_FUNCTION;

	status = g_GssApi->gss_set_neg_mechs(minor_status, cred_handle, mech_set);
	WLog_DBG(TAG, "gss_set_neg_mechs: %s (0x%08"PRIX32")",
	         GetSecurityStatusString(status), status);
	return status;
}
