#!/bin/bash -xe
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

MANIFEST=com.freerdp.FreeRDP

BUILD_BASE=$(mktemp -d)
if [ $# -gt 0 ];
then
	BUILD_BASE=$1
fi

echo "Using $BUILD_BASE as temporary build directory"
REPO=$BUILD_BASE/repo
BUILD=$BUILD_BASE/build
STATE=$BUILD_BASE/state

BUILDER=$(which flatpak-builder)
if [ ! -x "$BUILDER" ];
then
	echo "command 'flatpak-builder' could not be found, please install and add to PATH"
	exit 1
fi

FLATPAK=$(which flatpak)
if [ ! -x "$FLATPAK" ];
then
	echo "command 'flatpak' could not be found, please install and add to PATH"
	exit 1
fi

flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo --user
flatpak-builder -v --repo=$REPO --state-dir=$STATE $BUILD $SCRIPT_DIR/$MANIFEST.json --force-clean --user --install-deps-from=flathub
flatpak build-bundle -v $REPO $MANIFEST.flatpak $MANIFEST --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo
