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

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "sdl_widget.hpp"
#include "../sdl_utils.hpp"

#include "res/sdl3_resource_manager.hpp"

#include <freerdp/log.h>

#if defined(WITH_SDL_IMAGE_DIALOGS)
#include <SDL3_image/SDL_image.h>
#endif

#define TAG CLIENT_TAG("SDL.widget")

static const SDL_Color backgroundcolor = { 0x38, 0x36, 0x35, 0xff };

static const Uint32 hpadding = 10;

SdlWidget::SdlWidget(SDL_Renderer* renderer, const SDL_FRect& rect, bool input)
    : _rect(rect), _input(input)
{
	assert(renderer);

	auto ops = SDL3ResourceManager::get(SDLResourceManager::typeFonts(),
	                                    "OpenSans-VariableFont_wdth,wght.ttf");
	if (!ops)
		widget_log_error(-1, "SDLResourceManager::get");
	else
	{
		_font = TTF_OpenFontIO(ops, 1, 64);
		if (!_font)
			widget_log_error(-1, "TTF_OpenFontRW");
	}
}

#if defined(WITH_SDL_IMAGE_DIALOGS)
SdlWidget::SdlWidget(SDL_Renderer* renderer, const SDL_FRect& rect, SDL_IOStream* ops) : _rect(rect)
{
	if (ops)
	{
		_image = IMG_LoadTexture_IO(renderer, ops, 1);
		if (!_image)
			widget_log_error(-1, "IMG_LoadTexture_IO");
	}
}
#endif

SdlWidget::SdlWidget(SdlWidget&& other) noexcept
    : _font(std::move(other._font)), _image(other._image), _rect(std::move(other._rect)),
      _input(other._input), _wrap(other._wrap), _text_width(other._text_width)
{
	other._font = nullptr;
	other._image = nullptr;
}

SDL_Texture* SdlWidget::render_text(SDL_Renderer* renderer, const std::string& text,
                                    SDL_Color fgcolor, SDL_FRect& src, SDL_FRect& dst)
{
	auto surface = TTF_RenderUTF8_Blended(_font, text.c_str(), fgcolor);
	if (!surface)
	{
		widget_log_error(-1, "TTF_RenderText_Blended");
		return nullptr;
	}

	auto texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_DestroySurface(surface);
	if (!texture)
	{
		widget_log_error(-1, "SDL_CreateTextureFromSurface");
		return nullptr;
	}

	int w = 0;
	int h = 0;
	TTF_SizeUTF8(_font, text.c_str(), &w, &h);

	src.w = w;
	src.h = h;
	/* Do some magic:
	 * - Add padding before and after text
	 * - if text is too long only show the last elements
	 * - if text is too short only update used space
	 */
	dst = _rect;
	dst.x += hpadding;
	dst.w -= 2 * hpadding;
	const float scale = dst.h / src.h;
	const float sws = (src.w) * scale;
	const float dws = (dst.w) / scale;
	if (dst.w > sws)
		dst.w = sws;
	if (src.w > dws)
	{
		src.x = src.w - dws;
		src.w = dws;
	}
	return texture;
}

SDL_Texture* SdlWidget::render_text_wrapped(SDL_Renderer* renderer, const std::string& text,
                                            SDL_Color fgcolor, SDL_FRect& src, SDL_FRect& dst)
{
	Sint32 w = 0;
	Sint32 h = 0;
	TTF_SizeUTF8(_font, " ", &w, &h);
	auto surface = TTF_RenderUTF8_Blended_Wrapped(_font, text.c_str(), fgcolor, _text_width);
	if (!surface)
	{
		widget_log_error(-1, "TTF_RenderText_Blended");
		return nullptr;
	}

	src.w = surface->w;
	src.h = surface->h;

	auto texture = SDL_CreateTextureFromSurface(renderer, surface);
	SDL_DestroySurface(surface);
	if (!texture)
	{
		widget_log_error(-1, "SDL_CreateTextureFromSurface");
		return nullptr;
	}

	/* Do some magic:
	 * - Add padding before and after text
	 * - if text is too long only show the last elements
	 * - if text is too short only update used space
	 */
	dst = _rect;
	dst.x += hpadding;
	dst.w -= 2 * hpadding;
	const float scale = static_cast<float>(src.h) / static_cast<float>(src.w);
	auto dh = src.h * scale;
	if (dh < dst.h)
		dst.h = dh;

	return texture;
}

