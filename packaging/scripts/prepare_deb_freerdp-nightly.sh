#!/bin/sh
set -e

if [ ! -d external/webview ] && [ "${FREERDP_SKIP_WEBVIEW_CLONE:-0}" != 1 ]; then
  git clone -b navigation-listener --depth=1 https://github.com/akallabeth/webview external/webview
fi

if [ -L debian ]; then
  rm debian
elif [ -e debian ]; then
  echo "debian already exists" >&2
  exit 1
fi

cp -a packaging/deb/freerdp-nightly debian

source_version=${SOURCE_VERSION:-}
if [ -z "$source_version" ]; then
  source_version=$(git rev-parse --short HEAD 2>/dev/null || printf unknown)
fi
printf '%s\n' "$source_version" >.source_version
