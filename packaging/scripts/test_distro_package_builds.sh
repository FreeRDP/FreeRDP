#!/usr/bin/env bash

set -uo pipefail

SCRIPT_PATH=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
REPO_ROOT=$(cd -- "$SCRIPT_PATH/../.." && pwd)

# the order here is rolling for each family and then each family is sorted new to old
DEFAULT_IMAGES=(
	"archlinux:base-devel"
	"debian:sid-slim"
	"opensuse/tumbleweed:latest"
	"fedora:rawhide"
	"almalinux:10-kitten"
	"ubuntu:26.04"
	"ubuntu:24.04"
	"ubuntu:22.04"
    "ubuntu:20.04"
	"fedora:44"
    "fedora:43"
	"almalinux:10"
    "almalinux:9"
	"almalinux:8"
	"opensuse/leap:16.0"
	"opensuse/leap:15"
	"debian:bullseye-slim"
    "debian:bookworm-slim"
    "debian:trixie-slim"
	"debian:forky-slim"
)

IMAGES=()
LOG_DIR="$REPO_ROOT/build/docker-package-tests"
PULL_POLICY="missing"
RPM_AV1="auto"
MODE="package"
JOBS=1
STOP_ON_FAIL=0
LIST_ONLY=0
TIMEOUT_SECONDS=0

