/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SSH Agent Virtual Channel Extension
 *
 * Copyright 2013 Christian Hofstaedtler
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

#ifndef SSHAGENT_MAIN_H
#define SSHAGENT_MAIN_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <winpr/stream.h>

#include <freerdp/svc.h>
#include <freerdp/addin.h>
#include <freerdp/channels/log.h>

#define DVC_TAG CHANNELS_TAG("sshagent.client")
#ifdef WITH_DEBUG_SSHAGENT
#define DEBUG_SSHAGENT(...) WLog_DBG(DVC_TAG, __VA_ARGS__)
#else
#define DEBUG_SSHAGENT(...) do { } while (0)
#endif

#endif /* SSHAGENT_MAIN_H */

