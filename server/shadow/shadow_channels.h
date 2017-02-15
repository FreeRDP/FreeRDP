/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_SHADOW_SERVER_CHANNELS_H
#define FREERDP_SHADOW_SERVER_CHANNELS_H

#include <freerdp/server/shadow.h>

#include <winpr/crt.h>
#include <winpr/synch.h>

#include "shadow_encomsp.h"
#include "shadow_remdesk.h"
#include "shadow_rdpsnd.h"
#include "shadow_audin.h"
#include "shadow_rdpgfx.h"

#ifdef __cplusplus
extern "C" {
#endif

UINT shadow_client_channels_post_connect(rdpShadowClient* client);
void shadow_client_channels_free(rdpShadowClient* client);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_SHADOW_SERVER_CHANNELS_H */
