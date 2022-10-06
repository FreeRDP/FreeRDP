/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Licensing API
 *
 * Copyright 2018 David Fort <contact@hardening-consulting.com>
 * Copyright 2022 Armin Novak <armin.novak@thincast.com>
 * Copyright 2022 Thincast Technologies GmbH
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

#ifndef FREERDP_LICENSE_H
#define FREERDP_LICENSE_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum
	{
		LICENSE_STATE_INITIAL,
		LICENSE_STATE_CONFIGURED,
		LICENSE_STATE_REQUEST,
		LICENSE_STATE_NEW_REQUEST,
		LICENSE_STATE_PLATFORM_CHALLENGE,
		LICENSE_STATE_PLATFORM_CHALLENGE_RESPONSE,
		LICENSE_STATE_COMPLETED,
		LICENSE_STATE_ABORTED
	} LICENSE_STATE;

	typedef enum
	{
		LICENSE_TYPE_INVALID = 0,
		LICENSE_TYPE_NONE,
		LICENSE_TYPE_ISSUED
	} LICENSE_TYPE;

	typedef struct rdp_license rdpLicense;

	FREERDP_API rdpLicense* license_get(rdpContext* context);
	FREERDP_API LICENSE_STATE license_get_state(const rdpLicense* license);
	FREERDP_API LICENSE_TYPE license_get_type(const rdpLicense* license);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_LICENSE_H */
