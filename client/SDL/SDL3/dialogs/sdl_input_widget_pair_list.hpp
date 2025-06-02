/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
 *
 * Copyright 2025 Armin Novak <armin.novak@thincast.com>
 * Copyright 2025 Thincast Technologies GmbH
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

#include <string>
#include <vector>
#include <SDL3/SDL.h>

#include "sdl_widget_list.hpp"
#include "sdl_input_widget_pair.hpp"
#include "sdl_buttons.hpp"

class SdlInputWidgetPairList : public SdlWidgetList
{
  public:
	SdlInputWidgetPairList(const std::string& title, const std::vector<std::string>& labels,
	                       const std::vector<std::string>& initial,
	                       const std::vector<Uint32>& flags);
	SdlInputWidgetPairList(const SdlInputWidgetPairList& other) = delete;
	SdlInputWidgetPairList(SdlInputWidgetPairList&& other) = delete;

	~SdlInputWidgetPairList() override;

	SdlInputWidgetPairList& operator=(const SdlInputWidgetPairList& other) = delete;
	SdlInputWidgetPairList& operator=(SdlInputWidgetPairList&& other) = delete;

	int run(std::vector<std::string>& result);

  protected:
	bool updateInternal() override;
	ssize_t get_index(const SDL_MouseButtonEvent& button);

  private:
	enum
	{
		INPUT_BUTTON_ACCEPT = 1,
		INPUT_BUTTON_CANCEL = -2
	};

	ssize_t next(ssize_t current);
	[[nodiscard]] bool valid(ssize_t current) const;
	std::shared_ptr<SdlInputWidgetPair> get(ssize_t index);

	std::vector<std::shared_ptr<SdlInputWidgetPair>> _list;
};
