#!/bin/bash
set -euo pipefail

# Ensure ccache dir exists and limit size
export CCACHE_DIR=/ccache
ccache --max-size=5G || true
command -v ccache >/dev/null 2>&1 || echo "ccache not found, continuing without cache"

# build function
build_target() {
  cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/opt/freerdp \
        -DWITH_PROXY=ON \
        -DWITH_PROXY_APP=ON \
        -DWITH_PROXY_MODULES=ON \
        -DWITH_SERVER=ON \
        -DWITH_CLIENT=OFF \
        -DWITH_CHANNELS=ON \
        -DWITH_CUPS=OFF \
        -DWITH_PRINTER=OFF \
        -DWITH_FFMPEG=OFF \
        -DWITH_SWSCALE=OFF \
        -DOPENSSL_ROOT_DIR=/usr/local/openssl \
        -DCMAKE_PREFIX_PATH=/usr/local/openssl \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache && \
    cmake --build build --parallel "$(nproc)" --target freerdp-proxy && \
    cmake --build build --target proxy-demo-plugin --parallel "$(nproc)" && \
    rm -rf /opt/freerdp/lib/freerdp3/proxy/proxy-demo-plugin.so || true && mkdir -p /opt/freerdp/lib/freerdp3/proxy && \
    cp /rdp-proxy/build/server/proxy/modules/demo/libproxy-demo-plugin.so /opt/freerdp/lib/freerdp3/proxy/proxy-demo-plugin.so
}

# start/stop proxy helpers
PROXY_BIN="./build/server/proxy/cli/freerdp-proxy"
PROXY_ARGS="proxy.ini"
PROXY_PID=""

start_proxy() {
    rm -rf /opt/freerdp/lib/freerdp3/proxy/proxy-demo-plugin.so || true && mkdir -p /opt/freerdp/lib/freerdp3/proxy && \
    cp /rdp-proxy/build/server/proxy/modules/demo/libproxy-demo-plugin.so /opt/freerdp/lib/freerdp3/proxy/proxy-demo-plugin.so && \
    sleep 0.5
  
  if [[ -x "$PROXY_BIN" ]]; then
    "$PROXY_BIN" $PROXY_ARGS &
    PROXY_PID=$!
    echo "Started freerdp-proxy (pid=$PROXY_PID)"
  else
    echo "Proxy binary not found or not executable: $PROXY_BIN"
  fi
}

stop_proxy() {
  if [[ -n "${PROXY_PID:-}" ]]; then
    echo "Stopping freerdp-proxy (pid=$PROXY_PID)"
    kill "$PROXY_PID" 2>/dev/null || true
    wait "$PROXY_PID" 2>/dev/null || true
    PROXY_PID=""
  fi
}

trap 'stop_proxy || true' EXIT

# initial build and start
build_target
echo "Build complete. The freerdp-proxy binary is located in ./build"
start_proxy

# Start file watcher to auto-rebuild on changes under /rdp-proxy/server
if command -v inotifywait >/dev/null 2>&1; then
  echo "Starting watcher: will rebuild on changes under /rdp-proxy/server"
  while true; do
    inotifywait -r -e modify,create,delete,move /rdp-proxy/server >/dev/null 2>&1 || true
    sleep 0.5
    echo "Change detected: incremental rebuild..."
    stop_proxy
    build_target || echo "Rebuild failed"
    start_proxy
    echo "Rebuild finished. Waiting for next change..."
  done
else
  echo "inotifywait not installed; running proxy in foreground"
  exec "$PROXY_BIN" $PROXY_ARGS
fi
