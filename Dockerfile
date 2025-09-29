FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive
ARG TZ=Etc/UTC

# Install build deps (note: remove libssl-dev to avoid distro OpenSSL)
RUN ln -fs /usr/share/zoneinfo/$TZ /etc/localtime && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
      ca-certificates tzdata git build-essential cmake ninja-build pkg-config \
      debhelper cdbs dpkg-dev clang-format ccache perl zlib1g-dev \
      xmlto docbook-xsl xsltproc \
      libxkbfile-dev libx11-dev libwayland-dev libxrandr-dev libxi-dev libxrender-dev \
      libxext-dev libxinerama-dev libxfixes-dev libxcursor-dev libxv-dev libxdamage-dev \
      libxtst-dev libcups2-dev libpcsclite-dev libasound2-dev libpulse-dev libgsm1-dev \
      libusb-1.0-0-dev uuid-dev libxml2-dev libfaad-dev libfaac-dev libsdl2-dev \
      libsdl2-ttf-dev libcjson-dev libpkcs11-helper-dev liburiparser-dev libkrb5-dev \
      libsystemd-dev libfuse3-dev libswscale-dev libcairo2-dev libavutil-dev libavcodec-dev \
      libswresample-dev libwebkit2gtk-4.1-dev libpkcs11-helper1-dev openssl \
      ccache inotify-tools libavformat-dev && \
      mkdir -p /ccache && chown root:root /ccache && ccache --max-size=5G || true

WORKDIR /rdp-proxy

# Build and install OpenSSL 3.1.1 to /usr/local/openssl, register its libs and remove build deps
RUN apt-get update && apt-get install -y --no-install-recommends git perl zlib1g-dev && \
    git clone --depth 1 -b openssl-3.1.1 https://github.com/openssl/openssl.git /rdp-proxy/openssl && \
    cd /rdp-proxy/openssl && \
    ./Configure linux-x86_64 --prefix=/usr/local/openssl --openssldir=/usr/local/openssl shared zlib && \
    make -j"$(nproc)" && make install_sw && \
    echo "/usr/local/openssl/lib" > /etc/ld.so.conf.d/openssl.conf && ldconfig && \
    rm -rf /rdp-proxy/openssl

ENV OPENSSL_ROOT_DIR=/usr/local/openssl
ENV LD_LIBRARY_PATH=/usr/local/openssl/lib
ENV PATH=/usr/local/openssl/bin:$PATH

RUN mkdir -p /etc/freerdp-proxy/certs && \
    /usr/bin/openssl req -x509 -newkey rsa:4096 -days 3650 -nodes \
      -subj "/C=US/ST=State/L=City/O=ExampleOrg/OU=Proxy/CN=freerdp-proxy" \
      -keyout /etc/freerdp-proxy/certs/privkey.pem \
      -out /etc/freerdp-proxy/certs/cert.pem && \
    chmod 600 /etc/freerdp-proxy/certs/privkey.pem && chmod 644 /etc/freerdp-proxy/certs/cert.pem

# Copy your cloned FreeRDP repo into the image (build context must contain the repo root)
COPY . /rdp-proxy
# If you prefer to git clone inside the image, replace the COPY with a git clone step.

# Configure and build only the proxy target noninteractively
RUN cmake -S . -B build -G Ninja \
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
      -DWITH_FFMPEG=OFF -DWITH_SWSCALE=OFF \
      -DOPENSSL_ROOT_DIR=/usr/local/openssl \
      -DCMAKE_PREFIX_PATH=/usr/local/openssl && \
    cmake --build build --parallel $(nproc) --target freerdp-proxy

ENTRYPOINT [ "./build.sh" ]
