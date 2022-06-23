/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * pf_channel_drdynvc
 *
 * Copyright 2022 David Fort <contact@hardening-consulting.com>
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
#ifndef SERVER_PROXY_CHANNELS_PF_CHANNEL_DRDYNVC_H_
#define SERVER_PROXY_CHANNELS_PF_CHANNEL_DRDYNVC_H_

#include <freerdp/server/proxy/proxy_context.h>

BOOL pf_channel_setup_drdynvc(proxyData* pdata, pServerStaticChannelContext* channel);

#endif /* SERVER_PROXY_CHANNELS_PF_CHANNEL_DRDYNVC_H_ */
