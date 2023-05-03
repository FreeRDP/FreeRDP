#!/bin/sh

rm -rf $(pwd)/build/$TARGET_ARCH
mkdir -p $(pwd)/build/$TARGET_ARCH
docker build -t win32-builder --build-arg ARCH .
docker compose up dist-builder