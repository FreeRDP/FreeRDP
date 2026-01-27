#include "sdl_connection_dialog_hider.hpp"
#include "../sdl_context.hpp"

SDLConnectionDialogHider::SDLConnectionDialogHider(SdlContext* sdl)
    : _sdl(sdl), _visible(_sdl->getDialog().isVisible())
{
	_sdl->getDialog().show(false);
}

SDLConnectionDialogHider::~SDLConnectionDialogHider()
{
	_sdl->getDialog().show(_visible);
}
