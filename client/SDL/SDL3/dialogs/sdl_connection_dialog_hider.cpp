#include "sdl_connection_dialog_hider.hpp"
#include "../sdl_freerdp.hpp"

SDLConnectionDialogHider::SDLConnectionDialogHider(SdlContext* sdl)
    : _sdl(sdl), _visible(_sdl->dialog.isVisible())
{
	_sdl->dialog.show(false);
}

SDLConnectionDialogHider::~SDLConnectionDialogHider()
{
	_sdl->dialog.show(_visible);
}
