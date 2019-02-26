#ifndef FREERDP_PROXY_CLIENT_CHANNELS_H
#define FREERDP_PROXY_CLIENT_CHANNELS_H

#include <freerdp/freerdp.h>
#include <freerdp/client/channels.h>

int tf_on_channel_connected(freerdp* instance, const char* name,
                            void* pInterface);
int tf_on_channel_disconnected(freerdp* instance, const char* name,
                               void* pInterface);

void tf_OnChannelConnectedEventHandler(void* context,
                                       ChannelConnectedEventArgs* e);
void tf_OnChannelDisconnectedEventHandler(void* context,
        ChannelDisconnectedEventArgs* e);

#endif /* FREERDP_PROXY_CLIENT_CHANNELS_H */
