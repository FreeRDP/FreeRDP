/**
 * FreeRDP: A Remote Desktop Protocol client.
 * File System Virtual Channel
 *
 * Copyright 2010-2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2010-2011 Vic Lee
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

#ifndef __DEVMAN_H
#define __DEVMAN_H

DEVMAN* devman_new(rdpSvcPlugin* plugin);
void devman_free(DEVMAN* devman);
boolean devman_load_device_service(DEVMAN* devman, RDP_PLUGIN_DATA* plugin_data);
DEVICE* devman_get_device_by_id(DEVMAN* devman, uint32 id);

#endif /* __DEVMAN_H */
