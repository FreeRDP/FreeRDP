# Copy this template to your project (at the location specified in your project.yaml), or otherwise add it
# parallel to your project.yaml at projects/<name>/Dockerfile.
# This file must set up the environment, install your project's dependencies and build the project.
# The scanner builds it with your repository as the build context, so `COPY . /src` puts the checkout inside
# the image. Network access is available during this Dockerfile build and is removed after this point, so
# all code that an agent might need to perform its tests must be fetched here.
FROM debian:trixie
ENV DEBIAN_FRONTEND=noninteractive \
    CC=clang-22 \
    CXX=clang++-22
RUN sed -i '/^Components:/ s/$/ non-free/' /etc/apt/sources.list.d/debian.sources && \
      echo "deb http://deb.debian.org/debian trixie-backports main contrib non-free" > /etc/apt/sources.list.d/trixie-backports.list && \
			apt update && apt install -y --no-install-recommends \
      ca-certificates build-essential clang-22 llvm-22 cmake ninja-build pkg-config git libkrb5-dev libcjson-dev \
			libx264-dev libgstreamermm-1.0-dev libswscale-dev libcairo2-dev libsoxr-dev libpcsclite-dev libicu-dev libusb-1.0-0-dev \
			libxkbcommon-dev libxkbfile-dev libx11-dev libwayland-dev libxrandr-dev libxi-dev libxrender-dev \
      libxext-dev libxinerama-dev libxfixes-dev libxcursor-dev libxv-dev libxdamage-dev libxtst-dev \
      libcups2-dev libcairo2-dev libpcsclite-dev libasound2-dev libswscale-dev libpulse-dev \
      libavformat-dev libavcodec-dev libavutil-dev libfuse3-dev libgsm1-dev libva-dev \
      libswresample-dev libusb-1.0-0-dev libudev-dev libfaad-dev libsoxr-dev libdbus-glib-1-dev \
      libpam0g-dev uuid-dev libcjson-dev libsdl3-dev libsdl3-ttf-dev libsdl3-image-dev libsystemd-dev \
      libwebkitgtk-6.0-dev liburiparser-dev libopus-dev libwebp-dev libpng-dev libjpeg-dev \
      opensc-pkcs11 libv4l-dev libcbor-dev libfido2-dev ocl-icd-dev ocl-icd-opencl-dev opencl-headers \
      opencl-c-headers libmp3lame-dev libaom-dev libfaac-dev libdav1d-dev libasan8 libyuv-dev libopenh264-dev \
			libfdk-aac-dev libsso-mib-dev libclang-rt-22-dev \
    && rm -rf /var/lib/apt/lists/*

# scanner contract: the checkout lives inside the image, at /src
COPY . /src
WORKDIR /src

# Build the project as its maintainers build it, with debug info (-g) and frame pointers. A step that fails fails the
# build, and primary_contact gets the log.
RUN cmake -S /src -B /src/build -G Ninja \
			--preset full-binaries \
      -DCMAKE_BUILD_TYPE=Debug \
			-DWITH_UNICODE_BUILTIN=ON  \
			-DWITH_DSP_EXPERIMENTAL=ON \
      -DWITH_GFX_AZURE=ON \
      -DWITH_XDG_AAD_AUTH_HELPER=ON \
      -DWITHOUT_WINPR_3x_DEPRECATED=ON \
      -DWITHOUT_FREERDP_3x_DEPRECATED=ON \
			-DBUILD_TESTING=ON \
      -DCMAKE_CTEST_ARGUMENTS="-DExperimentalTest;--output-on-failure;--no-compress-output" \
			-DWITH_SANITIZE_ADDRESS=ON \
     && cmake --build /src/build -j"$(nproc)"

# Run the cheap tests so you know the image works, but do not let a failing test abort the build. Leave anything worth
# exercising (binaries, test drivers, examples) where threat_model.md says it is.
RUN cd /src/build && (ctest --output-on-failure -j"$(nproc)" --timeout 300 || echo "WARNING: ctest reported failures")
