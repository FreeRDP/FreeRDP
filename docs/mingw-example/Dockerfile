FROM ubuntu:22.04 as builder
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq
RUN apt-get install -y xz-utils wget make nasm git ninja-build autoconf automake libtool texinfo help2man yasm gcc pkg-config

# SETUP WORKSPACE
WORKDIR /tmp
# RUN wget https://github.com/mstorsjo/llvm-mingw/releases/download/20230320/llvm-mingw-20230320-ucrt-ubuntu-18.04-x86_64.tar.xz -O llvm.tar.xz && \
RUN wget https://github.com/mstorsjo/llvm-mingw/releases/download/20230320/llvm-mingw-20230320-msvcrt-ubuntu-18.04-x86_64.tar.xz -O llvm.tar.xz && \
  tar -xf llvm.tar.xz && \
  cp -a /tmp/llvm-mingw-20230320-msvcrt-ubuntu-18.04-x86_64/* /usr/ && \
  rm -rf /tmp/*

RUN mkdir /src
WORKDIR /src

# SETUP TOOLCHAIN
RUN mkdir /src/patch
ARG ARCH
ENV TOOLCHAIN_ARCH=$ARCH

FROM builder as cmake-builder
RUN apt-get install -y cmake
COPY toolchain/cmake /src/toolchain/cmake
ENV TOOLCHAIN_NAME=$TOOLCHAIN_ARCH-w64-mingw32
ENV TOOLCHAIN_CMAKE=/src/toolchain/cmake/$TOOLCHAIN_NAME-toolchain.cmake

FROM builder as meson-builder
RUN apt-get install -y meson
COPY toolchain/meson /src/toolchain/meson
ENV TOOLCHAIN_MESON=/src/toolchain/meson/$TOOLCHAIN_ARCH.txt

# BUILD ZLIB
FROM cmake-builder AS zlib-build
RUN git clone https://github.com/madler/zlib.git /src/zlib
WORKDIR /src/zlib
RUN git fetch; git checkout 04f42ceca40f73e2978b50e93806c2a18c1281fc
RUN mkdir /src/zlib/build
WORKDIR /src/zlib/build
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_CMAKE -G Ninja -Wno-dev -DCMAKE_INSTALL_PREFIX=/build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build . -j `nproc`
RUN cmake --install .

# BUILD OPENSSL
FROM cmake-builder AS openssl-build
RUN git clone https://github.com/janbar/openssl-cmake.git /src/openssl
WORKDIR /src/openssl
RUN mkdir /src/openssl/build
WORKDIR /src/openssl/build
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_CMAKE -G Ninja -Wno-dev -DCMAKE_INSTALL_PREFIX=/build \
             -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
RUN cmake --build . -j `nproc`
RUN cmake --install . 

# BUILD OPENH264
FROM meson-builder AS openh264-build
RUN git clone https://github.com/cisco/openh264 /src/openh264
WORKDIR /src/openh264
RUN git fetch; git checkout 0a48f4d2e9be2abb4fb01b4c3be83cf44ce91a6e
RUN mkdir /src/openh264/out
WORKDIR /src/openh264/out
RUN meson .. . --cross-file $TOOLCHAIN_MESON --prefix=/build
RUN ninja -j `nproc`
RUN ninja install

# # BUILD LIBUSB
FROM cmake-builder AS libusb-build
RUN git clone https://github.com/libusb/libusb.git /src/libusb
WORKDIR /src/libusb
RUN git fetch; git checkout 4239bc3a50014b8e6a5a2a59df1fff3b7469543b
RUN mkdir m4; autoreconf -ivf
RUN sed -i.bak "s/-mwin32//g" ./configure
RUN sed -i.bak "s/--add-stdcall-alias//g" ./configure
RUN ./configure --host=$TOOLCHAIN_NAME --prefix=/build
RUN make -j `nproc` && make install

# BUILD FAAC
FROM cmake-builder AS faac-build
RUN git clone https://github.com/knik0/faac.git /src/faac
WORKDIR /src/faac
RUN git fetch; git checkout 78d8e0141600ac006a94ac6fd5601f599fa5b65b
RUN sed -i.bak "s/-Wl,--add-stdcall-alias//g" ./libfaac/Makefile.am
RUN mkdir m4; autoreconf -ivf
RUN sed -i.bak "s/-mwin32//g" ./configure
RUN ./configure --host=$TOOLCHAIN_NAME --prefix=/build
RUN make -j `nproc` && make install

# BUILD FAAD2
FROM cmake-builder AS faad2-build
RUN git clone https://github.com/knik0/faad2.git /src/faad2
WORKDIR /src/faad2
RUN git fetch; git checkout 3918dee56063500d0aa23d6c3c94b211ac471a8c
RUN sed -i.bak "s/-Wl,--add-stdcall-alias//g" ./libfaad/Makefile.am
RUN mkdir m4; autoreconf -ivf
RUN sed -i.bak "s/-mwin32//g" ./configure
RUN ./configure --host=$TOOLCHAIN_NAME --prefix=/build
RUN make -j `nproc` && make install

# BUILD OPENCL-HEADERS
FROM cmake-builder AS opencl-headers
RUN git clone https://github.com/KhronosGroup/OpenCL-Headers.git /src/opencl-headers
WORKDIR /src/opencl-headers
RUN git fetch; git checkout 4fdcfb0ae675f2f63a9add9552e0af62c2b4ed30
RUN mkdir /src/opencl-headers/build
WORKDIR /src/opencl-headers/build
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_CMAKE -G Ninja -Wno-dev -DCMAKE_INSTALL_PREFIX=/build \
             -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF
RUN cmake --build . -j `nproc`
RUN cmake --install . 

# BUILD OPENCL
FROM cmake-builder AS opencl-build
COPY --from=opencl-headers /build /build
RUN git clone https://github.com/KhronosGroup/OpenCL-ICD-Loader.git /src/opencl
WORKDIR /src/opencl
RUN git fetch; git checkout b1bce7c3c580a8345205cf65fc1a5f55ba9cdb01
RUN echo 'set_target_properties (OpenCL PROPERTIES PREFIX "")' >> CMakeLists.txt
RUN mkdir /src/opencl/build
WORKDIR /src/opencl/build
RUN cmake .. -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_CMAKE -G Ninja -Wno-dev -DCMAKE_INSTALL_PREFIX=/build \
             -DBUILD_SHARED_LIBS=OFF -DOPENCL_ICD_LOADER_DISABLE_OPENCLON12=ON  \
             -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF \
             -DCMAKE_C_FLAGS="${CMAKE_C_FLAGS} -I/build/include/" \
             -DCMAKE_CXX_FLAGS="${CMAKE_CXX_FLAGS} -I/build/include/"
RUN cmake --build . -j `nproc`
RUN cmake --install . 

# BUILD FREERDP
FROM cmake-builder AS freerdp-build
COPY --from=zlib-build /build /build
COPY --from=openssl-build /build /build
COPY --from=openh264-build /build /build
COPY --from=libusb-build /build /build
COPY --from=faac-build /build /build
COPY --from=faad2-build /build /build
COPY --from=opencl-build /build /build
RUN git clone https://github.com/FreeRDP/FreeRDP.git /src/FreeRDP
RUN mkdir /src/FreeRDP/build
WORKDIR /src/FreeRDP/build

ARG ARCH
RUN /bin/bash -c "( [[ $ARCH == aarch64 ]] && printf 'arm64' || printf $ARCH ) > arch.txt"

RUN bash -c "cmake .. -DCMAKE_TOOLCHAIN_FILE=$TOOLCHAIN_CMAKE -G Ninja -Wno-dev -DCMAKE_INSTALL_PREFIX=/build \
             -DWITH_X11=OFF -DWITH_MEDIA_FOUNDATION=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release \
             -DUSE_UNWIND=OFF \
             -DWITH_ZLIB=ON  -DZLIB_INCLUDE_DIR=/build  \
             -DWITH_OPENH264=ON -DOPENH264_INCLUDE_DIR=/build/include -DOPENH264_LIBRARY=/build/lib/libopenh264.dll.a \
             -DOPENSSL_INCLUDE_DIR=/build/include \
             -DWITH_OPENCL=ON -DOpenCL_INCLUDE_DIR=/build/include -DOpenCL_LIBRARIES=/build/lib/OpenCL.a \
             -DLIBUSB_1_INCLUDE_DIRS=/build/include/libusb-1.0 -DLIBUSB_1_LIBRARIES=/build/lib/libusb-1.0.a \
             -DWITH_WINPR_TOOLS=OFF -DWITH_WIN_CONSOLE=ON -DWITH_PROGRESS_BAR=OFF \
             -DWITH_FAAD2=ON -DFAAD2_INCLUDE_DIR=/build/include -DFAAD2_LIBRARY=/build/lib/libfaad.a \
             -DWITH_FAAC=ON -DFAAC_INCLUDE_DIR=/build/include -DFAAC_LIBRARY=/build/lib/libfaac.a \
						 -DCMAKE_SYSTEM_PROCESSOR=$( cat arch.txt ) \
             -DCMAKE_C_FLAGS=\"${CMAKE_C_FLAGS} -static -Wno-error=incompatible-function-pointer-types -DERROR_OPERATION_IN_PROGRESS=0x00000149\" \
						 "
RUN cmake --build . -j `nproc`
RUN cmake --install . 
RUN cp -a /usr/$ARCH-w64-mingw32/bin/* /build/bin;