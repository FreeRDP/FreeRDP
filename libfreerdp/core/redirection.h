/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Server Redirection
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_LIB_CORE_REDIRECTION_H
#define FREERDP_LIB_CORE_REDIRECTION_H

typedef struct rdp_redirection rdpRedirection;

#include "rdp.h"

#include <freerdp/freerdp.h>
#include <freerdp/log.h>
#include <freerdp/api.h>

#include <winpr/wlog.h>
#include <winpr/stream.h>

FREERDP_LOCAL int rdp_recv_enhanced_security_redirection_packet(rdpRdp* rdp, wStream* s);

FREERDP_LOCAL int rdp_redirection_apply_settings(rdpRdp* rdp);

FREERDP_LOCAL rdpRedirection* redirection_new(void);
FREERDP_LOCAL void redirection_free(rdpRedirection* redirection);

#define REDIR_TAG FREERDP_TAG("core.redirection")
#ifdef WITH_DEBUG_REDIR
#define DEBUG_REDIR(...) WLog_DBG(REDIR_TAG, __VA_ARGS__)
#else
#define DEBUG_REDIR(...) \
	do                   \
	{                    \
	} while (0)
#endif

#endif /* FREERDP_LIB_CORE_REDIRECTION_H */
