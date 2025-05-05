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
#include <algorithm>

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

SdlWidget::SdlWidget([[maybe_unused]] std::shared_ptr<SDL_Renderer>& renderer,
                     const SDL_FRect& rect, bool input)
    : _engine(TTF_CreateRendererTextEngine(renderer.get()), TTF_DestroySurfaceTextEngine),
      _rect(rect), _input(input)
{
	assert(renderer);

	auto ops = SDL3ResourceManager::get(SDLResourceManager::typeFonts(),
	                                    "OpenSans-VariableFont_wdth,wght.ttf");
	if (!ops)
		widget_log_error(false, "SDLResourceManager::get");
	else
	{
		_font = std::shared_ptr<TTF_Font>(TTF_OpenFontIO(ops, true, 64), TTF_CloseFont);
		if (!_font)
			widget_log_error(false, "TTF_OpenFontRW");
	}
}

#if defined(WITH_SDL_IMAGE_DIALOGS)
SdlWidget::SdlWidget(std::shared_ptr<SDL_Renderer>& renderer, const SDL_FRect& rect,
                     SDL_IOStream* ops)
    : _engine(TTF_CreateRendererTextEngine(renderer.get()), TTF_DestroySurfaceTextEngine),
      _rect(rect)
{
	if (ops)
	{
		_image = std::shared_ptr<SDL_Texture>(IMG_LoadTexture_IO(renderer.get(), ops, 1),
		                                      SDL_DestroyTexture);
		if (!_image)
			widget_log_error(false, "IMG_LoadTexture_IO");
	}
}
#endif

SdlWidget::SdlWidget(SdlWidget&& other) noexcept
    : _font(std::move(other._font)), _image(std::move(other._image)),
      _engine(std::move(other._engine)), _rect(other._rect), _input(other._input),
      _wrap(other._wrap), _text_width(other._text_width)
{
	other._font = nullptr;
	other._image = nullptr;
	other._engine = nullptr;
}

std::shared_ptr<SDL_Texture> SdlWidget::render_text(std::shared_ptr<SDL_Renderer>& renderer,
                                                    const std::string& text, SDL_Color fgcolor,
                                                    SDL_FRect& src, SDL_FRect& dst) const
{
	auto surface = std::shared_ptr<SDL_Surface>(
	    TTF_RenderText_Blended(_font.get(), text.c_str(), 0, fgcolor), SDL_DestroySurface);
	if (!surface)
	{
		widget_log_error(false, "TTF_RenderText_Blended");
		return nullptr;
	}

	auto texture = std::shared_ptr<SDL_Texture>(
	    SDL_CreateTextureFromSurface(renderer.get(), surface.get()), SDL_DestroyTexture);
	if (!texture)
	{
		widget_log_error(false, "SDL_CreateTextureFromSurface");
		return nullptr;
	}

	if (!_engine)
	{
		widget_log_error(false, "TTF_CreateRendererTextEngine");
		return nullptr;
	}

	std::unique_ptr<TTF_Text, decltype(&TTF_DestroyText)> txt(
	    TTF_CreateText(_engine.get(), _font.get(), text.c_str(), text.size()), TTF_DestroyText);

	if (!txt)
	{
		widget_log_error(false, "TTF_CreateText");
		return nullptr;
	}
	int w = 0;
	int h = 0;
	if (!TTF_GetTextSize(txt.get(), &w, &h))
	{
		widget_log_error(false, "TTF_GetTextSize");
		return nullptr;
	}

	src.w = static_cast<float>(w);
	src.h = static_cast<float>(h);
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
	dst.w = std::min(dst.w, sws);
	if (src.w > dws)
	{
		src.x = src.w - dws;
		src.w = dws;
	}
	return texture;
}

static float scale(float dw, float dh)
{
	const auto scale = dh / dw;
	const auto dr = dh * scale;
	return dr;
}

