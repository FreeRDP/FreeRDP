/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Services for virtual channel plugins
 *
 * Copyright 2026 David Fort <contact@hardening-consulting.com>
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

#ifndef FREERDP_LIB_CORE_PLUGIN_SERVICES_H
#define FREERDP_LIB_CORE_PLUGIN_SERVICES_H

#include <freerdp/api.h>
#include <freerdp/freerdp.h>
#include <freerdp/wts_services.h>

/** @brief the context's IWTSPluginServiceProvider (rdpContext::pluginServiceProvider), one
 *  reference for the caller */
WINPR_ATTR_NODISCARD
FREERDP_LOCAL IWTSPluginServiceProvider* plugin_services_new(rdpContext* context);

/** @brief cuts the provider from the context's client services, before the client frees them
 *  (ContextFree): the shared window change subscription is dropped, and the services handed out
 *  then answer E_UNEXPECTED. Channels and plugins may still hold them. */
FREERDP_LOCAL void plugin_services_detach(IWTSPluginServiceProvider* provider);

#endif /* FREERDP_LIB_CORE_PLUGIN_SERVICES_H */
