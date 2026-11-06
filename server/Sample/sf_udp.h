/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Sample Server (UDP multitransport)
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

#ifndef FREERDP_SERVER_SAMPLE_SF_UDP_H
#define FREERDP_SERVER_SAMPLE_SF_UDP_H

#include <stdio.h>

#include <freerdp/config.h>
#include <freerdp/settings.h>

#include "sfreerdp.h"

#if defined(WITH_RDPEUDP)

/** Offers the reliable tunnel. Soft-Sync moves the dynamic channels onto this tunnel. */
WINPR_ATTR_NODISCARD
BOOL sf_udp_configure(rdpSettings* settings, BOOL udp);

/** How long the main loop waits. */
WINPR_ATTR_NODISCARD
DWORD sf_udp_wait_ms(BOOL udp);

/** Examines the channels. drdynvc opens one time only. With a tunnel it must wait until the
 *  client joined its channels. The shadow server also waits for the activation. */
WINPR_ATTR_NODISCARD
BOOL sf_udp_check_channels(HANDLE vcm, BOOL udp, BOOL activated);

/** Sends an echo every second. The echo shows the round trip of a dynamic channel. The round trip
 *  is over the UDP tunnel after Soft-Sync moved the channel there. */
void sf_udp_tick(testPeerContext* context, BOOL udp);

void sf_udp_free(testPeerContext* context);

void sf_udp_usage(FILE* fp);

/** @return TRUE if arg is the option */
WINPR_ATTR_NODISCARD
BOOL sf_udp_parse_option(const char* arg, BOOL* udp);

#else

/* These are macros. Without UDP the arguments do not exist, and the code stays as on master. */
#define sf_udp_configure(settings, udp) (TRUE)
#define sf_udp_wait_ms(udp) (INFINITE)
#define sf_udp_check_channels(vcm, udp, activated) WTSVirtualChannelManagerCheckFileDescriptor(vcm)
#define sf_udp_tick(context, udp) \
	do                            \
	{                             \
	} while (0)
#define sf_udp_free(context) \
	do                       \
	{                        \
	} while (0)
#define sf_udp_usage(fp) \
	do                   \
	{                    \
	} while (0)
#define sf_udp_parse_option(arg, udp) (FALSE)

#endif

#endif /* FREERDP_SERVER_SAMPLE_SF_UDP_H */
