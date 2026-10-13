/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * External command transport
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef FREERDP_CORE_PROXY_COMMAND_H
#define FREERDP_CORE_PROXY_COMMAND_H

#include <freerdp/freerdp.h>
#include <freerdp/transport_io.h>
#include <winpr/sysinfo.h>
#include <winpr/interlocked.h>

typedef struct
{
	rdpContext* context;
	ULONGLONG deadline;
	volatile LONG received;
} proxyCommandBase;

WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL proxy_command_check(proxyCommandBase* base);
FREERDP_LOCAL void proxy_command_mark_received(proxyCommandBase* base);
WINPR_ATTR_NODISCARD
FREERDP_LOCAL DWORD proxy_command_wait_timeout(proxyCommandBase* base, DWORD requested);
WINPR_ATTR_NODISCARD
FREERDP_LOCAL BOOL proxy_command_validate_settings(rdpSettings* settings);
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpTransportLayer* proxy_command_connect(rdpContext* context, const char* hostname,
                                                       int port, DWORD timeout);
WINPR_ATTR_NODISCARD
FREERDP_LOCAL rdpTransportLayer* proxy_command_platform_connect(rdpContext* context,
                                                                const char* command, DWORD timeout);

#endif
