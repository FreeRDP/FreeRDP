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

#include <string>
#include <algorithm>

#include "../sdl_freerdp.hpp"

SdlWaylandCliprdrContext::SdlWaylandCliprdrContext(SdlContext* sdl)
    : SdlCliprdrContext(sdl), _display(wl_display_connect(nullptr)),
      _registry(wl_display_get_registry(_display)), _seat(nullptr, wl_seat_destroy),
      _data_device_manager(nullptr, wl_data_device_manager_destroy), _running(true),
      _thread(&SdlWaylandCliprdrContext::run, this)
{
}

SdlWaylandCliprdrContext::~SdlWaylandCliprdrContext()
{
	_running = false;
	_thread.join();
	wl_registry_destroy(_registry);
	wl_display_disconnect(_display);
}

void SdlWaylandCliprdrContext::run()
{
	const wl_registry_listener registry_listener = {
		&SdlWaylandCliprdrContext::global_registry_add,
		&SdlWaylandCliprdrContext::global_registry_remove
	};
	auto rc = wl_registry_add_listener(_registry, &registry_listener, this);
	while (_running)
	{
		auto rc = wl_display_roundtrip(_display);
		if (rc < 0)
			break;
	}
}

void SdlWaylandCliprdrContext::global_registry_add(void* data, wl_registry* wl_registry,
                                                   uint32_t name, const char* interface,
                                                   uint32_t version)
{
	auto ctx = static_cast<SdlWaylandCliprdrContext*>(data);
	const std::string cname(interface);
	if (cname.compare("wl_seat") == 0)
	{
		auto ptr = reinterpret_cast<wl_seat*>(
		    wl_registry_bind(wl_registry, name, &wl_seat_interface, std::min(5u, version)));
		ctx->_seat.reset(ptr);
		ctx->_registry_map.emplace(name, cname);
	}
	else if (cname.compare("wl_data_device_manager") == 0)
	{
		auto ptr = reinterpret_cast<wl_data_device_manager*>(wl_registry_bind(
		    wl_registry, name, &wl_data_device_manager_interface, std::min(3u, version)));
		ctx->_data_device_manager.reset(ptr);
		ctx->_registry_map.emplace(name, cname);
	}
}

void SdlWaylandCliprdrContext::global_registry_remove(void* data, wl_registry* wl_registry,
                                                      uint32_t name)
{
	auto ctx = static_cast<SdlWaylandCliprdrContext*>(data);

	auto cname = ctx->_registry_map[name];
	if (cname.compare("wl_seat") == 0)
		ctx->_seat.reset();
	else if (cname.compare("wl_data_device_manager") == 0)
		ctx->_data_device_manager.reset();
}
