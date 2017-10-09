#!/bin/sh

ln -s packaging/deb/freerdp-nightly  debian
git rev-parse --short HEAD > .source_version
