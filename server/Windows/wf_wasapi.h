#ifndef FREERDP_SERVER_WIN_WASAPI_H
#define FREERDP_SERVER_WIN_WASAPI_H

#include <freerdp/server/rdpsnd.h>
#include "wf_interface.h"

WINPR_ATTR_NODISCARD int wf_rdpsnd_set_latest_peer(wfPeerContext* peer);

WINPR_ATTR_NODISCARD int wf_wasapi_activate(RdpsndServerContext* context);

WINPR_ATTR_NODISCARD int wf_wasapi_get_device_string(LPWSTR pattern, LPWSTR* deviceStr);

WINPR_ATTR_NODISCARD DWORD WINAPI wf_rdpsnd_wasapi_thread(LPVOID lpParam);

#endif /* FREERDP_SERVER_WIN_WASAPI_H */
