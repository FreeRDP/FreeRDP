#!/bin/sh

if [ -z ${FREERDP_SDL_OFF} ];
then
	echo "SDL $(which sdl-freerdp)"
	sdl-freerdp $@
	exit $rc
else
	if [ -z $XDG_SESSION_TYPE ];
	then
		echo "XDG_SESSION_TYPE undefined"
		exit -1
	elif [ "$XDG_SESSION_TYPE" = "wayland" ];
	then
		if [ -z $FREERDP_WAYLAND_OFF ];
		then
			echo "wayland $(which wlfreerdp)"
			wlfreerdp $@
			exit $rc
		else
			echo "X11 $(which xfreerdp)"
			xfreerdp $@
			exit $rc
		fi
	elif [ "$XDG_SESSION_TYPE" = "x11" ];
	then
		echo "X11 $(which xfreerdp)"
		xfreerdp $@
		exit $rc
	else
		echo "XDG_SESSION_TYPE $XDG_SESSION_TYPE not handled"
		exit -1
	fi
fi