SdlWidget::~SdlWidget()
{
	TTF_CloseFont(_font);
	if (_image)
		SDL_DestroyTexture(_image);
}

bool SdlWidget::error_ex(Uint32 res, const char* what, const char* file, size_t line,
                         const char* fkt)
{
	static wLog* log = nullptr;
	if (!log)
		log = WLog_Get(TAG);
	return sdl_log_error_ex(res, log, what, file, line, fkt);
}

static bool draw_rect(SDL_Renderer* renderer, const SDL_FRect* rect, SDL_Color color)
{
	const int drc = SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	if (widget_log_error(drc, "SDL_SetRenderDrawColor"))
		return false;

	const int rc = SDL_RenderFillRect(renderer, rect);
	return !widget_log_error(rc, "SDL_RenderFillRect");
}

bool SdlWidget::fill(SDL_Renderer* renderer, SDL_Color color)
{
	std::vector<SDL_Color> colors = { color };
	return fill(renderer, colors);
}

bool SdlWidget::fill(SDL_Renderer* renderer, const std::vector<SDL_Color>& colors)
{
	assert(renderer);
	SDL_BlendMode mode = SDL_BLENDMODE_INVALID;
	SDL_GetRenderDrawBlendMode(renderer, &mode);
	SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
	for (auto color : colors)
	{
		draw_rect(renderer, &_rect, color);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
	}
	SDL_SetRenderDrawBlendMode(renderer, mode);
	return true;
}

bool SdlWidget::update_text(SDL_Renderer* renderer, const std::string& text, SDL_Color fgcolor,
                            SDL_Color bgcolor)
{
	assert(renderer);

	if (!fill(renderer, bgcolor))
		return false;
	return update_text(renderer, text, fgcolor);
}

bool SdlWidget::wrap() const
{
	return _wrap;
}

bool SdlWidget::set_wrap(bool wrap, size_t width)
{
	_wrap = wrap;
	_text_width = width;
	return _wrap;
}

const SDL_FRect& SdlWidget::rect() const
{
	return _rect;
}

bool SdlWidget::update_text(SDL_Renderer* renderer, const std::string& text, SDL_Color fgcolor)
{

	if (text.empty())
		return true;

	SDL_FRect src{};
	SDL_FRect dst{};

	SDL_Texture* texture = nullptr;
	if (_image)
	{
		texture = _image;
		dst = _rect;
		int w = 0;
		int h = 0;
		auto rc = SDL_QueryTexture(_image, nullptr, nullptr, &w, &h);
		src.w = w;
		src.h = h;
		if (rc < 0)
			widget_log_error(rc, "SDL_QueryTexture");
	}
	else if (_wrap)
		texture = render_text_wrapped(renderer, text, fgcolor, src, dst);
	else
		texture = render_text(renderer, text, fgcolor, src, dst);
	if (!texture)
		return false;

	const int rc = SDL_RenderTexture(renderer, texture, &src, &dst);
	if (!_image)
		SDL_DestroyTexture(texture);
	if (rc < 0)
		return !widget_log_error(rc, "SDL_RenderCopy");
	return true;
}

bool clear_window(SDL_Renderer* renderer)
{
	assert(renderer);

	const int drc = SDL_SetRenderDrawColor(renderer, backgroundcolor.r, backgroundcolor.g,
	                                       backgroundcolor.b, backgroundcolor.a);
	if (widget_log_error(drc, "SDL_SetRenderDrawColor"))
		return false;

	const int rcls = SDL_RenderClear(renderer);
	return !widget_log_error(rcls, "SDL_RenderClear");
}
