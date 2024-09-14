/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Settings
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2016 Armin Novak <armin.novak@gmail.com>
 * Copyright 2023 Armin Novak <anovak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#ifndef FREERDP_SETTINGS_H
#define FREERDP_SETTINGS_H

#include <winpr/timezone.h>
#include <winpr/wlog.h>

#include <freerdp/api.h>
#include <freerdp/config.h>
#include <freerdp/types.h>
#include <freerdp/redirection.h>

#if !defined(WITH_OPAQUE_SETTINGS)
#include <freerdp/settings_types_private.h>
#endif

#include <freerdp/settings_keys.h>
#include <freerdp/settings_types.h>

#include <freerdp/crypto/certificate.h>
#include <freerdp/crypto/privatekey.h>

#ifdef __cplusplus
extern "C"
{
#endif

	/** \defgroup rdpSettings rdpSettings
	 * \brief This is the FreeRDP settings module.
	 *
	 * Settings are used to store configuration data for an RDP connection.
	 * There are 3 different settings for each client and server:
	 *
	 * 1. The initial connection supplied by the user
	 * 2. The settings sent from client or server during capability exchange
	 * 3. The settings merged from the capability exchange and the initial configuration.
	 *
	 * The lifetime of the settings is as follows:
	 * 1. Initial configuration is saved and will be valid for the whole application lifecycle
	 * 2. The client or server settings from the other end are valid from capability exchange until
	 * the connection is ended (disconnect/redirect/...)
	 * 3. The merged settings are created from the initial configuration and server settings and
	 * have the same lifetime, until the connection ends
	 *
	 *
	 * So, when accessing the settings always ensure to know which one you are operating on! (this
	 * is especially important for the proxy where you have a RDP client and RDP server in the same
	 * application context)
	 *
	 * @{
	 */

	typedef struct rdp_settings rdpSettings;

/**
 * rdpSettings creation flags
 */
#define FREERDP_SETTINGS_SERVER_MODE 0x00000001
#define FREERDP_SETTINGS_REMOTE_MODE 0x00000002

	/** \brief Free a settings struct with all data in it
	 *
	 *  \param settings A pointer to the settings to free, May be NULL
	 */
	FREERDP_API void freerdp_settings_free(rdpSettings* settings);

	/** \brief creates a new setting struct
	 *
	 *  \param flags Flags for creation, use \b FREERDP_SETTINGS_SERVER_MODE for server settings, 0
	 * for client.
	 *
	 *  \return A newly allocated settings struct or NULL
	 */
	WINPR_ATTR_MALLOC(freerdp_settings_free, 1)
	FREERDP_API rdpSettings* freerdp_settings_new(DWORD flags);

	/** \brief Creates a deep copy of settings
	 *
	 *  \param settings A pointer to a settings struct to copy. May be NULL (returns NULL)
	 *
	 *  \return A newly allocated copy of \b settings or NULL
	 */
	WINPR_ATTR_MALLOC(freerdp_settings_free, 1)
	FREERDP_API rdpSettings* freerdp_settings_clone(const rdpSettings* settings);

	/** \brief Deep copies settings from \b src to \b dst
	 *
	 * The function frees up all allocated data in \b dst before copying the data from \b src
	 *
	 * \param dst A pointer for the settings to copy data to. May be NULL (fails copy)
	 * \param src A pointer to the settings to copy. May be NULL (fails copy)
	 *
	 *  \return \b TRUE for success, \b FALSE for failure.
	 */
	FREERDP_API BOOL freerdp_settings_copy(rdpSettings* dst, const rdpSettings* src);

	/** \brief copies one setting identified by \b id from \b src to \b dst
	 *
	 * The function frees up all allocated data in \b dst before copying the data from \b src
	 *
	 * \param dst A pointer for the settings to copy data to. May be NULL (fails copy)
	 * \param src A pointer to the settings to copy. May be NULL (fails copy)
	 * \param id The settings identifier to copy
	 *
	 *  \return \b TRUE for success, \b FALSE for failure.
	 */

	FREERDP_API BOOL freerdp_settings_copy_item(rdpSettings* dst, const rdpSettings* src,
	                                            SSIZE_T id);

	/** \brief Dumps the contents of a settings struct to a WLog logger
	 *
	 *  \param log The logger to write to, must not be NULL
	 *  \param level The WLog level to use for the log entries
	 *  \param settings A pointer to the settings to dump. May be NULL.
	 */
	FREERDP_API void freerdp_settings_dump(wLog* log, DWORD level, const rdpSettings* settings);

	/** \brief Dumps the difference between two settings structs to a WLog
	 *
	 *  \param log The logger to write to, must not be NULL.
	 *  \param  level The WLog level to use for the log entries.
	 *  \param src A pointer to the settings to dump. May be NULL.
	 *  \param other A pointer to the settings to dump. May be NULL.
	 *
	 *  \return \b TRUE if not equal, \b FALSE otherwise
	 */
	FREERDP_API BOOL freerdp_settings_print_diff(wLog* log, DWORD level, const rdpSettings* src,
	                                             const rdpSettings* other);

	FREERDP_API void freerdp_addin_argv_free(ADDIN_ARGV* args);

	WINPR_ATTR_MALLOC(freerdp_addin_argv_free, 1)
	FREERDP_API ADDIN_ARGV* freerdp_addin_argv_new(size_t argc, const char* argv[]);

	WINPR_ATTR_MALLOC(freerdp_addin_argv_free, 1)
	FREERDP_API ADDIN_ARGV* freerdp_addin_argv_clone(const ADDIN_ARGV* args);

	FREERDP_API BOOL freerdp_addin_argv_add_argument(ADDIN_ARGV* args, const char* argument);
	FREERDP_API BOOL freerdp_addin_argv_add_argument_ex(ADDIN_ARGV* args, const char* argument,
	                                                    size_t len);
	FREERDP_API BOOL freerdp_addin_argv_del_argument(ADDIN_ARGV* args, const char* argument);

	FREERDP_API int freerdp_addin_set_argument(ADDIN_ARGV* args, const char* argument);
	FREERDP_API int freerdp_addin_replace_argument(ADDIN_ARGV* args, const char* previous,
	                                               const char* argument);
	FREERDP_API int freerdp_addin_set_argument_value(ADDIN_ARGV* args, const char* option,
	                                                 const char* value);
	FREERDP_API int freerdp_addin_replace_argument_value(ADDIN_ARGV* args, const char* previous,
	                                                     const char* option, const char* value);

	FREERDP_API BOOL freerdp_device_collection_add(rdpSettings* settings, RDPDR_DEVICE* device);

	/** \brief Removed a device from the settings, returns ownership of the allocated device to
	 * caller.
	 *
	 *  \param settings the settings to remove the device from
	 *  \param device the device to remove
	 *
	 *  \since version 3.4.0
	 *
	 *  \return \b TRUE if the device was removed, \b FALSE if device was not found or is NULL
	 */
	FREERDP_API BOOL freerdp_device_collection_del(rdpSettings* settings,
	                                               const RDPDR_DEVICE* device);
	FREERDP_API RDPDR_DEVICE* freerdp_device_collection_find(rdpSettings* settings,
	                                                         const char* name);
	FREERDP_API RDPDR_DEVICE* freerdp_device_collection_find_type(rdpSettings* settings,
	                                                              UINT32 type);

	FREERDP_API void freerdp_device_free(RDPDR_DEVICE* device);

	WINPR_ATTR_MALLOC(freerdp_device_free, 1)
	FREERDP_API RDPDR_DEVICE* freerdp_device_new(UINT32 Type, size_t count, const char* args[]);

	WINPR_ATTR_MALLOC(freerdp_device_free, 1)
	FREERDP_API RDPDR_DEVICE* freerdp_device_clone(const RDPDR_DEVICE* device);

	FREERDP_API BOOL freerdp_device_equal(const RDPDR_DEVICE* one, const RDPDR_DEVICE* other);

	FREERDP_API void freerdp_device_collection_free(rdpSettings* settings);

	FREERDP_API BOOL freerdp_static_channel_collection_add(rdpSettings* settings,
	                                                       ADDIN_ARGV* channel);
	FREERDP_API BOOL freerdp_static_channel_collection_del(rdpSettings* settings, const char* name);
	FREERDP_API ADDIN_ARGV* freerdp_static_channel_collection_find(rdpSettings* settings,
	                                                               const char* name);
#if defined(WITH_FREERDP_DEPRECATED)
	FREERDP_API WINPR_DEPRECATED(ADDIN_ARGV* freerdp_static_channel_clone(ADDIN_ARGV* channel));
#endif

	FREERDP_API void freerdp_static_channel_collection_free(rdpSettings* settings);

	FREERDP_API BOOL freerdp_dynamic_channel_collection_add(rdpSettings* settings,
	                                                        ADDIN_ARGV* channel);
	FREERDP_API BOOL freerdp_dynamic_channel_collection_del(rdpSettings* settings,
	                                                        const char* name);
	FREERDP_API ADDIN_ARGV* freerdp_dynamic_channel_collection_find(const rdpSettings* settings,
	                                                                const char* name);

#if defined(WITH_FREERDP_DEPRECATED)
	FREERDP_API WINPR_DEPRECATED(ADDIN_ARGV* freerdp_dynamic_channel_clone(ADDIN_ARGV* channel));
#endif

	FREERDP_API void freerdp_dynamic_channel_collection_free(rdpSettings* settings);
	FREERDP_API void freerdp_capability_buffer_free(rdpSettings* settings);
	FREERDP_API BOOL freerdp_capability_buffer_copy(rdpSettings* settings, const rdpSettings* src);

	FREERDP_API void freerdp_server_license_issuers_free(rdpSettings* settings);
	FREERDP_API BOOL freerdp_server_license_issuers_copy(rdpSettings* settings, char** addresses,
	                                                     UINT32 count);

	FREERDP_API void freerdp_target_net_addresses_free(rdpSettings* settings);
	FREERDP_API BOOL freerdp_target_net_addresses_copy(rdpSettings* settings, char** addresses,
	                                                   UINT32 count);

	FREERDP_API void freerdp_performance_flags_make(rdpSettings* settings);
	FREERDP_API void freerdp_performance_flags_split(rdpSettings* settings);

	FREERDP_API BOOL freerdp_set_gateway_usage_method(rdpSettings* settings,
	                                                  UINT32 GatewayUsageMethod);
	FREERDP_API void freerdp_update_gateway_usage_method(rdpSettings* settings,
	                                                     UINT32 GatewayEnabled,
	                                                     UINT32 GatewayBypassLocal);

	/* DEPRECATED:
	 * the functions freerdp_get_param_* and freerdp_set_param_* are deprecated.
	 * use freerdp_settings_get_* and freerdp_settings_set_* as a replacement!
	 */
#if defined(WITH_FREERDP_DEPRECATED)
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_get_bool instead",
	                                 BOOL freerdp_get_param_bool(const rdpSettings* settings,
	                                                             int id));
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_bool instead",
	                                 int freerdp_set_param_bool(rdpSettings* settings, int id,
	                                                            BOOL param));

	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_get_int[16|32] instead",
	                                 int freerdp_get_param_int(const rdpSettings* settings,
	                                                           int id));
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_int[16|32] instead",
	                                 int freerdp_set_param_int(rdpSettings* settings, int id,
	                                                           int param));

	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_uint32 instead",
	                                 UINT32 freerdp_get_param_uint32(const rdpSettings* settings,
	                                                                 int id));
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_uint32 instead",
	                                 int freerdp_set_param_uint32(rdpSettings* settings, int id,
	                                                              UINT32 param));

	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_get_uint64 instead",
	                                 UINT64 freerdp_get_param_uint64(const rdpSettings* settings,
	                                                                 int id));
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_uint64 instead",
	                                 int freerdp_set_param_uint64(rdpSettings* settings, int id,
	                                                              UINT64 param));

	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_get_string instead",
	                                 char* freerdp_get_param_string(const rdpSettings* settings,
	                                                                int id));
	FREERDP_API WINPR_DEPRECATED_VAR("Use freerdp_settings_set_string instead",
	                                 int freerdp_set_param_string(rdpSettings* settings, int id,
	                                                              const char* param));
