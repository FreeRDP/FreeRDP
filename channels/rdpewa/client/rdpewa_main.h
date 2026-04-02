/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * WebAuthn Virtual Channel Extension [MS-RDPEWA]
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

#ifndef FREERDP_CHANNEL_RDPEWA_CLIENT_MAIN_H
#define FREERDP_CHANNEL_RDPEWA_CLIENT_MAIN_H

#include <freerdp/config.h>

#include <freerdp/dvc.h>
#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/freerdp.h>
#include <freerdp/channels/log.h>
#include <freerdp/client/channels.h>

#define TAG CHANNELS_TAG("rdpewa.client")

typedef struct
{
	GENERIC_DYNVC_PLUGIN base;
	rdpContext* rdp_context;
} RDPEWA_PLUGIN;

/** @brief Per-channel callback with async work tracking */
typedef struct
{
	GENERIC_CHANNEL_CALLBACK base;
	HANDLE workerThread;
} RDPEWA_CHANNEL_CALLBACK;

#endif /* FREERDP_CHANNEL_RDPEWA_CLIENT_MAIN_H */
