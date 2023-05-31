/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Clipboard Channel
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
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
#include "sdl_wayland_cliprdr_context.hpp"

#include "../sdl_freerdp.hpp"

SdlWaylandCliprdrContext::SdlWaylandCliprdrContext(SdlContext* sdl)
    : SdlCliprdrContext(sdl), _display(wl_display_connect(nullptr)),
      _registry(wl_display_get_registry(_display))
{
	const wl_registry_listener registry_listener = {
		&SdlWaylandCliprdrContext::global_registry_add,
		&SdlWaylandCliprdrContext::global_registry_remove
	};
	auto rc = wl_registry_add_listener(_registry, &registry_listener, this);
	WLog_INFO("xxxx", "aaa '%d'", rc);
}

SdlWaylandCliprdrContext::~SdlWaylandCliprdrContext()
{
	wl_data_device_manager_destroy(_data_device_manager);
	wl_registry_destroy(_registry);
	wl_display_disconnect(_display);
}

void SdlWaylandCliprdrContext::global_registry_add(void* data, wl_registry* wl_registry,
                                                   uint32_t name, const char* interface,
                                                   uint32_t version)
{
	WLog_INFO("xxxx", "aaa '%s'", interface);
}

void SdlWaylandCliprdrContext::global_registry_remove(void* data, wl_registry* wl_registry,
                                                      uint32_t name)
{
	WLog_INFO("xxxx", "aaa '%u'", name);
}
