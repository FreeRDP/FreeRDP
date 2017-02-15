/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Sample Server (Audio Input)
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

#ifndef SF_AUDIN_H
#define SF_AUDIN_H

#include <freerdp/freerdp.h>
#include <freerdp/listener.h>
#include <freerdp/server/audin.h>

#include "sfreerdp.h"

void sf_peer_audin_init(testPeerContext* context);

#endif /* WF_AUDIN_H */

