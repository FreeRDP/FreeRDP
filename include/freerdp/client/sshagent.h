/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SSH Agent Virtual Channel Extension
 *
 * Copyright 2017 Ben Cohen
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

#ifndef FREERDP_CHANNEL_CLIENT_SSHAGENT_H
#define FREERDP_CHANNEL_CLIENT_SSHAGENT_H

#include <freerdp/types.h>

#include <freerdp/message.h>
#include <freerdp/channels/cliprdr.h>
#include <freerdp/freerdp.h>

typedef struct _sshagent_client_context
{
  int ProtocolVersion;
  int MaxConnections;
} SSHAgentClientContext;


/*
 * The channel is defined by the sshagent channel in xrdp as follows.
 *
 * Server to client commands
 * -------------------------
 *
 * Capabilities (at start of channel stream):
 *
 *    INT32  SA_TAG_CAPABILITY
 *    INT32  SSHAGENT_CHAN_PROT_VERSION := 1
 *    INT32  SSHAGENT_MAX_CONNECTIONS
 *
 * Open connection:
 *
 *    INT32  SA_TAG_OPEN
 *    INT32  Connection id (0, ..., SSHAGENT_MAX_CONNECTIONS - 1)
 *
 * Send data:
 * 
 *    INT32  SA_TAG_WRITE
 *    INT32  Connection id (0, ..., SSHAGENT_MAX_CONNECTIONS - 1)
 *    INT32  Data length
 *    DATA   ...
 *
 * Close connection:
 *
 *    INT32  SA_TAG_CLOSE
 *    INT32  Connection id (0, ..., SSHAGENT_MAX_CONNECTIONS - 1)
 *      
 * Client to server commands
 * -------------------------
 *
 * Capabilities (in reply to server capabilities):
 *
 *    INT32  SA_TAG_CAPABILITY
 *    INT32  SSHAGENT_CHAN_PROT_VERSION := 1
 *    INT32  SSHAGENT_MAX_CONNECTIONS
 *
 * Send data:
 * 
 *    INT32  SA_TAG_WRITE
 *    INT32  Connection id (0, ..., SSHAGENT_MAX_CONNECTIONS - 1)
 *    INT32  Data length
 *    DATA   ...
 *
 * Close connection (abnormal):
 *
 *    INT32  SA_TAG_CLOSE
 *    INT32  Connection id (0, ..., SSHAGENT_MAX_CONNECTIONS - 1)
 */

#endif /* FREERDP_CHANNEL_CLIENT_SSHAGENT_H */
