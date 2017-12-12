/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Mac OS X Server
 *
 * Copyright 2012 Corey Clayton <can.of.tuna@gmail.com>
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

#ifndef FREERDP_SERVER_MAC_PEER_H
#define FREERDP_SERVER_MAC_PEER_H

#include "mf_interface.h"

BOOL mf_peer_accepted(freerdp_listener* instance, freerdp_peer* client);

#endif /* FREERDP_SERVER_MAC_PEER_H */
