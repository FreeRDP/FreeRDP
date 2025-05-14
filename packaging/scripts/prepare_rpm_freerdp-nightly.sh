#!/bin/bash

if [ ! -d external/webview ]; then
  git clone -b navigation-listener --depth=1 https://github.com/akallabeth/webview external/webview
fi

git rev-parse --short HEAD >source_version
