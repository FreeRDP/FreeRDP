/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Authentication redirection virtual channel, in-process servicing
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

#ifndef FREERDP_CHANNEL_RDPEAR_CLIENT_INPROC_H
#define FREERDP_CHANNEL_RDPEAR_CLIENT_INPROC_H

#include <winpr/wtypes.h>
#include <winpr/stream.h>

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/dvc.h>

/** In-process servicing of the redirected [MS-RDPEAR] calls with the NLA context of a connection */
typedef struct rdpear_inproc rdpearInproc;

FREERDP_LOCAL void rdpear_inproc_free(rdpearInproc* inproc);

WINPR_ATTR_MALLOC(rdpear_inproc_free, 1)
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpearInproc* rdpear_inproc_new(rdpContext* context);

/** Service one TSRemoteGuardPacket and write the reply to the channel.
 *
 * @return 0 on success, otherwise a Win32 error code
 */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL UINT rdpear_inproc_receive(rdpearInproc* inproc, IWTSVirtualChannel* channel,
                                         wStream* s);

#endif /* FREERDP_CHANNEL_RDPEAR_CLIENT_INPROC_H */
