/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * JSON parser wrapper
 *
 * Copyright 2024 Armin Novak <anovak@thincast.com>
 * Copyright 2024 Thincast Technologies GmbH
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

#ifndef WINPR_UTILS_JSON
#define WINPR_UTILS_JSON

#include <winpr/winpr.h>
#include <winpr/string.h>
#include <winpr/wtypes.h>

/** @defgroup WINPR_JSON WinPR JSON wrapper
 *  @since version 3.6.0
 *  @brief Wrapper around cJSON or JSONC libraries
 *  @{
 */

#ifdef __cplusplus
extern "C"
{
#endif

	typedef void WINPR_JSON;

	/**
	 * @brief Get the library version string
	 *
	 * @param buffer a string buffer to hold the version string
	 * @param len the length of the buffer
	 * @return length of the version string in bytes or negative for error
	 * @since version 3.6.0
	 */
	WINPR_API int WINPR_JSON_version(char* buffer, size_t len);

	/**
	 * @brief Delete a @ref WINPR_JSON object
	 *
	 * @param item The instance to delete
	 * @since version 3.6.0
	 */
	WINPR_API void WINPR_JSON_Delete(WINPR_JSON* item);

	/**
	 * @brief Parse a '\0' terminated JSON string
	 *
	 * @param value A '\0' terminated JSON string
	 * @return A @ref WINPR_JSON object holding the parsed string or \b NULL if failed
	 * @since version 3.6.0
	 */
	WINPR_ATTR_MALLOC(WINPR_JSON_Delete, 1)
	WINPR_API WINPR_JSON* WINPR_JSON_Parse(const char* value);

	/**
	 * @brief Parse a JSON string
	 *
	 * @param value A JSON string
	 * @param buffer_length The length in bytes of the JSON string
	 * @return A @ref WINPR_JSON object holding the parsed string or \b NULL if failed
	 * @since version 3.6.0
	 */
	WINPR_ATTR_MALLOC(WINPR_JSON_Delete, 1)
	WINPR_API WINPR_JSON* WINPR_JSON_ParseWithLength(const char* value, size_t buffer_length);

	/**
	 * @brief Get the number of arrayitems from an array
	 *
	 * @param array the JSON instance to query
	 * @return number of array items
	 * @since version 3.6.0
	 */
	WINPR_API size_t WINPR_JSON_GetArraySize(const WINPR_JSON* array);

	/**
	 * @brief Return a pointer to an item in the array
	 *
	 * @param array the JSON instance to query
	 * @param index The index of the array item
	 * @return A pointer to the array item or \b NULL if failed
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_GetArrayItem(const WINPR_JSON* array, size_t index);

	/**
	 * @brief Return a pointer to an JSON object item
	 * @param object the JSON object
	 * @param string the name of the object
	 * @return A pointer to the object identified by @ref string or \b NULL
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_GetObjectItem(const WINPR_JSON* object, const char* string);

	/**
	 * @brief Same as @ref WINPR_JSON_GetObjectItem but with case insensitive matching
	 *
	 * @param object the JSON instance to query
	 * @param string the name of the object
	 * @return A pointer to the object identified by @ref string or \b NULL
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_GetObjectItemCaseSensitive(const WINPR_JSON* object,
	                                                            const char* string);

	/**
	 * @brief Check if JSON has an object matching the name
	 * @param object the JSON instance
	 * @param string the name of the object
	 * @return \b TRUE if found, \b FALSE otherwise
	 * @since version 3.6.0
	 */
	WINPR_API BOOL WINPR_JSON_HasObjectItem(const WINPR_JSON* object, const char* string);

	/**
	 * @brief Return an error string
	 * @return A string describing the last error that occured or \b NULL
	 * @since version 3.6.0
	 */
	WINPR_API const char* WINPR_JSON_GetErrorPtr(void);

	/**
	 * @brief Return the String value of a JSON item
	 * @param item the JSON item to query
	 * @return The string value or \b NULL if failed
	 * @since version 3.6.0
	 */
	WINPR_API const char* WINPR_JSON_GetStringValue(WINPR_JSON* item);

	/**
	 * @brief Return the Number value of a JSON item
	 * @param item the JSON item to query
	 * @return The Number value or \b NaN if failed
	 * @since version 3.6.0
	 */
	WINPR_API double WINPR_JSON_GetNumberValue(const WINPR_JSON* item);

	/**
	 * @brief Check if JSON item is valid
	 * @param item the JSON item to query
	 * @return \b TRUE if valid, \b FALSE otherwise
	 * @since version 3.6.0
	 */
	WINPR_API BOOL WINPR_JSON_IsInvalid(const WINPR_JSON* item);

	/**
	 * @brief Check if JSON item is BOOL value False
	 * @param item the JSON item to query
	 * @return \b TRUE if False, \b FALSE otherwise
	 * @since version 3.6.0
	 */
	WINPR_API BOOL WINPR_JSON_IsFalse(const WINPR_JSON* item);

	/**
	 * @brief Check if JSON item is BOOL value True
	 * @param item the JSON item to query
	 * @return \b TRUE if True, \b FALSE otherwise
	 * @since version 3.6.0
	 */
	WINPR_API BOOL WINPR_JSON_IsTrue(const WINPR_JSON* item);

	/**
	 * @brief Check if JSON item is of type BOOL
	 * @param item the JSON item to query
	 * @return \b TRUE if the type is BOOL, \b FALSE otherwise
	 * @since version 3.6.0
	 */
	WINPR_API BOOL WINPR_JSON_IsBool(const WINPR_JSON* item);

	/**
	 * @brief Check if JSON item is Null
	 * @param item the JSON item to query
	 * @return \b TRUE if it is Null, \b FALSE otherwise
	 * @since version 3.6.0
	 */
	WINPR_API BOOL WINPR_JSON_IsNull(const WINPR_JSON* item);

	/**
	 * @brief Check if JSON item is of type Number
	 * @param item the JSON item to query
	 * @return \b TRUE if the type is Number, \b FALSE otherwise
	 * @since version 3.6.0
	 */
	WINPR_API BOOL WINPR_JSON_IsNumber(const WINPR_JSON* item);

	/**
	 * @brief Check if JSON item is of type String
	 * @param item the JSON item to query
	 * @return \b TRUE if the type is String, \b FALSE otherwise
	 * @since version 3.6.0
	 */
	WINPR_API BOOL WINPR_JSON_IsString(const WINPR_JSON* item);

	/**
	 * @brief Check if JSON item is of type Array
	 * @param item the JSON item to query
	 * @return \b TRUE if the type is Array, \b FALSE otherwise
	 * @since version 3.6.0
	 */
	WINPR_API BOOL WINPR_JSON_IsArray(const WINPR_JSON* item);

	/**
	 * @brief Check if JSON item is of type Object
	 * @param item the JSON item to query
	 * @return \b TRUE if the type is Object, \b FALSE otherwise
	 * @since version 3.6.0
	 */
	WINPR_API BOOL WINPR_JSON_IsObject(const WINPR_JSON* item);

	/**
	 * @brief WINPR_JSON_CreateNull
	 * @return a new JSON item of type and value Null
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_CreateNull(void);

	/**
	 * @brief WINPR_JSON_CreateTrue
	 * @return a new JSON item of type Bool and value True
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_CreateTrue(void);

	/**
	 * @brief WINPR_JSON_CreateFalse
	 * @return a new JSON item of type Bool and value False
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_CreateFalse(void);

	/**
	 * @brief WINPR_JSON_CreateBool
	 * @param boolean the value the JSON item should have
	 * @return a new JSON item of type Bool
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_CreateBool(BOOL boolean);

	/**
	 * @brief WINPR_JSON_CreateNumber
	 * @param num the number value of the new item
	 * @return a new JSON item of type Number
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_CreateNumber(double num);

	/**
	 * @brief WINPR_JSON_CreateString
	 * @param string The string value of the new item
	 * @return a new JSON item of type String
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_CreateString(const char* string);

	/**
	 * @brief WINPR_JSON_CreateArray
	 * @return a new JSON item of type array, empty
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_CreateArray(void);

	/**
	 * @brief WINPR_JSON_CreateObject
	 * @return a new JSON item of type Object
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_CreateObject(void);

	/**
	 * @brief WINPR_JSON_AddNullToObject
	 * @param object The JSON object the new item is added to
	 * @param name The name of the object
	 * @return the new JSON item added
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_AddNullToObject(WINPR_JSON* object, const char* name);

	/**
	 * @brief WINPR_JSON_AddTrueToObject
	 * @param object The JSON object the new item is added to
	 * @param name The name of the object
	 * @return the new JSON item added
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_AddTrueToObject(WINPR_JSON* object, const char* name);

	/**
	 * @brief WINPR_JSON_AddFalseToObject
	 * @param object The JSON object the new item is added to
	 * @param name The name of the object
	 * @return the new JSON item added
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_AddFalseToObject(WINPR_JSON* object, const char* name);

	/**
	 * @brief WINPR_JSON_AddBoolToObject
	 * @param object The JSON object the new item is added to
	 * @param name The name of the object
	 * @return the new JSON item added
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_AddBoolToObject(WINPR_JSON* object, const char* name,
	                                                 BOOL boolean);

	/**
	 * @brief WINPR_JSON_AddNumberToObject
	 * @param object The JSON object the new item is added to
	 * @param name The name of the object
	 * @return the new JSON item added
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_AddNumberToObject(WINPR_JSON* object, const char* name,
	                                                   double number);

	/**
	 * @brief WINPR_JSON_AddStringToObject
	 * @param object The JSON object the new item is added to
	 * @param name The name of the object
	 * @return the new JSON item added
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_AddStringToObject(WINPR_JSON* object, const char* name,
	                                                   const char* string);

	/**
	 * @brief WINPR_JSON_AddObjectToObject
	 * @param object The JSON object the new item is added to
	 * @param name The name of the object
	 * @return the new JSON item added
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_AddObjectToObject(WINPR_JSON* object, const char* name);

	/**
	 * @brief WINPR_JSON_AddArrayToObject
	 * @param object The JSON object the new item is added to
	 * @param name The name of the object
	 * @return the new JSON item added
	 * @since version 3.6.0
	 */
	WINPR_API WINPR_JSON* WINPR_JSON_AddArrayToObject(WINPR_JSON* object, const char* name);

	/**
	 * @brief Add an item to an existing array
	 * @param array An array to add to, must not be \b NULL
	 * @param item An item to add, must not be \b NULL
	 * @return \b TRUE for success, \b FALSE for failure
	 * @since version 3.7.0
	 */
	WINPR_API BOOL WINPR_JSON_AddItemToArray(WINPR_JSON* array, WINPR_JSON* item);

	/**
	 * @brief Serialize a JSON instance to string
	 * for minimal size without formatting see @ref WINPR_JSON_PrintUnformatted
	 *
	 * @param item The JSON instance to serialize
	 * @return A string representation of the JSON instance or \b NULL
	 * @since version 3.6.0
	 */
	WINPR_API char* WINPR_JSON_Print(WINPR_JSON* item);

	/**
	 * @brief Serialize a JSON instance to string without formatting
	 * for human readable formatted output see @ref WINPR_JSON_Print
	 *
	 * @param item The JSON instance to serialize
	 * @return A string representation of the JSON instance or \b NULL
	 * @since version 3.6.0
	 */
	WINPR_API char* WINPR_JSON_PrintUnformatted(WINPR_JSON* item);

#ifdef __cplusplus
}
#endif

/** @} */

#endif
