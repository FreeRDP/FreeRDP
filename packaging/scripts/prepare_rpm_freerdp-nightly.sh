#!/bin/bash

if [ ! -d external/webview ]; then
  git clone -b navigation-listener --depth=1 https://github.com/akallabeth/webview external/webview
fi

(
	cd external/webview
	git archive --format=tar --prefix=webview HEAD --output ~/rpmbuild/SOURCES/webview.tar.bz2
)

git rev-parse --short HEAD >source_version
