#!/bin/bash -xe

RPMBUILD_BASE="~/rpmbuild/SOURCES"
if [ $# -gt 0 ]; then
  RPMBUILD_BASE="$1"
fi

if [ ! -d external/webview ]; then
  git clone -b navigation-listener --depth=1 https://github.com/akallabeth/webview external/webview
fi

(
  cd external/webview
  mkdir -p "$RPMBUILD_BASE"
  git archive --format=tar --prefix=webview/ HEAD --output $RPMBUILD_BASE/webview.tar.bz2
)

git rev-parse --short HEAD >source_version
