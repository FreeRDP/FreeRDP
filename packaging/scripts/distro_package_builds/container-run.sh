#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=common.sh
. "$SCRIPT_DIR/common.sh"

main()
{
	local family
	local mode=${MODE:-package}

	if [[ -r /etc/os-release ]]; then
		# shellcheck disable=SC1091
		. /etc/os-release
	fi

	log "Image: ${IMAGE_NAME}"
	log "OS: ${PRETTY_NAME:-unknown}"

	family=$(detect_package_family)
	case "$family" in
		debian | rpm | arch)
			# shellcheck disable=SC1090
			. "$SCRIPT_DIR/$family.sh"
			;;
		*)
			echo "Unsupported package family: $family" >&2
			return 1
			;;
	esac

	case "$mode" in
		probe)
			package_probe
			;;
		bootstrap)
			package_bootstrap
			;;
		deps)
			package_deps
			;;
		package)
			package_build
			;;
		*)
			echo "Unsupported MODE: $mode" >&2
			return 1
			;;
	esac
}

main "$@"
