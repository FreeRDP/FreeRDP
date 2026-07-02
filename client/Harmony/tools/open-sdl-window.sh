#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-sdl-probe"
CLIENT_BIN="${BUILD_DIR}/client/SDL/SDL3/sdl-freerdp"

HOST="${1:-}"
USERNAME="${2:-}"
PASSWORD="${3:-}"

if [[ -z "${HOST}" || -z "${USERNAME}" || -z "${PASSWORD}" ]]; then
  echo "Usage: $0 <host> <username> <password> [extra freerdp args...]"
  exit 1
fi

shift 3

if [[ ! -x "${CLIENT_BIN}" ]]; then
  cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DWITH_CLIENT=ON \
    -DWITH_CLIENT_SDL=ON \
    -DWITH_CLIENT_SDL3=ON \
    -DWITH_CLIENT_SDL2=OFF \
    -DWITH_SERVER=OFF \
    -DWITH_X11=OFF \
    -DWITH_WAYLAND=OFF \
    -DWITH_MANPAGES=OFF
  cmake --build "${BUILD_DIR}" --target sdl3-freerdp -j4
fi

exec "${CLIENT_BIN}" \
  "/v:${HOST}" \
  "/u:${USERNAME}" \
  "/p:${PASSWORD}" \
  /cert:ignore \
  /gfx \
  +clipboard \
  /dynamic-resolution \
  "$@"
