/**
 * WinPR: Windows Portable Runtime
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_H
#define WINPR_H

#include <winpr/platform.h>
#include <winpr/cast.h>
#include <winpr/wtypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

	WINPR_API void winpr_get_version(int* major, int* minor, int* revision);

	WINPR_ATTR_NODISCARD
	WINPR_API const char* winpr_get_version_string(void);

	WINPR_ATTR_NODISCARD
	WINPR_API const char* winpr_get_build_revision(void);

	WINPR_ATTR_NODISCARD
	WINPR_API const char* winpr_get_build_config(void);

	/** @brief set \b vendor and \b product information for an application
	 *
	 * This sets the application details for an application instance. These values determine where
	 * to look for configuration files and other vendor/product specific settings data.
	 *
	 * @note When calling this function, the compile time options \b
	 * WINPR_USE_VENDOR_PRODUCT_CONFIG_DIR is ignored and the config path will always have the
	 * format 'vendor/product' or 'vendor/product1' (1 for the actual version set)
	 *
	 * @param vendor A vendor name to use. Must not be \b nullptr. Must not contain forbidden
	 * filesystem symbols for any os. Must be less than \b MAX_PATH bytes.
	 * @param product A product name to use. Must not be \b nullptr. Must not contain forbidden
	 * filesystem symbols for any os. Must be less than \b MAX_PATH bytes.
	 * @param version An optional versioning value to append to paths to settings. Use \b -1 to
	 * disable.
	 *
	 * @return \b TRUE if set successfully, \b FALSE in case of any error.
	 * @since version 3.23.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API BOOL winpr_setApplicationDetails(const char* vendor, const char* product,
	                                           SSIZE_T version);

	/** @brief Get the current \b vendor string of the application. Defaults to \ref
	 * WINPR_VENDOR_STRING
	 *
	 * @return The current string set as \b vendor.
	 * @since version 3.23.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API const char* winpr_getApplicationDetailsVendor(void);

	/** @brief Get the current \b product string of the application. Defaults to \ref
	 * WINPR_PRODUCT_STRING
	 *
	 * @return The current string set as \b product.
	 * @since version 3.23.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API const char* winpr_getApplicationDetailsProduct(void);

	/** @brief Get the current \b version of the application. Defaults to \ref WINPR_API_VERSION
	 * if \b WITH_RESOURCE_VERSIONING is defined, otherwise \b -1
	 *
	 * @return The current number set as \b version
	 * @since version 3.23.0
	 */
	WINPR_ATTR_NODISCARD
	WINPR_API SSIZE_T winpr_getApplicationDetailsVersion(void);

#ifdef __cplusplus
}
#endif

#endif /* WINPR_H */
