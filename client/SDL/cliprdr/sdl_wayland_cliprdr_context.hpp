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
#pragma once

#include <memory>
#include <map>
#include <thread>
#include <atomic>

#include <wayland-client-protocol.h>

#include "sdl_cliprdr_context.hpp"

class SdlWaylandCliprdrContext : public SdlCliprdrContext
{
	friend class SdlCliprdrContext;

  protected:
	SdlWaylandCliprdrContext(SdlContext* sdl);
	virtual ~SdlWaylandCliprdrContext();

  private:
	void run();

  private:
	static void global_registry_add(void* data, struct wl_registry* wl_registry, uint32_t name,
	                                const char* interface, uint32_t version);

	static void global_registry_remove(void* data, struct wl_registry* wl_registry, uint32_t name);

  private:
	std::map<uint32_t, std::string> _registry_map;
	wl_display* _display;
	wl_registry* _registry;

	using WlSeatPtr = std::unique_ptr<wl_seat, decltype(&wl_seat_destroy)>;
	WlSeatPtr _seat;

	using WlDataDeviceManagerPtr =
	    std::unique_ptr<wl_data_device_manager, decltype(&wl_data_device_manager_destroy)>;
	WlDataDeviceManagerPtr _data_device_manager;
	std::atomic<bool> _running;
	std::thread _thread;
};