#endif

	/** \brief Returns \b TRUE if settings are in a valid state, \b FALSE otherwise
	 *
	 *  This function is meant to replace tideous return checks for \b freerdp_settings_set_* with a
	 * single check after these calls.
	 *
	 *  \param settings the settings instance to check
	 *
	 *  \return \b TRUE if valid, \b FALSE otherwise
	 */
	FREERDP_API BOOL freerdp_settings_are_valid(const rdpSettings* settings);

	/** \brief Returns a boolean settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the boolean key
	 */
	FREERDP_API BOOL freerdp_settings_get_bool(const rdpSettings* settings,
	                                           FreeRDP_Settings_Keys_Bool id);

	/** \brief Sets a BOOL settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_bool(rdpSettings* settings, FreeRDP_Settings_Keys_Bool id,
	                                           BOOL param);

	/** \brief Returns a INT16 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the INT16 key
	 */
	FREERDP_API INT16 freerdp_settings_get_int16(const rdpSettings* settings,
	                                             FreeRDP_Settings_Keys_Int16 id);

	/** \brief Sets a INT16 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_int16(rdpSettings* settings,
	                                            FreeRDP_Settings_Keys_Int16 id, INT16 param);

	/** \brief Returns a UINT16 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the UINT16 key
	 */
	FREERDP_API UINT16 freerdp_settings_get_uint16(const rdpSettings* settings,
	                                               FreeRDP_Settings_Keys_UInt16 id);

	/** \brief Sets a UINT16 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_uint16(rdpSettings* settings,
	                                             FreeRDP_Settings_Keys_UInt16 id, UINT16 param);

	/** \brief Returns a INT32 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the INT32 key
	 */
	FREERDP_API INT32 freerdp_settings_get_int32(const rdpSettings* settings,
	                                             FreeRDP_Settings_Keys_Int32 id);

	/** \brief Sets a INT32 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_int32(rdpSettings* settings,
	                                            FreeRDP_Settings_Keys_Int32 id, INT32 param);

	/** \brief Returns a UINT32 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the UINT32 key
	 */
	FREERDP_API UINT32 freerdp_settings_get_uint32(const rdpSettings* settings,
	                                               FreeRDP_Settings_Keys_UInt32 id);

	/** \brief Sets a UINT32 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_uint32(rdpSettings* settings,
	                                             FreeRDP_Settings_Keys_UInt32 id, UINT32 param);

	/** \brief Returns a INT64 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the INT64 key
	 */
	FREERDP_API INT64 freerdp_settings_get_int64(const rdpSettings* settings,
	                                             FreeRDP_Settings_Keys_Int64 id);

	/** \brief Sets a INT64 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_int64(rdpSettings* settings,
	                                            FreeRDP_Settings_Keys_Int64 id, INT64 param);

	/** \brief Returns a UINT64 settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the value of the UINT64 key
	 */
	FREERDP_API UINT64 freerdp_settings_get_uint64(const rdpSettings* settings,
	                                               FreeRDP_Settings_Keys_UInt64 id);

	/** \brief Sets a UINT64 settings value.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_uint64(rdpSettings* settings,
	                                             FreeRDP_Settings_Keys_UInt64 id, UINT64 param);

	/** \brief Returns a immutable string settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the immutable string pointer
	 */
	FREERDP_API const char* freerdp_settings_get_string(const rdpSettings* settings,
	                                                    FreeRDP_Settings_Keys_String id);

	/** \brief Returns a string settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the string pointer
	 */
	FREERDP_API char* freerdp_settings_get_string_writable(rdpSettings* settings,
	                                                       FreeRDP_Settings_Keys_String id);

	/** \brief Sets a string settings value. The \b param is copied.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set. If NULL allocates an empty string buffer of \b len size,
	 * otherwise a copy is created. \param len The length of \b param, 0 to remove the old entry.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_string_len(rdpSettings* settings,
	                                                 FreeRDP_Settings_Keys_String id,
	                                                 const char* param, size_t len);

	/** \brief Sets a string settings value. The \b param is copied.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set. If NULL removes the old entry, otherwise a copy is created.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_string(rdpSettings* settings,
	                                             FreeRDP_Settings_Keys_String id,
	                                             const char* param);

	/** \brief appends a string to a settings value. The \b param is copied.
	 *  If the initial value of the setting was not empty, @code <old value><separator><param>
	 * @endcode is created
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param separator The separator string to use. May be NULL (no separator)
	 *  \param param The value to append
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_append_string(rdpSettings* settings,
	                                                FreeRDP_Settings_Keys_String id,
	                                                const char* separator, const char* param);

	/** \brief Sets a string settings value. The \b param is converted to UTF-8 and the copy stored.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set. If NULL removes the old entry, otherwise a copy is created.
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_string_from_utf16(rdpSettings* settings,
	                                                        FreeRDP_Settings_Keys_String id,
	                                                        const WCHAR* param);

	/** \brief Sets a string settings value. The \b param is converted to UTF-8 and the copy stored.
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *  \param param The value to set. If NULL removes the old entry, otherwise a copy is created.
	 *  \param length The length of the WCHAR string in number of WCHAR characters
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_string_from_utf16N(rdpSettings* settings,
	                                                         FreeRDP_Settings_Keys_String id,
	                                                         const WCHAR* param, size_t length);
	/** \brief Return an allocated UTF16 string
	 *
	 * \param settings A pointer to the settings struct to use
	 * \param id The settings identifier
	 *
	 * \return An allocated, '\0' terminated WCHAR string or NULL
	 */
	FREERDP_API WCHAR* freerdp_settings_get_string_as_utf16(const rdpSettings* settings,
	                                                        FreeRDP_Settings_Keys_String id,
	                                                        size_t* pCharLen);

	/** \brief Returns a immutable pointer settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the immutable pointer value
	 */
	FREERDP_API const void* freerdp_settings_get_pointer(const rdpSettings* settings,
	                                                     FreeRDP_Settings_Keys_Pointer id);

	/** \brief Returns a mutable pointer settings value
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to query
	 *
	 *  \return the mutable pointer value
	 */
	FREERDP_API void* freerdp_settings_get_pointer_writable(rdpSettings* settings,
	                                                        FreeRDP_Settings_Keys_Pointer id);

	/** \brief Set a pointer to value \b data
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to update
	 *  \param data The data to set (direct update, no copy created, previous value overwritten)
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_pointer(rdpSettings* settings,
	                                              FreeRDP_Settings_Keys_Pointer id,
	                                              const void* data);

	/** \brief Set a pointer to value \b data
	 *
	 *  \param settings A pointer to the settings to query, must not be NULL.
	 *  \param id The key to update
	 *  \param data The data to set (copy created, previous value freed)
	 *
	 *  \return \b TRUE for success, \b FALSE for failure
	 */
	FREERDP_API BOOL freerdp_settings_set_pointer_len(rdpSettings* settings,
	                                                  FreeRDP_Settings_Keys_Pointer id,
	                                                  const void* data, size_t len);

	FREERDP_API const void* freerdp_settings_get_pointer_array(const rdpSettings* settings,
	                                                           FreeRDP_Settings_Keys_Pointer id,
	                                                           size_t offset);
	FREERDP_API void* freerdp_settings_get_pointer_array_writable(const rdpSettings* settings,
	                                                              FreeRDP_Settings_Keys_Pointer id,
	                                                              size_t offset);
	FREERDP_API BOOL freerdp_settings_set_pointer_array(rdpSettings* settings,
	                                                    FreeRDP_Settings_Keys_Pointer id,
	                                                    size_t offset, const void* data);

	FREERDP_API BOOL freerdp_settings_set_value_for_name(rdpSettings* settings, const char* name,
	                                                     const char* value);

	/** \brief Get a key index for the name string of that key
	 *
	 *  \param value A key name string like FreeRDP_ServerMode
	 *
	 *  \return The key index or -1 in case of an error (e.g. name does not exist)
	 */
	FREERDP_API SSIZE_T freerdp_settings_get_key_for_name(const char* value);

	/** \brief Get a key type for the name string of that key
	 *
	 *  \param value A key name string like FreeRDP_ServerMode
	 *
	 *  \return The key type (e.g. FREERDP_SETTINGS_TYPE_BOOL) or -1 in case of an error (e.g. name
	 * does not exist)
	 */
	FREERDP_API SSIZE_T freerdp_settings_get_type_for_name(const char* value);

	/** \brief Get a key type for the key index
	 *
	 *  \param key The key index like FreeRDP_ServerMode
	 *
	 *  \return The key type (e.g. FREERDP_SETTINGS_TYPE_BOOL) or -1 in case of an error (e.g. name
	 * does not exist)
	 */
	FREERDP_API SSIZE_T freerdp_settings_get_type_for_key(SSIZE_T key);

	/** \brief Returns the type name for a \b key
	 *
	 *  \param key the key number to stringify
	 *  \return the type name of the key or \b FREERDP_SETTINGS_TYPE_UNKNOWN
	 */
	FREERDP_API const char* freerdp_settings_get_type_name_for_key(SSIZE_T key);

	/** \brief Returns the type name for a \b type
	 *
	 *  \param type the type to stringify
	 *  \return the name of the key or \b FREERDP_SETTINGS_TYPE_UNKNOWN
	 */
	FREERDP_API const char* freerdp_settings_get_type_name_for_type(SSIZE_T type);

	/** \brief Returns the type name for a \b key
	 *
	 *  \param key the key number to stringify
	 *  \return the name of the key or \b NULL
	 */
	FREERDP_API const char* freerdp_settings_get_name_for_key(SSIZE_T key);

	/** \brief helper function to get a mask of supported codec flags.
	 *
	 *  This function checks various settings to create a mask of supported codecs
	 *  \b FreeRDP_CodecFlags defines the codecs
	 *
	 *  \param settings the settings to check
	 *
	 *  \return a mask of supported codecs
	 */
	FREERDP_API UINT32 freerdp_settings_get_codecs_flags(const rdpSettings* settings);

	/** \brief Parse capability data and apply to settings
	 *
	 *  The capability message is stored in raw form in the settings, the data parsed and applied to
	 * the settings.
	 *
	 *  \param settings A pointer to the settings to use
	 *  \param capsFlags A pointer to the capablity flags, must have capsCount fields
	 *  \param capsData A pointer array to the RAW capability data, must have capsCount fields
	 *  \param capsSizes A pointer to an array of RAW capability sizes, must have capsCount fields
	 *  \param capsCount The number of capabilities contained in the RAW data
	 *  \param serverReceivedCaps Indicates if the parser should assume to be a server or client
	 * instance
	 *
	 *  \return \b TRUE for success, \b FALSE in case of an error
	 */
	FREERDP_API BOOL freerdp_settings_update_from_caps(rdpSettings* settings, const BYTE* capsFlags,
	                                                   const BYTE** capsData,
	                                                   const UINT32* capsSizes, UINT32 capsCount,
	                                                   BOOL serverReceivedCaps);

	/** \brief A helper function to return the correct server name.
	 *
	 * The server name might be in key FreeRDP_ServerHostname or if used in
	 * FreeRDP_UserSpecifiedServerName. This function returns the correct name to use.
	 *
	 *  \param settings The settings to query, must not be NULL.
	 *
	 *  \return A string pointer or NULL in case of failure.
	 */
	FREERDP_API const char* freerdp_settings_get_server_name(const rdpSettings* settings);

	/** \brief Returns a stringified representation of RAIL support flags
	 *
	 *  \param flags The flags to stringify
	 *  \param buffer A pointer to the string buffer to write to
	 *  \param length The size of the string buffer
	 *
	 *  \return A pointer to \b buffer for success, NULL otherwise
	 */
	FREERDP_API const char* freerdp_rail_support_flags_to_string(UINT32 flags, char* buffer,
	                                                             size_t length);

	/** \brief Returns a stringified representation of the RDP protocol version.
	 *
	 *  \param version The RDP protocol version number.
	 *
	 *  \return A string representation of the protocol version as "RDP_VERSION_10_11" or
	 * "RDP_VERSION_UNKNOWN" for invalid/unknown versions
	 */
	FREERDP_API const char* freerdp_rdp_version_string(UINT32 version);

	/** \brief Returns a string representation of \b RDPDR_DTYP_*
	 *
	 *  \param type The integer of the \b RDPDR_DTYP_* to stringify
	 *
	 *  \return A string representation of the \b RDPDR_DTYP_* or "RDPDR_DTYP_UNKNOWN"
	 */
	FREERDP_API const char* freerdp_rdpdr_dtyp_string(UINT32 type);

	FREERDP_API const char* freerdp_encryption_level_string(UINT32 EncryptionLevel);
	FREERDP_API const char* freerdp_encryption_methods_string(UINT32 EncryptionLevel, char* buffer,
	                                                          size_t size);

	/** \brief returns a string representation of \b RNS_UD_XXBPP_SUPPORT values
	 *
	 *  return A string reprenentation of the bitmask.
	 */
	FREERDP_API const char* freerdp_supported_color_depths_string(UINT16 mask, char* buffer,
	                                                              size_t size);

	/** \brief return the configuration directory for the library
	 *  @return The current configuration path or \b NULL
	 *  @since version 3.6.0
	 */
	WINPR_ATTR_MALLOC(free, 1)
	FREERDP_API char* freerdp_settings_get_config_path(void);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* FREERDP_SETTINGS_H */
