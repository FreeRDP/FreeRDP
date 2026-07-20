#!/usr/bin/env bash

set -euo pipefail

OPENSSL_TAG="openssl-3.6.1"
OPENSSL_HASH="b1bfedcd5b289ff22aee87c9d600f515767ebf45f77168cb6d64f231f518a82e"
OPENSSL_URL="https://github.com/openssl/openssl/releases/download"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
HARMONY_PREBUILT="${PROJECT_ROOT}/client/Harmony/native/third_party/openssl-prebuilt"

BUILD_ARCH="arm64-v8a"
OHOS_NDK_HOME=""

usage() {
  cat <<EOF
Usage: ${BASH_SOURCE[0]} --ndk <OHOS_NDK_HOME> [--arch arm64-v8a|armeabi-v7a|x86_64]

Cross-compile OpenSSL ${OPENSSL_TAG} for HarmonyOS/OpenHarmony.

Options:
  --ndk   Path to HarmonyOS NDK (e.g. \$OHOS_NDK_HOME)
  --arch  Target architecture (default: arm64-v8a)
  --dst   Output directory (default: ${HARMONY_PREBUILT})
  --help  Show this help

Output per architecture:
  <dst>/<arch>/include/openssl/*.h
  <dst>/<arch>/libssl.a
  <dst>/<arch>/libcrypto.a
EOF
  exit 0
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --ndk) OHOS_NDK_HOME="$2"; shift 2 ;;
    --arch) BUILD_ARCH="$2"; shift 2 ;;
    --dst) HARMONY_PREBUILT="$2"; shift 2 ;;
    --help) usage ;;
    *) echo "Unknown argument: $1"; usage ;;
  esac
done

if [[ -z "${OHOS_NDK_HOME}" ]]; then
  echo "Error: --ndk is required"
  usage
fi

if [[ ! -d "${OHOS_NDK_HOME}/native/llvm/bin" ]]; then
  echo "Error: Invalid OHOS_NDK_HOME: ${OHOS_NDK_HOME}"
  echo "Expected llvm toolchain at: ${OHOS_NDK_HOME}/native/llvm/bin"
  exit 1
fi

case "${BUILD_ARCH}" in
  arm64-v8a)
    TARGET="aarch64-linux-ohos"
    OPENSSL_TARGET="ohos-aarch64"
    ;;
  armeabi-v7a)
    TARGET="armv7a-linux-ohos"
    OPENSSL_TARGET="ohos-arm"
    CFLAGS_EXTRA="-D__ARM_MAX_ARCH__=8"
    ;;
  x86_64)
    TARGET="x86_64-linux-ohos"
    OPENSSL_TARGET="ohos-x86_64"
    ;;
  *)
    echo "Unsupported architecture: ${BUILD_ARCH}"
    exit 1
    ;;
esac

OUTPUT_DIR="${HARMONY_PREBUILT}/${BUILD_ARCH}"
BUILD_DIR="${PROJECT_ROOT}/build-harmony-openssl"
CACHE_DIR="${PROJECT_ROOT}/cache"

log() { printf '[harmony-openssl] %s\n' "$*" >&2; }

run_cmd() {
  log "RUN: $*"
  "$@"
  local rc=$?
  if [[ ${rc} -ne 0 ]]; then
    log "ERROR: command returned ${rc}"
    exit ${rc}
  fi
}

setup_toolchain() {
  local ndk_bin="${OHOS_NDK_HOME}/native/llvm/bin"
  export CC="${ndk_bin}/clang"
  export CXX="${ndk_bin}/clang++"
  export AR="${ndk_bin}/llvm-ar"
  export LD="${ndk_bin}/ld.lld"
  export STRIP="${ndk_bin}/llvm-strip"
  export RANLIB="${ndk_bin}/llvm-ranlib"
  export NM="${ndk_bin}/llvm-nm"
  export CFLAGS="-target ${TARGET} --sysroot=${OHOS_NDK_HOME}/native/sysroot -D__MUSL__ ${CFLAGS_EXTRA:-}"
  export CXXFLAGS="${CFLAGS}"
}

prepare_patch() {
  local patch_file="${SCRIPT_DIR}/harmony-openssl.patch"
  if [[ -f "${patch_file}" ]]; then
    log "Applying HarmonyOS target patch to ${BUILD_DIR}..."
    run_cmd patch -d "${BUILD_DIR}" -p1 -s < "${patch_file}"
  else
    log "ERROR: HarmonyOS target patch not found at ${patch_file}"
    exit 1
  fi
}

download_source() {
  local tarball="${CACHE_DIR}/${OPENSSL_TAG}.tar.gz"
  local sha256_file="${tarball}.sha256"

  run_cmd mkdir -p "${CACHE_DIR}" "${BUILD_DIR}"

  if [[ -f "${tarball}" ]]; then
    log "Using cached ${tarball}"
  else
    log "Downloading ${OPENSSL_URL}/${OPENSSL_TAG}/${OPENSSL_TAG}.tar.gz ..."
    if command -v curl >/dev/null 2>&1; then
      run_cmd curl -L -o "${tarball}" "${OPENSSL_URL}/${OPENSSL_TAG}/${OPENSSL_TAG}.tar.gz"
    elif command -v wget >/dev/null 2>&1; then
      run_cmd wget -O "${tarball}" "${OPENSSL_URL}/${OPENSSL_TAG}/${OPENSSL_TAG}.tar.gz"
    else
      log "ERROR: Neither curl nor wget found"
      exit 1
    fi
  fi

  echo "${OPENSSL_HASH}  ${tarball}" > "${sha256_file}"

  if command -v sha256sum >/dev/null 2>&1; then
    run_cmd sha256sum -c "${sha256_file}"
  elif command -v shasum >/dev/null 2>&1; then
    run_cmd shasum -a 256 -c "${sha256_file}"
  else
    log "WARNING: No sha256sum/shasum found, skipping hash verification"
  fi

  run_cmd rm -rf "${BUILD_DIR}"
  run_cmd mkdir -p "${BUILD_DIR}"
  run_cmd tar xzf "${tarball}" -C "${BUILD_DIR}" --strip-components=1
}

build_openssl() {
  log "Building OpenSSL ${OPENSSL_TAG} for ${BUILD_ARCH} (${OPENSSL_TARGET})..."
  cd "${BUILD_DIR}"

  make clean 2>/dev/null || true

  run_cmd ./Configure "${OPENSSL_TARGET}" \
    --prefix="${OUTPUT_DIR}" \
    --libdir="${OUTPUT_DIR}" \
    --openssldir="${OUTPUT_DIR}" \
    no-shared \
    no-module \
    no-tests \
    no-apps \
    no-docs \
    no-legacy \
    no-dso \
    no-ssl-trace

  run_cmd make build_libs -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

  log "Installing headers and libraries..."
  run_cmd mkdir -p "${OUTPUT_DIR}/include"
  run_cmd cp -R include/openssl "${OUTPUT_DIR}/include/"

  run_cmd cp libssl.a "${OUTPUT_DIR}/"
  run_cmd cp libcrypto.a "${OUTPUT_DIR}/"

  local third_party_dir="${HARMONY_PREBUILT}/.."
  if [[ -f "${third_party_dir}/NOTICE" ]]; then
    run_cmd cp "${third_party_dir}/NOTICE" "${OUTPUT_DIR}/"
  fi

  log "Done: ${OUTPUT_DIR}"
  ls -lh "${OUTPUT_DIR}/libssl.a" "${OUTPUT_DIR}/libcrypto.a"
  log "OpenSSL build complete for ${BUILD_ARCH}"
}

main() {
  log "Building OpenSSL for HarmonyOS: arch=${BUILD_ARCH}"

  download_source
  prepare_patch
  setup_toolchain
  build_openssl

  log "Success! Output at: ${OUTPUT_DIR}"
}

main
