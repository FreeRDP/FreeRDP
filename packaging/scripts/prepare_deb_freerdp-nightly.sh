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

# this is for adding optional build dependencies based on what is available in the distribution
if command -v apt-cache >/dev/null 2>&1; then
  add_optional_build_depend() {
    pkg=$1
    min_version=${2:-}
    candidate=$(apt-cache policy "$pkg" | awk '/Candidate:/ { print $2; exit }')
    if [ -n "$candidate" ] && [ "$candidate" != "(none)" ]; then
      if [ -n "$min_version" ] && ! dpkg --compare-versions "$candidate" ge "$min_version"; then
        return
      fi
      if [ -n "$min_version" ]; then
        optional_build_depends="${optional_build_depends} ${pkg} (>= ${min_version}),
"
      else
        optional_build_depends="${optional_build_depends} ${pkg},
"
      fi
    fi
  }

  add_optional_build_depend libaom-dev 2.0
  add_optional_build_depend libopenh264-dev
  add_optional_build_depend libyuv-dev
  add_optional_build_depend libmp3lame-dev
  add_optional_build_depend libfaad-dev
  add_optional_build_depend libgstreamer1.0-dev
  add_optional_build_depend libgstreamer-plugins-base1.0-dev
fi

if [ -n "$optional_build_depends" ]; then
  tmp_control=$(mktemp)
  awk -v deps="$optional_build_depends" '
    /^ libavutil-dev,$/ {
      print
      printf "%s", deps
      next
    }
    { print }
  ' debian/control >"$tmp_control"
  mv "$tmp_control" debian/control
fi

source_version=${SOURCE_VERSION:-}
if [ -z "$source_version" ]; then
  source_version=$(git rev-parse --short HEAD 2>/dev/null || printf unknown)
fi
printf '%s\n' "$source_version" >.source_version
