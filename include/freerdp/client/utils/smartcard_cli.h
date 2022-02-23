/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Smartcard client functions
 *
 * Copyright 2021 David Fort <contact@hardening-consulting.com>
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

#ifndef UTILS_SMARTCARD_CLI_H__
#define UTILS_SMARTCARD_CLI_H__

#include <freerdp/api.h>
#include <freerdp/settings.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

	FREERDP_API BOOL freerdp_smartcard_list(const rdpSettings* settings);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* UTILS_SMARTCARD_CLI_H__ */
