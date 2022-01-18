/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Sample Server (Advanced Input)
 *
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

#ifndef FREERDP_SERVER_SAMPLE_SF_AINPUT_H
#define FREERDP_SERVER_SAMPLE_SF_AINPUT_H

#include <freerdp/freerdp.h>
#include <freerdp/listener.h>
#include <freerdp/server/ainput.h>

#include "sfreerdp.h"

void sf_peer_ainput_init(testPeerContext* context);
void sf_peer_ainput_uninit(testPeerContext* context);

BOOL sf_peer_ainput_running(testPeerContext* context);
BOOL sf_peer_ainput_start(testPeerContext* context);
BOOL sf_peer_ainput_stop(testPeerContext* context);

#endif /* FREERDP_SERVER_SAMPLE_SF_AINPUT_H */
