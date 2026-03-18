#!/bin/bash -xe

RPMBUILD_BASE="~/rpmbuild/SOURCES"
if [ $# -gt 0 ]; then
  RPMBUILD_BASE="$1"
fi

git rev-parse --short HEAD >source_version
