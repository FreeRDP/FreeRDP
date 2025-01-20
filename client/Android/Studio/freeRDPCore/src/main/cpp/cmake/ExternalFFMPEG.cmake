include(ExternalProject)

ExternalProject_Add(
  ffmpeg
  DOWNLOAD_COMMAND
	  GIT_REPOSITORY https://git.ffmpeg.org/ffmpeg.git
	  GIT_TAG        n7.1
  CONFIGURE_COMMAND
  	mkdir -p <BINARY_DIR>
  	cd <BINARY_DIR>
	<SOURCE_DIR>/configure \
        --cross-prefix="${BUILD_HOST}-" \
        --sysroot="${ANDROID_NDK}/toolchains/llvm/prebuilt/${TOOLCHAIN}/sysroot" \
        --arch="${TARGET_ARCH}" \
        --cpu="${TARGET_CPU}" \
        --cc="${CC}" \
        --cxx="${CXX}" \
        --ar="${AR}" \
        --nm="${NM}" \
        --ranlib="${RANLIB}" \
        --strip="${STRIP}" \
        --extra-ldflags="${LDFLAGS}" \
	--prefix=<INSTALL_DIR> \
        --pkg-config="${HOST_PKG_CONFIG_PATH}" \
        --target-os=android \
        ${ARCH_OPTIONS} \
        --enable-cross-compile \
        --enable-pic \
        --enable-jni --enable-mediacodec \
        --enable-shared \
        --disable-vulkan \
        --disable-stripping \
        --disable-programs --disable-doc --disable-avdevice --disable-avfilter --disable-avformat
  BUILD_COMMAND
	  make -j -C <BINARY_DIR>
  INSTALL_COMMAND
	  make -j -C <BINARY_DIR> install
)

