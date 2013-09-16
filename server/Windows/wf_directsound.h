#ifndef WF_DSOUND_H
#define WF_DSOUND_H

#include <freerdp/server/rdpsnd.h>
#include "wf_interface.h"

int wf_rdpsnd_set_latest_peer(wfPeerContext* peer);

int wf_directsound_activate(RdpsndServerContext* context);

DWORD WINAPI wf_rdpsnd_directsound_thread(LPVOID lpParam);

#endif