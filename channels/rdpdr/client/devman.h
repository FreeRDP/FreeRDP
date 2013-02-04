/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Device Redirection Virtual Channel
 *
 * Copyright 2010-2011 Vic Lee
 * Copyright 2010-2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_RDPDR_CLIENT_DEVMAN_H
#define FREERDP_CHANNEL_RDPDR_CLIENT_DEVMAN_H

BOOL devman_load_device_service(DEVMAN* devman, RDPDR_DEVICE* device);
DEVICE* devman_get_device_by_id(DEVMAN* devman, UINT32 id);

DEVMAN* devman_new(rdpSvcPlugin* plugin);
void devman_free(DEVMAN* devman);

#endif /* FREERDP_CHANNEL_RDPDR_CLIENT_DEVMAN_H */
