/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#import "mfreerdp.h"
#import "mf_client.h"

#import "freerdp/freerdp.h"
#import "freerdp/channels/channels.h"
#import "freerdp/client/cliprdr.h"

int mac_cliprdr_send_client_format_list(CliprdrClientContext* cliprdr);

void mac_cliprdr_init(mfContext* mfc, CliprdrClientContext* cliprdr);
void mac_cliprdr_uninit(mfContext* mfc, CliprdrClientContext* cliprdr);
