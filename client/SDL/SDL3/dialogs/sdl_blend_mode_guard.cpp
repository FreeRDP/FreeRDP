#include "sdl_blend_mode_guard.hpp"

SdlBlendModeGuard::SdlBlendModeGuard(const std::shared_ptr<SDL_Renderer>& renderer,
                                     SDL_BlendMode mode)
    : _renderer(renderer)
{
	const auto rcb = SDL_GetRenderDrawBlendMode(_renderer.get(), &_restore_mode);
	if (!rcb)
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
		            "[%s] SDL_GetRenderDrawBlendMode() failed with %s", __func__, SDL_GetError());
	else
	{
		const auto rbm = SDL_SetRenderDrawBlendMode(_renderer.get(), mode);
		if (!rbm)
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
			            "[%s] SDL_SetRenderDrawBlendMode() failed with %s", __func__,
			            SDL_GetError());
		else
			_current_mode = mode;
	}
}

bool SdlBlendModeGuard::update(SDL_BlendMode mode)
{
	if (_current_mode != mode)
	{
		if (!SDL_SetRenderDrawBlendMode(_renderer.get(), mode))
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
			            "[%s] SDL_SetRenderDrawBlendMode() failed with %s", __func__,
			            SDL_GetError());
			return false;
		}
		_current_mode = mode;
	}
	return true;
}

SdlBlendModeGuard::~SdlBlendModeGuard()
{
	const auto rbm = SDL_SetRenderDrawBlendMode(_renderer.get(), _restore_mode);
	if (!rbm)
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
		            "[%s] SDL_SetRenderDrawBlendMode() failed with %s", __func__, SDL_GetError());
}
