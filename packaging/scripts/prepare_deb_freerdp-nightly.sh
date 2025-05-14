#!/bin/sh

if [ ! -d external/webview ]; then
  git clone -b navigation-listener --depth=1 https://github.com/akallabeth/webview external/webview
fi

ln -s packaging/deb/freerdp-nightly debian
git rev-parse --short HEAD >.source_version