usage()
{
	cat <<EOF
Usage: ${0##*/} [options]

Build the nightly Debian/RPM/Arch packages inside locally available Docker distro
images. The source is a snapshot of the current working tree, so uncommitted
changes are tested. The script writes a generated Dockerfile per image and uses
cached bootstrap/build-dependency stages to speed up repeated runs.

Options:
  --image IMAGE        Test one image. Can be repeated.
  --images LIST        Comma-separated images to test.
  --log-dir DIR        Directory for per-image logs and artifacts.
                       Default: build/docker-package-tests
  --pull POLICY        Docker run pull policy: never, missing, always.
                       Default: missing
  --mode MODE          package, deps, or probe. deps builds the cached
                       BuildRequires stage. probe only validates container
                       startup, OS detection, and source snapshot access.
                       Default: package
  --jobs N             Run up to N images in parallel. Output still goes to
                       per-image logs. Default: 1
  --timeout SECONDS    Timeout per image. 0 disables timeout. Default: 0
  --stop-on-fail       Stop after the first failing image. With --jobs, stops
                       after the current batch finishes.
  --list               List candidate images and whether they exist locally.
  -h, --help           Show this help.

Examples:
  packaging/scripts/test_distro_package_builds.sh --list
  packaging/scripts/test_distro_package_builds.sh --mode probe
  packaging/scripts/test_distro_package_builds.sh --mode deps --jobs 4
  packaging/scripts/test_distro_package_builds.sh --image fedora:rawhide
EOF
}

add_images()
{
	local list=$1
	local image
	local -a parsed_images

	IFS=',' read -r -a parsed_images <<<"$list"
	for image in "${parsed_images[@]}"; do
		if [[ -n "$image" ]]; then
			IMAGES+=("$image")
		fi
	done
}

while [[ $# -gt 0 ]]; do
	case "$1" in
		--image)
			if [[ $# -lt 2 ]]; then
				echo "--image requires an argument" >&2
				exit 2
			fi
			IMAGES+=("$2")
			shift 2
			;;
		--images)
			if [[ $# -lt 2 ]]; then
				echo "--images requires an argument" >&2
				exit 2
			fi
			add_images "$2"
			shift 2
			;;
		--log-dir)
			if [[ $# -lt 2 ]]; then
				echo "--log-dir requires an argument" >&2
				exit 2
			fi
			LOG_DIR=$2
			shift 2
			;;
		--pull)
			if [[ $# -lt 2 ]]; then
				echo "--pull requires an argument" >&2
				exit 2
			fi
			PULL_POLICY=$2
			shift 2
			;;
		--mode)
			if [[ $# -lt 2 ]]; then
				echo "--mode requires an argument" >&2
				exit 2
			fi
			MODE=$2
			shift 2
			;;
		--jobs)
			if [[ $# -lt 2 ]]; then
				echo "--jobs requires an argument" >&2
				exit 2
			fi
			JOBS=$2
			shift 2
			;;
		--timeout)
			if [[ $# -lt 2 ]]; then
				echo "--timeout requires an argument" >&2
				exit 2
			fi
			TIMEOUT_SECONDS=$2
			shift 2
			;;
		--stop-on-fail)
			STOP_ON_FAIL=1
			shift
			;;
		--list)
			LIST_ONLY=1
			shift
			;;
		-h | --help)
			usage
			exit 0
			;;
		*)
			echo "Unknown option: $1" >&2
			usage >&2
			exit 2
			;;
	esac
done

case "$PULL_POLICY" in
	never | missing | always)
		;;
	*)
		echo "--pull must be one of: never, missing, always" >&2
		exit 2
		;;
esac

case "$MODE" in
	package | deps | probe)
		;;
	*)
		echo "--mode must be one of: package, deps, probe" >&2
		exit 2
		;;
esac

if ! [[ "$TIMEOUT_SECONDS" =~ ^[0-9]+$ ]]; then
	echo "--timeout must be an integer number of seconds" >&2
	exit 2
fi

if ! [[ "$JOBS" =~ ^[1-9][0-9]*$ ]]; then
	echo "--jobs must be a positive integer" >&2
	exit 2
fi

if [[ ${#IMAGES[@]} -eq 0 ]]; then
	IMAGES=("${DEFAULT_IMAGES[@]}")
fi

image_safe_name()
{
	local image=$1
	image=${image//\//_}
	image=${image//:/_}
	image=${image//[^A-Za-z0-9_.-]/_}
	printf '%s' "$image"
}

have_image()
{
	docker image inspect "$1" >/dev/null 2>&1
}

if [[ "$LIST_ONLY" -eq 1 ]]; then
	for image in "${IMAGES[@]}"; do
		if have_image "$image"; then
			printf 'present  %s\n' "$image"
		else
			printf 'missing  %s\n' "$image"
		fi
	done
	exit 0
fi

if ! command -v docker >/dev/null 2>&1; then
	echo "docker is required" >&2
	exit 1
fi

if [[ "$TIMEOUT_SECONDS" -gt 0 ]] && ! command -v timeout >/dev/null 2>&1; then
	echo "timeout is required when --timeout is used" >&2
	exit 1
fi

TMP_ROOT=$(mktemp -d "${TMPDIR:-/tmp}/freerdp-distro-builds.XXXXXX")
cleanup()
{
	rm -rf "$TMP_ROOT"
}
trap cleanup EXIT

mkdir -p "$LOG_DIR"

SOURCE_ARCHIVE="$TMP_ROOT/freerdp-src.tar.gz"
SOURCE_LIST="$TMP_ROOT/source-files.list"
DEPS_SOURCE_ARCHIVE="$TMP_ROOT/freerdp-deps-src.tar.gz"
DEPS_SOURCE_LIST="$TMP_ROOT/deps-source-files.list"
SOURCE_VERSION=${SOURCE_VERSION:-}
if [[ -z "$SOURCE_VERSION" ]]; then
	SOURCE_VERSION=$(git -C "$REPO_ROOT" rev-parse --short HEAD 2>/dev/null || printf unknown)
fi

(
	cd "$REPO_ROOT" || exit 1
	if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
		while IFS= read -r -d '' path; do
			case "$path" in
				debian | debian/* | build | build/* | build-*)
					continue
					;;
			esac
			if [[ -e "$path" ]]; then
				printf '%s\0' "$path"
			fi
		done < <(git ls-files -co --exclude-standard -z) >"$SOURCE_LIST"
	else
		find . \
			\( -path './.git' -o -path './debian' -o -path './build' -o -path './build-*' \) -prune -o \
			-type f -print0 >"$SOURCE_LIST"
	fi
	tar --null -czf "$SOURCE_ARCHIVE" --files-from "$SOURCE_LIST"

	while IFS= read -r -d '' path; do
		case "$path" in
			packaging/deb/freerdp-nightly | packaging/deb/freerdp-nightly/* | \
				packaging/rpm/freerdp-nightly.spec | \
				packaging/scripts/arch_PKGBUILD | \
				packaging/scripts/prepare_deb_freerdp-nightly.sh)
				printf '%s\0' "$path"
				;;
		esac
	done <"$SOURCE_LIST" >"$DEPS_SOURCE_LIST"

	tar --null -czf "$DEPS_SOURCE_ARCHIVE" --files-from "$DEPS_SOURCE_LIST"
)

CONTAINER_SCRIPT="$TMP_ROOT/run-package-build.sh"
cat >"$CONTAINER_SCRIPT" <<'CONTAINER_SCRIPT'
#!/usr/bin/env bash

set -euo pipefail

log()
{
	printf '\n==> %s\n' "$*"
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

install_deb_bootstrap()
{
	export DEBIAN_FRONTEND=noninteractive
	log "Installing Debian package-build bootstrap"
	apt-get update
	apt-get install -y --no-install-recommends \
		ca-certificates \
		devscripts \
		dpkg-dev \
		equivs \
		fakeroot \
		git
}

setup_deb_bootstrap()
{
	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BOOTSTRAP:-0}"; then
		log "Using cached Debian package-build bootstrap"
		export DEBIAN_FRONTEND=noninteractive
		return
	fi

	install_deb_bootstrap
}

prepare_deb_control()
{
	log "Preparing generated debian/control"
	./packaging/scripts/prepare_deb_freerdp-nightly.sh
}

install_deb_builddeps()
{
	prepare_deb_control

	log "Installing Debian Build-Depends"
	mk-build-deps -i -r \
		-t "apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends -y" \
		debian/control
}

build_deb()
{
	setup_deb_bootstrap
	prepare_source
	prepare_deb_control

	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
		log "Using cached Debian Build-Depends"
	else
		log "Installing Debian Build-Depends"
		mk-build-deps -i -r \
			-t "apt-get -o Debug::pkgProblemResolver=yes --no-install-recommends -y" \
			debian/control
	fi

	log "Building Debian package"
	dpkg-buildpackage -us -uc -b

	log "Collecting Debian artifacts"
	find .. -maxdepth 1 \( -name '*.deb' -o -name '*.buildinfo' -o -name '*.changes' \) \
		-exec cp -v {} "$(artifact_dir)"/ \;
}

install_arch_bootstrap()
{
	log "Installing Arch package-build bootstrap"
	pacman -Syu --noconfirm --needed \
		base-devel \
		ca-certificates \
		git \
		sudo \
		tar \
		xz

	if ! id -u builduser >/dev/null 2>&1; then
		useradd -m -U builduser
	fi
	printf 'builduser ALL=(ALL) NOPASSWD: ALL\n' >/etc/sudoers.d/builduser
	chmod 0440 /etc/sudoers.d/builduser
}

setup_arch_bootstrap()
{
	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BOOTSTRAP:-0}"; then
		log "Using cached Arch package-build bootstrap"
		return
	fi

	install_arch_bootstrap
}

prepare_arch_build_context()
{
	log "Preparing Arch makepkg context"
	rm -rf /work/arch-build /work/arch-src
	mkdir -p /work/arch-build /work/arch-src/freerdp
	tar -C /work/src -cf - . | tar -C /work/arch-src/freerdp -xf -
	tar -C /work/arch-src -czf /work/arch-build/freerdp-src.tar.gz freerdp
	cp -v /work/src/packaging/scripts/arch_PKGBUILD /work/arch-build/PKGBUILD
	chown -R builduser:builduser /work/arch-build
}

install_arch_builddeps_from_pkgbuild()
{
	local -a packages=()

	log "Installing Arch package dependencies"
	# shellcheck disable=SC1091
	source /work/arch-build/PKGBUILD
	packages=("${depends[@]}" "${makedepends[@]}")
	if [[ ${#packages[@]} -gt 0 ]]; then
		pacman -S --noconfirm --needed "${packages[@]}"
	fi
}

install_arch_builddeps()
{
	setup_arch_bootstrap
	prepare_source
	prepare_arch_build_context
	install_arch_builddeps_from_pkgbuild
}

build_arch()
{
	setup_arch_bootstrap
	prepare_source
	prepare_arch_build_context

	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
		log "Using cached Arch package dependencies"
	else
		install_arch_builddeps_from_pkgbuild
	fi

	log "Building Arch package"
	cd /work/arch-build
	sudo -u builduser \
		env \
		CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-$(nproc)}" \
		FREERDP_SOURCE_ARCHIVE=/work/arch-build/freerdp-src.tar.gz \
		makepkg --noconfirm --needed --cleanbuild --force

	log "Collecting Arch artifacts"
	find /work/arch-build -maxdepth 1 \( -name '*.pkg.tar.*' -o -name '*.src.tar.*' \) \
		-exec cp -v {} "$(artifact_dir)"/ \;
}

configure_dnf_commands()
{
	local dnf_cmd

	dnf_cmd=$(command -v dnf5 || command -v dnf)
	RPM_BUILDDEP_KIND=dnf
	RPM_BUILDDEP_CMD=("${dnf_cmd}" -y builddep --spec)
	RPM_INSTALL_CMD=("${dnf_cmd}" -y install)
}

install_dnf_bootstrap()
{
	local dnf_cmd
	local alma_release_devel=almalinux-release-devel

	dnf_cmd=$(command -v dnf5 || command -v dnf)
	log "Installing RPM package-build bootstrap with ${dnf_cmd}"
	"${dnf_cmd}" -y install dnf-plugins-core "dnf-command(builddep)" || true
	"${dnf_cmd}" -y install \
		ca-certificates \
		findutils \
		git \
		rpm-build \
		tar \
		which \
		xz || true

	if [[ -f /etc/almalinux-release ]]; then
		if [[ "${NAME:-}" == "AlmaLinux Kitten" ]]; then
			alma_release_devel=almalinux-kitten-release-devel
		fi
		"${dnf_cmd}" -y install epel-release || true
		"${dnf_cmd}" -y install "$alma_release_devel" || true
		if [[ "${VERSION_ID:-}" == 8* ]]; then
			"${dnf_cmd}" config-manager --set-enabled powertools || true
		else
			"${dnf_cmd}" config-manager --set-enabled crb || true
		fi
	fi

	configure_dnf_commands
}

configure_zypper_commands()
{
	RPM_INSTALL_CMD=(zypper --non-interactive install --no-recommends)
}

install_zypper_bootstrap()
{
	log "Installing RPM package-build bootstrap with zypper"
	zypper --non-interactive refresh
	zypper --non-interactive install --no-recommends \
		ca-certificates \
		findutils \
		git \
		rpm-build \
		tar \
		which \
		xz

	configure_zypper_commands
}

rpm_av1_args()
{
	case "${RPM_AV1:-auto}" in
		on)
			printf '%s\0%s\0' --with av1
			;;
		off)
			printf '%s\0%s\0' --without av1
			;;
		auto)
			;;
	esac
}

rpm_av1_builddep_args()
{
	case "${RPM_AV1:-auto}" in
		on)
			printf '%s\0%s\0' -D '_with_av1 1'
			;;
		off)
			printf '%s\0%s\0' -D '_without_av1 1'
			;;
		auto)
			;;
	esac
}

normalize_rpm_requirement()
{
	local requirement=$1

	if [[ "$requirement" != \(* && "$requirement" =~ ^([^[:space:]<>=]+)[[:space:]]*(<=|>=|=|<|>) ]]; then
		requirement=${BASH_REMATCH[1]}
	fi

	printf '%s\n' "$requirement"
}

install_rpm_bootstrap()
{
	if command -v zypper >/dev/null 2>&1; then
		install_zypper_bootstrap
	elif command -v dnf5 >/dev/null 2>&1 || command -v dnf >/dev/null 2>&1; then
		install_dnf_bootstrap
	else
		echo "No supported RPM package manager found" >&2
		exit 1
	fi
}

configure_rpm_commands()
{
	if command -v zypper >/dev/null 2>&1; then
		configure_zypper_commands
	elif command -v dnf5 >/dev/null 2>&1 || command -v dnf >/dev/null 2>&1; then
		configure_dnf_commands
	else
		echo "No supported RPM package manager found" >&2
		exit 1
	fi
}

setup_rpm_bootstrap()
{
	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BOOTSTRAP:-0}"; then
		log "Using cached RPM package-build bootstrap"
		configure_rpm_commands
		return
	fi

	install_rpm_bootstrap
}

install_rpm_builddeps()
{
	local -a builddep_args=()
	local -a rpmspec_args=()
	local -a requires=()
	local requirement

	if [[ "${RPM_BUILDDEP_KIND:-}" == dnf ]]; then
		while IFS= read -r -d '' arg; do
			builddep_args+=("$arg")
		done < <(rpm_av1_builddep_args)

		log "Installing RPM BuildRequires"
		"${RPM_BUILDDEP_CMD[@]}" "${builddep_args[@]}" packaging/rpm/freerdp-nightly.spec
		return
	fi

	while IFS= read -r -d '' arg; do
		rpmspec_args+=("$arg")
	done < <(rpm_av1_builddep_args)

	log "Installing RPM BuildRequires"
	while IFS= read -r requirement; do
		requirement=$(normalize_rpm_requirement "$requirement")
		requires+=("$requirement")
	done < <(
		rpmspec "${rpmspec_args[@]}" -q --buildrequires packaging/rpm/freerdp-nightly.spec |
			sed -E -e '/^[[:space:]]*$/d' |
			sed -E -e '/^(and|or)$/d' |
			sort -u
	)

	if [[ ${#requires[@]} -gt 0 ]]; then
		"${RPM_INSTALL_CMD[@]}" "${requires[@]}"
	fi
}

build_rpm()
{
	local artifact
	local rpm_topdir="$HOME/rpmbuild"
	local -a rpm_args=()

	setup_rpm_bootstrap
	prepare_source

	if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
		log "Using cached RPM BuildRequires"
	else
		install_rpm_builddeps
	fi

	while IFS= read -r -d '' arg; do
		rpm_args+=("$arg")
	done < <(rpm_av1_args)

	log "Preparing RPM source archive"
	mkdir -p "$rpm_topdir/BUILD" "$rpm_topdir/BUILDROOT" "$rpm_topdir/RPMS" \
		"$rpm_topdir/SOURCES" "$rpm_topdir/SPECS" "$rpm_topdir/SRPMS"
	rm -rf /work/rpm-src
	mkdir -p /work/rpm-src/freerdp-nightly-3.0
	tar -C /work/src -cf - . | tar -C /work/rpm-src/freerdp-nightly-3.0 -xf -
	tar -C /work/rpm-src -cjf "$rpm_topdir/SOURCES/freerdp-nightly-3.0.tar.bz2" freerdp-nightly-3.0
	cp -v /work/src/source_version "$rpm_topdir/SOURCES/source_version"

	log "Building RPM package"
	rpmbuild -ba --define "_topdir $rpm_topdir" "${rpm_args[@]}" packaging/rpm/freerdp-nightly.spec

	log "Collecting RPM artifacts"
	while IFS= read -r -d '' artifact; do
		cp -v "$artifact" "$(artifact_dir)"/
	done < <(find "$rpm_topdir" \( -name '*.rpm' -o -name '*.src.rpm' \) -print0)
}

main()
{
	if [[ -r /etc/os-release ]]; then
		# shellcheck disable=SC1091
		. /etc/os-release
	fi

	log "Image: ${IMAGE_NAME}"
	log "OS: ${PRETTY_NAME:-unknown}"

	if [[ "${MODE:-package}" == "bootstrap" ]]; then
		if command -v apt-get >/dev/null 2>&1; then
			install_deb_bootstrap
		elif command -v pacman >/dev/null 2>&1; then
			install_arch_bootstrap
		elif command -v zypper >/dev/null 2>&1 || command -v dnf5 >/dev/null 2>&1 || command -v dnf >/dev/null 2>&1; then
			install_rpm_bootstrap
		else
			echo "No supported package manager found" >&2
			exit 1
		fi
		return 0
	fi

	if [[ "${MODE:-package}" == "probe" ]]; then
		log "Package manager: $(command -v apt-get || command -v pacman || command -v zypper || command -v dnf5 || command -v dnf || printf unknown)"
		log "Checking source snapshot"
		test -s /input/freerdp-src.tar.gz
		return 0
	fi

	if [[ "${MODE:-package}" == "deps" ]]; then
		if command -v apt-get >/dev/null 2>&1; then
			setup_deb_bootstrap
			prepare_source
			if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
				log "Using cached Debian Build-Depends"
			else
				install_deb_builddeps
			fi
		elif command -v pacman >/dev/null 2>&1; then
			if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
				log "Using cached Arch package dependencies"
			else
				install_arch_builddeps
			fi
		elif command -v zypper >/dev/null 2>&1 || command -v dnf5 >/dev/null 2>&1 || command -v dnf >/dev/null 2>&1; then
			setup_rpm_bootstrap
			prepare_source
			if env_true "${FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS:-0}"; then
				log "Using cached RPM BuildRequires"
			else
				install_rpm_builddeps
			fi
		else
			echo "No supported package manager found" >&2
			exit 1
		fi
		return 0
	fi

	if command -v apt-get >/dev/null 2>&1; then
		build_deb
	elif command -v pacman >/dev/null 2>&1; then
		build_arch
	elif command -v zypper >/dev/null 2>&1 || command -v dnf5 >/dev/null 2>&1 || command -v dnf >/dev/null 2>&1; then
		build_rpm
	else
		echo "No supported package manager found" >&2
		exit 1
	fi
}

main "$@"
CONTAINER_SCRIPT

chmod +x "$CONTAINER_SCRIPT"

declare -A RESULTS=()
failures=0
tested=0
skipped=0
batch_failures=0
stop_requested=0

target_for_mode()
{
	case "$MODE" in
		probe)
			printf 'probe'
			;;
		deps)
			printf 'builddeps'
			;;
		package)
			printf 'package'
			;;
	esac
}

write_dockerfile()
{
	local dockerfile=$1

cat >"$dockerfile" <<'DOCKERFILE'
ARG BASE_IMAGE=scratch

FROM ${BASE_IMAGE} AS probe
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ARG IMAGE_NAME
ARG IMAGE_SAFE
ARG RPM_AV1
ARG SOURCE_VERSION
ENV IMAGE_NAME=${IMAGE_NAME}
ENV IMAGE_SAFE=${IMAGE_SAFE}
ENV RPM_AV1=${RPM_AV1}
ENV SOURCE_VERSION=${SOURCE_VERSION}
WORKDIR /work
COPY run-package-build.sh /input/run-package-build.sh
COPY freerdp-src.tar.gz /input/freerdp-src.tar.gz
RUN chmod +x /input/run-package-build.sh && \
	mkdir -p /input /logs /work && \
	MODE=probe /bin/bash /input/run-package-build.sh

FROM ${BASE_IMAGE} AS bootstrap
SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ARG IMAGE_NAME
ARG IMAGE_SAFE
ARG RPM_AV1
ARG SOURCE_VERSION
ENV IMAGE_NAME=${IMAGE_NAME}
ENV IMAGE_SAFE=${IMAGE_SAFE}
ENV RPM_AV1=${RPM_AV1}
ENV SOURCE_VERSION=${SOURCE_VERSION}
WORKDIR /work
COPY run-package-build.sh /input/run-package-build.sh
RUN chmod +x /input/run-package-build.sh && \
	mkdir -p /input /logs /work && \
	MODE=bootstrap /bin/bash /input/run-package-build.sh

FROM bootstrap AS builddeps
COPY freerdp-deps-src.tar.gz /input/freerdp-src.tar.gz
ENV FREERDP_PACKAGE_TEST_SKIP_BOOTSTRAP=1
RUN FREERDP_SKIP_WEBVIEW_CLONE=1 MODE=deps /bin/bash /input/run-package-build.sh

FROM builddeps AS package
COPY freerdp-src.tar.gz /input/freerdp-src.tar.gz
ENV FREERDP_PACKAGE_TEST_SKIP_BOOTSTRAP=1
ENV FREERDP_PACKAGE_TEST_SKIP_BUILDDEPS=1
RUN rm -rf /logs/artifacts && MODE=package /bin/bash /input/run-package-build.sh
DOCKERFILE
}

prepare_docker_context()
{
	local context_dir=$1
	local dockerfile_copy=$2

	rm -rf "$context_dir"
	mkdir -p "$context_dir"
	cp "$CONTAINER_SCRIPT" "$context_dir/run-package-build.sh"
	cp "$SOURCE_ARCHIVE" "$context_dir/freerdp-src.tar.gz"
	cp "$DEPS_SOURCE_ARCHIVE" "$context_dir/freerdp-deps-src.tar.gz"
	write_dockerfile "$context_dir/Dockerfile"
	cp "$context_dir/Dockerfile" "$dockerfile_copy"
}

extract_package_artifacts()
{
	local image_tag=$1
	local safe=$2
	local cid
	local artifacts_root="$LOG_DIR/artifacts"

	if [[ -e "$artifacts_root" && ! -w "$artifacts_root" ]]; then
		printf 'Artifact root is not writable, using %s\n' "$LOG_DIR/artifacts-local"
		artifacts_root="$LOG_DIR/artifacts-local"
	fi

	mkdir -p "$artifacts_root"
	rm -rf "$artifacts_root/${safe:?}"
	mkdir -p "$artifacts_root/${safe:?}"

	cid=$(docker create "$image_tag")
	if ! docker cp "$cid:/logs/artifacts/$safe/." - | tar --no-same-owner -C "$artifacts_root/$safe" -xf -; then
		docker rm "$cid" >/dev/null
		return 1
	fi
	docker rm "$cid" >/dev/null
}

run_logged_command()
{
	local log_file=$1
	shift

	if [[ "$JOBS" -eq 1 ]]; then
		"$@" 2>&1 | tee -a "$log_file"
		return "${PIPESTATUS[0]}"
	fi

	"$@" >>"$log_file" 2>&1
}

write_result()
{
	local safe=$1
	local status=$2

	printf '%s\n' "$status" >"$TMP_ROOT/result-$safe"
}

run_image()
{
	local image=$1
	local safe
	local log_file
	local context_dir
	local dockerfile_copy
	local image_tag
	local target
	local status
	local -a docker_cmd
	local -a run_cmd

	safe=$(image_safe_name "$image")
	log_file="$LOG_DIR/${safe}.log"
	context_dir="$TMP_ROOT/context-$safe"
	dockerfile_copy="$dockerfile_dir/${safe}.Dockerfile"
	image_tag="freerdp-package-test:${safe}-${RPM_AV1}-${MODE}"
	target=$(target_for_mode)

	{
		printf '\n### Testing %s\n' "$image"
		printf 'Dockerfile: %s\n' "$dockerfile_copy"
		printf 'Docker target: %s\n' "$target"
		printf 'Parallel jobs: %s\n' "$JOBS"
	} >"$log_file"

	prepare_docker_context "$context_dir" "$dockerfile_copy"

	docker_cmd=(
		docker build
		--target "$target"
		--build-arg "BASE_IMAGE=$image"
		--build-arg "IMAGE_NAME=$image"
		--build-arg "IMAGE_SAFE=$safe"
		--build-arg "RPM_AV1=$RPM_AV1"
		--build-arg "SOURCE_VERSION=$SOURCE_VERSION"
		--tag "$image_tag"
	)
	if [[ "$PULL_POLICY" == "always" ]]; then
		docker_cmd+=(--pull)
	fi
	docker_cmd+=("$context_dir")

	if [[ "$TIMEOUT_SECONDS" -gt 0 ]]; then
		run_cmd=(timeout "$TIMEOUT_SECONDS" "${docker_cmd[@]}")
	else
		run_cmd=("${docker_cmd[@]}")
	fi

	if run_logged_command "$log_file" "${run_cmd[@]}"; then
		if [[ "$MODE" == "package" ]]; then
			if ! run_logged_command "$log_file" extract_package_artifacts "$image_tag" "$safe"; then
				write_result "$safe" "FAIL(artifact-copy)"
				return 1
			fi
		fi
		write_result "$safe" "PASS"
		return 0
	else
		status=$?
	fi

	write_result "$safe" "FAIL($status)"
	return "$status"
}

record_result()
{
	local image=$1
	local safe
	local status
	local log_file

	safe=$(image_safe_name "$image")
	log_file="$LOG_DIR/${safe}.log"

	if [[ -r "$TMP_ROOT/result-$safe" ]]; then
		status=$(<"$TMP_ROOT/result-$safe")
	else
		status="FAIL(no-result)"
	fi

	RESULTS["$image"]=$status
	case "$status" in
		PASS)
			printf 'PASS %-35s log: %s\n' "$image" "$log_file"
			;;
		SKIP)
			printf 'SKIP %-35s image not present locally\n' "$image"
			skipped=$((skipped + 1))
			;;
		*)
			printf 'FAIL %-35s status: %s log: %s\n' "$image" "$status" "$log_file"
			failures=$((failures + 1))
			batch_failures=$((batch_failures + 1))
			;;
	esac
}

remove_active_job()
{
	local idx=$1

	unset 'ACTIVE_PIDS[idx]'
	unset 'ACTIVE_IMAGES[idx]'
	ACTIVE_PIDS=("${ACTIVE_PIDS[@]}")
	ACTIVE_IMAGES=("${ACTIVE_IMAGES[@]}")
}

pid_is_running_job()
{
	local pid=$1
	local running_pid

	while IFS= read -r running_pid; do
		if [[ "$running_pid" == "$pid" ]]; then
			return 0
		fi
	done < <(jobs -pr)

	return 1
}

record_one_finished_job()
{
	local idx
	local pid
	local image

	for idx in "${!ACTIVE_PIDS[@]}"; do
		pid=${ACTIVE_PIDS[$idx]}
		image=${ACTIVE_IMAGES[$idx]}
		if ! pid_is_running_job "$pid"; then
			wait "$pid" 2>/dev/null || true
			record_result "$image"
			remove_active_job "$idx"
			return 0
		fi
	done

	return 1
}

wait_for_one_active_job()
{
	if [[ ${#ACTIVE_PIDS[@]} -eq 0 ]]; then
		return 0
	fi

	wait -n "${ACTIVE_PIDS[@]}" || true
	if record_one_finished_job; then
		return 0
	fi

	wait "${ACTIVE_PIDS[0]}" 2>/dev/null || true
	record_result "${ACTIVE_IMAGES[0]}"
	remove_active_job 0
}

wait_for_active_jobs()
{
	batch_failures=0
	while [[ ${#ACTIVE_PIDS[@]} -gt 0 ]]; do
		wait_for_one_active_job
	done
}

dockerfile_dir="$LOG_DIR/dockerfiles"
mkdir -p "$dockerfile_dir"

declare -a ACTIVE_PIDS=()
declare -a ACTIVE_IMAGES=()

for image in "${IMAGES[@]}"; do
	safe=$(image_safe_name "$image")
	log_file="$LOG_DIR/${safe}.log"

	if ! have_image "$image" && [[ "$PULL_POLICY" == "never" ]]; then
		printf 'SKIP %-35s image not present locally\n' "$image" | tee "$log_file"
		write_result "$safe" "SKIP"
		record_result "$image" >/dev/null
		continue
	fi

	tested=$((tested + 1))

	if [[ "$JOBS" -eq 1 ]]; then
		run_image "$image" || true
		record_result "$image"
		if [[ "$STOP_ON_FAIL" -eq 1 && "$batch_failures" -gt 0 ]]; then
			break
		fi
		continue
	fi

	printf 'START %-34s log: %s\n' "$image" "$log_file"
	run_image "$image" &
	ACTIVE_PIDS+=("$!")
	ACTIVE_IMAGES+=("$image")

	while [[ ${#ACTIVE_PIDS[@]} -ge "$JOBS" ]]; do
		wait_for_one_active_job
		if [[ "$STOP_ON_FAIL" -eq 1 && "$batch_failures" -gt 0 ]]; then
			stop_requested=1
			break
		fi
	done
	if [[ "$stop_requested" -eq 1 ]]; then
		break
	fi
done

if [[ ${#ACTIVE_PIDS[@]} -gt 0 ]]; then
	wait_for_active_jobs
fi

printf '\nSummary:\n'
for image in "${IMAGES[@]}"; do
	if [[ -n "${RESULTS[$image]:-}" ]]; then
		printf '  %-35s %s\n' "$image" "${RESULTS[$image]}"
	fi
done
printf '\nTested: %d  Skipped: %d  Failed: %d\n' "$tested" "$skipped" "$failures"
printf 'Logs: %s\n' "$LOG_DIR"

if [[ "$failures" -gt 0 ]]; then
	exit 1
fi

