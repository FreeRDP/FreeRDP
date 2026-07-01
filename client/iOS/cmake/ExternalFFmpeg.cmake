include(ExternalProject)
include(DepVersions)

set(_ff_arch "arm64")

if(PLATFORM MATCHES "SIMULATOR")
  set(_ff_sdk "iphonesimulator")
  set(_ff_min "-mios-simulator-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  set(_ff_triple "-target ${_ff_arch}-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}-simulator")
  set(_ff_platform "iPhoneSimulator")
else()
  set(_ff_sdk "iphoneos")
  set(_ff_min "-miphoneos-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
  set(_ff_triple "")
  set(_ff_platform "iPhoneOS")
endif()

execute_process(
  COMMAND xcrun --sdk ${_ff_sdk} --show-sdk-path OUTPUT_VARIABLE _ff_sysroot OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(_ff_flags "-arch ${_ff_arch} ${_ff_min} -isysroot ${_ff_sysroot} ${_ff_triple}")

set(_ff_fw ${DEPS_INSTALL_DIR}/FFmpeg.framework)

file(
  WRITE ${CMAKE_BINARY_DIR}/FFmpeg-Info.plist
  "<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">
<plist version=\"1.0\"><dict>
  <key>CFBundleDevelopmentRegion</key><string>en</string>
  <key>CFBundleExecutable</key><string>FFmpeg</string>
  <key>CFBundleIdentifier</key><string>org.ffmpeg.FFmpeg</string>
  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>
  <key>CFBundleName</key><string>FFmpeg</string>
  <key>CFBundlePackageType</key><string>FMWK</string>
  <key>CFBundleShortVersionString</key><string>${FFMPEG_VERSION}</string>
  <key>CFBundleVersion</key><string>${FFMPEG_VERSION}</string>
  <key>MinimumOSVersion</key><string>${CMAKE_OSX_DEPLOYMENT_TARGET}</string>
  <key>CFBundleSupportedPlatforms</key><array><string>${_ff_platform}</string></array>
</dict></plist>
"
)

ExternalProject_Add(
  ffmpeg
  PREFIX ${CMAKE_BINARY_DIR}/ffmpeg
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  # Keep sources in the build tree; ExternalProject must not write into client/iOS.
  BINARY_DIR ${CMAKE_BINARY_DIR}/external/ffmpeg
  URL https://github.com/FFmpeg/FFmpeg/archive/refs/tags/${FFMPEG_VERSION}.tar.gz
  URL_HASH ${FFMPEG_HASH}
  CONFIGURE_COMMAND
    <SOURCE_DIR>/configure --prefix=${DEPS_INSTALL_DIR} --enable-cross-compile --target-os=darwin --arch=${_ff_arch}
    --cc=clang --sysroot=${_ff_sysroot} "--extra-cflags=${_ff_flags}" "--extra-ldflags=${_ff_flags}" --disable-gpl
    --enable-static --disable-shared --enable-pic --disable-x86asm --disable-programs --disable-doc --disable-debug
    --disable-network --disable-autodetect --disable-everything --disable-avdevice --disable-avformat --disable-avfilter
    --disable-swscale --disable-swresample --enable-avcodec --enable-avutil --enable-decoder=h264 --enable-parser=h264
    --enable-decoder=hevc --enable-parser=hevc --enable-videotoolbox --enable-hwaccel=h264_videotoolbox
    --enable-hwaccel=hevc_videotoolbox
  BUILD_COMMAND make -j
  INSTALL_COMMAND make -j install
  COMMAND ${CMAKE_COMMAND} -E make_directory ${_ff_fw}/Headers
  COMMAND
    sh -c
    "clang -dynamiclib -arch ${_ff_arch} -isysroot ${_ff_sysroot} ${_ff_min} ${_ff_triple} -install_name @rpath/FFmpeg.framework/FFmpeg -Wl,-force_load,${DEPS_INSTALL_DIR}/lib/libavcodec.a -Wl,-force_load,${DEPS_INSTALL_DIR}/lib/libavutil.a -framework VideoToolbox -framework CoreMedia -framework CoreVideo -framework CoreFoundation -o ${_ff_fw}/FFmpeg"
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${DEPS_INSTALL_DIR}/include/libavcodec ${_ff_fw}/Headers/libavcodec
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${DEPS_INSTALL_DIR}/include/libavutil ${_ff_fw}/Headers/libavutil
  COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_BINARY_DIR}/FFmpeg-Info.plist ${_ff_fw}/Info.plist
  BUILD_BYPRODUCTS ${_ff_fw}/FFmpeg
)
