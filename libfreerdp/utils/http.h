/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Simple HTTP client request utility
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

#pragma once

#include <freerdp/api.h>

/** Replaces credential values in an HTTP request or response body by their length.
 *
 * Recognized are the form fields of an OAuth token request and the JSON fields of a token
 * response that carry a credential. Everything else, a discovery document in particular, is
 * copied through unchanged.
 *
 * @param data The body, may be \b nullptr if \b length is 0
 * @param length The length of the body in bytes, without a terminator
 * @return A newly allocated, NUL terminated copy with every credential value replaced, to be
 *         released with free(), or \b nullptr if the copy could not be allocated.
 */
WINPR_ATTR_MALLOC(free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL char* freerdp_http_redact_for_log(const char* data, size_t length);
