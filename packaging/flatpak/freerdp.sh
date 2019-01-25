#!/bin/sh
if [ -z $XDG_SESSION_TYPE ];
then
	echo "XDG_SESSION_TYPE undefined"
	exit -1
elif [ "$XDG_SESSION_TYPE" = "wayland" ];
then
	echo "wayland $(which wlfreerdp)"
	wlfreerdp $@
elif [ "$XDG_SESSION_TYPE" = "x11" ];
then
	echo "X11 $(which xfreerdp)"
	xfreerdp $@
	exit $rc
else
	echo "XDG_SESSION_TYPE $XDG_SESSION_TYPE not handled"
	exit -1
fi