std::shared_ptr<SDL_Texture> SdlWidget::render_text_wrapped(std::shared_ptr<SDL_Renderer>& renderer,
                                                            const std::string& text,
                                                            SDL_Color fgcolor, SDL_FRect& src,
                                                            SDL_FRect& dst) const
{
	assert(_text_width < INT32_MAX);

	auto surface = std::shared_ptr<SDL_Surface>(
	    TTF_RenderText_Blended_Wrapped(_font.get(), text.c_str(), 0, fgcolor,
	                                   static_cast<int>(_text_width)),
	    SDL_DestroySurface);
	if (!surface)
	{
		widget_log_error(false, "TTF_RenderText_Blended");
		return nullptr;
	}

	src.w = static_cast<float>(surface->w);
	src.h = static_cast<float>(surface->h);

	auto texture = std::shared_ptr<SDL_Texture>(
	    SDL_CreateTextureFromSurface(renderer.get(), surface.get()), SDL_DestroyTexture);
	if (!texture)
	{
		widget_log_error(false, "SDL_CreateTextureFromSurface");
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
	auto dh = scale(src.w, src.h);
	dst.h = std::min<float>(dh, dst.h);

	return texture;
}

SdlWidget::~SdlWidget() = default;

bool SdlWidget::error_ex(bool success, const char* what, const char* file, size_t line,
                         const char* fkt)
{
	if (success)
	{
		// Flip SDL3 convention to existing code convention to minimize code changes
		return false;
	}
	static wLog* log = nullptr;
	if (!log)
		log = WLog_Get(TAG);
	// Use -1 as it indicates error similar to SDL2 conventions
	// sdl_log_error_ex treats any value other than 0 as SDL error
	return sdl_log_error_ex(-1, log, what, file, line, fkt);
}

bool SdlWidget::createWindowAndRenderer(const std::string& title, size_t width, size_t height,
                                        std::shared_ptr<SDL_Window>& _window,
                                        std::shared_ptr<SDL_Renderer>& _renderer)
{
	auto w = WINPR_ASSERTING_INT_CAST(int, width);
	auto h = WINPR_ASSERTING_INT_CAST(int, height);
	SDL_Renderer* renderer = nullptr;
	SDL_Window* window = nullptr;
	auto rc = SDL_CreateWindowAndRenderer(
	    title.c_str(), w, h, SDL_WINDOW_MOUSE_FOCUS | SDL_WINDOW_INPUT_FOCUS, &window, &renderer);
	_renderer = std::shared_ptr<SDL_Renderer>(renderer, SDL_DestroyRenderer);
	_window = std::shared_ptr<SDL_Window>(window, SDL_DestroyWindow);
	return rc;
}

static bool draw_rect(std::shared_ptr<SDL_Renderer>& renderer, const SDL_FRect* rect,
                      SDL_Color color)
{
	const auto drc = SDL_SetRenderDrawColor(renderer.get(), color.r, color.g, color.b, color.a);
	if (widget_log_error(drc, "SDL_SetRenderDrawColor"))
		return false;

	const auto rc = SDL_RenderFillRect(renderer.get(), rect);
	return !widget_log_error(rc, "SDL_RenderFillRect");
}

bool SdlWidget::fill(std::shared_ptr<SDL_Renderer>& renderer, SDL_Color color) const
{
	std::vector<SDL_Color> colors = { color };
	return fill(renderer, colors);
}

bool SdlWidget::fill(std::shared_ptr<SDL_Renderer>& renderer,
                     const std::vector<SDL_Color>& colors) const
{
	assert(renderer);
	SDL_BlendMode mode = SDL_BLENDMODE_INVALID;
	const auto rcb = SDL_GetRenderDrawBlendMode(renderer.get(), &mode);
	if (widget_log_error(rcb, "SDL_GetRenderDrawBlendMode()"))
		return false;

	const auto rbm = SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_NONE);
	if (widget_log_error(rbm, "SDL_SetRenderDrawBlendMode(SDL_BLENDMODE_NONE)"))
		return false;

	SDL_BlendMode cmode = SDL_BLENDMODE_NONE;
	for (auto color : colors)
	{
		if (!draw_rect(renderer, &_rect, color))
			return false;

		if (cmode == SDL_BLENDMODE_NONE)
		{
			const auto rba = SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_ADD);
			if (widget_log_error(rba, "SDL_SetRenderDrawBlendMode(SDL_BLENDMODE_NONE)"))
				return false;
			cmode = SDL_BLENDMODE_ADD;
		}
	}

	const auto rbr = SDL_SetRenderDrawBlendMode(renderer.get(), mode);
	return !widget_log_error(rbr, "SDL_SetRenderDrawBlendMode(mode)");
}

bool SdlWidget::update_text(std::shared_ptr<SDL_Renderer>& renderer, const std::string& text,
                            SDL_Color fgcolor, SDL_Color bgcolor) const
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

bool SdlWidget::update_text(std::shared_ptr<SDL_Renderer>& renderer, const std::string& text,
                            SDL_Color fgcolor) const
{

	if (text.empty())
		return true;

	SDL_FRect src{};
	SDL_FRect dst{};

	std::shared_ptr<SDL_Texture> texture;
	if (_image)
	{
		texture = _image;
		dst = _rect;
		auto propId = SDL_GetTextureProperties(_image.get());
		auto w = SDL_GetNumberProperty(propId, SDL_PROP_TEXTURE_WIDTH_NUMBER, -1);
		auto h = SDL_GetNumberProperty(propId, SDL_PROP_TEXTURE_HEIGHT_NUMBER, -1);
		if (w < 0 || h < 0)
			widget_log_error(false, "SDL_GetTextureProperties");
		src.w = static_cast<float>(w);
		src.h = static_cast<float>(h);
	}
	else if (_wrap)
		texture = render_text_wrapped(renderer, text, fgcolor, src, dst);
	else
		texture = render_text(renderer, text, fgcolor, src, dst);
	if (!texture)
		return false;

	const auto rc = SDL_RenderTexture(renderer.get(), texture.get(), &src, &dst);
	return !widget_log_error(rc, "SDL_RenderCopy");
}

bool SdlWidget::clear_window(std::shared_ptr<SDL_Renderer>& renderer)
{
	assert(renderer);

	const auto rbm = SDL_SetRenderDrawBlendMode(renderer.get(), SDL_BLENDMODE_NONE);
	if (widget_log_error(rbm, "SDL_SetRenderDrawBlendMode(SDL_BLENDMODE_NONE)"))
		return false;

	const auto drc = SDL_SetRenderDrawColor(renderer.get(), backgroundcolor.r, backgroundcolor.g,
	                                        backgroundcolor.b, backgroundcolor.a);
	if (widget_log_error(drc, "SDL_SetRenderDrawColor"))
		return false;

	const auto rcls = SDL_RenderClear(renderer.get());
	return !widget_log_error(rcls, "SDL_RenderClear");
}
