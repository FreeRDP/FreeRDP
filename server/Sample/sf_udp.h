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

/** Configures the reliable tunnel; Soft-Sync then migrates dynamic channels to it. */
WINPR_ATTR_NODISCARD
BOOL sf_udp_configure(rdpSettings* settings, BOOL udp);

/** Returns the timeout for the main loop wait. */
WINPR_ATTR_NODISCARD
DWORD sf_udp_wait_ms(BOOL udp);

/** Checks channel status. Since drdynvc is opened only once, it must wait for the session
 *  to be activated when using a tunnel; the shadow server also waits for activation. */
WINPR_ATTR_NODISCARD
BOOL sf_udp_check_channels(HANDLE vcm, BOOL udp, BOOL activated);

/** Sends a periodic echo to measure dynamic channel RTT, which occurs over the UDP tunnel
 *  after Soft-Sync migration. */
void sf_udp_tick(testPeerContext* context, BOOL udp);

void sf_udp_free(testPeerContext* context);

void sf_udp_usage(FILE* fp);

/** @return TRUE if arg matches the option */
WINPR_ATTR_NODISCARD
BOOL sf_udp_parse_option(const char* arg, BOOL* udp);

#else

/* Stubs for non-UDP builds to maintain compatibility with the main server loop. */
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

#endif /* End of FREERDP_SERVER_SAMPLE_SF_UDP_H */
