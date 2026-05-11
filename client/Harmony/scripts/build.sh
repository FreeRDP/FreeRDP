#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

log() {
  printf '[build] %s\n' "$1" >&2
}

read_local_property() {
  local properties_file="$1"
  local key="$2"

  if [[ ! -f "${properties_file}" ]]; then
    return 0
  fi

  awk -F= -v property_key="${key}" '
    $1 == property_key {
      sub(/^[^=]*=/, "", $0)
      print
      exit
    }
  ' "${properties_file}"
}

resolve_sdk_dir() {
  local sdk_dir="${DEVECO_SDK_HOME:-}"

  if [[ -z "${sdk_dir}" ]]; then
    local properties_file="${PROJECT_DIR}/local.properties"

    sdk_dir="$(read_local_property "${properties_file}" "hwsdk.dir")"
    if [[ -z "${sdk_dir}" ]]; then
      sdk_dir="$(read_local_property "${properties_file}" "sdk.dir")"
    fi
  fi
  if [[ -z "${sdk_dir}" ]]; then
    sdk_dir="/Applications/DevEco-Studio.app/Contents/sdk"
  fi

  if [[ ! -d "${sdk_dir}" ]]; then
    printf 'Invalid HarmonyOS SDK directory: %s\n' "${sdk_dir}" >&2
    exit 1
  fi

  printf '%s\n' "${sdk_dir}"
}

resolve_hvigor_bin() {
  local sdk_dir="$1"

  if command -v hvigorw >/dev/null 2>&1; then
    command -v hvigorw
    return 0
  fi

  local deveco_root
  deveco_root="$(cd "${sdk_dir}/.." && pwd)"
  local hvigor_bin="${deveco_root}/tools/hvigor/bin/hvigorw"

  if [[ -x "${hvigor_bin}" ]]; then
    printf '%s\n' "${hvigor_bin}"
    return 0
  fi

  printf 'Unable to find hvigorw. Expected: %s or in PATH\n' "${hvigor_bin}" >&2
  exit 1
}

detect_native_changes() {
  if ! command -v git >/dev/null 2>&1; then
    return 0
  fi

  local repo_root
  repo_root="$(cd "${PROJECT_DIR}/../.." && pwd)"

  if ! git -C "${repo_root}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    return 0
  fi

  local changes
  changes="$(
    {
      git -C "${repo_root}" diff --name-only HEAD -- \
        client/Harmony/cmake \
        client/Harmony/native \
        client/Harmony/entry/src/main/cpp 2>/dev/null || true
      git -C "${repo_root}" ls-files --others --exclude-standard -- \
        client/Harmony/cmake \
        client/Harmony/native \
        client/Harmony/entry/src/main/cpp 2>/dev/null || true
    } | awk 'NF && !seen[$0]++'
  )"

  if [[ -n "${changes}" ]]; then
    printf '%s\n' "${changes}"
  fi
}

build() {
  local mode="$1"

  local sdk_dir
  local hvigor_bin

  sdk_dir="$(resolve_sdk_dir)"
  hvigor_bin="$(resolve_hvigor_bin "${sdk_dir}")"

  log "SDK: ${sdk_dir}"
  log "hvigorw: ${hvigor_bin}"
  log "Build mode: ${mode}"

  local mode_file="${PROJECT_DIR}/.build-signing-mode"
  echo "${mode}" > "${mode_file}"
  trap "rm -f '${mode_file}'" RETURN

  local native_changes
  native_changes="$(detect_native_changes)"

  if [[ -n "${native_changes}" ]]; then
    log "Native/CMake changes detected, running clean first"
    printf '%s\n' "${native_changes}"
    (
      cd "${PROJECT_DIR}"
      env -u DEVECO_SDK_HOME -u HOS_SDK_HOME -u OHOS_SDK_HOME -u OHOS_BASE_SDK_HOME \
        DEVECO_SDK_HOME="${sdk_dir}" \
        "${hvigor_bin}" clean
    )
  fi

  log "Building HarmonyOS application package (${mode})"
  (
    cd "${PROJECT_DIR}"
    env -u DEVECO_SDK_HOME -u HOS_SDK_HOME -u OHOS_SDK_HOME -u OHOS_BASE_SDK_HOME \
      DEVECO_SDK_HOME="${sdk_dir}" \
      "${hvigor_bin}" assembleApp -p product=default -p buildMode="${mode}"
  )

  local out_dir="${PROJECT_DIR}/build/outputs/default/${mode}"
  mkdir -p "${out_dir}"

  local entry_build_dir="${PROJECT_DIR}/entry/build/default/outputs/default"

  if [[ -f "${entry_build_dir}/entry-default-signed.hap" ]]; then
    cp "${entry_build_dir}/entry-default-signed.hap" "${out_dir}/"
  fi
  if [[ -f "${entry_build_dir}/entry-default-unsigned.hap" ]]; then
    cp "${entry_build_dir}/entry-default-unsigned.hap" "${out_dir}/"
  fi

  local app_build_dir="${PROJECT_DIR}/build/outputs/default"
  if compgen -G "${app_build_dir}/*-default-signed.app" >/dev/null 2>&1; then
    cp "${app_build_dir}/"*-default-signed.app "${out_dir}/"
  fi
  if compgen -G "${app_build_dir}/*-default-unsigned.app" >/dev/null 2>&1; then
    cp "${app_build_dir}/"*-default-unsigned.app "${out_dir}/"
  fi

  log "Artifacts saved to: ${out_dir}"
  ls -la "${out_dir}/"
}

print_usage() {
  printf 'Usage: %s [debug|release|--release]\n' "${0##*/}"
  printf '\n'
  printf '  debug        Build debug package (default)\n'
  printf '  release      Build release package\n'
  printf '  --release    Same as release\n'
}

main() {
  local build_mode="debug"

  if [[ "${1:-}" == "release" || "${1:-}" == "--release" ]]; then
    build_mode="release"
  elif [[ "${1:-}" == "debug" || "${1:-}" == "" ]]; then
    build_mode="debug"
  elif [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    print_usage
    exit 0
  else
    print_usage
    exit 1
  fi

  build "${build_mode}"
  log "Build completed successfully (${build_mode})"
}

main "$@"
