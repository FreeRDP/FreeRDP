#!/usr/bin/env bash

set -euo pipefail

log()
{
	printf '\n==> %s\n' "$*"
}

env_true()
{
	case "${1:-0}" in
		1 | true | TRUE | yes | YES | on | ON)
			return 0
			;;
		*)
			return 1
			;;
	esac
}

prepare_source()
{
	log "Preparing source snapshot"
	rm -rf /work/src
	mkdir -p /work/src
	tar -xzf /input/freerdp-src.tar.gz -C /work/src
	cd /work/src
	printf '%s\n' "${SOURCE_VERSION:-unknown}" >source_version
	printf '%s\n' "${SOURCE_VERSION:-unknown}" >.source_version
}

artifact_dir()
{
	local dir="/logs/artifacts/${IMAGE_SAFE}"
	mkdir -p "$dir"
	printf '%s' "$dir"
}

detect_package_family()
{
	if [[ "${PACKAGE_FAMILY:-auto}" != "auto" ]]; then
		printf '%s\n' "$PACKAGE_FAMILY"
	elif command -v apt-get >/dev/null 2>&1; then
		printf 'debian\n'
	elif command -v pacman >/dev/null 2>&1; then
		printf 'arch\n'
	elif command -v zypper >/dev/null 2>&1 || command -v dnf5 >/dev/null 2>&1 || command -v dnf >/dev/null 2>&1; then
		printf 'rpm\n'
	else
		echo "No supported package manager found" >&2
		return 1
	fi
}
