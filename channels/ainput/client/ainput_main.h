/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Advanced Input Virtual Channel Extension
 *
 * Copyright 2022 Armin Novak <anovak@thincast.com>
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

#ifndef FREERDP_CHANNEL_AINPUT_CLIENT_MAIN_H
#define FREERDP_CHANNEL_AINPUT_CLIENT_MAIN_H

#include <freerdp/config.h>
#include <freerdp/dvc.h>
#include <freerdp/types.h>
#include <freerdp/addin.h>
#include <freerdp/channels/log.h>

#define DVC_TAG CHANNELS_TAG("ainput.client")
#ifdef WITH_DEBUG_DVC
#define DEBUG_DVC(...) WLog_DBG(DVC_TAG, __VA_ARGS__)
#else
#define DEBUG_DVC(...) \
	do                 \
	{                  \
	} while (0)
#endif

#endif /* FREERDP_CHANNEL_AINPUT_CLIENT_MAIN_H */
