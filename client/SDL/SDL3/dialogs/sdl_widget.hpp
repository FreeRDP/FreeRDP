/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SDL Client helper dialogs
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

#include <string>

#include <vector>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#if !defined(HAS_NOEXCEPT)
#if defined(__clang__)
#if __has_feature(cxx_noexcept)
#define HAS_NOEXCEPT
#endif
#elif defined(__GXX_EXPERIMENTAL_CXX0X__) && __GNUC__ * 10 + __GNUC_MINOR__ >= 46 || \
    defined(_MSC_FULL_VER) && _MSC_FULL_VER >= 190023026
#define HAS_NOEXCEPT
#endif
#endif

#ifndef HAS_NOEXCEPT
#define noexcept
#endif

class SdlWidget
{
  public:
	SdlWidget(SDL_Renderer* renderer, const SDL_FRect& rect, bool input);
	SdlWidget(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_IOStream* ops);
	SdlWidget(const SdlWidget& other) = delete;
	SdlWidget(SdlWidget&& other) noexcept;
	virtual ~SdlWidget();

	SdlWidget& operator=(const SdlWidget& other) = delete;
	SdlWidget& operator=(SdlWidget&& other) = delete;

	bool fill(SDL_Renderer* renderer, SDL_Color color);
	bool fill(SDL_Renderer* renderer, const std::vector<SDL_Color>& colors);
	bool update_text(SDL_Renderer* renderer, const std::string& text, SDL_Color fgcolor);
	bool update_text(SDL_Renderer* renderer, const std::string& text, SDL_Color fgcolor,
	                 SDL_Color bgcolor);

	[[nodiscard]] bool wrap() const;
	bool set_wrap(bool wrap = true, size_t width = 0);
	[[nodiscard]] const SDL_FRect& rect() const;

#define widget_log_error(res, what) SdlWidget::error_ex(res, what, __FILE__, __LINE__, __func__)
	static bool error_ex(Uint32 res, const char* what, const char* file, size_t line,
	                     const char* fkt);

  private:
	SDL_Texture* render_text(SDL_Renderer* renderer, const std::string& text, SDL_Color fgcolor,
	                         SDL_FRect& src, SDL_FRect& dst);
	SDL_Texture* render_text_wrapped(SDL_Renderer* renderer, const std::string& text,
	                                 SDL_Color fgcolor, SDL_FRect& src, SDL_FRect& dst);

	TTF_Font* _font = nullptr;
	SDL_Texture* _image = nullptr;
	SDL_FRect _rect;
	bool _input = false;
	bool _wrap = false;
	size_t _text_width = 0;
};

bool clear_window(SDL_Renderer* renderer);
